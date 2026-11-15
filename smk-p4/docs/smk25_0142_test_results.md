# Validação SMK-P4 0.14.2

| Comando executado a partir da raiz | Status | Cobertura |
|---|---|---|
| powershell -File smk-p4/tests/run_smk25_0142.ps1 | PASS | Codec V1/V2/V3, NVS mock, CRC/readback, 16 fontes, Pickup, roteamento por dispositivo/canal, overflow, Stop por borda |
| powershell -File smk-p4/tests/run_performance_014.ps1 | PASS | Macros, MIDI Learn, Pickup e fila de controle |
| powershell -File smk-p4/tests/run_sound_editor.ps1 | PASS | Oito vozes, Juno/FM, 32 algoritmos, PCM, Compare/Revert, presets e real-time allocation probe |
| powershell -File smk-p4/tests/run_sound_editor_ui.ps1 | PASS | LVGL, fonte/destino, Pickup A/B, carga de patch sujo, navegação, Save As somente na soltura válida, Stop com toque e sustain |
| powershell -File smk-p4/tests/run_fx_tests.ps1 all | PASS | Chorus, delay, reverb, EQ, transações e recall de FX |
| powershell -File smk-p4/tests/run_harmony.ps1 | PASS | Harmony Assist, quantização, grupos de notas, nota/sustain e regressões de áudio |
| powershell -File smk-p4/tests/run_usb.ps1 | PASS | Host/descritores, enumeração, desconexão, limpeza e reconexão simuladas |
| C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8 -j 8 | PASS | Candidato com gates elétricos desabilitados |
| C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8-bench -j 8 | PASS | Candidato bench com gates e pinagem do perfil existente |
| python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --environment guition-editor-8 | PASS | Configuração do editor |
| python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --musical --environment guition-editor-8-bench | PASS | Configuração do bench |

O teste com AMY real comparou PCM Juno e FM byte a byte entre o perfil genérico e o Legacy S3 com os knobs em repouso. A suíte no CI inclui os testes de perfil e roteamento e executa o teste LVGL no runner existente do Sound Editor. Os logs locais foram gravados em build/smk25_0142_*.log.

**MANUAL** para medições da Guition e comportamento físico do SMK25. O [procedimento de qualificação](smk25_0142_hardware_candidate.md) inclui notas/sustain, os bancos, Note Off do pad B8, soltura do Stop, reconexão, persistência e oito vozes sob carga.
