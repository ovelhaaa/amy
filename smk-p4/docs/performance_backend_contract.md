# PerformanceBackend — contrato musical para Integration 0.4

Auditoria 0.3B sobre `3dd0cf28`, incluindo a correção USB `a31a78bc`.
Assinaturas verificadas em [performance_backend.h](../main/performance_backend.h)
e implementação em [local_performance_backend.h](../main/local_performance_backend.h).
Não descreve uma UI Blueprint já integrada. Regressões executáveis em
[performance_backend_contract_test.cpp](../tests/performance_backend_contract_test.cpp).

## Ownership, tarefas e ordem de serviço

Um único produtor UI utiliza uma instância do backend local e sua fila
`amy::Commands`. O owner AMY consome essa fila; o Host USB produz em OUTRA fila
SPSC (`UsbInput::commands`). Não enviar eventos USB pela fila UI nem chamar o
backend de várias tarefas. A referência `state()` é de armazenamento mutável
do backend: lê-la/copiar na tarefa UI; não compartilhar entre tarefas sem uma
nova estratégia explícita. O backend não é uma interface de acesso ao AMY core.

`standalone_main.cpp` possui mailbox estático de um elemento, sobrescrito pelo
owner sem espera. O loop UI faz `xQueuePeek(...,0)`, `publish(...)`,
`publishMidi(...)`, `serviceDisplayTest()` (LVGL) e `PerformanceUi::service()`,
depois cede cerca de10ms. Logs/heap são consultados a cada5s. O owner tem
prioridade5/pilha12288bytes; USB prioridade4/pilha6144bytes. Não pressupor afinidade
de core: o owner é criado com `xTaskCreate`, não com pinning de core.

Preservar a ordem de boot: owner inicializa/publica snapshot válido antes do
Host qualificado e das alocações LVGL. A UI nova não deve consumir a reserva
interna de boot antes da admissão AMY. Não ler Runtime::snapshot diretamente da
tarefa UI: usar a cópia do mailbox e a publicação do backend.

O port LVGL9.3 inicializa display/touch, tick baseado em `esp_timer`, RGB565,
render parcial e buffer PSRAM de51200bytes; `lv_timer_handler()` é servido nessa
mesma tarefa UI. Callbacks, publicações e chamadas ao backend devem continuar
serializados. A futura UI deve substituir a apresentação antiga, não criar um
segundo produtor musical concorrente. Construir/recriar widgets não deve enviar
patch, master, notas ou Panic automaticamente. Áudio/USB continuam independentes
se display/UI falhar.

## API pública real

