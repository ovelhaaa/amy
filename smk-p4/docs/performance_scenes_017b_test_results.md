# Resultados da 0.17B

## Fundação registrada no HEAD inicial da PR #47

| Comando ou verificação | Resultado |
| --- | --- |
| `./smk-p4/tests/run_performance_scenes_017b.ps1` | PASS. Oito slots em NVS simulado, round trip de domínios e rotas, rename, tombstone, CRC, falha de escrita simulada, coexistência com namespaces existentes, banco futuro e reserva de capacidade. Validação seletiva com lock, preservação de tempo e rejeição de referências e rotas. |
| `./smk-p4/tests/run_pattern_library_017a.ps1` | PASS. Inclui banco antigo válido ao lado de banco futuro e prova preservação dos dois após tentativa de salvar e excluir. |
| `./smk-p4/tests/run_step_sequencer_016a.ps1` | PASS. Clock, PCM, mailbox e ownership. |
| `./smk-p4/tests/run_step_sequencer_ui_016b.ps1` | PASS. LVGL host e cancelamento entre Storage Quiet e a escrita. |
| `./smk-p4/tests/run_step_sequencer_midi_016c.ps1` | PASS. Roteamento, captura e Live Transpose. |
| GCC host com `-O2 -fstack-usage` no runtime | PASS. `applySequencer()` usa 208 B, ante 320 B registrados na 0.17A. `serviceSequencer()`, `apply()` e `render()` medem 176 B, 176 B e 64 B. A medição é por função e não mede o high-water da tarefa. |
| Toolchain ESP-IDF, flags de `standalone_main.cpp` e `-fsyntax-only` | PASS para uma unidade de tradução que instancia `NvsSceneStore` e chama `begin()` e `commit()`. |
| `pio run -d smk-p4 -e guition-editor-8 -j 8` | PASS com `C:/.platformio/penv/Scripts/pio.exe`. |
| `pio run -d smk-p4 -e guition-editor-8-bench -j 8` | PASS com `C:/.platformio/penv/Scripts/pio.exe`. |
| Transação ao vivo, rollback, timeout, troca com sustains e jitter de Scene | PENDING. O Audio Owner ainda não consome planos de cena. |
| Scene Browser, modal, acesso rápido e screenshots 800×480 | PENDING. O teste LVGL cobre o browser de Patterns. |
| Heap real NVS/LVGL, métricas de áudio com Scenes e high-water da Guition | PENDING. Dependem da integração e da placa. |

## Recall parcial no Audio Owner

| Comando ou verificação | Resultado |
| --- | --- |
| `./smk-p4/tests/run_performance_scenes_017b.ps1 -ReuseObjects` | PASS. Backend local, runtime AMY de oito vozes, FX manual e efetivo, HARMONY, ARP, TEMPO e carga seletiva de PATTERN. Cobre troca imediata, locks seletivos, Next Bar, ocupação da mailbox, cancelamento explícito, Stop, Panic, revisão obsoleta e rejeição de PATCH e de Pattern ausente. O teste preservou uma nota manual ao carregar um Pattern. |
| `./smk-p4/tests/run_step_sequencer_016a.ps1` | PASS. Clock, PCM, mailbox e ownership. |
| `./smk-p4/tests/run_step_sequencer_ui_016b.ps1` | PASS. LVGL e AMY, com regressão do browser de Patterns. |
| `./smk-p4/tests/run_step_sequencer_midi_016c.ps1` | PASS. Roteamento, gravação, Harmony e Live Transpose. |
| `./smk-p4/tests/run_pattern_library_017a.ps1` | PASS. Biblioteca, dirty state de Pattern, confirmação, troca musical e cancelamento. |
| `./smk-p4/tests/run_performance_lock_0152a.ps1` | PASS. Regressão de locks. |
| `./smk-p4/tests/run_patch_load_0152.ps1` | PASS. Regressão do loader independente de Scene. |
| `./smk-p4/tests/run_sound_editor.ps1` | PASS. Edição com AMY e persistência V4 pelo fluxo de presets. |
| `./smk-p4/tests/run_arp_015.ps1 -Runtime -BaselineRoot build/arp_015_baseline` | PASS. Ownership, gates, latch, Harmony e igualdade de PCM com ARP desligado. |
| `./smk-p4/tests/run_fx_tests.ps1 all` | PASS. FX, Global Tempo/Delay SYNC, transições, fila e persistência de presets V4. |
| `./smk-p4/tests/run_usb.ps1` | PASS. Enumeração, RX, desconexão e reconexão com mock IDF. |
| `pio run -d smk-p4 -e guition-editor-8 -j 8` | PASS. RAM e hash no documento de hardware. |
| `pio run -d smk-p4 -e guition-editor-8-bench -j 8` | PASS. RAM e hash no documento de hardware. |
| Recall de PATCH e MACROS, dirty state de Scene, operações do store pelo firmware, touchscreen e atalhos MIDI | PENDING. |
| Qualificação física, tempo de transição de patch, heap real NVS/LVGL e high-water da tarefa | PENDING. |

No host, a aplicação Next Bar iniciou 0, 128 e 128 frames após a fronteira a 40, 120 e 240 BPM, respectivamente. O maior atraso observado foi de 2,67 ms, dentro de um bloco de 256 frames. O heap probe protegeu `apply()` e `render()` no teste. A mailbox ocupa 1.464 B no host, o runtime 14.856 B e o snapshot 2.576 B. Com `-fstack-usage`, o compilador RISC-V do perfil bench reportou 240 B em `serviceScene()`, 128 B em `apply()` e 32 B em `render()`. As medições por função excluem a stack de chamadas descendentes.
