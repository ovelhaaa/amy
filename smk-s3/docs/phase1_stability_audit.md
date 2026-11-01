# Fase 1 — auditoria de estabilidade fundamental

Data: 2026-11-01. Escopo: USB MIDI, EventBus, ciclo de vida da saída PCM e
observabilidade. Não certifica uso em palco. Resultados físicos permanecem
pendentes; os testes de host usam o código de produção com RTOS/USB/I²S simulados.

## Diagnóstico baseado na entrada desta alteração

| Arquivo / função | Classificação / gravidade | Evidência anterior | Resultado |
| --- | --- | --- | --- |
| `usb_midi_host.cpp::findMidiInterface` | Confirmado / P0, memória | Lia `p[1]` com apenas `p < end`; casts de interface/endpoint sem verificar tamanho mínimo; avanço por bLength sem limite restante. | Walker independente, com span explícito, cabeçalho completo, total validado, comprimentos mínimos e avanço limitado. Valida a configuração inteira, inclusive depois do endpoint escolhido. |
| `findMidiInterface` | Confirmado / P0, protocolo | Qualquer interface vendor-specific com bulk IN era aceita como MIDI. | Aceita somente Audio/MIDIStreaming 1.0, alt zero, bulk IN não zero e MPS full-speed válido. Sem VID/PID presumido. |
| `transferCallback` | Confirmado / P0, memória | Incrementava quatro mesmo com `actual_num_bytes % 4 != 0`. | Confere tamanho contra buffer e pedido; rejeita transfer incompleto inteiro e aciona limpeza/Panic. |
| `begin`, `midiClientTask` | Confirmado / P0, inicialização | Ignorava criação das duas tarefas; registro falho deixava handle da tarefa encerrada. | Checa tarefas/semaforos/install, aguarda resultado explícito do registro e desfaz inicialização com join cooperativo. Não assume propriedade de host já instalado. |
| `handleDeviceConnection`, `startMidiIn` | Confirmado / P0, estado | Conexão era sinalizada antes de alocar/submeter recepção; falha de ressubmissão liberava transfer sem retirar conexão. | Ready somente depois da primeira submissão; erros retiram Ready, sinalizam Panic e drenam recursos. |
| `handleDeviceDisconnection`, `stop` | Confirmado / P0, ciclo de vida | Liberava interface/fechava device com transfer pendente; stop deletava tarefas externamente e não deregistrava cliente. | Um único proprietário USB drena callbacks após halt/flush, libera transfer, interface e device, deregistra cliente; daemon espera dispositivos livres antes de uninstall. |
| `midi_parser.cpp` | Confirmado / P0, validação | CIN/status e bytes de dados podiam divergir; system-common preservava running status; reconexão não limpava parser. | Validação de mensagens de canal, reset de running status em system-common e nos limites da conexão. Curva/callback/contadores preservados. |
| `event_bus.cpp` | Correto / P0 preservado | FIFO único, reserva, latch separado de Panic, geração e reconhecimento condicionado à geração entregue. Se uma liberação não cabe, solicita Panic. | Sem alteração na implementação. Ampliados testes de perda de NoteOff/Stop/AllNotesOff/sustain-off, descarte obsoleto e contagem. |
| `audio_task.cpp::taskRoutine`, `stop` | Risco confirmado / P0, propriedade | Parada assíncrona não oferecia confirmação para destruição; handle escrito pelas duas tarefas sem sincronização. | Flag atômica de atividade separada do pedido de parada; `waitStopped(timeout_ms)` só autoriza destruição depois de stop do DAC e notificação ao engine. Handle passa a ser escrito só no controle. |
| `pcm5102_output.cpp`, `AudioTask` | Correto / P0 preservado | DMA pré-carregado com silêncio; buffer nulo/tamanho inválido e escrita curta/falha encerram saída, mute/disable e notificam engine. | Preservado. Não implementada recuperação automática. Testados timeout, tamanho inválido, encerramento e rejeição de segunda partida enquanto ativo. |
| `amy_commands.cpp` / `AmyAdapter` | Correto com limites / P0 preservado | Proprietário exclusivo do AMY, PCM pré-alocado com release/acquire e gerações, silêncio por starvation, join antes de destruir engine; startWorker não permite segunda partida. | Mantido. Acrescentada regressão que rejeita reinício do worker parado. |
| `diagnostics`, `app_main`, `PCM5102Output::write` | Confirmado / P1, medição | Métricas USB dependiam de evento descartável; `audio_underruns` mede falhas de escrita; store do acumulado local desfazia reset da janela. | Host passa a atualizar USB diretamente; erros/timeouts de escrita e deadlines têm contadores separados; incremento da janela não restaura valor anterior ao reset. Nome legado mantido e significado documentado. |

