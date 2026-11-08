# 0.11.1 investigation — incomplete

Baseline: dd491f039561771385b2672c86ca3a315adca8bf. This is not a 0.11.1 release report and no new synthesis parameter is qualified.

## Findings

* `UiBlueprintPerformanceAdapter::editIndex` exposes cutoff, resonance, attack, release, FM depth, operator level and ratio. The owner additionally checks the patch's materialized eight-voice map. The registry alone is not the runtime authority.
* Registry `amp.vel_sens`, `lfo.1.rate_hz` and `lfo.1.depth` claim Available but the adapter has no matching edit command. Modulation slot source and destination also claim Available in the registry although the matrix only changes `UiModel`.
* The live editor's MOD widget previously marked the first mod-wheel-to-pitch row Available based solely on model values and accepted model-only changes. The fix disables those buttons for the real editor and labels the routes Planned; the independent CC1 vibrato is unchanged. The demo backend retains its prior behavior.
* User preset v1 uses fixed 128-byte records, 17 float positions, dual keys, CRC and ROM discriminator. Existing 24 KiB NVS admission assumes seven entries per blob and a 126-entry GC reserve; actual free capacity has not been measured. Extending `Count` without changing this encoding overlaps its ROM and CRC fields, so no new persistent parameters should be enabled before a separately qualified codec and non-destructive storage strategy exist.
* The current Juno capture recognizes exactly three breakpoints but checks envelope topology on the first voice only. New ADSR controls require validating all voice breakpoints and native restoration, not merely adding a slider.
* Prior hardware receipt records a 0.11 image written to the identified board and successful readback, not the current device state. Existing rollback artifacts are in the 0.11 worktree's `build/editor011/flash`; their continued presence and hashes have not been verified in this investigation.

## Validation and remaining work

Baseline `run_harmony.ps1` and `run_sound_editor.ps1` passed in the preceding session. These cover existing controls, not the 0.11.1 objectives. A full inventory, P0/P1 integration, versioned persistence, functional tests, target builds, current backup and physical candidate are outstanding. No NVS, firmware, AMY core or hardware driver was modified. P4 hardware and 800x480 request takes precedence for this task over the older S3-specific specification; no S3 behavior is changed.

Current change validation: PASS direct compilation of `modulation_matrix.cpp` using g++ C++20, warnings as errors and the pinned LVGL headers. This is not a widget interaction test. Full preview and on-device interaction have not been qualified.
PASS `./smk-p4/ui-preview/run_preview.ps1` and `./smk-p4/tests/run_sound_editor_ui.ps1`: existing preview and live-editor host suites. They do not specifically assert the disabled MOD buttons or constitute physical validation.

## Incremento em RAM — Decay e Sustain Juno (0.11.1 incompleta)

Dois IDs novos comandam breakpoints nativos do primeiro oscilador de amplitude
compatível, após verificar a topologia nas oito vozes. Os demais envelopes são
preservados; em Moving Strings o segundo oscilador possui envelope diferente.
A disponibilidade depende da topologia e faixa capturada. Compare e Revert
restauram os valores nativos; a restauração do tempo de decay usa amostras,
sem arredondamento por milissegundos. A UI apresenta o snapshot confirmado.

O codec v1 foi limitado explicitamente aos 17 campos originais; rejeita Save
quando o mask contém os novos IDs. Não há persistência v2, migração nem Recall
desses parâmetros. Portanto o Gate A e a versão 0.11.1 permanecem incompletos.
Não houve alteração das partições ou da NVS. O pedido explícito para P4 tem
precedência para este incremento sobre o alvo S3 mais antigo em specs.md;
nenhum caminho S3 ou núcleo AMY foi alterado.

Testes host com AMY real: patches 4, 0, 2, 3 e 31 tiveram PCM alterado para
ambos os parâmetros em uma e oito vozes, incluindo sustain, retrigger e release;
contagem de clipping foi zero nos cenários executados. IDs 1 e 8 foram
indisponíveis por topologia/faixa. Compare de Decay e Revert de Sustain em ID 4
restauraram PCM de nota nova byte a byte; o teste de estado verifica oito vozes,
preservação de outro envelope e ausência de alocações no caminho observado.
Isso não comprova todas as topologias, deadlines no P4 ou operação física.
Os gates B–H e o inventário completo continuam pendentes.

Validação deste incremento:

| Verificação | Resultado |
|---|---|
| `./smk-p4/tests/run_sound_editor.ps1` | PASS — AMY/PCM, codec v1, Harmony, USB e catálogo host; testes não qualificam hardware |
| `./smk-p4/tests/run_sound_editor_ui.ps1` | PASS — suíte LVGL existente; não cobre exaustivamente os novos sliders |
| Novos parâmetros com persistência v2 | 0; NOT RUN — v2 não implementado |
| Migração v1 → v2 | NOT RUN |
| Smoke físico | MANUAL / NOT RUN; firmware não gravado |
