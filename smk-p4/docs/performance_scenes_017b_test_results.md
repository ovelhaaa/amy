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

## Integração final

| Comando | Resultado |
| --- | --- |
| `./smk-p4/tests/run_performance_scenes_017b.ps1 -ReuseObjects -RuntimeOnly` | PASS. Sete domínios, FULL e SELECTIVE, lock de FX, rotas customizadas em troca de topologia, 229 máscaras conferidas, referência de Pattern obsoleta e codec v1/v2. |
| Mesmo comando, concorrência | PASS. Stop, Panic, troca externa, Storage Quiet, desconexão USB, timeout, falha de alocação e recuperação. Via Patch, as duas gerações esperadas foram confirmadas. As oito vozes ocupadas foram liberadas pela barreira. Panic depois da confirmação do patch resultou em PARTIAL_APPLY com PATCH aplicado. |
| `run_step_sequencer_016a.ps1`, `run_step_sequencer_midi_016c.ps1` e `run_pattern_library_017a.ps1` | PASS. Clock, notas, sequenciador, captura MIDI, biblioteca e persistência de Patterns. |
| `run_performance_lock_0152a.ps1` e `run_patch_load_0152.ps1` | PASS. Locks e loader legado. |
| `run_sound_editor.ps1` | PASS. Inclui Sound Editor, factory patch loader, Harmony e presets V4. |
| `run_arp_015.ps1 -Runtime -BaselineRoot build/arp_015_baseline` e `run_fx_tests.ps1 all` | PASS. Arp, FX e Global Tempo, incluindo Delay SYNC. |
| `run_usb.ps1` e `run_smk25_0142.ps1` | PASS. USB host simulado e integração SMK25. |

Nas trocas PATCH Next Bar, o início da aplicação ficou 0, 85,33 e 42,67 frames após a fronteira (40, 120 e 240 BPM). O teste de FULL RECALL mediu 720 µs de tempo computacional no host, incluindo 674 µs no loader. A confirmação percorreu duas chamadas do owner. Em 3.537 blocos, `apply()` mediu 0,5/35,1 µs (média/máximo) e `render()` 6,6/246,1 µs. O teste do heap cobre essas duas funções. A carga de patch usa o caminho existente no worker de síntese, que aloca enquanto a saída entrega silêncio. O tempo em hardware e a duração física do silêncio permanecem PENDING.

O runtime host ocupa 14.880 B, o snapshot 2.584 B e a mailbox 1.472 B. O compilador RISC-V do perfil editor reportou 1.680 B em `serviceScene()`, 1.312 B em `applyEdit(const edit::Request&)`, 1.344 B em `loadPendingPatch()`, 128 B em `apply()` e 32 B em `render()`. A soma dos frames conhecidos na entrada de uma aplicação de MACROS é 6.608 B de uma stack configurada para 12.800 B. Essa estimativa exclui chamadas descendentes e interrupções. O high-water real segue PENDING.

A partição NVS é de 24 KiB. O registro v2 usa 96–816 B, com 20 B por tombstone. A reserva conservadora para o pior caso continua em 464 entradas para 16 cópias, com uma página de reserva para GC. A ocupação real com presets V4 e Patterns depende dos dados da placa.

### Touchscreen e release gate

**PASS** no teste LVGL com AMY real derivado de `run_step_sequencer_ui_016b.ps1` com a cópia local do LVGL 9.3 fixado. Foram exercitados os oito slots, captura, cancelamento antes de escrita, rename, recall, substituição de fila, cancelamento, atalhos, dirty state híbrido, delete, referência Pattern obsoleta e PARTIAL_APPLY. O teste de geometria confirmou que os atalhos ficam abaixo dos sliders. O LVGL host reportou 66.376 B livres no browser vazio e 52.280 B com o modal. A primeira execução encadeada coincidiu com a regeneração ESP-IDF do componente gerenciado e perdeu o cabeçalho LVGL. A validação com cópia fixa isolou essa dependência de compilação.

Capturas de 800×480: [vazio](performance_scenes_017b_shots/scenes-empty.png), [catálogo](performance_scenes_017b_shots/scenes-eight.png), [modal](performance_scenes_017b_shots/scenes-selective.png), [pendente](performance_scenes_017b_shots/scenes-pending.png), [ativa](performance_scenes_017b_shots/scenes-active.png), [modificada](performance_scenes_017b_shots/scenes-dirty.png), [falha](performance_scenes_017b_shots/scenes-failed.png), [aplicação parcial](performance_scenes_017b_shots/scenes-partial.png) e [atalhos](performance_scenes_017b_shots/scenes-quick.png).

**PASS** nos dois builds da Guition. RAM, Flash e SHA-256 estão no [candidato de hardware](performance_scenes_017b_hardware_candidate.md).

**NO-GO para aceitação integral da 0.17B.** O loader existente ainda aloca na fase silenciada da tarefa `amy_owner`. O probe confirmou ausência de alocação em `apply()` e `render()`, mas não cobre o requisito de ausência de alocação durante toda a tarefa. A UI também agrega VALIDATING, PREPARING, APPLYING e VERIFYING dentro de uma passagem síncrona, sem snapshots observáveis para cada etapa. A qualificação física permanece PENDING.