## Critérios e arquitetura

- Sem novas funcionalidades musicais, dependências declaradas, pinagem ou formatos persistidos.
- AMY core e outras plataformas não foram modificados. Mudanças ficam na aplicação SMK-S3 e seus adaptadores.
- O README raiz já possui marcadores de conflito e conteúdo ESP32-P4. Não foi reescrito nesta estabilização. `specs.md` prevê evolução em etapas; os recursos posteriores já existentes foram preservados conforme o pedido explícito.
- Estado USB: Disconnected → Enumerating → Configuring → Ready; qualquer falha de recepção passa a Recovering, limpa recursos e retorna a Disconnected. Falha de instalação/tarefa/registro é Error. Não há ressubmissão cega de endpoint parado.
- Falha de recepção com dispositivo ainda plugado requer desconectar/reconectar. Não há nova enumeração artificial nem reset improvisado do host.
- Ready significa recepção configurada/submetida, não prova de que o controlador enviou notas.
- `stop()` USB é uma operação de ciclo de vida, chamada por um único controle, fora de callbacks, áudio e ISR. Aguarda conclusão das tarefas, sem force-delete. Cada tarefa continua processando eventos do driver para concluir callbacks; o timeout de 20 ms é da espera de eventos, não delay de sincronização.
- A tarefa cliente é a única que acessa handles, transfers e estado mutável do parser. Estado público, VID/PID, curva e contadores usam atomics. A API pública `parser()` deve ser usada externamente apenas para leituras de diagnósticos; process/reset pertencem ao proprietário USB.
- `AudioTask::stop()` apenas solicita parada. Antes de destruir engine/output, o proprietário deve obter `AudioTask::waitStopped(timeout_ms) == true`. Se retornar false, manter ambos vivos. Serializar start/wait/destruição em uma tarefa de ciclo de vida. `AmyAdapter` mantém sua proibição de reiniciar worker no mesmo estado.
- Não foram adicionados logs, heap ou esperas ao loop de saída PCM. Semáforo de término é criado na inicialização; a saída apenas sinaliza sua conclusão. USB pode alocar transfer em configuração, fora do áudio.

### Limite da API de descritores

`findMidiEndpoint(data, accessible_bytes, result)` nunca lê além do span fornecido.
Os testes usam comprimentos reais, incluindo todos os prefixos truncados.
Na integração, ESP-IDF entrega um descritor de configuração em cache e não
expõe separadamente o tamanho da alocação. O adaptador usa o `wTotalLength`
desse cache como span, assumindo a validade da alocação que pertence ao SDK.
A segurança da enumeração interna do ESP-IDF é uma fronteira externa a esta
auditoria; não se afirma resistência a corrupção da própria memória do SDK.

## Métricas e custo

