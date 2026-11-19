# SMK-P4 0.18-B.1 — Relatório de Hardening do Piloto FX, Touch UX e Qualificação de Build

**Status da etapa:** Hardening em host e qualificação de compilação ESP-IDF 5.5.2 **CONCLUÍDOS** (`feature/smk-p4-ui-redesign-018a`)  
**Veredito de governança:** **CONDITIONAL GO** para ensaio controlado em bancada (após autorização humana para o SHA-256 do binário `bench`) | **NO-GO** para release de palco ou merge em `main` até aprovação física de `A1–A5`, Touch `GT911`, `USB MIDI Host` e escuta estéreo `PCM5102A`.

---

## 1. Identificação e Escopo Preservado

| Item | Evidência verificada |
|---|---|
| Repositório / Branch | `https://github.com/ovelhaaa/amy` · `feature/smk-p4-ui-redesign-018a` |
| Commit-base verificado no início da solicitação | `51c592b4a4ce1dcec37848718386535bdf6634e5` |
| Série de commits implementada na 0.18-B.1 | `29566947`, `6727474c`, `3f3b594e`, `88f81839`, `7d9e27ba`, `6c353c16` (+ este relatório e capturas em `docs/`) |
| Domínio protegido preservado (**zero alterações**) | `src/*`, `smk-p4/components/amy_runtime/*`, `smk-p4/components/audio_pcm5102/*`, `smk-p4/components/usb_midi_host/*`, `smk-p4/main/local_performance_backend.h` e layout persistente `fx::Record` V2 de 32 B (`fx_state.h`) |
| Ações físicas proibidas não executadas | Nenhum flash (`-t upload`), nenhum monitor serial, nenhum merge em `main`, nenhuma remoção de branch |

---

## 2. Diagnóstico Inicial em 4 Níveis

Separação estrita do estado encontrado no ponto de partida (`51c592b4`):

| Nível | Situação verificada no início da 0.18-B.1 |
|---|---|
| **1. Confirmado no código** | • `fx_state.h` (`applyBusFx`) aplica `chorus_level = 0.0f` real quando `ChorusMode::Off`, preservando `cfg.chorus_depth` em RAM e NVS V2 (`32 B`); Delay e Reverb não possuem bypass individual em `FxConfig` (`bypass_all` silencia/zera toda a cadeia `CH>DLY>REV>EQ`).<br>• `UiBlueprintPerformanceAdapter::isParameterEditable()` bloqueava `fx.delay.time_ms`/`division` por `SYNC`, mas **não** bloqueava `fx.chorus.depth` quando `chorus_mode == Off`.<br>• `UiBlueprintPerformanceAdapter` emitia `model_.bumpRevision()` em todo ciclo com `harmonyState()` não nulo e incondicionalmente em `releaseAllUiNotes()`.<br>• `parameter_slider.cpp` usava 7 registros individuais de evento LVGL sem proteção contra `LV_EVENT_PRESS_LOST`, `LV_EVENT_CANCEL`, `LV_EVENT_INDEV_RESET`, animações de transição padrão de tema nem destruição de painel em arraste (`lv_obj_clean`).<br>• Firmware de produção segue usando `BottomNavBar` legado de 3 bancos cíclicos (`BANK: MAIN >`, `BANK: TOOLS >`, `BANK: SYS >`), com o piloto contextual apenas em `EffectsScreen`. |
| **2. Demonstrado apenas no protótipo (`0.18-A`)** | • Shell visual de 5 áreas (`TOCAR | TIMBRES | RITMO | FX | MAIS`), cabeçalhos Variante A (`44 px`) / B (`68 px`), overlays com `PANIC` acima do backdrop e wireframe `Encadear [NÃO IMPLEMENTADO]` em [ui_prototype_018a.cpp](../ui-preview/ui_prototype_018a.cpp) (não integrado ao firmware ESP-IDF). |
| **3. Testado apenas em host (MinGW + LVGL 9.3.0)** | • Coalescência de 35 ms no arraste básico, 40 ciclos `SEQ<->FX`, `500` trocas FX em RAM, suítes FX DSP (`19` suítes), Sound Editor, Harmony 14 telas, Performance Lock e geração de capturas `800×480` RGB565. **Não qualifica áudio I²S nem touchscreen físico.** |
| **4. Pendente de qualificação física na Guition ESP32-P4** | • Sessão histórica da `0.17B` (`18/11/2026`, COM13, ESP32-P4 rev. 1.3, MAC `80:F1:B2:D3:CC:86`) cobriu somente **A1 PASS em repouso** (`243.283 B` internos, `30.347.784 B` PSRAM, `83.192 B` pool LVGL, `DMA_underruns = UNMEASURED`) e **A3 parcial PASS** (538 Note On/Off ch1 com `USB state=2 connects=1` e `trace_dropped=1.020`).<br>• Seguem pendentes no hardware novo: reteste `A1`, **A2** (`GT911` touch/drag real em `800×480`), **A3 completo** (Pitch Bend, Mod, sustain, desconexão->Panic e reconexão), **A4** (audição `PCM5102A` + `DMA_underruns`), **A5** (Scenes/NVS) e **A–H**. |

