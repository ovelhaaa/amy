# Fase 3B.2A — preparação verificável (entrega parcial)

**A fase não está concluída. Nenhuma API desta entrega certifica Materialized
ou Ready de um patch. Layer B continua desabilitada.**

## A — auditoria anterior à implementação

Branch `feature/smk-s3-dual-amy-runtime`, HEAD inicial
`11feb64535c37f062f3cd6da8114aa352cb5246b`, árvore limpa. Commits existentes
preservados; sem reset, rebase, merge, push ou upload. Lidos AGENTS.md, specs,
README, relatórios 3A/3B/3B.1/3B.2, fontes e runners/CI. README contém conflitos
AMY/P4 preexistentes; specs pede 284×76, perfil local conserva 240×240. Não
resolver esses conflitos por inferência de hardware.

| Operação | Recurso / evidência (arquivo, função) | Pode falhar? | Propaga erro? | Altera estado ativo? |
| --- | --- | --- | --- | --- |
| Validação | `patches.c:patches_load_patch_checked`, ROM/custo/vozes/bus | Sim | Sim, enum de admissão | Não na recusa direta |
| Admissão | mesma função, plano de blocos contíguos e mapas | Sim | Sim | Só após planejar |
| Preparação | `instrument.c:instrument_prepare/instrument_init/voice_fifo_init`, container + duas FIFOs | Sim | Sim, NULL | Não até commit |
| Parser | `patches.c:parse_patch_string_to_queue`, `parse.c:amy_parse_message/amy_parse_synth_layer_message/amy_parse_transfer_layer_message` | Sim | Não integralmente | Sim: reset imediato, MIDI, transferências |
| Conversão | `amy.c:amy_event_to_deltas_queue`, synthinfo e referências | Sim | Não, void e ensure ignorado | Sim: aloca oscs, aplica comandos synth e highest_bus |
| Deltas | `amy.c:add_delta_to_queue/delta_get/deltas_add_pool_block` | Sim | Não no wrapper void; get retorna NULL; teto aborta | Sim, fila/pool globais |
| Envelope | `amy.c:play_delta/ensure_osc_allocd/alloc_osc` | Sim | ensure bool; play void descarta | Sim: pode crescer após publicar container |
| FM/modulação | `amy.c:play_delta`, MOD_SOURCE/ALGO_SOURCE/CHAINED_OSC | Sim | Incompleto; alguns caminhos desreferenciam após ensure ignorado | Sim: referências, roles e início do modulador |
| Primeira nota | `interp_partials.c:partials_note_on/interp_partials_note_on` | Sim | Void; alocação lazy pode falhar | Sim |
| Buffers globais | `amy.c:oscs_init/global_init/config_chorus/config_echo/config_reverb`, `algorithms.c:algo_init`, `oscillators.c:ks_init`, `pcm.c:pcm_load/pcm_load_file` | Sim | Parcial/void em vários pontos | Sim: engine, bus, sample e scratch compartilhados |
| Commit atual | `instrument.c:instrument_commit`, seguido do loader/parser/deltas | Sim depois de publicar | Sem confirmação de materialização | Sim, irreversível para o patch antigo |

`alloc_osc` usa **uma alocação contígua** para synthinfo, mod_synthinfo, tempos
e valores dos dois envelopes. Não existem quatro mallocs independentes para
essas classes no HEAD. LUTs de seno/ondas são tabelas estáticas escolhidas
lazy, não heap por nota; operadores FM usam oscs e scratch global de boot.
PCM ROM não exige carregar arquivo; PCM de arquivo/transferência não é isolado.
KS tem buffers globais de boot. Partials podem alocar/grow no primeiro On.

Fila: lista ordenada estável por tempo relativo ao wrap, free-list global,
16 blocos de 2048 deltas. Deltas não retêm synth; RESET usa data.i, e osc pode
ser bus. Referências de modulação/chain/FM também precisam de relocação.
Contar nós não valida envelopes, DSP ou efeitos. Não criar segundo scheduler.

### Decisão e aceitação do subconjunto

Um patch wire não é programa puro; nem parser em fila privada nem play_delta
em oscs reservados provam isolamento de FX/MIDI/samples. Um patch inteiro
precisa de representação validada sem efeitos imediatos, classificação de
dependências e aplicação checked. Alterar todos esses caminhos nesta rodada
seria reestruturação ampla. Usar a exceção explícita do pedido: entregar
primitivas seguras de reserva, sem anunciar Materialized/Ready.

