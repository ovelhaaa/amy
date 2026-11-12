# SMK-P4 0.14.1 ‚Äî resultados do incremento

A auditoria e o painel de offsets legados est√£o implementados. O reset usa a solicita√ß√£o de Sound Edit j√° versionada por patch e gera√ß√£o e o snapshot de confirma√ß√£o do owner. Os codecs V1/V2/V3 e o DSP AMY continuam com a implementa√ß√£o da 0.14.

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

O teste LVGL j√° executado pelo CI verifica as tr√™s linhas do painel, reset em dois passos, confirma√ß√£o e recupera√ß√£o do cutoff manual no runtime. A captura do host est√° em `build/editor011/legacy_offsets_0141.ppm`.

No Windows, o runner de Sound Editor mediu 200 blocos com oito vozes. Aplica√ß√£o e render combinados deram m√©dia de 80,87 ¬µs e m√°ximo de 102,20 ¬µs na refer√™ncia, com 112,40 ¬µs de m√©dia e 186,10 ¬µs de m√°ximo durante mudan√ßas nas macros de f√°brica. O runner tamb√©m verificou PCM dos controles existentes e restaura√ß√£o em Compare/Revert. O teste de offsets novo comprova a mudan√ßa do cutoff aplicado. A compara√ß√£o PCM de fixtures V1/V2/V3 entre as duas vers√µes est√° pendente.

Consultar a [matriz de aceita√ß√£o](performance_0141_test_matrix.md), o [or√ßamento de persist√™ncia](performance_0141_persistence.md) e os [hashes das imagens](performance_0141_hardware_candidate.md). O incremento ainda n√£o qualifica o release 0.14.1.

## Incremento de rotas no owner

O cÛdigo 9299ba73fa2848ac8bf17cc120173e9e8ec78271 passou pelo runner Sound Editor, pela suÌte Performance 0.14, pelo runner de UI, e pelo teste transacional `run_fx_tests.ps1 gate_b_tx`. O teste de 48 rotas combina Synth e FX em oito vozes. Em Windows, 200 blocos alternando as oito posiÁıes entre neutro e 127 mediram 92,93 µs de mÈdia e 736,60 µs de m·ximo para submiss„o, aplicaÁ„o e render combinados, com pico 415, clipping zero, nenhuma chamada de alocaÁ„o na janela instrumentada e nenhum incremento de OOM. Os testes exercitam o DSP por mailbox. As mediÁıes de deadline e de memÛria din‚mica no P4 continuam pendentes.

A configuraÁ„o customizada fica na RAM e o Save È recusado enquanto o formato transacional estiver pendente. [ComposiÁ„o e validaÁ„o](performance_0141_macro_routes.md).