---

## 3. Problemas Encontrados e Correções Implementadas

### 3.1 Correção obrigatória: Chorus OFF × `fx.chorus.depth` (`29566947`)
- **Diagnóstico:** `EffectsScreen` sinalizava Chorus desligado, porém `UiBlueprintPerformanceAdapter::isParameterEditable()` não verificava `fx.chorus.mode == 0`, deixando o slider, o botão de card e o Inspetor de `fx.chorus.depth` editáveis na UI.
- **Arquitetura escolhida (fronteira limpa entre UI e Engine):**
  - Mantido intacto `fx_state.h` (`applyBusFx()` aplica `chorus_level = 0.0f` em `Off` sem zerar `cfg.chorus_depth`), bem como a aceitação de rotas de macro/MIDI (`performance_factory_routes.h`) e carga de presets NVS V2 no engine/backend.
  - Em `UiBlueprintPerformanceAdapter::isParameterEditable()` ([ui_blueprint_performance_adapter.cpp](../main/ui_blueprint_performance_adapter.cpp)) e em `UiDemoBackend::isParameterEditable()` ([ui_demo_backend.cpp](../components/ui_blueprint/src/ui_demo_backend.cpp)), `fx.chorus.depth` retorna `false` enquanto `fx.chorus.mode == OFF` (`0`), voltando a `true` nos modos ativos (`1..5`).
  - Na apresentação (`parameter_slider.cpp`, `parameter_card.cpp` e `effects_screen.cpp`), quando `!isParameterEditable()`:
    - o widget aplica `LV_STATE_DISABLED` visualmente esmaecido, bloqueia comandos de edição por toque e mostra badge `OFF` em `fx.chorus.depth` (sem alterar nem zerar o valor salvo no backend);
    - o rótulo principal de valor continua espelhando exclusivamente o snapshot confirmado pelo Audio Owner;
    - transições `OFF -> ON -> OFF` via toque ou atualização externa (MIDI/macros/snapshot) atualizam editabilidade e exibem novamente o `Depth` preservado.

### 3.2 Robustez dos sliders touchscreen e prevenção de drift TLSF (`6727474c`, `88f81839`)
- **Máquina de estados e ciclo de toque (`parameter_slider.cpp`):**
  - Implementada máquina explícita `SliderUiState::{Confirmed, Drag, Pending, Rejected}` no `SliderCtx`.
  - **Coalescência de 35 ms (`3.1`):** durante arraste contínuo (`LV_EVENT_VALUE_CHANGED` com `LV_STATE_PRESSED`), atualiza o valor local de prévia imediatamente (distinto visualmente com `EDIT` / estado âmbar), envia `requestParameterFloat()` respeitando janela `>= 35 ms` e garante commit único no encerramento caso a posição final difira do último valor enviado (evitando duplicação se já coincidirem). Clampa limites min/max, respeita passos discretos, valores negativos e escala logarítmica.
  - **Cancelamento e rejeição (`3.2`):** tratados `LV_EVENT_RELEASED`, `LV_EVENT_PRESS_LOST`, `LV_EVENT_CANCEL` e `LV_EVENT_INDEV_RESET` — protegendo contra disparo tardio de `LV_EVENT_VALUE_CHANGED` que o LVGL 9.3 pode emitir após perda/cancelamento de toque sem `LV_STATE_PRESSED`. Em cancelamento sem commit ativo, restaura exatamente o valor confirmado do snapshot; em rejeição imediata, `Requested -> Rejected` tardio ou timeout no backend, exibe indicador `REJ` e reverte para o valor confirmado no snapshot; guard `in_teardown_move` evita UAF/escrita em `lv_obj_clean(detail_panel_)` ou troca de módulo FX com comando pendente, usando tokens monotonamente validados via `fx_pending_token_` no adapter.
  - **Alterações externas (`3.3`):** rastreamento separado entre valor em edição local e último snapshot confirmado garante que atualizações externas (ex.: SMK25 / macros) durante o arraste ou após soltar mantenham visível a divergência em `DRAG`/`WAIT` e sincronizem assim que a interação ou confirmação termina — incluindo desabilitação imediata em mid-drag caso `fx.chorus.mode` mude para `OFF`.
