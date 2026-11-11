# SMK-P4 0.13.1 — Hardening final

| Gate | Resultado | Evidência |
|---|---|---|
| 0 | PASS | Branch auditada a partir de `f75625d73a124d87c24f63c96fbeba874d054e57`. O remoto já incluía `7622c2229c4f66bc80abf0ab60715ecccc28c2ec`, revisado antes das mudanças. |
| A | PASS | Regressão 0.13.1 cobre falha de patch, geração obsoleta, timeout, descarte de comandos vencidos e ausência de ACK de FX. |
| B | PASS | Regressão 0.13.1 cobre token, valor e geração incorretos, Master intercalado, Patch Change e timeout. |
| C | PASS | Harness LVGL cobre instância única, expiração e alteração de slot com o mesmo nome. Regressão 0.13.1 cobre duas conclusões `Confirmed`. O teste de commit cobre leitura indisponível, bloqueio sem escrita e reconciliação. |
| D | PASS | `run_sound_editor.ps1` executou `stability_0131_regression_test`. O job host no GitHub Actions recebe o LVGL 9.3.0. |
| E | PASS | Inventário conferido com o catálogo ROM e a lista de admissão. Referências de FM e escopo da evidência física corrigidos. |
| F | PASS | Comparação PCM byte a byte e suítes host de editor, FX, Harmony e USB. |
| G | PASS | Dois builds PlatformIO e auditorias da configuração concluídos com código 0. |
| H | PASS | Relatórios de resultado, estado, UI, catálogo e evidência física atualizados. |
| Hardware | NOT RUN | Smoke físico do candidato pendente. |

## Regressões executadas

Todos os comandos abaixo concluíram com código 0. Os logs locais estão em `build/editor011`.

| Comando | Log |
|---|---|
| `powershell -File smk-p4/tests/run_sound_editor.ps1` | `final_editor.log`, `stability_0131_regression_test.log` |
| `powershell -File smk-p4/tests/run_sound_editor_ui.ps1` | `final_ui.log` |
| `powershell -File smk-p4/tests/run_fx_tests.ps1 all` | `final_fx.log` |
| `powershell -File smk-p4/tests/run_fx_tests.ps1 gate_k_commit` | `final_commit_semantics.log` |
| `powershell -File smk-p4/tests/run_harmony.ps1 -ReuseObjects` | `final_harmony.log` |
| `powershell -File smk-p4/tests/run_usb.ps1` | `final_usb.log` |
| `python smk-p4/tests/audit_patch_library.py --check` | `final_factory.log` |
| `smk-p4/tests/compare_editor_factory.ps1 -Baseline <worktree em 7622c222> -EditorBaseline` | `final_pcm.log` |

O PCM original dos patches 4, 8, 31, 128, 135, 138 e 142 ficou byte a byte idêntico ao da referência auditada. A suíte também verificou oito vozes, FM 1–32, controles de operadores com notas ativas, Sustain, Panic, Harmony Assist, FX, alocação no render e a troca de patches e presets. O diff em `src/` está vazio.

## Candidatos de firmware

Código compilado em `bfcddfe540b46a0f89e87be74df64abbc269ca97` com ESP-IDF 5.5.2 e GCC RISC-V 14.2.0. Comando `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8 -e guition-editor-8-bench -j 8`, código 0. Log `build/editor011/qualified_build.log`. `check_blueprint_build.py` passou nos dois perfis, incluindo `--musical` no bench.

| Perfil | Firmware | Flash usada | RAM estática | SHA-256 de firmware.bin |
|---|---:|---:|---:|---|
| guition-editor-8 | 1.245.472 B | 1.244.736 B (7,4%) | 40.388 B (12,3%) | `6BB85D5115CC340BA782C390494EB019CE8E6FC4D0CD012338793C9927DBBC9B` |
| guition-editor-8-bench | 1.346.176 B | 1.345.464 B (8,0%) | 40.232 B (12,3%) | `CAD08D469692993BE418FA2A62ECBE539DEBD02264659BA93D90F7A152C8BEF5` |

Binários em `smk-p4/.pio/build/<perfil>/`. O SHA-256 de `partitions.bin` (3.072 B) é `73C0B5C3E5FCBA3A151CC70C453C93DD5F4798899E7F2F8CCA76DA1F32FFC501` nos dois perfis. O `bootloader.bin` (23.120 B) tem SHA-256 `9779A6E6E7C9423B88A98AFC87C20A8C3527DC73293E1C9FB9C3C4C718EDDA90` no editor e `7B513D40C892B80C4D9067B84722140B35DD03BF00D20C44C4BAD995F408B3A5` no bench.

## Limites da validação

O prazo de Recall invalida comandos ainda na fila. Uma carga de patch já iniciada pode terminar após o prazo. A reconciliação de um Save incerto exige que o marcador esperado aponte para registros íntegros de síntese e FX. Enquanto essa prova faltar, o slot permanece protegido contra gravações na sessão.

SRAM e PSRAM livres, áudio PCM5102A, underruns, touchscreen e reconexão no hardware permanecem **MANUAL / NOT RUN**. Consulte a [evidência física](stability_0131_hardware_evidence.md).

**GO para iniciar a 0.14 sobre a base de software validada.** A qualificação física continua pendente.
