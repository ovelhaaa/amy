# SMK-P4 0.18-B — Relatório do Piloto FX, Coalescência TLSF e Validação

## 1. Identificação e Escopo Preservado

- **Branch de trabalho:** `feature/smk-p4-ui-redesign-018a`
- **Commit-base verificado:** `579de2b3a34e4f21b9dc28f2a55ad97a9b47031e` (`579de2b3 Record physical SMK25 enumeration and MIDI smoke`)
- **Domínios protegidos inalterados:** `smk-p4/components/amy_runtime/*`, `smk-p4/components/audio_pcm5102/*`, `smk-p4/components/usb_midi_host/*` e o layout persistente de 32 bytes de FX (`fx::Record` V2 em `fx_state.h`) permanecem intactos, sem alterações de DSP, fila SPSC, NVS, ownership de notas ou flash em hardware.

---

## 2. Alterações Implementadas na 0.18-B

### 2.1 B.1 — Exposição Completa de EQ 3 Bandas e Bypass Global no Contrato UI
1. **[ui_parameters.cpp](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/components/ui_blueprint/src/ui_parameters.cpp):** adicionados os descritores `fx.eq.mid_db` (`Float -12..12 dB`), `fx.eq.high_db` (`Float -12..12 dB`) e `fx.bypass_all` (`Bool`) com `FeatureStatus::Available`.
2. **[ui_blueprint_performance_adapter.cpp](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/main/ui_blueprint_performance_adapter.cpp):** atualizados `isFxId()`, inicialização no construtor, sincronização de snapshot e rollback de timeout para os 3 novos IDs.
3. **[local_performance_backend.h](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/main/local_performance_backend.h):** roteamento em `LocalAmyPerformanceBackend::setFxParameter()` para `desired_fx_.eq_mid_db`, `desired_fx_.eq_high_db` e `desired_fx_.bypass_all`.

### 2.2 B.2 — Coalescência de Arraste em `parameter_slider.cpp`
- **[parameter_slider.cpp](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/components/ui_blueprint/widgets/parameter_slider.cpp):** unificados `LV_EVENT_VALUE_CHANGED`, `LV_EVENT_RELEASED` e `LV_EVENT_REFRESH` em uma única alocação `SliderCtx` (`attachLvContext`):
  - Durante arraste contínuo (`LV_STATE_PRESSED`), envios são coalescidos em janela `>= 35 ms` (`lv_tick_get()`) enquanto o rótulo numérico acompanha localmente;
  - Se `requestParameterFloat()` retornar `UiCommandState::Requested`, o valor exibido reflete a solicitação em andamento; se `!res.accepted()`, reverte imediatamente para o valor confirmado em `model()`;
  - Em `LV_EVENT_RELEASED`, caso haja posição final ainda não enviada (`pending_release_commit`), dispara commit exato para o Audio Owner.

### 2.3 B.3 — Piloto Contextual `EffectsScreen` e Determinismo TLSF do Shell
- **[effects_screen.cpp](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/components/ui_blueprint/screens/effects_screen.cpp):**
  - Sidebar esquerda permanente em ordem TLSF LIFO (`DELAY`, `REVERB`, `CHORUS`, `MASTER`, cadeia fixa não editável `CH>DLY>REV>EQ` / `CHAIN: ACTIVE|BYPASSED`, retorno `< TOCAR` via `requestReturnToPerformance()`).
  - Painel direito contextual exibindo exclusivamente o efeito selecionado (com `FOCUS >`, `- FINE`, `+ FINE` no Inspetor e avisos honestos `BYPASS INDIV: N/A` em Delay/Reverb e `CHORUS OFF REAL`).
  - Troca in-place via `lv_obj_clean(detail_panel_)` + persistência em `nav.subTab(ScreenId::Effects)` sem chamar `navigateTo()` nem `releaseAllUiNotes()`, preservando notas USB/Touch e transporte de `Arp`/`Sequencer`.