Arquivos previstos: amy.c/h (APIs compatíveis), harness real/runners e relatório.
Preparação de capacidade DSP deve alocar fora das tabelas ativas, validar
limites, descartar tudo em OOM e publicar capacidade somente após preflight.
Lote de deltas deve obter todos os nós antes de alterar a fila de destino e
propagar OOM/teto sem abort. Preservar ordenação/timeline e wrappers legados.
Testar cada malloc real, crescimento de envelopes, descarte, recuperação,
estado A/B/drums e deltas futuros, mutações em cópias isoladas, regressões e
builds 128/256. Nenhuma dependência/ESP-IDF no core, nenhum GPIO novo.
APIs novas owner-only, síncronas, fora do render/On/Off. Não habilitar switching
durante performance sem benchmark físico.

Baseline: safety/performance/patch integrity/expansion/UI PASS. Primeira AMY
via Windows PowerShell 5: FAIL do runner ao tratar stderr esperado do probe
negativo como NativeCommandError; os testes anteriores ao probe passaram.
Reexecução no PowerShell 7: PASS, inclusive owner e probe negativo. Logs ignorados
`build/phase3b2a_baseline_*.log`.

## B/C — arquitetura e APIs entregues

Produção mantém um engine/owner, carregador de admissão opt-in e barreiras
globais existentes. Não há chamada nova no adapter/app_main nem readiness
nova. O fluxo de um patch de produção permanece o da 3B.2, com suas limitações.
As primitivas novas são infraestrutura C portátil para uma futura preparação
de patch; hoje são exercitadas pelo harness, não usadas para certificar boot.

| API | Garantia exata |
| --- | --- |
| `delta_get_checked` | Obtém nó ou retorna INVALID/NO_MEMORY/CAPACITY, sem abort no teto e sem log |
| `add_delta_batch_to_queue_checked` | Recebe array limitado de valores; obtém todos os nós antes de publicar; falha não altera a fila de destino; sucesso usa a mesma ordenação estável relativa ao wrap |
| `amy_osc_bank_prepare` | Valida IDs/capacidades/duplicatas, aloca token e um bloco destacado por osc, inicializa synthinfo/mod_synthinfo/envelopes sem instalar ponteiros nas tabelas globais |
| `amy_osc_bank_commit` | Preflight de todos os ponteiros esperados e proprietários de osc antes de qualquer swap; copia estado musical corrente e vetores antigos, conserva as capacidades novas; sem malloc |
| `amy_osc_bank_discard` | Libera somente blocos destacados/token; idempotente pelo ponteiro NULL; após commit não libera blocos ativos |

`amy_resource_result` descreve **somente a operação**: OK, INVALID, NO_MEMORY,
CAPACITY, STALE. Não é `PatchPrepareStatus` nem ACK. Exaustão incrementa o
contador existente `amy_get_oom_count()` sem emitir logs nas APIs novas.
Falha de validação não é OOM. O token de banco é opaco; inicializar saída com
NULL, não copiar sua propriedade. Prepare recusa saída já ocupada sem perder
o token anterior. Falha no prepare deixa saída NULL; falha no commit conserva
token para descarte. Sucesso consome token e zera saída.

Cada pedido reserva pelo menos DEFAULT_NUM_BREAKPOINTS por conjunto, inclusive
quando o pedido é zero, pois reset instala o gate padrão. Não reduz capacidade
existente. Crescer capacidade no banco não aplica parâmetros de patch: commit
copia inclusive modificações de valores/fase/mod_synthinfo ocorridas desde
prepare. LUTs continuam referências estáticas, não memória do token. Nenhum
container, mapa de voz, FIFO, MIDI, bus, sample, FX ou nota é recriado por ele.
Não se cria engine espelho nem scheduler adicional.