| Assinatura | Comportamento no LocalAmyPerformanceBackend |
|---|---|
| `void service(uint32_t now_ms)` | No-op; não bombeia AMY, USB nem mailbox. O loop externo continua necessário. |
| `bool connected() const` | Último `Snapshot::ready` publicado. Não representa cabo USB nem saída I²S física. |
| `bool busy() const` | Sempre false; não é indicador de espaço na fila ou confirmação pendente. |
| `const PerformanceState& state() const` | DTO aplicado mais recente, combinado com a publicação MIDI. |
| `uint32_t stateRevision() const` | Incrementa em CADA `publish`, mesmo com valores idênticos. Não é ACK de comando. Usar comparação `!=`, admitindo wrap. |
| `const char* status() const` | `AMY INIT/ERROR`, `LOCAL AMY / PCM5102A` ou `LOCAL AMY / I2S DISABLED`, conforme ready/flag física. |
| `const char* takeNotice()` | Implementação herdada: nullptr. Não há fila de notices local. |
| `bool local() const` | true. |
| `bool midiError() const` | Último booleano de erro do Host publicado; separado da readiness AMY. |
| `uint16_t patchCount() const` | 2, mesmo antes da engine ficar pronta. |
| `uint16_t patchAt(uint16_t index) const` | Índice0→128, índice1→135; fora do catálogo→UINT16_MAX. |
| `bool patchInfo(uint16_t index, PatchInfo& info, uint32_t now_ms)` | Consulta síncrona estática, preenche id/nome/available=true; índice inválido false, sem comando. `index` não é patch_id. |
| `bool setPatch(uint16_t id, uint32_t now_ms)` | Exige ready e id128/135. Dispara Panic, depois tenta enfileirar Patch. false por fila cheia AINDA pode ter disparado Panic. |
| `bool setMacro(uint8_t index, uint8_t value, uint32_t now_ms)` | Sempre false, sem atualização local nem comando. Todas as macros indisponíveis. |
| `bool setMaster(uint8_t value, uint32_t now_ms)` | Exige ready e0..127; tenta enfileirar Master. Estado aplicado não muda até owner+publicação. |
| `bool noteOn(uint8_t note, uint8_t velocity, uint32_t now_ms)` | Exige ready, nota0..127, velocity0..127. Com velocity>0, cria token e enfileira; pitch já registrado é sucesso idempotente, sem novo comando. Velocity0 chama noteOff e retorna true. |
| `void noteOff(uint8_t note)` | Para pitch0..127 com token local, grava latch prioritário de release e limpa token local; não usa espaço na fila. Sem token, no-op. |
| `void heldNote(uint8_t note)` | No-op. Não renova lease nem confirma que a nota existe no owner. |
| `void panic()` | Incrementa geração UI e limpa tokens locais imediatamente; owner cancela fontes e comandos antigos no próximo apply. Sem retorno/ACK síncrono. |

Não existem `setOctaveShift()`, `setMasterVolume()`, `selectProgram()` ou setters
de parâmetros DSP nessa interface. `PatchInfo` tem `uint16_t id`, `char name[25]`
e `bool available`. Os nomes vêm da tabela `kPatches`, não do catálogo demonstrativo.

## Modelo de leitura e fontes

`PerformanceState` é alias de `smk::link::State` para apresentação; no standalone
não há tráfego UART/S3. Campos desse DTO não adquirem automaticamente o significado
do backend remoto. `publish` recebe cópia do snapshot do owner; `publishMidi`
recebe leituras atômicas do Host/UsbInput, de outro instante, sem snapshot conjunto.

| Campo/necessidade UI | Fonte efetiva local | Limite de interpretação |
|---|---|---|
| `patch_id`, `engine_patch` | Ambos `amy::Snapshot::patch_id` | Identificam banco aplicado; não o pedido ainda na fila. |
| `patch_name` | `kPatches[patchIndex(id)].name`; desconhecido→`ERROR` | Buffer25 com terminador; copiar quando precisar reter o texto. |
| `master` | `amy::Snapshot::master` | Inteiro0..127. Render suaviza ganho;127 corresponde a aproximadamente0,496 FS de master, não ganho unitário. |
| `active_notes` | `amy::Snapshot::active_notes` | Gates lógicos usados USB+Touch, máximo4; não separa fontes nem mede caudas de envelope/DSP. |
| `sample_rate_hz`, `block_frames` | Constantes48000/256 | Configuração, não medição elétrica do clock. |
| `flags & kAudioLive` (2) | `running && physical` do snapshot externo do owner | RAM-only pode continuar ready sem esse bit. |
| `flags & kUsbConnected` (1) | `Host::state()==Connected` | Não inferir a partir de connected() do backend. |
| `midi_events` | `UsbInput::events` | Eventos reconhecidos/roteados no cabo0; pode crescer mesmo se a fila descartar o evento. Consultar drops separadamente. |
| `max_render_us` | `Snapshot` externo: máximo de render medido no owner | Máximo cumulativo; não duração LVGL nem tempo total de serviço. |
| `deadline_misses` | `Snapshot` externo: `render_deadlines` | Somente render; não inclui `service_deadlines`/compute nem DMA underruns. |
| `commands_dropped` | `Commands::overflows()` da UI | Tentativas UI rejeitadas por fila cheia; não soma USB nem rejeição por polifonia. Atualizado em publish. |
| `macros[8]` | Defaults do DTO | Nomes vazios, valor0, supported=false. Não anunciar parâmetros operacionais. |
| `bpm_centi`, `pcm_starvations`, `event_overflows`, `free_internal`, `free_psram` | Não preenchidos por este backend | Zero é indisponibilidade, não medição zero. Não fabricar BPM, heap ou underruns na UI. |
| Disponibilidade engine | `connected()`/snapshot.ready | Sem método `ready()` público adicional. |
| Erro engine | `status()`/connected(), logs owner | Sem código ESP de erro no DTO; não confundir com midiError. |
| Erro/conexões USB | `midiError()` e flags; contadores completos no Host/log USB | Host ERROR transitório pode aguardar confirmação de remoção; o contador errors só cresce se a falha persistir. |

