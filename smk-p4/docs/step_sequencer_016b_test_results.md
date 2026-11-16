# SMK-P4 0.16B — Resultados de testes

Baseline `ecea8310`, HEAD da branch 0.16A e da PR #43 antes da criação
da branch 0.16B. A interface e os testes desta entrega são para a
Guition ESP32-P4. As metas de múltiplas pistas e gravação do
`specs.md` do SMK-S3 permanecem fora deste incremento monofônico.

| Comando, a partir da raiz | Status | Evidência |
| --- | --- | --- |
| `./smk-p4/tests/run_step_sequencer_ui_016b.ps1` | PASS | LVGL 9.3 com ponteiro simulado, adapter, mailbox, Audio Owner e AMY no mesmo processo. |
| `./smk-p4/tests/run_step_sequencer_016a.ps1` | PASS | 36 combinações de clock, 9.216 onsets, comparação PCM, 10.000 requests SPSC, Stop com fila cheia, Panic e probe de heap. |
| `./smk-p4/tests/run_arp_015.ps1 -Runtime -BaselineRoot build/tempo_015_baseline` | PASS | Clock, Tap Tempo, Arpeggiator, isolamento de origem e PCM idêntico à baseline com Arp desligado. |
| `./smk-p4/tests/run_harmony.ps1 -ReuseObjects` | PASS | Harmony, admissão, retrigger e USB musical. |
| `./smk-p4/tests/run_performance_lock_0152a.ps1` | PASS | Máscaras, integridade de notas, Panic e delay. |
| `./smk-p4/tests/run_sound_editor.ps1` | PASS | Sound Editor, presets, PCM, fábrica e regressões de estabilidade. O código de teste existente emite warnings de indentação. |
| `./smk-p4/tests/run_sound_editor_ui.ps1` | PASS | LVGL do editor e FX, seleção de patch, confirmação, heap e atalhos SMK25. |
| `./smk-p4/tests/run_fx_tests.ps1 all` | PASS | Delay SYNC, transições de BPM, conflitos e persistência FX. |
| `./smk-p4/tests/run_usb.ps1` | PASS | Enumeração, recepção e recuperação com IDF simulado. |
| `./smk-p4/tests/run_smk25_0142.ps1` | PASS | Perfil, mapeamento e roteamento do controlador. |
| `./smk-p4/ui-preview/run_preview.ps1 -LvglOnly` | PASS | Preparação do LVGL para a suíte 0.16B em checkout limpo com a dependência gerenciada instalada. |
| `./smk-p4/ui-preview/run_preview.ps1` | FAIL | Preview legado tem uma falha de compilação preexistente no include de `fm_topology.h`. Uma tentativa local de ligar os objetos do AMY avançou até a asserção legada de `M8 DRIVE`. A suíte LVGL dedicada e a suíte Sound Editor UI passaram. |
| `pio run -d smk-p4 -e guition-editor-8 -j 8` | PASS | Build com o PlatformIO 6.1.18 de `C:/.platformio/penv/Scripts/pio.exe`. |
| `pio run -d smk-p4 -e guition-editor-8-bench -j 8` | PASS | Build de bancada com o mesmo toolchain. |
| `python smk-p4/tests/check_blueprint_build.py` com as opções de ambos os perfis da CI | PASS | Gates elétricos, LVGL 9.3 com pool de 128 KiB, AMY a 48 kHz e blocos de 256 frames. |
| Ensaios de toque, I²S, áudio e carga na Guition | MANUAL | Qualificação física **PENDING**. |

A suíte 0.16B verificou edição dos 16 passos, notas, pausas, velocity, gate,
transporte, confirmação por snapshot, fila cheia, rejeição e timeout. O
playhead mudou de coluna com o owner e desapareceu no Stop. O teste exercitou
Arpeggiator e Delay SYNC simultâneos à sequência, notas USB mantidas após
Stop, troca de patch com confirmação de descarte e Panic em outra tela.
O mock de NVS registrou zero writes.

O teste de timing passou por 50 combinações origem/destino de divisão e
swing 50% e 75%. Cada caso verificou o gate em curso, a próxima fronteira
e os 127 onsets seguintes. No trace 0.16A reexecutado, o atraso máximo
foi de 254 frames para onset e 255 frames para gate, dentro do bloco de
256 frames a 48 kHz. Os seis PNGs do harness foram conferidos em 800×480.

## RAM e desempenho

| Medida | 0.16A | 0.16B | Diferença |
| --- | ---: | ---: | ---: |
| Runtime AMY no host | 12.408 B | 12.512 B | +104 B |
| Snapshot AMY no host | 2.408 B | 2.496 B | +88 B |
| Engine do sequenciador no host | 120 B | 136 B | +16 B |
| Mailbox do sequenciador no host | 452 B | 456 B | +4 B |
| Heap LVGL livre na abertura de SEQ | 57.992 B | 55.920 B | −2.072 B |
| RAM estática PIO, editor | 42.220 B | 43.000 B | +780 B |
| RAM estática PIO, bench | 43.160 B | 43.388 B | +228 B |

O comparativo LVGL usou o mesmo harness, pool e backend. A tela original
foi compilada do commit de referência e trocada no link. O heap livre em
FX era 70.224 B nos dois casos antes de abrir SEQ. Com padrão e controles
exercitados, a suíte mediu 49.224 B livres em SEQ e 63.344 B em FX, estáveis
por 40 ciclos de navegação.

Três rodadas alternadas do mesmo probe host reusaram o GCC, os objetos C
do AMY e a carga de voz. Cada rodada fez sete passes de 2.048 blocos após
64 blocos de aquecimento. A tabela usa a mediana das três médias de
render por bloco.

| Carga | 0.16A | 0.16B |
| --- | ---: | ---: |
| Oito notas MIDI, sequenciador parado | 60,68 µs | 59,43 µs |
| Sete notas MIDI, sequenciador ativo | 62,50 µs | 59,95 µs |

O intervalo entre as médias das rodadas ativas foi 61,43–63,42 µs na
0.16A e 57,79–66,85 µs na 0.16B. A variação host impede atribuir ganho de
CPU à alteração. Na suíte de UI, render AMY, publicação e refresh LVGL
somaram 172,42 µs/bloco em média no host. A comparação de deadlines,
stack, CPU, refill e I²S real depende do ensaio físico.