- **Achado e correção TLSF mid-heap no shell compartilhado:**
  - Durante a regressão `SEQ <-> FX` (`step_sequencer_ui_016b_test.cpp:443`), diagnóstico em GDB revelou que `used_cnt` permanecia idêntico (`1227` blocos em `SEQ`, `908` em `FX`), mas `free_size` alternava em `16 B` (`47336 -> 47352` em `SEQ`; `67424 -> 67408` em `FX`) devido a realocações em widgets persistentes do shell enquanto telas estavam montadas.
  - Eliminadas as realocações dinâmicas pós-build:
    1. **[tab_bar.cpp](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/components/ui_blueprint/widgets/tab_bar.cpp):** `BottomNavBar` agora pré-associa `&theme().tab_button_active` ao estado `LV_STATE_CHECKED` e alterna apenas `lv_obj_add_state`/`remove_state` no `refresh()`.
    2. **[ui_widgets.h](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/components/ui_blueprint/include/ui_widgets.h)** e **[status_strip.cpp](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/components/ui_blueprint/widgets/status_strip.cpp):** `StatusStrip::tempo_label_` agora usa buffer fixo `tempo_buf_[24]` + `lv_label_set_text_static()`.
    3. **[effects_screen.cpp](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/components/ui_blueprint/screens/effects_screen.cpp):** labels dinâmicos e botões do Inspetor convertidos para `timing_buf_`/`notice_buf_`/`insp_*_buf_` com `lv_label_set_text_static()`.
- **Polimento visual 0.18-A:** corrigido o recorte na 2ª linha de `COMPORTAMENTO CONFIRMADO` em [ui_prototype_018a.cpp](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/ui-preview/ui_prototype_018a.cpp) (`12_fx_chorus_off.png`) via `createWrapLabel` e ampliada a largura do label de status do Arpejador (`16_ritmo_arp.png`).

---

## 3. Resultados Verificados (Pós-Edição Final)

| Verificação / Comando | Status | Evidências e Métricas Exatas |
|---|---|---|
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_step_sequencer_ui_016b.ps1` | **PASS** | 40 ciclos `SEQ <-> FX` com zero variação de pool TLSF (`sequencer=47424 bytes`, `FX=67448 bytes`, **+4.024 B livres** no painel FX vs `63424 B` na 0.17B); `404.82 us/block`; `settings=32960 B`, `editor=35032 B`; `017A` e `017B` aprovados |
| `powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_preview.ps1` | **PASS** | `blueprint_test_suite.exe` `[1/7]..[7/7]`; 700 montagens de tela `baseline_free=63624 after_free=63624 diff=0 peak_used=110320 frag=0%`; 23 PNGs `800×480` incluindo 4 módulos FX; troca in-place e persistência de sub-tab sem liberar notas ativas |
| `powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_preview_018a.ps1` | **PASS** | `[T1]..[T7]`; 500 transições `objs_before=107 -> objs_after=107`, `free=69032B diff=0B peak_used=73432B frag=4%`; duração host `median=2842.1us P95=4880.6us max=8395.3us`; 19 PNGs `800×480` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_fx_tests.ps1` | **PASS** | 19/19 suítes FX: `delay_sync_0151_test`, `gate_a..gate_m`, `gate_b_fx_test`, `chorus_test`, `reverb_test`, `delay_test`, `eq_test`, `fx_preset_persistence_test` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_sound_editor_ui.ps1` | **PASS** | Coalescência `<35ms` (`tok0+1` em arraste, `tok0+2` no `RELEASED`), preservação de 2 notas (Touch + USB) nos 4 módulos, `SYNC` lock em `MANUAL TIME`, round-trip `fx.eq.mid_db`/`eq.high_db`/`bypass_all` + 4 existentes, **500 trocas in-place em FX**: `objs_before=99 objs_after=99`, `LVGL free_before=75088B free_after=75088B diff=0B peak_used=83400B frag=1%`, host `median=2437.8us P95=4012.2us max=7820.9us` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_harmony_ui.ps1` | **PASS** | `PASS real LVGL + AMY Harmony: deferred confirmation,14 screens,real key,Off,planned options,stepper,channel,timeout` |
| Inspeção visual `800×480` (`ui_018b_shots/` + `16_ritmo_arp.png`) | **PASS** *(com nota menor em 0.18-A)* | 8 capturas FX inspecionadas (4 `DemoBackend` + 4 `AMY` live): zero sobreposição, zero corte em `CH>DLY>REV>EQ` e `ST: CONFIRMED \| PLAN: fx.drive/routing`. Em `16_ritmo_arp.png` (protótipo 0.18-A), a haste esquerda do `'H'` em `H:0 P:0 \| WAITING FOR ARP OWNER` ainda tangencia o limite esquerdo do label (`248 px`) por ~2 px. |
| Build ESP-IDF (`idf.py build`) e testes físicos na Guition ESP32-P4 | **NOT TESTED** | Proibido gravar firmware ou rodar sessão física nesta fase sem aprovação humana |