A extensão foi necessária dentro de amy.c: alloc_osc publicava diretamente nas
tabelas e resetava por índice, e o alocador de deltas abortava no teto. Não
havia extensão do adapter que pudesse impedir esses efeitos. Extraído somente
um helper de alocação/reset por ponteiro, compartilhado com alloc_osc legado.
Assinaturas antigas preservadas; wrappers legados continuam com sua semântica,
incluindo o teto fatal de delta_get legado. APIs checked novas são opt-in por
chamada; a guarda AMY_PATCH_CHECKED_RESOURCE_ADMISSION e o pool SMK de 120 não
mudam. São candidatas a upstream após revisão; nenhuma proposta enviada.

## Materialized, Ready e recursos ainda lazy

**Resources Assigned:** resultado antigo da 3B.2; mapas/container atribuídos,
nunca prova de configuração executada. O valor não foi renomeado para Ready.

**Materialized (contrato pretendido, BLOCKED):** programa de patch validado,
parâmetros estruturais efetivamente aplicados em preparação isolada e todas as
dependências obrigatórias disponíveis. Capacidade reservada ou deltas obtidos
não satisfazem esse contrato. Não existe resultado novo que afirme esse estado.

**Ready (contrato pretendido, BLOCKED):** instrumento Materialized publicado
pelo owner para o destino/configuração corretos, sem dependências obrigatórias
falíveis pendentes. Não é confirmação de nota já reproduzida. Não foi criada
publicação/ACK nem ligação a PerformanceState; B permanece indisponível.

Os blocos novos preparam concretamente synthinfo, mod_synthinfo, ambos os
vetores de tempos e ambos os vetores de valores. Deltas dentro da capacidade
de breakpoint reservada não precisam crescer esse bloco, comprovado com
play_delta real e alocador armado para falhar. Isso não cobre parâmetros de
patch ainda não aplicados, referências a outros oscs, scratch global ausente,
dependências PCM/FX, partials lazy ou recursos de hooks externos.

O caminho legado continua lazy: conversão de eventos pode alocar oscs/referências,
play_delta pode expandir envelopes, partials podem expandir oscs/vetores na
primeira nota, e FX podem alocar buffers ao serem configurados. LUT lazy escolhe
tabela estática, sem malloc. Não se usa primeiro render para validar recursos
declarados Ready. Não foi acrescentado heap ao processamento de notas/amostras;
também não se afirma que o heap lazy preexistente foi eliminado do produto.

## Atomicidade, ownership e falhas após commit

Banco: falha de validação/OOM descarta todos os temporários e não muda tabelas.
Commit verifica TODOS os destinos primeiro; um segundo destino obsoleto não
publica o primeiro. Depois do preflight não há operação falível prevista: são
cópias limitadas, swaps e free. Este é commit de **capacidade DSP**, não commit
de instrumento/patch. Alteração do ponteiro DSP ou da voz proprietária recusa
commit como STALE. Valores correntes não são versão de patch: são copiados no
commit para evitar restaurar um snapshot musical antigo.

Owner-only durante toda a vida do token; nenhum acesso concorrente, engine
reset/stop/restart, transferência de token entre tarefas ou retenção entre
vidas do engine é suportado. Não há garantia contra ABA após lifetime/reset;
descartar ANTES de reset/stop. Não há protocolo de versão de configuração nem
request IDs; será requisito do backend futuro, não deste token de capacidade.

Lote: nós são obtidos em lista privada antes da inserção. Falha retorna todos
os nós adquiridos à free-list, preservando destino/head/qsize/deltas futuros.
Se um bloco de pool cresceu antes de uma segunda alocação falhar, o bloco
permanece **capacidade reutilizável do engine**, registrado em delta_blocks e
liberado em deltas_pool_free/amy_stop. Não é temporário perdido. A ordem interna
da free-list pode mudar no cancelamento; sua contagem e integridade permanecem.
O teste de falha no segundo crescimento comprova essa distinção. Atomicidade
de destino não é rollback byte a byte do cache do alocador.

O lote não valida semântica de parâmetros nem executa deltas: publicar resets,
FX ou deltas inválidos por essa API ainda é responsabilidade do chamador. Não
se reservam todos os recursos de um patch. O ponto irreversível do loader
anterior continua a liberação dos mapas antigos/publicação do container antes
do parser/play_delta. Falhas posteriores, inclusive falha dupla, **não** ganham
rollback de patch nesta entrega. Não aplicar o banco como workaround e depois
anunciar Ready. Um backend completo precisa recusar antes desse ponto ou
isolar/desativar o destino após falha; esse caminho ainda é BLOCKED.