- **Correção crítica de memória/LVGL (`88f81839`):**
  - Durante os testes expandidos, identificou-se que `blueprint_test_suite.exe` esgotava o pool TLSF ao montar duas telas simultaneamente e `sound_editor_ui_test.cpp` acusava divergência de `2.576 B` após 32 ciclos de popup (`memory_before=58392 != memory_after=60968`).
  - A causa-raiz estava nos `lv_slider`s com estilo padrão do `lv_theme_default`, que criavam animações ocultas `transition_delayed` (~9 structs `lv_anim_t` ativas) quando `LV_STATE_DISABLED` alternava, somado a 7 `lv_event_dsc_t` por slider.
  - **Solução:** aplicação de `lv_obj_remove_style_all(slider)` antes dos estilos explícitos do tema, consolidação dos 7 handlers de eventos em um único callback `LV_EVENT_ALL` por slider e compactação de `SliderCtx` via enum compacto (`ToneColor`). Resultado: `blueprint_test_suite.exe` (`7/7`) e `sound_editor_ui_test.cpp` (32 ciclos) voltaram a **PASS** com `0 B` de desvio, sem alterar nenhuma asserção legada de memória.

### 3.3 Refinamento visual da tela FX e fonte Montserrat ASCII (`3f3b594e`, `7d9e27ba`)
- **Arquitetura mantida:** sidebar esquerda fixa (`DELAY`, `REVERB`, `CHORUS`, `MASTER`, cadeia fixa `CH>DLY>REV>EQ`, badge `ATIVA`/`BYPASS`, `< TOCAR`) + painel contextual à direita apenas com os controles do módulo selecionado (`effects_screen.cpp`).
- **Limpeza de jargão e hierarquia musical:** removidos textos persistentes de engenharia (`BYPASS INDIV: N/A`, `FxConfig/IUiBackend`, `applyBusFx()`, `MIX 0% != BYPASS M3/M7`). Priorizados nome, leitura em `Montserrat 20` e área de toque em cards `StageStacked`; badges `EDIT`/`WAIT`/`REJ`/`OFF`/`SYNC`/`LOCK` ocupam apenas posição discreta à direita sem competir com o controle musical.
- **Inspetor contextual recolhível sem realocações:** barra inferior fixa alterna entre modo recolhido (`AJUSTE FINO` + resumo do parâmetro focado + `-`/`+`) e modo expandido (`FECHAR` + `PARAM >` + `- FINO` + `+ FINO`), preservando `inspector_expanded_` entre os módulos FX e usando exclusivamente buffers estáticos (`lv_label_set_text_static`).
- **Preservação de verdade técnica:** cadeia fixa mantida sem inventar bypass individual em Delay ou Reverb (notas curtas e honestas substituem o jargão: *"Delay sem bypass individual. Mix 0% mantem macros."*), EQ de 3 bandas e bypass global preservados, `fx.delay.time_ms` travado em `SYNC` e `fx.chorus.depth` bloqueado quando `OFF`.
- **Correção tipográfica LVGL (`7d9e27ba`):** a inspeção visual das capturas `800×480` revelou travessões UTF-8 (`—`) renderizados como caixas de glifo ausente (`[]`) porque os builds LVGL do projeto habilitam apenas glifos ASCII em `Montserrat 14/20/28`. Substituídos por separador ASCII `|` com `non-ascii == 0` nos arquivos UI alterados.