O bit remoto `kControlState=0x80` não é mantido por publish local. Antes da primeira
publicação, não interpretar valores default como estado aplicado. `publishMidi`
NÃO incrementa stateRevision; no loop atual a publicação owner a precede a cada
iteração. Uma integração que filtre tudo por revisão deve manter essa ordem ou
tratar MIDI separadamente; não mudar silenciosamente essa semântica.

Não usar `Snapshot::generation` como ACK de um gesto. O backend observa a geração
atual de Commands para limpar tokens, preservando notas novas diante de um
snapshot pré-Panic atrasado; os testes existentes cobrem essa ordem.

Há mais diagnóstico real fora do DTO: render médio, frames escritos, compute
deadlines, write errors/timeouts/refill risks, stack, heap, LVGL heap, overflow
USB, parse_errors, ignored_cable_packets, polyphony_rejected e recuperação Host.
Usar as fontes do owner/Host/logs indicadas na matriz. Nenhum campo de heap/OOM
de AMY deve ser apresentado como telemetria USB, ou vice-versa.

## Comandos, confirmação e pressão de fila

| Ação UI | Operação | Confirmação observável |
|---|---|---|
| Selecionar patch | `setPatch(id,now_ms)` | Snapshot posterior com patch_id correspondente. Mesmo id também pode silenciar notas. |
| Master | `setMaster(value,now_ms)` | Snapshot posterior com master aplicado; ganho PCM suavizado por amostra. |
| Pressionar nota | `noteOn(note,velocity,now_ms)` | active_notes é agregado; não existe ACK/token público por nota. |
| Liberar nota | `noteOff(note)` | Latch consumido pelo owner; active_notes agregado pode incluir USB/caudas não representadas. |
| Panic | `panic()` | Owner cancela gates/sustain/bend/mod de ambas as fontes e invalida filas antigas. |
| Macro | `setMacro(...)` | Indisponível: false. Widget deve permanecer desabilitado. |
| Oitava | Transformar nota no lado UI | Salvar a nota efetivamente enviada e usar essa mesma nota no release. |

Cada fila SPSC tem32 posições e o owner drena no máximo32 de cada por apply.
`true` de push é aceitação na fila. O owner ainda pode ignorar geração antiga,
Note On antigo (idade≥10s), gate já liberado ou rejeitar por capacidade. Uma
segunda noteOn do mesmo pitch pode retornar true sem push. Não usar retorno como
prova de nota audível. `false` também cobre indisponibilidade/argumentos inválidos
e exaustão de tokens; não há enum público de motivo.

Em fila cheia, Master/Note On retornam false e incrementam overflow. Não ficar em
loop de retry bloqueante. Fazer coalescência de controles na UI e retry em ticks;
cancelar On ainda não aceito quando seu gesto termina. A UI antiga limita
controles a40ms e debounce de patch a150ms, sem impor esses intervalos no backend.
Note Off UI e Panic não competem por posições: release latch e geração atômicos.
No USB, releases/pedal/CC120/123 passam pela fila própria; se um evento crítico
não couber, a geração USB é cancelada para impedir nota presa.

## Identidade e lease: obrigações do teclado futuro