## D — testes AMY reais e mutações

O harness da 3B.2 foi ampliado; mesmos fontes C do firmware, engine/FIFOs reais,
somente alocador de plataforma com falha na chamada N. amy.c de recursos usa
MALLOC_CAPS_DEFINED para impedir inlining que contorne a injeção; free é
envolvido pelo linker. Nada de injeção no firmware. Snapshots comparam mapas,
patch/vozes/níveis, synthinfo, mod_synthinfo, conteúdo profundo dos envelopes,
estado global (exceto contador OOM), campos/links de cada delta futuro e qsize.

| Status | Ensaio e limite |
| --- | --- |
| PASS | ROM válido/inexistente, custo/vozes/bus inválidos, user patch fora de faixa, fragmentação e falta de vozes da 3B.2 preservados |
| PASS | Falhas container/FIFO released/FIFO active reais, A/B/drums e sustain preservados |
| PASS | Token DSP e cada bloco co-alocado: falha em todas as 121 posições no banco máximo de 120 oscs; saída NULL, temporários liberados, snapshots intactos |
| PASS | Dois oscs ativos: preparo/descartes não alteram DSP; commit sem malloc, copia envelopes/mod_synthinfo/fase atuais; drums e demais oscs intactos |
| PASS | Segundo destino tornou-se obsoleto por crescimento real de envelope: commit recusa sem publicar o primeiro |
| PASS | Osc ausente, capacidades zero normalizadas ao gate padrão, IDs/duplicatas/capacidades inválidos, saída ocupada e commit sem token |
| PASS | Breakpoint na posição máxima aplicado por play_delta real sem alocação após reservar capacidade |
| PASS | 100 tentativas alternando OOM/descarte/recuperação com instrumentos FM ativos; rastreador exige zero temporários restantes |
| PASS | Lote de dois deltas: primeiro nó disponível, crescimento para o segundo falha; destino e free count anteriores preservados |
| PASS | Lote 2050 com um nó livre: primeiro crescimento de 2048 funciona, segundo falha; 2049 nós livres recuperados e bloco registrado no cache do engine |
| PASS | Teto real de 16×2048 nós com fila futura ativa: retorna CAPACITY sem abort, não publica parcialmente; liberação recompõe a capacidade total |
| PASS | Lote checked e fila legada comparados em timestamps iguais e wrap uint32; sucesso global incrementa qsize exatamente uma vez |
| PASS | 100 trocas de patches FM A/B e bateria, teardown/recriação B, fragmentação real 60 livres/maior trecho 1 e prova negativa de OOM pós-admissão existentes preservados |
| BLOCKED | Patch de usuário válido/malformado como programa isolado checked; ROM inteiro materializado antes de publicar; não confundir testes de admissão com essas garantias |
| BLOCKED | Falhas de buffers globais/FX/PCM/scratch/partials como parte de transação de patch; não abrangidos pelas primitivas |
| NOT RUN | Deltas globais/FX executados em preparação isolada, pois essa execução não foi implementada |

Não há malloc independente de “mod_synthinfo”, “envelope” e “breakpoint”: todos
falham juntos no mesmo bloco. O harness testa os blocos contendo cada recurso,
incluindo envelope expandido; não inventa pontos de injeção inexistentes.
Estruturas incompletas não são renderizadas. A prova negativa preexistente de
OOM depois de Resources Assigned continua explícita, sem marcar Ready.

Mutações com `-Mutations`, somente em cópias ignoradas em
`build/smk_amy_boot/mutations/`:

| Status | Mutação | Esperado / observado |
| --- | --- | --- |
| PASS | OOM do token retorna OK | Assert de amy_osc_bank_prepare/NO_MEMORY falha; exit −1073740791 |
| PASS | Commit omite cópia de valores do envelope | Assert de breakpoint_values falha; exit −1073740791 |

O runner exige tanto exit não zero quanto a asserção da invariante pretendida;
erro de compilação/link não conta como mutação detectada. Fonte versionado
permanece correto. Duas falhas intermediárias de desenvolvimento foram do
harness: conversão int→enum exigiu cast explícito, e comparação global precisou
permitir o incremento diagnóstico OOM legado. Corrigidas antes da validação.

