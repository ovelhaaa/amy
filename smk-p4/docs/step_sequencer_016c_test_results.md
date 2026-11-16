# SMK-P4 0.16C — resultados de testes

Base de comparação `1010b66c` (0.16B). Candidato de software para
revisão. Qualificação física **PENDING**.

## Host e firmware

| Comando a partir da raiz | Status | Evidência |
| --- | --- | --- |
| `./smk-p4/tests/run_step_sequencer_midi_016c.ps1` | PASS | AMY real e pacotes USB simulados. Modos, canais/fonte, velocity, sustain, audition, transposição com Harmony, wrap/REST, repetição e acordes, timeout, Stop cheio, Panic, patch, Arp em outro canal e reconnect. Probe de heap em apply/render. |
| `./smk-p4/tests/run_step_sequencer_016a.ps1` | PASS | 36 casos de clock, 9.216 onsets, PCM de referência, 10.000 requests SPSC, stop prioritário, BPM e ownership. Atrasos máximos host de 254 frames no onset e 255 no gate. |
| `./smk-p4/tests/run_step_sequencer_ui_016b.ps1` | PASS | LVGL 9.3, AMY real, ponteiro simulado, regressões 0.16B e fluxos 0.16C. Confirmação e timeout, fila cheia, coalescência de gate com edição MIDI simultânea, mudança rápida de modo, REST, editor, navegação e conexão. O último ajuste foi recompilado e reexecutado com os mesmos flags e objetos das telas intactas. |
| `./smk-p4/tests/run_arp_015.ps1 -Runtime -BaselineRoot build/tempo_015_baseline` | PASS | Arp, clock e BPM globais, fase, Harmony e PCM idêntico com Arp desligado. |
| `./smk-p4/tests/run_harmony.ps1 -ReuseObjects` | PASS | Escalas, quantizadores, vozes, sustain, USB, retrigger e patches. |
| `./smk-p4/tests/run_performance_lock_0152a.ps1` | PASS | Máscaras de patch, integridade de notas, Panic e FX. |
| `./smk-p4/tests/run_sound_editor.ps1` | PASS | Sound Editor, motor de patches, presets e PCM. |
| `./smk-p4/tests/run_sound_editor_ui.ps1` | PASS | LVGL do editor/FX, popups, Panic, atalhos SMK25 e heap. |
| `./smk-p4/tests/run_fx_tests.ps1 all` | PASS | Delay SYNC, FX, persistência e concorrência. A primeira rodada falhou na asserção que compara rejeições a Panics no teste concorrente. A rodada completa seguinte passou. |
| `./smk-p4/tests/run_usb.ps1` e `./smk-p4/tests/run_smk25_0142.ps1` | PASS | USB Host simulado, enumeração, desconexão, reconnect e perfil de knobs, pads e transporte do SMK25. |
| `pio run -d smk-p4 -e guition-editor-8 -j 8` | PASS | PlatformIO 6.1.18 em `C:/.platformio/penv/Scripts/pio.exe`, ESP-IDF 5.5.2. |
| `pio run -d smk-p4 -e guition-editor-8-bench -j 8` | PASS | Imagem elétrica de bancada e SHA-256 arquivados. |
| `python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-editor --full-factory --harmony --unified-engine --sound-library --environment <perfil>` | PASS | Os dois perfis passaram, com `--musical` apenas no bench. AMY 48 kHz / 256, oito vozes, LVGL 128 KiB e gates elétricos verificados. |
| Guition e SMK25 reais | PENDING (MANUAL) | Ensaios de áudio, toque, USB, deadlines, memória dinâmica e stack conforme roteiro de bancada. |

O teste de MIDI grava tanto teclas individuais quanto simultâneas,
valida Note On com velocity zero, libera uma nota convencional gravada
antes da troca de modo e valida o transiente quando o controlador
reconecta. A suite de UI abre e fecha SEQ e FX por 40 ciclos com heap
estável. Os testes de foco USB usam dispositivo e I/O simulados.

## Comparação de recursos

| Medida | 0.16B | 0.16C | Delta |
| --- | ---: | ---: | ---: |
| Runtime (host) | 12.512 B | 13.112 B | +600 B |
| Snapshot (host) | 2.496 B | 2.528 B | +32 B |
| Engine sequenciador (host) | 136 B | 168 B | +32 B |
| Mailbox sequenciador (host) | 456 B | 476 B | +20 B |
| Stack compilado `Runtime::applyUsb` (host GCC, `-fstack-usage`) | 208 B | 224 B | +16 B |
| Stack compilado `Runtime::apply` (host GCC) | 176 B | 176 B | 0 B |
| Stack compilado `Runtime::render` (host GCC) | 64 B | 64 B | 0 B |
| Heap LVGL livre em SEQ após exercícios (host) | 49.224 B | 47.296 B | −1.928 B |
| Heap LVGL livre em FX após exercícios (host) | 63.344 B | 63.424 B | +80 B |
| RAM estática PIO, editor | 43.000 B | 44.012 B | +1.012 B |
| RAM estática PIO, bench | 43.388 B | 43.852 B | +464 B |
| Flash PIO, editor | 1.345.222 B | 1.390.116 B | +44.894 B |
| Flash PIO, bench | 1.447.144 B | 1.492.028 B | +44.884 B |

Os painéis abertos deixaram 32.824 B livres (MIDI Settings) e 34.864 B
(editor) no pool LVGL de 128 KiB. Os números anteriores de heap e PIO
vêm dos relatórios 0.16B. Os binários cresceram 44.896 B nos dois
perfis. [Baseline de testes](step_sequencer_016b_test_results.md) ·
[Baseline de firmware](step_sequencer_016b_hardware_candidate.md)

### Tempo de execução no host

Mesmo GCC, mesmos objetos AMY e mesmo programa para a base e a 0.16C.
Cada rodada processou 5.000 blocos após 128 de aquecimento, com
sequenciador ativo e quatro pares USB Note Off/On por bloco. A tabela
usa a mediana das médias de cinco rodadas alternadas.

| Carga | Apply por bloco | Render por bloco |
| --- | ---: | ---: |
| 0.16B, notas musicais | 21,35 µs | 44,85 µs |
| 0.16C, NORMAL PLAY | 21,84 µs | 45,98 µs |
| 0.16C, STEP RECORD / AUDITION OFF | 0,14 µs | 8,78 µs |

Apply inclui a drenagem e roteamento dos oito eventos e a admissão
das vozes diretas quando aplicável. Em STEP RECORD, a carga de vozes
fica reduzida à sequência. Os intervalos de render Normal
(43,61–46,55 µs na base e 43,45–47,10 µs na 0.16C) se sobrepõem.
O stack compilado é por função. Stack de tarefa, latência, deadlines,
início de áudio e underruns físicos dependem do ensaio em Guition.

## Capturas host em 800×480

- [STEP INPUT, MIDI / RECORD SETTINGS](step_sequencer_016c_shots/midi-settings.png)
- [STEP RECORD e cursor após wrap](step_sequencer_016c_shots/step-record.png)
- [Editor ampliado do passo](step_sequencer_016c_shots/step-editor.png)
- [LIVE TRANSPOSE durante reprodução](step_sequencer_016c_shots/live-transpose.png)
- [NORMAL PLAY após reconexão simulada](step_sequencer_016c_shots/normal.png)
- [Feedback com buffer local cheio](step_sequencer_016c_shots/edit-buffer-full.png)
