# Auditoria de Capacidades, Base Histórica e Dependências — SMK-P4 0.18-A

## 1. Identidade de Branch, SHA e Gates Físicos Pendentes

| Item | Evidência verificada |
| --- | --- |
| Branch de trabalho isolada (`0.18-A`) | `feature/smk-p4-ui-redesign-018a` |
| Commit-base verificado (`0.17B` / PR #47) | `579de2b3a34e4f21b9dc28f2a55ad97a9b47031e` (`579de2b3 Record physical SMK25 enumeration and MIDI smoke`) |
| `origin/feature/smk-p4-performance-scenes-017b` | `579de2b3a34e4f21b9dc28f2a55ad97a9b47031e` |
| `refs/pull/47/head` | `579de2b3a34e4f21b9dc28f2a55ad97a9b47031e` (rascunho aberta; **merge NO-GO**) |
| Escopo preservado sem alteração | `smk-p4/components/amy_runtime/*`, `smk-p4/components/audio_pcm5102/*`, `smk-p4/components/usb_midi_host/*`, `smk-p4/main/*` |

### Gates Físicos da 0.17B na Guition ESP32-P4

Com base em [`performance_scenes_017b_memory_recovery.md`](performance_scenes_017b_memory_recovery.md), [`performance_scenes_017b_first_hardware_session.md`](performance_scenes_017b_first_hardware_session.md) e [`performance_scenes_017b_test_results.md`](performance_scenes_017b_test_results.md):

| Gate físico (COM13, rev. 1.3, MAC `80:F1:B2:D3:CC:86`) | Estado atual | Evidência / Pendência |
| --- | --- | --- |
| **A1 · Boot com recuperação LVGL PSRAM (`128 KiB`)** | **PASS (em repouso)** | `ADMISSION admitted=1`, `ENGINE ready=1`, `AUDIO I2S_PRODUCING`, `OUTPUT08 frames` 266.240 → 1.235.712, repouso 243.283 B interno / 30.347.784 B PSRAM / 83.192 B pool LVGL; **`DMA_underruns = UNMEASURED`**. |
| **A2 · Touchscreen GT911 / UI interativa** | **MANUAL / NOT TESTED** | Refresh e heap LVGL observados na serial, sem teste de toque nem validação visual física na 0.17B. |
| **A3 · USB MIDI Host SMK25 V2** | **PARTIAL PASS** | Enumeração (`USB state=2 connects=1`), 538 eventos Note On/Off canal 1 com 0 perdas/overflows (`trace_dropped = 1.020`). Faltam pitch bend, modulação, desconexão/reconexão e troca de perfil. |
| **A4 · Audição PCM5102A estéreo** | **MANUAL / NOT TESTED** | Sinal digital reportado; escuta física estéreo (ruído, clipping, Note Off, Panic) pendente. |
| **A5 · Scenes no aparelho / NVS real** | **MANUAL / NOT TESTED** | Sem exercício de captura/recall/cancela na placa física. |
| **A–H · Estresse físico, `SWAP08`, carga combinada** | **MANUAL / NOT TESTED** | Exceção do carregador de patches marcada como `PROPOSED` (até 24 alocações / 2.880 B em host). |

> [!WARNING]
> Testes em host LVGL, capturas `800×480` e builds não qualificam áudio I²S, touchscreen GT911, PSRAM ou USB Host na Guition.

---

## 2. Inventário das 15 Telas Atuais (`ui_blueprint`) e Pontos de Recriação

No baseline 0.17B (`ui_screens.h`, `ui_blueprint.cpp`, `ui_navigation.cpp`, `tab_bar.cpp`, `status_strip.cpp`), existem **15 telas** distribuídas em **3 bancos cíclicos** (`MAIN`, `TOOLS`, `SYS`):

| ScreenId | Banco | Arquivo | Entrada real em `IUiBackend` | Estado real | Recriação LVGL observada |
| --- | --- | --- | --- | --- | --- |
| `Performance` | MAIN | `performance_screen.cpp` | Macros `1..8`, `requestSelectPatch`, `routingSnapshot`, `legacyOffsets`, `sceneSlot`, Keyboard Touch | **AVAILABLE** | Destrói e recria tela inteira em `navigateTo` e troca de sub-modo (`MACROS` ↔ `KEYS`). |
| `PatchBrowser` | MAIN | `patch_browser_screen.cpp` | `UiModel::catalog()`, `isPatchSelectable`, `requestSelectPatch`, `confirmPatchDiscard`, `userPreset` | **AVAILABLE** (`P4 ROM` / `USER NVS`) | Destrói/recria em navegação e muda página reconstruindo lista. |
| `SoundEditor` | MAIN | `sound_editor_screen.cpp` | `hasSoundEditor`, `requestSoundAction`, parâmetros `osc.*`/`fm.*`/`filter.*`/`env.*`/`amp.*`/`lfo.*`, `userPreset*` | **AVAILABLE** (FM + Sub no escopo suportado) | 7 sub-tabs; cada troca chama `navigateTo(SoundEditor, tab)` recriando toda a tela; botões `EXPAND` (`66×18`) e ops (`54×24`) pequenos em `fm_algorithm_selector.cpp`. |
| `Layers` | MAIN | `layers_screen.cpp` | Somente leitura / status `PLANNED` (`layer_b.*` bloqueado) | **PLANNED** (sem dual-layer/split no engine unificado) | Recria em navegação. |
| `Effects` | MAIN | `effects_screen.cpp` | `fx.delay.*`, `fx.chorus.*`, `fx.reverb.*`, `fx.master_tone`, `fx.eq.low_db`, `delayStatus()` | **AVAILABLE** parcial (ver Seção 4) | 11 cards simultâneos na mesma tela; recria em navegação; sem bypass individual. |
| `Mixer` | TOOLS | `mixer_screen.cpp` | Apenas leitura visual / `PLANNED` | **PLANNED / PARTIAL** | Recria em navegação. |
| `Modulation` | TOOLS | `modulation_screen.cpp` | `mod.*` **`PLANNED`**, routing parcial via `routingSnapshot` | **PLANNED** (`mod.slot1.*`) | 2 sub-tabs; recria em navegação. |
| `Arpeggiator` | TOOLS | `arpeggiator_screen.cpp` | `arpStatus()`, `requestArpTransport`, `requestClearLatch`, `arp.*` | **AVAILABLE** | Botão PLAY/STOP sem badge explícito quando desativado no demo; recria em navegação. |
| `Sequencer` | TOOLS | `sequencer_screen.cpp` | `sequencerStatus()`, `sequencerPattern()`, `requestSequencer`, `patternSlot`, `requestPattern` | **AVAILABLE** (`DEMO` se `sequencerStatus()==nullptr`) | Mistura PLAY, EDIT, Live Transpose e Step Record numa tela; recria em navegação. |
| `Drums` | TOOLS | `drums_screen.cpp` | `drums.*` em `UiModel` (somente RAM demo) | **PLANNED** | Controles `DEMO`/`PLANNED` recriados em navegação. |
| `Harmony` | TOOLS | `harmony_screen.cpp` | `harmony.*`, `live_harmony`, `harmony.preset` | **AVAILABLE** (parcial — ver Seção 3) | 2 sub-tabs recriando toda a árvore por sub-tab. |
| `Scenes` | SYS | `scenes_screen.cpp` | `sceneSlot()`, `sceneView()`, `requestScene()`, modal seletivo/rename/delete | **AVAILABLE** | Recria tela e abre/fecha subpainéis dinâmicos; sem `Encadear`. |
| `MidiSettings` | SYS | `midi_screen.cpp` | `hasMidiPerformance`, `midiPerformanceAction`, `midiProfileName`, `midiBankName`, `midiLearnStatus` | **AVAILABLE** (parcial) | 2 sub-tabs recriados; perfil e custom targets dependentes de hardware. |
| `GlobalSettings` | SYS | `settings_screen.cpp` | `sys.*` (maioria `PLANNED`/`DEMO`), `sessionLockMask` em runtime | **PLANNED / PARCIAL** | Recria em navegação. |
| `Diagnostics` | SYS | `diagnostics_screen.cpp` | `model().diagnostics()`, counters USB/AMY/Heap/LVGL | **AVAILABLE** (em firmware bench) / **DEMO** em host | Recria em navegação. |

### Gargalos Estruturais Encontrados no Shell Atual
1. **Bump de revisão por tick** (`smk-p4/main/ui_blueprint_performance_adapter.cpp:1704-1714`): sempre que `backend_.harmonyState()` não é nulo, o adapter chama `model_.bumpRevision()`, obrigando `UiBlueprint::service()` (`ui_blueprint.cpp:114-120`) a atualizar `status_strip_`, `nav_bar_` e toda a tela ativa via `LV_EVENT_REFRESH` a cada ciclo.
2. **Desmontagem total a cada troca de tela/sub-tab** (`smk-p4/components/ui_blueprint/src/ui_navigation.cpp:116-143`): `navigateTo()` executa `unmountCurrent()` e `mountScreen()` até mesmo ao mudar apenas `sub_tab` interno.
3. **Liberação global de notas UI em qualquer navegação** (`ui_navigation.cpp` → `unmountCurrent()` chama `backend_->releaseAllUiNotes()`): embora `releaseAllUiNotes()` no adapter libere apenas `gestures_` (Touch) e preserve `usb_`, acoplar limpeza de notas à troca de telas sem teclado mistura ciclo de vida de UI com ciclo de vida musical.

---

## 3. Auditoria Harmônica: `Q` (`scale_enable`) vs `CH` (`chord_enable`) vs `SEQ SCALE QUANTIZE` vs Fontes MIDI

Prova direta em `smk-p4/components/amy_runtime/amy_runtime.cpp:238-255` e `smk-p4/components/amy_runtime/harmony.h:7-36`:

1. **`Q` (`harmony.scale_enable` / `harmony::Config::scale_on`) e `CH` (`harmony.chord_enable` / `harmony::Config::chord_on`) são controles independentes**:
   - Ativar `Q` não ativa `CH`; ativar `CH` não ativa `Q`; o cabeçalho nunca deve unificá-los num toggle único.
2. **Escopo exato por origem de nota em `Runtime::startGroup()` (`amy_runtime.cpp:246-254`)**:
   - `NoteOrigin::UsbDirect` e `NoteOrigin::Arpeggiator`: aplicam `Q` e `CH` **somente** quando `c.channel == state_.harmony.channel` (`harmony.usb_channel`, canais `1..16`, equivalente 0-based `0..15`; não há modo Omni). Notas de outros canais passam secas (`scale_on = chord_on = false`).
   - `NoteOrigin::Touch`: aplica `Q` e `CH` **somente** quando `state_.harmony.touch` (`harmony.apply_touch`) é `true`.
   - `NoteOrigin::Sequencer`: **sobrescreve** `config.scale_on = sequencer_.status().quantize` e **força `config.chord_on = false`**. O sequenciador lê `root`, `scale` e `quant` de `state_.harmony`, mas **não obedece ao toggle `Q` do cabeçalho** e nunca gera acordes `CH`.
3. **Restrições e rejeições confirmadas pelo owner (`harmony::valid`, `harmony.h:32-36`)**:
   - `harmony.chord_type` índices `10` (`ADD9`) e `11` (`9TH`) são rejeitados (`c.chord != 10 && c.chord != 11`).
   - `harmony.voicing` suporta apenas `0` (`CLOSE`) e `1` (`OPEN`); índices superiores não são válidos no firmware atual (`c.open < 2`).
   - `harmony.chord_type` `12` (`DIAT TRIAD`) e `13` (`DIAT 7`) exigem escala de 7 notas (`degrees(scale) == 7`).
   - Inversão `3` exige acorde de 4 notas (`7`, `8`, `9` ou `13`).

---

## 4. Auditoria Real de FX, Bypass e Parâmetros no Contrato Atual

Prova direta em `smk-p4/components/amy_runtime/fx_state.h:31-189`, `performance_factory_routes.h:120-149` e `smk-p4/main/ui_blueprint_performance_adapter.cpp:22-357`:

| Módulo / Parâmetro | Contrato em `IUiBackend` / `UiModel` | Realidade em `FxConfig` / `applyBusFx` | Observação UX obrigatória |
| --- | --- | --- | --- |
| **Delay (`fx.delay.*`)** | `mode` (`FREE`/`SYNC`), `division`, `time_ms` (travado em `SYNC`), `feedback`, `mix`, `delayStatus()` | **Não existe** campo/comando individual ON/OFF ou bypass de Delay em `FxConfig`. | `mix = 0%` **NÃO equivale a bypass**: sobrescreve mix manual e **M3/M7** ainda somam `DelayMix` em `effectiveFx()`. Exibir `BYPASS INDIV: N/A`. |
| **Reverb (`fx.reverb.*`)** | `size`, `mix` | **Não existe** ON/OFF ou bypass individual de Reverb em `FxConfig`. | `mix = 0%` **NÃO neutraliza M6/M7** em `effectiveFx()`. Exibir `BYPASS INDIV: N/A`. |
| **Chorus (`fx.chorus.*`)** | `mode` (`OFF..VIBRATO`), `depth` | **Único módulo FX com OFF real**: `ChorusMode::Off` aplica `chorus_level = 0.0f` em `applyBusFx()` (`fx_state.h:176-178`). | Macros modulam apenas `chorus_depth`; logo `OFF` desliga realmente o efeito. |
| **Master & EQ** | `perf.master_volume`, `fx.master_tone`, `fx.eq.low_db` | `FxConfig` possui `eq_mid_db`, `eq_high_db` e `bypass_all` (`fx_state.h:56-58,161`), **sem `ParameterDescriptor` nem entrada em `isFxId()` / `setFxParameter()`**. | Registrar como gap para **0.18-B**; não simular controles falsos na 0.18-A. |
| **FX Planned** | `fx.drive`, `fx.routing` | `FeatureStatus::Planned`; cadeia fixa `CHORUS -> DELAY -> REVERB -> EQ`. | Manter bloqueado/sinalizado como `PLANNED`. |

---

## 5. Baseline do Ambiente de Preview & Capturas `800×480`

- **Pipeline**: MinGW GCC 14.2.0 (`-O2 -std=c++20 -Wall -Wextra -Werror`) compilando **LVGL 9.3.0** (`managed_components/lvgl__lvgl`) com pool TLSF de **128 KiB** (`lv_conf.h`) e framebuffer `RGB565` `800×480` salvo em PPM P6 e convertido para PNG (`smk-p4/ui-preview/run_preview.ps1`).
- **Correções mínimas aplicadas ao harness de preview para restabelecer compilação pós-0.17B**:
  1. `smk-p4/ui-preview/run_preview.ps1`: adicionados includes `amy_runtime`/`src` e link de `amy_runtime.o` + objetos AMY C (`build/harmony09/*.c.o`) devido à referência real de `fm_algorithm_selector.cpp` ao símbolo `algorithms[33]` (`src/algorithms.c`).
  2. `smk-p4/components/ui_blueprint/tests/blueprint_test_suite.cpp`: alinhadas 3 asserções defasadas pré-0.17B (`"M8 EDGE"`, navegação terminal em `Diagnostics`, isenção de badges no demo em `Arpeggiator`/`Sequencer`) + exceção explícita de altura `>= 18 px` restrita a `ScreenId::SoundEditor` para o widget herdado `fm_algorithm_selector.cpp:523` (`66×18`).
- **Resultado baseline pós-ajuste**: `run_preview.ps1` **PASS** (`[1/7]`..`[7/7]`, LVGL heap pós-700 trocas `baseline_free=63512 after_free=63512 diff=0 peak_used=110728 frag=0%`, 20 PNGs `800×480`).