## E — comandos, regressões e builds

Executados da raiz, PowerShell 7 (`pwsh -NoProfile -File <script>`):

```powershell
./tests/run_smk_safety.ps1
./tests/run_smk_amy_boot.ps1
./tests/run_smk_performance.ps1
./tests/run_smk_patch_integrity.ps1
./tests/run_smk_synth_expansion.ps1
./tests/run_smk_ui.ps1
./tests/run_smk_amy_boot.ps1 -ResourcesOnly -BlockSize 128
./tests/run_smk_amy_boot.ps1 -Legacy
./tests/run_smk_amy_boot.ps1 -Mutations
& C:/.platformio/penv/Scripts/python.exe -m platformio run -d smk-s3 -e esp32-s3-devkitc-1 -e esp32-s3-devkitc-1-128
git diff --check
```

**PASS:** seis suítes baseline no PowerShell 7 (cinco também passaram no 5),
seis regressões finais, recursos 128/256, legado e mutações. Runners host/CI
usam GCC/G++ existentes; nenhum pacote instalado. CI host agora inclui mutações,
recurso 128 e compatibilidade legada. Logs ignorados
`build/phase3b2a_final_{safety,amy,performance,patch,expansion,ui}.log`,
`phase3b2a_resources_128.log`, `phase3b2a_legacy.log`, `phase3b2a_mutations.log`.
Relinks pontuais do harness durante desenvolvimento reutilizaram os flags e
objetos do runner, sem substituir o core por mocks.

Compatibilidade legada PASS conserva o demo upstream 117 declarados/111 oscs
atribuídos, synth 1 30/36, diagnóstico esperado. Não é aprovação desse arranjo
incompleto nem alteração dos defaults. SMK segue default_synths=0 e Single.

Sanitizadores **BLOCKED**: probe real
`'int main(void) { return 0; }' | & clang '-fsanitize=address,undefined' -x c - -o build/phase3b2a_sanitizer_probe.exe`
falhou no link por libclang_rt.asan_dynamic.dll.a e
libclang_rt.asan_dynamic_runtime_thunk.a ausentes; log
`build/phase3b2a_sanitizer_probe.log`. Rastreamento de free/snapshots não substitui
ASan/UBSan. CI remota, TSan, hardware de outras plataformas/P4: **NOT RUN**.

Builds locais 48 kHz, PlatformIO/pioarduino 55.03.36-1, IDF 5.5.2,
Xtensa GCC 14.2.0. Primeira matriz PASS, log `build/phase3b2a_firmware.log`:

| Status | Ambiente/bloco | RAM estática | Flash |
| --- | --- | ---: | ---: |
| PASS | esp32-s3-devkitc-1 /256 | 30.116 /327.680 | 1.141.383 /16.777.216 |
| PASS | esp32-s3-devkitc-1-128 /128 | 30.116 /327.680 | 1.141.403 /16.777.216 |

Matriz final após contadores diagnósticos: **PASS**, exit 0, mesmos tamanhos,
48,923 s (256) e 55,635 s (128), log `build/phase3b2a_firmware_final.log`.
Sem upload. A comparação com a 3B.2 é
contra números registrados, não rebuild limpo do HEAD inicial: RAM igual,
Flash +128 bytes nos dois ambientes da primeira matriz. As novas APIs não são
chamadas pelo firmware e podem ser eliminadas pelo linker; tamanho de imagem
não representa o pico de heap se forem adotadas no futuro.

## Custos, tempo real e hardware

Medições de host (MinGW 64-bit, allocator do teste): synthinfo=384 bytes,
mod_synthinfo=52, pedido de capacidade=4. Cada bloco com 24+24 breakpoints
solicita 384+52+48×8=820 bytes contíguos. Banco de dois solicita **1.744 bytes**
incluindo token; banco máximo de 120 solicita **104.168 bytes** além do engine
ativo. São malloc requested bytes, não footprint real do alocador nem SRAM S3.
O número máximo é teste de limite, não recomendação de reservar o pool inteiro.

100 tentativas de prepare/OOM/discard mais snapshots completos levaram 7.962 us
na execução final de 256 e 8.975 us na de 128; inclui trabalho do harness e varia por carga.
Não é benchmark isolado nem WCET de preparação/commit/descarte no S3. SRAM/PSRAM
real, maior bloco, fragmentação física, stacks e CPU antes/depois: **MANUAL**.

