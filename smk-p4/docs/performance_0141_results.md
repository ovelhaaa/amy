# SMK-P4 0.14.1 — resultados do incremento

A auditoria e o painel de offsets legados estão implementados. O reset usa a solicitação de Sound Edit já versionada por patch e geração e o snapshot de confirmação do owner. Os codecs V1/V2/V3 e o DSP AMY continuam com a implementação da 0.14.

## Testes

| Comando | Resultado |
|---|---|
| powershell -File smk-p4/tests/run_performance_014.ps1 | PASS |
| powershell -File smk-p4/tests/run_sound_editor.ps1 | PASS |
| powershell -File smk-p4/tests/run_sound_editor_ui.ps1 | PASS |
| powershell -File smk-p4/tests/run_fx_tests.ps1 all | PASS, incluindo falhas transacionais com NVS simulada |
| powershell -File smk-p4/tests/run_harmony.ps1 | PASS |
| powershell -File smk-p4/tests/run_usb.ps1 | PASS |
| python smk-p4/tests/audit_patch_library.py --check | PASS |
| pio run -d smk-p4 -e guition-editor-8 -j 8 | PASS |
| pio run -d smk-p4 -e guition-editor-8-bench -j 8 | PASS |
| python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --environment guition-editor-8 | PASS |
| python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --musical --environment guition-editor-8-bench | PASS |
| Guition, touchscreen, USB e PCM5102A | MANUAL |

O teste LVGL já executado pelo CI verifica as três linhas do painel, reset em dois passos, confirmação e recuperação do cutoff manual no runtime. A captura do host está em `build/editor011/legacy_offsets_0141.ppm`.

No Windows, o runner de Sound Editor mediu 200 blocos com oito vozes. Aplicação e render combinados deram média de 80,87 µs e máximo de 102,20 µs na referência, com 112,40 µs de média e 186,10 µs de máximo durante mudanças nas macros de fábrica. O runner também verificou PCM dos controles existentes e restauração em Compare/Revert. O teste de offsets novo comprova a mudança do cutoff aplicado. A comparação PCM de fixtures V1/V2/V3 entre as duas versões está pendente.

Consultar a [matriz de aceitação](performance_0141_test_matrix.md), o [orçamento de persistência](performance_0141_persistence.md) e os [hashes das imagens](performance_0141_hardware_candidate.md). O incremento ainda não qualifica o release 0.14.1.

## Incremento de rotas no owner

O código 9299ba73fa2848ac8bf17cc120173e9e8ec78271 passou pelos runners Sound Editor, UI e Performance 0.14 e pelo teste transacional run_fx_tests.ps1 gate_b_tx. O teste de 48 rotas combina Synth e FX em oito vozes. Em Windows, 200 blocos alternando as posições entre neutro e 127 mediram 92,93 µs de média e 736,60 µs de máximo para submissão, aplicação e render combinados. O pico foi 415, com clipping zero, nenhuma chamada de alocação na janela instrumentada e nenhum incremento de OOM. Os testes exercitam o DSP por mailbox. As medições de deadline e memória dinâmica no P4 continuam pendentes.

A configuração customizada fica na RAM. O Save é recusado enquanto o formato transacional estiver pendente. [Composição e validação](performance_0141_macro_routes.md).
