# Resultados da 0.17B

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