Prepare: validação de duplicatas O(n²), no máximo 120 pedidos; n+1 alocações,
uma por bloco mais token. Commit: O(n×sets×MAX_BREAKPOINTS) de cópias limitadas,
nenhuma alocação, mas inclui free e não é aprovado para deadline. Descarte O(n)
frees. Banco pode falhar mesmo havendo oscs livres, por SRAM contígua insuficiente.
Pool delta cresce em blocos existentes de 2048; no SMK mantém fallback PSRAM
opt-in existente. Lote segura lock legado enquanto reserva/insere; ordenação
pode ser O(n×tamanho da fila). **Não usar essas APIs em render, On/Off ou ISR**.
Mover para outra tarefa sem ownership exclusivo não é solução. Adotar somente
em fase de manutenção síncrona do owner até medir custo e definir publicação.

Sem nova dependência/licença, mudança de sample rate, limite de oscs, GPIO,
display, VBUS, straps PCM5102A ou saída analógica. Não inferida revisão DevKitC.
Qualificação física **MANUAL**: repetir procedimento Single da 3B.2 nos dois
blocos, console status/audio_status/memory após audio_reset, patches 0/128/135,
acordes/pedal/drums, Panic, disconnect/100 reconnects e soak de duas horas.
Esperado: nenhuma nota presa/crash; registrar render máximo, deadlines/gaps,
erros I²S e mínimos/maiores blocos de memória. Capturar MIDI/analógico para
latência/clipping. Não há comando novo de banco/Ready/Layer B na placa.

## Critérios restantes, commits e entrada na 3B.2B

| Critério | Situação |
| --- | --- |
| Recursos/pontos de falha identificados | PASS auditoria por classe; dependências dinâmicas por patch ainda exigem programa validado |
| Falhas chegam ao resultado | PASS primitivas; BLOCKED parser/aplicação completos |
| Instrumento incompleto não recebe Ready | Nenhuma publicação nova; BLOCKED certificação de patch inteiro |
| Temporários descartados / demais instrumentos íntegros | PASS nos bancos/lotes/admissão testados |
| Preparação/commit reais | PASS capacidade DSP; BLOCKED transação de patch |
| AMY real com OOM antes/durante materialização | PASS reserva DSP/pool; BLOCKED todos os caminhos de patch/FX/primeira nota |
| Single e legado | PASS regressões |
| Builds disponíveis | PASS matrizes inicial e final 128/256 |
| Custos e limites | PASS documentados; MANUAL WCET/heap físico |

Commits locais por entrega:

- A `1e457859`: auditoria e recorte seguro.
- B `62b3f509`: obtenção/lote de deltas checked.
- C `1498eb35`: reserva destacada e commit de capacidade DSP.
- D `79f4ae1b`: diagnóstico de exaustão, OOM real, snapshots, mutações e CI.
- E: `docs(smk): record phase3b2a safe subset validation and remaining blockers`,
  commit que contém o fechamento deste relatório.

O usuário solicitou push ao final; publicar somente esta branch de trabalho
no origin, sem force-push nem alteração de main. Fetch antes da publicação
confirmou origin sem commits exclusivos (0 remotos /4 locais antes de E).

Próximo menor passo: representação limitada de patch local, sem comandos
imediatos, com validação sintática/semântica e dependências explícitas. Resolver
ROM/user patches por esse caminho checked, obter deltas e capacidades, aplicar
parâmetros com erros propagados sem tocar FX/MIDI/estado ativo, verificar
completude e só então publicar instrumento. Recusar explicitamente comandos
fora do subconjunto. Não usar o parser irrestrito para fingir isolamento.

Antes da 3B.2B: demonstrar Materialized para esse subconjunto com OOM em todas
as dependências; definir tratamento de recursos globais e primeira nota,
validação de versão/destino no commit e estado seguro após ponto irreversível.
Só depois acrescentar request IDs/ACKs concorrentes e cancelamento seletivo.
Isso não autoriza Layer B, dual routing, UI ou performance switching agora.
**Fase 3B.2A permanece parcial; requisitos de materialização integral não foram
convertidos em declaração de conclusão.**
