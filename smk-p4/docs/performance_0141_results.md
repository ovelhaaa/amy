# SMK-P4 0.14.1 — resultados

Código de firmware compilado em 650ab199f76787781d70d487e78e21072773d296.

A UI edita até 48 rotas, exibe Manual/Applied e confirma mudanças pelo owner. Save/Recall V4 grava o conjunto de rotas no próprio registro Synth, sob o marcador que já inclui FX. Compare usa ROM com macros suspensas. Revert restaura a referência salva, quando aplicável. MIDI Learn e Pickup aceitam destinos personalizados por banco/knob, com perfil V2 e leitura do perfil V1.

## Regressões

| Comando | Resultado |
|---|---|
| powershell -File smk-p4/tests/run_performance_014.ps1 | PASS |
| powershell -File smk-p4/tests/run_sound_editor.ps1 | PASS |
| powershell -File smk-p4/tests/run_sound_editor_ui.ps1 | PASS |
| powershell -File smk-p4/tests/run_fx_tests.ps1 all | PASS |
| powershell -File smk-p4/tests/run_harmony.ps1 | PASS |
| powershell -File smk-p4/tests/run_usb.ps1 | PASS |
| python smk-p4/tests/audit_patch_library.py --check | PASS |
| pio run -d smk-p4 -e guition-editor-8 -j 8 | PASS |
| pio run -d smk-p4 -e guition-editor-8-bench -j 8 | PASS |
| python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --environment guition-editor-8 | PASS |
| python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --musical --environment guition-editor-8-bench | PASS |
| CI remoto | NOT RUN. Os runners de editor, LVGL e FX estão no workflow |
| Guition bench | Gravação, verificação, PSRAM, inicialização do display e integridade da NVS PASS. Admissão de áudio FAIL. Ensaio musical NOT RUN. |

Os framebuffers do teste LVGL ficam em build/editor011/legacy_offsets_0141.ppm e build/editor011/routing_editor_0141.ppm. Os testes confirmam alteração no barramento AMY e restauração após remover a rota. O teste transacional cobre o ponto de publicação do marcador, corrupção V4 e a interrupção de nova tentativa após recuperar o banco anterior.

## PCM e tempo em Windows

O runtime 0.14 do commit 37c7caf0a794298f740efb52a672e05ae041ffd4 e o runtime 0.14.1 renderizaram nove trechos idênticos de fixtures V1/V2/V3. O corpus de 1.179.648 bytes teve SHA-256 317c7bd55ee5be52eb72901c3bc4864d11a32b6de6fe4d2a2d47c201c5233265 nos dois builds host. A comparação usou o mesmo harness, flags e objetos C, com fontes de runtime da revisão indicada e da árvore atual. O runner também valida PCM V4 antes e depois da serialização.

| Cenário host, oito vozes e 200 blocos | Média | Máximo |
|---|---:|---:|
| Referência, aplicação e render | 52,01 µs | 88,00 µs |
| Mudanças de macro de fábrica, aplicação e render | 69,98 µs | 105,30 µs |
| 48 rotas Synth/FX, submissão, aplicação e render | 86,79 µs | 127,50 µs |

No cenário de 48 rotas, o pico PCM foi 415, com clipping zero, zero alocações instrumentadas e nenhum incremento de OOM. A medição combina as etapas no host. Tempo de composição isolado, deadlines, margem de fila e memória dinâmica no ESP32-P4 ficam para o ensaio físico.

[Hashes e rollback](performance_0141_hardware_candidate.md) · [Compatibilidade e capacidade](performance_0141_persistence.md) · [Matriz de aceitação](performance_0141_test_matrix.md).
