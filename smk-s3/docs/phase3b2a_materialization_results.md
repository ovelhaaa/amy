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
| Buffers globais | `amy.c:oscs_init/buses_init/config_chorus/config_echo/config_reverb`, `algorithms.c:algo_init`, `oscillators.c:ks_init`, `pcm.c:pcm_load` | Sim | Parcial/void em vários pontos | Sim: engine, bus, sample e scratch compartilhados |
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
Reexecução no PowerShell 7 em andamento. Logs ignorados
`build/phase3b2a_baseline_*.log`.