| Métrica | Significado |
| --- | --- |
| `audio_underruns` / `underrun` legado | Erros ou escritas I²S incompletas; não demonstra underrun DMA. |
| `i2s_write_errors` | Uma ocorrência por chamada com erro, timeout ou tamanho retornado diferente do pedido. |
| `i2s_write_timeouts` | Subconjunto cujo código é ESP_ERR_TIMEOUT. |
| `render_deadline_misses` | Blocos cujo trabalho do proprietário excede frames/sample_rate; no AMY inclui comandos, render e cópia. Não inclui espera da escrita I²S. |
| `synth_pcm_starvations` | Blocos de silêncio por ausência de PCM válido; inclui partida e invalidação por Panic. |
| `usb_submit_failures` | Falha da primeira submissão ou de ressubmissão. |
| `usb_transfer_errors` | Conclusão com status de erro ou comprimento fora do buffer/pedido. Transfer incompleto é contado em midi_parse_errors. |
| `midi_events_dropped` | Evento MIDI cujo send ao EventBus foi rejeitado. |
| `event_queue_overflows` | Rejeições por capacidade; `events_dropped` também inclui eventos invalidados/suprimidos por Panic. |
| `usb_disconnects` | DEV_GONE do device aberto, uma vez por conexão; parada administrativa e erro de recepção não são desconexões físicas. |
| `usb_reconnects` | Sessões com recepção configurada, incluindo a primeira (semântica anterior). |
| `usb_connected` | Estado operacional atualizado pelo host, mesmo se notificação de UI não couber. |

`status` expõe os contadores adicionais; `audio_status` imprime erros I²S,
timeouts e deadlines com `dma_underrun=unobserved`. Não existe contador confiável
de underrun efetivo do DMA nesta integração. `audio_reset` zera a janela de
áudio, sem zerar USB/MIDI. Getter legado do DAC continua cumulativo por objeto.
Não foi medido CPU, jitter, heap livre ou latência analógica nesta máquina.
O custo adicional é fixo: dois semáforos USB em relação ao anterior, um
semáforo de término de áudio e contadores atômicos; sem nova fila PCM.

## Validação automatizada

Comandos da raiz, PowerShell, GCC/G++ no PATH:

```powershell
./tests/run_smk_safety.ps1
./tests/run_smk_amy_boot.ps1
./tests/run_smk_patch_integrity.ps1
./tests/run_smk_synth_expansion.ps1
C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-devkitc-1
git diff --check
```

| Status | Resultado |
| --- | --- |
| PASS | Safety: produção real do parser/EventBus/USB host/AudioTask/PCM5102Output contra RTOS e drivers simulados. Descritores truncados/malformados, endpoint ausente/inválido, CIN/status/dados inválidos, pacotes incompletos, limites negativos/excedidos de transfer, NoteOn/Off/velocity zero, falhas de install/tarefas/register/descriptor/claim/alloc/submit/ressubmit, callback cancelado, reconexão e cleanup com falhas transitórias. |
| PASS | EventBus: ordem FIFO, reserva, dois produtores, ISR simulado, Panic saturado, novas gerações durante reconhecimento, eventos obsoletos, perda de liberações e contagem exata. |
| PASS | Áudio simulado: DMA zerado antes de enable, preload/enable inválido, escrita curta/timeout, buffer nulo/tamanho inválido, fade estéreo, falha ao parar, join, rejeição de segunda partida ativa e deadline excedido. |
| PASS | AMY real: boot/vozes, Panic antes de renderizar, proprietário exclusivo, FIFO/PCM com gerações, concorrência, shutdown e falha de startup; patches/macros/FX/serialização e demais regressões dos scripts acima. Avisos preexistentes de casts ponteiro/inteiro no diagnóstico C do AMY em Windows. |
| PASS | Build ESP32-S3, PlatformIO 55.3.36 / ESP-IDF 5.5.2, bloco 256. Flash reportada: 1.122.267 bytes; RAM estática: 29.708 bytes. Sem baseline comparável nesta versão de toolchain. |
| PASS | `git diff --check`; 20 execuções adicionais de `./build/smk_safety/test_usb.exe`. |
| FAIL | Duas tentativas iniciais de build: bundle x509 ausente por instalação Python inconsistente; resolvido antes do build aprovado. |
| NOT RUN | Builds P4, Arduino, web e demais plataformas; arquivos dessas plataformas não foram alterados. |
| MANUAL | USB e áudio físicos, DMA, VBUS, mute analógico, jitter/latência, soak e consumo na placa. |