---

## 4. Comparativo de Recursos LVGL (Antes 0.17B vs Depois 0.18-B)

| Cenário Medido (Pool TLSF 128 KiB) | Antes (`0.17B` baseline) | Depois (`0.18-B` FX Piloto) |
|---|---|---|
| Heap LVGL livre com `Effects` montado (`step016b-ui`) | `63.424 B` | **`67.448 B`** (`+4.024 B`) |
| 40 ciclos alternados `SEQ <-> FX` (`step016b-ui`) | Determinístico | **Determinístico (`0 B` de desvio)** |
| 500 trocas de módulos dentro de `Effects` (`editor011`, adapter AMY real) | n/d (1 tela plana única) | **`objs 99->99`, `free 75.088 B -> 75.088 B` (`diff=0 B`), `peak_used=83.400 B`, `frag=1%`** |
| 700 montagens globais `UiBlueprint` (`blueprint_test_suite`, 4 abas FX incluídas) | `free 63.512 B`, `peak 110.728 B`, `frag 0%` | **`free 63.624 B`, `peak 110.320 B`, `frag 0%`** |
| Duração em **CPU Host Windows** (`500` trocas FX in-place) | n/d | **`median=2437.8 us`, `P95=4012.2 us`, `max=7820.9 us`** |

> [!WARNING]
> As durações acima (`median=2437.8 us`, `P95=4012.2 us`, `max=7820.9 us` e `404.82 us/block`) são medições em **CPU PC Host com framebuffer em RAM (`preview_harness.h`)**. Elas **não representam** latência física de leitura I2C no touchscreen **GT911**, tempo de flush **MIPI-DSI / PPA**, contenção de barramento PSRAM no **ESP32-P4** ou garantia de áudio sem underruns na **PCM5102A**.

---

## 5. Capturas `800×480` Geradas na 0.18-B

Em [smk-p4/docs/ui_018b_shots](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/docs/ui_018b_shots):
- **Demo (`preview_main.exe`):** [07_effects.png](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/docs/ui_018b_shots/07_effects.png), [07b_effects_reverb.png](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/docs/ui_018b_shots/07b_effects_reverb.png), [07c_effects_chorus.png](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/docs/ui_018b_shots/07c_effects_chorus.png), [07d_effects_master.png](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/docs/ui_018b_shots/07d_effects_master.png)
- **AMY Runtime Real (`sound_editor_ui_test.cpp`):** [live_01_fx_delay.png](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/docs/ui_018b_shots/live_01_fx_delay.png), [live_02_fx_reverb.png](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/docs/ui_018b_shots/live_02_fx_reverb.png), [live_03_fx_chorus.png](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/docs/ui_018b_shots/live_03_fx_chorus.png), [live_04_fx_master.png](file:///c:/Users/devx/Documents/PlatformIO/Projects/amy/smk-p4/docs/ui_018b_shots/live_04_fx_master.png)

---

## 6. Gates Físicos Pendentes na Guition ESP32-P4

Continuam obrigatórios em bancada antes de qualquer merge em `main` ou expansão para `0.18-C`:
1. **A2 — Touchscreen GT911 (800×480 física):** arraste contínuo e toque fino (`- FINE` / `+ FINE`, sidebar `140×50`, sliders `202 px` e `306 px` em Delay/Reverb/Chorus/Master) sem toques fantasmas ou perda de foco.
2. **A3 completo — USB MIDI Host (M-VAVE SMK25 V2):** Pitch Bend, Mod Wheel, sustain, desconexão física durante nota sustentada (Panic automático) e reconexão limpa.
3. **A4 — Áudio I²S / PCM5102A:** medição real de `DMA_underruns` (atualmente `UNMEASURED`) e audição estéreo limpa durante sweeps de EQ/Delay/Reverb/Chorus e alternâncias rápidas entre módulos FX com notas ativas.
4. **A5 e Estresse A–H:** comportamento de Scenes/NVS, `heap` interno, `PSRAM` e `LVGL_POOL` em uso prolongado (observando ainda o comportamento periódico de `model_.bumpRevision()` no adapter em placa real).