Validar0..127 antes de converter uma nota calculada para uint8_t, evitando wrap
na transposição. O backend aceita pitch0, mas o helper antigo TouchNote::press
rejeita0 e usa teclado mono; ele não é um adapter pronto para a faixa inteira
nem para múltiplos dedos. A oitava antiga usa bases24..84, com tecla superior+12.

Touch usa `(origem UI, pitch, token monotônico, geração)`. Há um token backend
por pitch; duas chamadas de mesma nota não criam dois owners nem refcount.
Uma única noteOff encerra esse gate. A futura UI deve manter ownership por gesto
e contagem por pitch antes de chamar o backend, usando primeiro On/último Off,
inclusive quando um callback antigo chegar após recriação de widgets. A API
não recebe ponteiro/ID de dedo nem devolve handle público de nota.

USB usa `(origem USB, canal0..15, pitch)` em vozes distintas, inclusive quando
Touch toca o mesmo C4. Releases funcionam nas duas ordens. Duplicação de SMK25
no cabo virtual1 é ignorada, não fundida com cabo0. Sustain, bend de±2 semitons
e CC1 (vibrato local5Hz, profundidade máxima0,5semitom) são USB por canal e não
alteram a nota Touch. USB disconnect/erro cancela SOMENTE USB e seus controles;
Panic global e troca de patch cancelam ambas as fontes.

Lease do owner: Touch expira dez segundos após aceitar o On, mesmo que a UI
continue responsiva e chame heldNote. Note On atrasado dez segundos também é
descartado, sem abrir gate. A UI antiga possui seu próprio limite10s na classe
TouchNote; releaseAll ainda deve ser servido para enviar seu Off pendente.
USB já soando não expira por esse lease; notas/sustain USB podem durar mais.

A expiração não confirma individualmente o token ao backend. Após expirar,
noteOn repetida do mesmo pitch continua idempotente e não renova/reinicia a nota
até noteOff/Panic limpar o token local. A futura UI deve exibir/respeitar o cap,
encerrar o gesto e emitir release; não prometer tecla Touch sustentada indefinida.
Não enviar On periódico para contornar o lease. Mudá-lo seria outra decisão de
produto/API, com reprodução, testes e nova validação.

Em RELEASED/PRESS_LOST/cancelamento, navegação, mudança de oitava, perda de foco
ou descarte do teclado, liberar exatamente as notas enviadas e servir releases
antes de destruir o produtor. `TouchNote::disconnect()` limpa somente seu estado
e NÃO envia Off; reservar para engine realmente desconectada/encerrada.
Destroy/recreate do backend/widget também não implica Panic automático. Para
navegar preservando USB, liberar apenas notas UI; usar Panic quando o usuário
realmente solicitar cancelamento global.

## Disponibilidade e decisões para 0.4

Master0 reduz ganho e não libera gates/sustain; usar Panic para cancelamento
global. Clipping no log ENGINE conta saturação digital antes do ganho master,
não é um medidor de amplitude/ruído da saída analógica.

Panic preserva patch e master aplicados; não repor valores demonstrativos na UI
ao acioná-lo. CC genérico pode ser contado em midi_events sem efeito no owner:
somente CC1/64/120/123 têm as ações locais descritas. Pressure, Program Change,
SysEx, clock/transport não são roteados para recursos musicais desse backend.

Operacional: duas ROMs, master, notas Touch limitadas por lease, quatro vozes
compartilhadas, USB Note On/Off/velocity, sustain, bend, CC1, Panic e recuperação
HS. Não operacional: macros, oito knobs mapeados, FX editáveis, mais patches,
arpejador, sequencer, scenes ou parâmetros demonstrativos do Blueprint.

Sem extensão pública necessária nesta rodada. Para0.4, integrar primeiro as ações
acima com um único produtor e validar a matriz. Campos sem fonte devem aparecer
indisponíveis. Se houver requisito real de telemetria adicional ou gestos além
desse contrato, propor extensão compatível com fontes/ownership/ACK e testes
antes de alterar a interface. Nunca chamar AMY diretamente de um widget.