A plataforma instalada resolveu e instalou seu ESP-IDF 5.5.2 sem mudança no
`platformio.ini` ou manifesto do projeto. O ambiente Python reportava metadados
`cryptography==44.0.3`, mas continha fontes de 50.0.0 e falhava importando
`mldsa`. Reinstalação do mesmo cache não resolveu; a mesma versão baixada sem
cache resolveu. Comandos de diagnóstico/reparo executados:

```powershell
# No diretório smk-s3/.pio/build/esp32-s3-devkitc-1:
& 'C:/.platformio/penv/.espidf-5.5.2/Scripts/python.exe' 'C:/.platformio/packages/framework-espidf/components/mbedtls/esp_crt_bundle/gen_crt_bundle.py' --input 'C:/.platformio/packages/framework-espidf/components/mbedtls/esp_crt_bundle/cacrt_all.pem' 'C:/.platformio/packages/framework-espidf/components/mbedtls/esp_crt_bundle/cacrt_local.pem'
# Mesmo pacote/versão, sem mudar dependências declaradas:
& 'C:/.platformio/penv/Scripts/uv.exe' pip install --python 'C:/.platformio/penv/.espidf-5.5.2/Scripts/python.exe' --reinstall cryptography==44.0.3
& 'C:/.platformio/penv/Scripts/uv.exe' --no-cache pip install --python 'C:/.platformio/penv/.espidf-5.5.2/Scripts/python.exe' --reinstall cryptography==44.0.3
```

A recompilação gerou alterações automáticas de sdkconfig 5.5.1 para 5.5.2;
essas alterações foram removidas do diff para preservar a configuração
versionada. O próximo build pode regenerá-las com essa instalação do SDK.
Não foram gravadas placas. Bloco 128 e ESP-IDF 5.3 são `NOT RUN` nesta máquina;
a matriz de CI existente permanece, sem consulta a resultados remotos.

Mocks verificam que nenhuma interface/device é liberada antes de completar o
transfer e que nenhuma tarefa é destruída à força. Não substituem a semântica
e a temporização reais do ESP-IDF, USB ou DMA. O teste ISR é uma simulação de
concorrência no host, não execução em interrupção Xtensa.

## Procedimento físico reproduzível — MANUAL, ainda não executado

1. Confirmar revisão N16R8, porta USB nativa versus UART, circuito VBUS, GPIOs existentes em `app_config.h`, straps do módulo PCM5102A e ligação XSMT/mute. Não usar este documento como novo mapa de pinos. DAC ligado a entrada de linha apropriada; não ligar fones diretamente a uma saída de linha sem estágio adequado.
2. Compilar/gravar o firmware e capturar UART. Confirmar sample rate de 48 kHz, tamanho do bloco efetivo e DMA 4 × bloco. Registrar versão do ESP-IDF, binário, cabeamento e fonte. O display/offsets permanecem os já configurados, sem nova confirmação física.
3. Executar `audio_reset`, `audio_status`, `status` e `memory`. Gravar ambos os canais por dez partidas; observar silêncio antes da reprodução e registrar transientes. Digital fade não garante ausência de transiente analógico. Medir WS/BCLK e ordem L/R com instrumentação ou modo de teste de tom existente.
4. Tocar acordes, velocity zero, sustain, pitch extremo/centro e CC por 10 minutos; comparar contadores. Esperar notas liberadas, ausência de falha de escrita e registrar starvations/deadlines com a carga e tamanho de bloco. Não converter zero erros de escrita em prova de zero underruns DMA.
5. Desconectar com acorde/pedal, arpejador e sequenciador ativos; aguardar silêncio sem reativação por evento antigo. Reconectar e tocar. Repetir 100 vezes, depois 20 vezes sob rajadas de CC/notas acima do consumo. Esperar Panic independentemente de overflow, Ready apenas com recepção funcional, sem reboot, memória estabilizada e um DEV_GONE por retirada. Separar caudas de efeitos de notas mantidas.
6. Em bancada USB programável, apresentar descritores sem endpoint, comprimentos curtos, interface vendor-specific e transfer com 1/2/3 bytes restantes. Esperar rejeição segura, contadores correspondentes e possibilidade de nova sessão após retirar/reconectar. Não usar um dispositivo de produção como gerador de falhas elétricas.
7. Para falha I²S, usar firmware de bancada com retorno do driver injetado, sem mudar pinagem de produção. Testar ESP_ERR_TIMEOUT, erro genérico, escrita curta e falha de disable. Esperar uma falha, stop/mute tentado, onAudioStopped, bloqueio musical no controle e ausência de novas escritas. Manter objetos vivos até waitStopped retornar true. Recuperação automática não é suportada; reiniciar pelo procedimento existente.
8. Executar soak de pelo menos quatro horas com carga reprodutível e reconexões; capturar áudio, heap mínimo, maior bloco livre, deadlines, erros I²S e resets. Para DMA e latência, registrar captura instrumental externa e sua metodologia.