### 3.4 Investigação e eliminação de `bumpRevision()` em repouso (`29566947`, `6727474c`)
- **Diagnóstico:** `UiBlueprintPerformanceAdapter::service()` incrementava `model_.revision()` incondicionalmente a cada ciclo quando `harmonyState()` não era nulo, provocando travessias de `refresh()`/`LV_EVENT_REFRESH` redundantes na UI mesmo sem mudança alguma de estado.
- **Correção:** introduzido cache de comparação de campos no bloco `harmonyState()` e guarda `if (released)` em `releaseAllUiNotes()`; em `ui_model.cpp`, `setValue()`/`setNotice()` passaram a incrementar `revision_` apenas quando há alteração efetiva; em `status_strip.cpp` e `tab_bar.cpp`, adicionadas comparações antes de `lv_label_set_text_static()`/`add_state`/`remove_state`.
- **Impacto medido em host:** o tempo médio por bloco do loop combinado em `step_sequencer_ui_016b` caiu de `404.82 us/block` (baseline `0.18-B`) para **`50.24 us/block`** (`-87,6%`), e `Effects` durante `200` ticks ociosos registrou `rev_delta = 0`, `diff = 0 B`.

---

## 4. Mudanças de Contrato em Testes Existentes (Documentadas)

Nenhuma asserção existente foi enfraquecida artificialmente. Duas atualizações contratuais estritamente justificadas foram aplicadas aos testes legados:
1. **[sound_editor_ui_test.cpp:119](../tests/sound_editor_ui_test.cpp#L119) (`auditInventory`):** adicionado salto explícito de edição interativa para `fx.chorus.depth` quando `fx.chorus.mode == 0`, simétrico à exceção já existente de `fx.delay.time_ms` sob `SYNC` e `fx.delay.division` sob `FREE`.
2. **[blueprint_test_suite.cpp:806](../components/ui_blueprint/tests/blueprint_test_suite.cpp#L806):** `ScreenId::Effects` foi isentada da exigência de badges textuais legados `AVAIL`/`PLAN` por parâmetro porque o piloto migrado utiliza o layout `StageStacked` (`StageMinimal`), alinhado à isenção já aplicada a `Scenes`, `Arpeggiator` e `Sequencer`.

---

## 5. Matriz Completa de Validação (`7d9e27ba` + `6c353c16`)

Legenda estrita: `PASS`, `FAIL`, `NOT RUN`, `BLOCKED`, `MANUAL`.

| Verificação / Comando | Status | Escopo e Resultados Observados |
|---|---|---|
| `powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_preview.ps1` | **PASS** | `blueprint_test_suite.exe` `[1/7]..[7/7]` + `23` PNGs `800×480`; `700` montagens de tela `baseline_free=63632 after_free=63632 diff=0 peak_used=109472 frag=0%` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_preview_018a.ps1` | **PASS** | `[T1]..[T7]` + `19` PNGs `800×480`; `500` transições `objs 107->107`, `free=69032B largest=66768B diff=0B peak_used=73432B frag=4%`, host `median=1126.3us P95=1240.8us max=1505.7us` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_fx_tests.ps1` | **PASS** | `19` suítes FX incluindo `delay_sync_0151_test`, `gate_a..gate_m` (`gate_g` expandido com Chorus `OFF->ON->OFF`, preserva Depth, rejeição/pendente e `100` ticks ociosos com `0` bumps), `chorus_test`, `reverb_test`, `delay_test`, `eq_test`, `fx_preset_persistence_test` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_sound_editor_ui.ps1` | **PASS** | `ProbeSliderBackend` (`3.1`, `3.2`, `3.3`), Chorus `OFF->ON->OFF` em `EffectsScreen`, mudança com pendente, preservação de notas Touch/USB + Sustain CC 64 + Arp/Seq/Locks, retorno `< TOCAR` preservando último sub-módulo, `500` trocas FX (`diff=0B`), `50` toggles do Inspetor (`diff=0B`), `200` ticks ociosos (`rev_delta=0`), `32` ciclos popup sem drift (`diff=0B`) e geração de `fx_01..fx_08` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_harmony_ui.ps1` | **PASS** | `14` telas, contrato diferido, `real key`, `Off`, `stepper`, `channel`, `timeout` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_step_sequencer_ui_016b.ps1` | **PASS** | `50.24 us/block`; `settings=32544 B`, `editor=34616 B`, `sequencer=47032 B`, **`FX=67720 B`**; `017A` Pattern Browser, `017B` Scenes + Modal Seletivo (`66128` -> `52056 B`) e `40` ciclos `SEQ<->FX` determinísticos ao byte |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_performance_lock_0152a.ps1` | **PASS** | Preservação de locks de sessão (`Fx`, `Harmony`, `Arp`, `Macros`) e fluxos de retorno |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_amy.ps1` | **PASS** | Execução autônoma validando `amy_runtime_test`, `usb_musical_test`, `polyphony_test`, `test_retrigger_scenarios`, `render_retrigger_probe`, `local_backend_test`, `performance_backend_contract_test` e `performance_ui_test` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_blueprint_adapter.ps1` | **BLOCKED** | Falha de linkagem pré-existente: o script concatena `build/ui_blueprint_host/ui_obj/*.o` (que já contém `amy_runtime.o` e `ui_blueprint_performance_adapter.o` de outros alvos) com `amy_runtime.cpp` e `ui_blueprint_performance_adapter.cpp` diretos. Mantido inalterado por disciplina de escopo; a lógica de adapter correspondente foi exercitada nos alvos integrados com AMY em host |
| `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8 -j 8` | **PASS** | Compilação real ESP-IDF 5.5.2 (sem warnings nos módulos alterados) |
| `python smk-p4/tests/check_blueprint_build.py ... --environment guition-editor-8` | **PASS** | Target ESP32-P4, gates elétricos, adapter ativo, árvore LVGL única, pool 128 KiB, AMY 48000/256 |
| `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8-bench -j 8` | **PASS** | Compilação real ESP-IDF 5.5.2 com USB/I²S/telemetria bench |
| `python smk-p4/tests/check_blueprint_build.py ... --musical --environment guition-editor-8-bench` | **PASS** | Target ESP32-P4, gates bench ativos, pool 128 KiB em PSRAM (`smkp4_lvgl_pool`), ausência de `.bss.work_mem_int.` |
| Testes físicos na placa Guition (`A1`, `A2`, `A3 completo`, `A4`, `A5`, `A–H`) | **MANUAL** | Proibido gravar flash ou rodar sessão física nesta etapa; qualificação de áudio, touch GT911 e USB SMK25 segue pendente |

---

## 6. Métricas de Recursos LVGL e Responsividade (Baseline `0.18-B` vs `0.18-B.1`)

Pool TLSF configurado em **128 KiB** (`LV_MEM_SIZE = 131072 B`) em host LVGL 9.3.0.

| Cenário Medido | Baseline (`0.18-A` / `0.18-B`) | Atual (`0.18-B.1` — pós `88f81839`/`7d9e27ba`) | Observação |
|---|---|---|---|
| **500 trocas FX in-place** (`editor011`, adapter AMY real) | `objs 99->99`, `free 75.088->75.088 B` (`diff=0 B`), `peak=83.400 B`, `frag=1%`, host `median=2437.8 us`, `P95=4012.2 us`, `max=7820.9 us` | **`objs 95->95`**, **`free 75.648->75.648 B` (`diff=0 B`)**, **`largest 75.584->75.584 B`**, `peak=85.104 B`, `frag=1%`, host **`median=745.0 us`**, **`P95=871.5 us`**, **`max=1102.0 us`** | **+560 B livres** em `free`; P95 host **−78%** após remoção de transições/callbacks redundantes e bumps ociosos |
| **500 transições 0.18-A** (`test_018a_suite`) | `objs 107->107`, `free=69.032 B`, `peak=73.432 B`, `frag=4%`, host `median=2842.1 us`, `P95=4880.6 us`, `max=8395.3 us` | **`objs 107->107`**, **`free 69.032->69.032 B` (`diff=0 B`)**, **`largest 66.768->66.768 B`**, `peak=73.432 B`, `frag=4%`, host **`median=1126.3 us`**, **`P95=1240.8 us`**, **`max=1505.7 us`** | Zero degradação estrutural; redução expressiva de tempo host pela limpeza de invalidações no shell |
| **50 toggles do Inspetor FX** (`AJUSTE FINO <-> FECHAR`) | n/d | **`objs 113->113`**, **`free 68.264->68.264 B` (`diff=0 B`)**, `frag=14%` | Zero realocação nos labels/botões estáticos ao expandir/recolher |
| **200 ticks ociosos em FX** (`rev_delta` / heap) | Bump contínuo por tick no adapter | **`rev_delta = 0`**, **`free 68.264->68.264 B` (`diff=0 B`)**, `frag=14%` | Elimina redesenhos LVGL contínuos em repouso |
| **Loop combinado render + pub + LVGL** (`step016b-ui`) | `404.82 us/block`, `FX free=67.448 B`, `40` ciclos `diff=0 B` | **`50.24 us/block`**, **`FX free=67.720 B`** (`+272 B`), `40` ciclos `SEQ<->FX` determinísticos (`0 B`) | Redução de **8×** no custo de controle em host; `+4.296 B` frente ao `0.17B` (`63.424 B`) |
| **700 montagens globais** (`blueprint_test_suite`) | `free=63.624 B`, `peak=110.320 B`, `frag=0%` | **`free 63.632->63.632 B` (`diff=0 B`)**, `peak=109.472 B`, `frag=0%` | Zero vazamento líquido e menor pico (`-848 B`) |

> [!WARNING]
> **Ressalva obrigatória (`HOST CPU != GUITION ESP32-P4`):** todas as durações em microssegundos acima foram medidas em CPU desktop Windows (`g++ -O2`) usando framebuffer RAM em [preview_harness.h](../ui-preview/preview_harness.h). Elas **não representam** latência de I2C/debounce do controlador capacitivo **GT911**, tempo de flush **MIPI-DSI / PPA**, contenção de barramento **PSRAM** no ESP32-P4 ou margem real da task `Audio Owner` / `DMA I²S` para **PCM5102A**.

---

## 7. Qualificação Real de Build ESP-IDF (`ESP32-P4` — ESP-IDF 5.5.2)

Compilações sequenciais executadas via `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e <perfil> -j 8` e auditadas com `check_blueprint_build.py`.

| Perfil | Status | DRAM0 `.data` / `.bss` (B) | RAM PIO (B / %) | Flash PIO (B / %) | `firmware.bin` (B) | SHA-256 `firmware.bin` | SHA-256 `firmware.elf` |
|---|---|---|---|---|---|---|---|
| **`guition-editor-8`** | **PASS** | `37.904` / `31.776` *(+160 B `.bss` vs 0.17B `31.616`)* | `69.680 / 327.680` (`21,3%`) | `1.470.266 / 16.777.216` (`8,8%`) | `1.470.992` | `99e2ed0a1434b94d20f6f1c40765cbe8cc2528f92352b0936fc4c22278d02fc5` | `5f6209b8a05ce88d84149322fc077ef97e43e6610e8f86800d8594aba3fa99ef` |
| **`guition-editor-8-bench`** | **PASS** | `38.332` / `31.888` *(+160 B `.bss` vs 0.17B `31.728`)* | `70.220 / 327.680` (`21,4%`) | `1.575.758 / 16.777.216` (`9,4%`) | `1.576.480` | `152b1d13a4ae3392efda3e8d0e2a42e76d8a01d62d67b1161604f7b75dfbec2f` | `1a4b13d16f41f3bbe91359a135117083842cfbf616f45878b814b633e95d88a8` |

Resumo detalhado de seções linkadas (`DIRAM` e `Flash` totais do resumo PIO):
- **`guition-editor-8`:** `DIRAM` total `168.378 B / 576.464 B (29,21%)` (`.text=98.698`, `.data=37.904`, `.bss=31.776`), `HP core RAM = 104 B`, Flash seção `.text=817.216`, `.rodata=516.448`, imagem total `1.470.982 B`.
- **`guition-editor-8-bench`:** `DIRAM` total `174.156 B / 576.464 B (30,21%)` (`.text=103.936`, `.data=38.332`, `.bss=31.888`), `HP core RAM = 104 B`, Flash seção `.text=896.466`, `.rodata=537.024`, imagem total `1.576.474 B`.
- Ambas as imagens passaram em `check_blueprint_build.py`, confirmando `CONFIG_IDF_TARGET_ESP32P4=1`, `CONFIG_SMKP4_UI_BLUEPRINT=1`, `CONFIG_LV_MEM_SIZE_KILOBYTES=128`, `AMY_SAMPLE_RATE=48000`, `AMY_BLOCK_SIZE=256`, presença de `smkp4_lvgl_pool` no `.map` e ausência de `.bss.work_mem_int.` em DRAM interna.
- `check_blueprint_build.py` emite explicitamente: `MANUAL: compilation does not qualify physical audio, USB, touch, memory or deadlines`.

---

## 8. Navegação, Preservação de Estado e Proposta `0.18-C`

### 8.1 Navegação legada (`ui_blueprint` produção) vs Proposta `0.18-A`
- **Firmware integrado atual (`BottomNavBar` + `UiNavigationManager`):** preserva os **3 bancos legados de 6 botões** (`BANK: MAIN >`, `BANK: TOOLS >`, `BANK: SYS >` em [tab_bar.cpp](../components/ui_blueprint/widgets/tab_bar.cpp)) para todas as 15 telas sem alterar o restante do produto.
- **Dentro de `EffectsScreen` (Piloto `0.18-B` / `0.18-B.1`):** usa navegação contextual com sidebar lateral (`DELAY`, `REVERB`, `CHORUS`, `MASTER`, `< TOCAR`) e persiste a última aba selecionada em `nav.subTab(ScreenId::Effects)`.
- **Verificações comprovadas nesta etapa:**
  - Alternar entre `Delay`, `Reverb`, `Chorus` e `Master` reconstrói apenas `detail_panel_` mantendo `last_fx_module`, **sem** disparar `Stop` de Arp/Sequencer, Panic ou liberação de notas USB MIDI / Sustain `CC 64` / Touch.
  - O retorno via `< TOCAR` restaura `PerformanceScreen` preservando `PerformanceLock`, transporte de Arp/Sequencer e notas ativas, mantendo o último módulo FX selecionado ao reentrar em `FX`.

### 8.2 Proposta de integração futura (`0.18-C` — **NÃO implementada nesta etapa**)
1. **Substituir o `BottomNavBar` cíclico de 3 bancos pelo rodapé fixo de 5 áreas** (`TOCAR | TIMBRES | RITMO | FX | MAIS`) apenas quando a validação física de `FX` (`A1–A5`) estiver aprovada na Guition.
2. **Adotar shell único de topo + overlays globais** (Variante A `44 px` com `Q` e `CH` independentes, `BPM` e `PANIC` acima do backdrop) e desacoplar `releaseAllUiNotes()` da desmontagem genérica de telas, mantendo-o restrito ao teclado Touch.
3. **Migrar progressivamente `RITMO` (`Sequenciador` PLAY/EDIT + `Arpejador` + `Padrões`) e `TIMBRES` (`Biblioteca` + `Síntese` + `Modulação`)**, mantendo `Camadas`, `Bateria` e `Encadear` explicitamente rotulados `PLANNED / NÃO IMPLEMENTADO` até haver contrato real no Audio Owner.

---

## 9. Capturas `800×480` Verificadas (`smk-p4/docs/ui_018b1_shots/`)

Todas as 8 capturas abaixo foram geradas a partir do backend real `LocalAmyPerformanceBackend` + `UiBlueprintPerformanceAdapter` (`AMY Runtime` ativo, `800×480` RGB565) pós-commit `7d9e27ba` e inspecionadas visualmente:

| Arquivo | Cenário capturado | Link |
|---|---|---|
| `fx_01_delay.png` | Delay ativo em modo `FREE` (`350 ms`, feedback `40 %`, mix `35 %`, `DIVISION` desabilitado) | [fx_01_delay.png](ui_018b1_shots/fx_01_delay.png) |
| `fx_02_reverb.png` | Reverb (`REVERB ROOM` `70 %`, `REVERB MIX` `30 %`, nota curta honesta sem bypass individual) | [fx_02_reverb.png](ui_018b1_shots/fx_02_reverb.png) |
| `fx_03_chorus_off.png` | **Chorus em `OFF`** — card/slider `CHORUS DEPTH` visivelmente desabilitado com badge `OFF` e aviso dinâmico de desligamento real | [fx_03_chorus_off.png](ui_018b1_shots/fx_03_chorus_off.png) |
| `fx_04_chorus_on.png` | **Chorus em `CLASSIC`** — `CHORUS DEPTH` reabilitado com leitura âmbar (`69 %`) e valor preservado após transição `OFF -> ON` | [fx_04_chorus_on.png](ui_018b1_shots/fx_04_chorus_on.png) |
| `fx_05_master.png` | Master & EQ (`FX BYPASS` `OFF`, `MASTER VOL`, `TILT TONE`, EQ 3 bandas `EQ LOW`/`MID`/`HIGH`, cadeia `CH>DLY>REV>EQ`) | [fx_05_master.png](ui_018b1_shots/fx_05_master.png) |
| `fx_06_inspector_expanded.png` | Inspetor expandido (`FECHAR`, `PARAM >`, `- FINO`, `+ FINO`, `CONFIRMED`, ajuste fino passo a passo) | [fx_06_inspector_expanded.png](ui_018b1_shots/fx_06_inspector_expanded.png) |
| `fx_07_slider_pending.png` | Estado pendente (`WAIT` âmbar em `DELAY MIX` `80 %`) durante interação/confirmação em curso | [fx_07_slider_pending.png](ui_018b1_shots/fx_07_slider_pending.png) |
| `fx_08_slider_rejected.png` | Rejeição explícita (`REJ` vermelho em `DELAY FDBK` revertido ao valor confirmado `40 %`) | [fx_08_slider_rejected.png](ui_018b1_shots/fx_08_slider_rejected.png) |

---

## 10. Plano de Verificação Física na Guition ESP32-P4 e Riscos

### 10.1 Checklist físico obrigatório (após aprovação do SHA-256 do bundle `guition-editor-8-bench`)
1. **A1 (Boot e margens de memória):** validar `BENCH_ID`, `ADMISSION admitted=1`, `ENGINE ready=1`, `AUDIO I2S_PRODUCING`, `smkp4_lvgl_pool` em PSRAM e ausência de erros/OOM em 30 s de captura serial passiva.
2. **A2 (Touchscreen GT911 em `800×480`):** validar toque em `DELAY`/`REVERB`/`CHORUS`/`MASTER`, `< TOCAR`, `- FINO`/`+ FINO`, toggle `AJUSTE FINO`/`FECHAR`, arraste rápido nos sliders de `fx.chorus.depth`/`fx.delay.mix`/`fx.eq.*`, arraste para fora do controle (forçando perda/cancelamento) e tentativa de toque em `CHORUS DEPTH` quando `OFF`.
3. **A3 completo (SMK25 V2 + interações externas):** tocar acorde com sustain `CC 64` mantido, alternar módulos FX e retornar via `< TOCAR` conferindo zero notas presas/dropouts; provocar mudança externa de `fx.chorus.mode` para `OFF` durante edição e testar desconexão física -> Panic automático -> reconexão sem reboot.
4. **A4 (Áudio I²S / PCM5102A e Chorus/EQ):** escuta estéreo limpa durante transições `Chorus OFF <-> Classic/Juno I`, sweeps contínuos de sliders EQ 3 bandas, Delay `FREE`/`SYNC`, Reverb e toggle de `FX BYPASS`, registrando `DMA_underruns` (ainda `UNMEASURED`).
5. **A5 e A–H:** salvar/recarregar Scenes envolvendo FX e NVS em placa real, além de corrida prolongada observando `free_internal`, `largest_block` e fragmentação real do pool LVGL em PSRAM.

### 10.2 Premissas de hardware e riscos residuais
- **Hardware não confirmado integralmente:** revisão de serigrafia da PCB Guition JC4880P433, straps exatos do módulo `PCM5102A`, VBUS do barramento USB HS0 e resposta de cancelamento/ruído do controlador `GT911` sob sweep contínuo só podem ser confirmados em bancada.
- **Riscos residuais:**
  - `run_blueprint_adapter.ps1` falha isoladamente por conflito de arquivos `.o` compartilhados em `build/ui_blueprint_host/ui_obj/` (script legado não modificado nesta etapa).
  - Latência de flush **MIPI-DSI** com `280 px` / `640 px` de painel na placa real ainda é desconhecida e pode exigir ajustes de throttle caso o barramento PSRAM apresente contenção sob carga de 8 vozes.

---

## 11. Veredito

- **Engenharia em Host & Build Qualification ESP-IDF:** **APROVADO (`PASS`)** em todas as metas de hardening da etapa `0.18-B.1`.
- **Bancada Guition ESP32-P4:** **CONDITIONAL GO** — pronto para gravação autorizada e execução da bateria `A1–A5` usando o build `guition-editor-8-bench` (`SHA-256 152b1d13a4ae3392efda3e8d0e2a42e76d8a01d62d67b1161604f7b75dfbec2f`).
- **Release de palco / Merge em `main`:** **NO-GO** até conclusão positiva dos testes físicos (`A2`, `A3 completo`, `A4` com `DMA_underruns` medidos, `A5` e `A–H`).