## Situação dos critérios de aceitação

| Critério | Evidência / situação |
| --- | --- |
| Limites USB e transfer | PASS nos spans analisados e testes automatizados; cache/alocação interna do SDK é fronteira externa explicitada acima. |
| Criação de tarefas e inicialização | PASS em injeção de falhas e compilação do target. |
| Conexão somente com recepção configurada | PASS em mock, inclusive alloc/submit/ressubmit falhos; MANUAL no SMK25 real. |
| Desconexão e liberação de notas com fila saturada | PASS no caminho host/EventBus; MANUAL para silêncio físico e controlador real. |
| FIFO, reserva e Panic sem eventos obsoletos | PASS nas regressões EventBus e proprietário AMY. |
| Falha de áudio em estado seguro e término confirmado | PASS em simulação; MANUAL para mute/disable elétrico e driver real. |
| Sem novos bloqueios no áudio | Revisão do diff: espera de término só no controle; saída apenas sinaliza semáforo ao terminar. MANUAL para prazo real. |
| Testes automatizados aplicáveis | PASS nos quatro scripts listados. |
| Build ESP32-S3 | PASS em bloco 256 / ESP-IDF 5.5.2; NOT RUN bloco 128 / ESP-IDF 5.3. |
| Progresso sob falha permanente do SDK | Não demonstrado: stop conserva memória e pode aguardar indefinidamente. Risco pendente descrito abaixo. |

A implementação de estabilização e a validação disponível foram entregues;
a Fase 1 não é declarada concluída como qualificação de instrumento enquanto
os critérios físicos e o limite de falha permanente do driver estiverem pendentes.

## Riscos restantes e próximo menor passo

- Recuperação USB depende de o driver concluir callbacks/limpeza. Em falha permanente do SDK, cleanup conserva os recursos e continua atendendo eventos; `stop()` pode aguardar indefinidamente. Não é seguro forçar delete/liberar memória nesse estado. Esse cenário exige qualificação física e, se observado, uma política de recuperação de sistema separada. Não há promessa de progresso sob falha permanente do driver.
- Depois de erro de recepção, exige retirada/reconexão física; perfis vendor-specific precisam de descritores reais e whitelist confirmada antes de eventual suporte. Os perfis persistidos não foram alterados.
- Notificações de UI USB ainda podem ser perdidas quando toda a fila está cheia; estado e contadores do host e Panic não dependem delas.
- O tempo de execução de comandos AMY, alocação durante troca de patch, acesso à Flash e jitter do escalonador continuam dependentes de medição. Não há ganho de desempenho ou latência declarado.
- A API de ciclo de vida exige um único chamador; destruir objetos antes do join continua sendo uso inválido. Não foi implementado restart automático de áudio.
- A fase não é considerada qualificada em hardware enquanto os testes MANUAL não forem registrados.

Próximo menor passo: executar a qualificação física e estabelecer uma baseline
com timestamps de recepção, despacho e consumo PCM. Para a Fase 2 solicitada,
usá-la para testar ordenação de controles em torno de NoteOn/Off, jitter por
bloco e voice stealing com cenários determinísticos, antes de mudar o agendador
ou acrescentar recursos musicais.
