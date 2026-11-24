# SMK-P4 0.18-C.1 — Relatorio de Integracao da Navegacao Musical e Performance Stage V3

**Status da etapa:** Integracao em host (`LVGL 9.3.0`) e qualificacao de build `ESP32-P4` (`ESP-IDF 5.5.2`) **CONCLUIDOS** na branch `feature/smk-p4-ui-redesign-018c1`.  
**Veredito de governanca:** **CONDITIONAL GO** para ensaio controlado em bancada | **NO-GO** para prontidao de palco ou merge em `main` ate qualificacao fisica na Guition ESP32-P4 (`A1–A5`, Touch `GT911`, `USB MIDI Host` SMK25 V2 e audicao `PCM5102A`).

---

## 1. Identificacao, Branch e Escopo Preservado

| Item | Evidencia verificada |
|---|---|
| Repositorio / Branch | `https://github.com/ovelhaaa/amy` · `feature/smk-p4-ui-redesign-018c1` |
| Commit-base verificado (`0.18-B.1`) | `e0e4185139741cd31720418ab1a1769ca6850056` |
| Motor DSP / Audio / USB preservados | Zero alteracoes em `src/*`, `smk-p4/components/amy_runtime/*`, `smk-p4/components/audio_pcm5102/*`, `smk-p4/components/usb_midi_host/*` |
| Acoes proibidas nao executadas | Nenhum flash (`-t upload`), nenhum teste fisico, nenhum merge em `main`, nenhuma alteracao no motor DSP |

---

## 2. Arquitetura Adotada

### 2.1 Shell Persistente e Cinco Areas Musicais (`UiArea`)
- Introduzida a abstracao `UiArea` (`Tocar`, `Timbres`, `Ritmo`, `Fx`, `Mais`) em [ui_navigation.h](../components/ui_blueprint/include/ui_navigation.h) e [ui_navigation.cpp](../components/ui_blueprint/src/ui_navigation.cpp), separada de `ScreenId` (mantendo todas as 15 telas legadas intactas).
- O rodape legado de 3 bancos ciclicos (`BANK: MAIN/TOOLS/SYS`) em [tab_bar.cpp](../components/ui_blueprint/widgets/tab_bar.cpp) foi substituido por 5 botoes permanentes (`TOCAR | TIMBRES | RITMO | FX | MAIS`) em posicao fixa, com destaque via `LV_STATE_CHECKED` e selecao em 1 toque.
- `UiNavigationManager` memoriza o ultimo `ScreenId` visitado por area (`area_screens_[5]`) e o `sub_tab` por tela (`sub_tabs_[15]`), montando apenas 1 `ScreenView` por vez em `content_`.
- Em `UiBlueprint::onAreaClick()` ([ui_blueprint.cpp](../components/ui_blueprint/src/ui_blueprint.cpp)):
  - Tocar em outra area navega diretamente para o ultimo modulo visitado daquela area.
  - Tocar novamente em `TIMBRES`, `RITMO` ou `MAIS` (ou no botao `MODS` do cabecalho) abre/fecha o seletor contextual de modulos em largura integral (`800 px`) sem comprimir telas legadas em `640 px`.

### 2.2 Cabecalho Musical Persistente (Variante A `800×44` + Overlays)
- Implementado em [status_strip.cpp](../components/ui_blueprint/widgets/status_strip.cpp) com buffers estaticos (`lv_label_set_text_static`) e estilos pre-anexados (`LV_STATE_CHECKED` / `LV_STATE_USER_1`) para evitar realocacoes TLSF em repouso:
  - Badge de modo (`P4 LIVE` / `DEMO`), Patch ativo + `MIDI`/`AUD`, botao global de `BPM` (`124 BPM INT`), fundamental/escala (`D MIN`), toggles independentes `Q` (`harmony.scale_enable`) e `CH` (`harmony.chord_enable`), botao contextual `MODS`/`TOCAR` e botao global `PANIC`.
- Overlays de `Tempo`, `Harmonia` e `Seletor de Modulos` cobrem apenas `y = kContentTop .. kContentBottom - 1` (`44..431`), mantendo o cabecalho e o `PANIC` **100% descobertos e clicaveis** acima do backdrop.
- O overlay de `Tempo` controla exclusivamente o relogio global unico (`perf.bpm`).
- O overlay de `Harmonia` expoe `Q` e `CH` independentes, exibe `CONFIRMADO` / `PENDING` / `REJEITADO` / `INDISPONIVEL`, e explica o escopo real: `USB Ch N + Arp + Touch` vs `SEQ` (que usa `SEQ QUANT` e forca `CH=OFF`), alem de documentar a rejeicao de `ADD9/9TH` e `DROP2/SPREAD/OCT+5` pelo Audio Owner.

### 2.3 Performance Stage V3 (`TOCAR`) e Integracao do Piloto FX
- `PerformanceScreen` ([performance_screen.cpp](../components/ui_blueprint/screens/performance_screen.cpp)) ocupa os `800 px` completos **sem sidebar permanente**:
  - Banner musical superior: Patch ativo, Cena ativa/estado, atalho `TIMBRES >` e pills compactas `ARP`, `SEQ`, `FX` e `Q/CH`.
  - Seletor de sub-modo: `MACROS 1-4`, `MACROS 5-8`, `CENAS 1-8`, `TECLADO` e `ENCADEAR [PLAN]` (proposta visual explicitamente marcada `NAO IMPLEMENTADO`).
  - Macros `1–4` / `5–8`: sliders amplos preservando macro routing, feedback MIDI e `PerformanceLock`, com faixa inferior de acesso rapido as 8 cenas (`C1..C8` + `SELETIVO >`).
  - Retirados da vista principal: Routing Editor (`sub_tab == 3`), Legacy Offsets (`sub_tab == 2`), telemetria tecnica e ajustes MIDI avancados (acessiveis via `TIMBRES > MODULACAO` e `MAIS`).
- Piloto `FX` ([effects_screen.cpp](../components/ui_blueprint/screens/effects_screen.cpp)): preservado integralmente da `0.18-B.1` (sidebar `DELAY / REVERB / CHORUS / MASTER`, `Chorus Depth` bloqueado em `OFF`, `Delay SYNC`, EQ 3 bandas, bypass global, inspetor recolhivel e maquina de estados dos sliders).

---

## 3. Correspondencia entre as 5 Areas e as 15 Telas Anteriores

| Area Fixa | Modulo / Rota | ScreenId | Estado Visual / Migracao |
|---|---|---|---|
| **TOCAR** | Palco Principal (`MACROS 1-4`, `5-8`, `CENAS 1-8`, `TECLADO`, `ENCADEAR [PLAN]`) | `Performance` (`0`) | **Migrado (Stage V3, 800 px sem sidebar)** |
| **TOCAR** | Recall Seletivo / Gestao de Cenas | `Scenes` (`14`) | Disponivel (subpainel dedicado via `SELETIVO`) |
| **TIMBRES** | Biblioteca | `PatchBrowser` (`1`) | Legado compatível (largura integral 800 px via seletor) |
| **TIMBRES** | Sintese | `SoundEditor` (`2`) | Legado compatível (7 sub-abas, largura integral) |
| **TIMBRES** | Camadas | `Layers` (`3`) | Legado `PLANNED` |
| **TIMBRES** | Mixer | `Mixer` (`4`) | Legado `PLANNED` |
| **TIMBRES** | Modulacao (inclui Routing Editor & Legacy Offsets) | `Modulation` (`5`) | Legado + rotas diretas para Routing Editor e Legacy |
| **RITMO** | Sequenciador | `Sequencer` (`8`) | Legado compatível (largura integral) |
| **RITMO** | Arpejador | `Arpeggiator` (`7`) | Legado compatível (largura integral) |
| **RITMO** | Bateria | `Drums` (`9`) | Legado `PLANNED` |
| **FX** | Delay / Reverb / Chorus / Master & EQ | `Effects` (`6`) | **Migrado (Piloto FX 0.18-B.1 preservado)** |
| **MAIS** | MIDI / SMK25 | `MidiSettings` (`11`) | Legado compatível (largura integral) |
| **MAIS** | Preferencias | `GlobalSettings` (`12`) | Legado compatível (largura integral) |
| **MAIS** | Diagnosticos | `Diagnostics` (`13`) | Legado compatível (largura integral) |
| **MAIS** | Harmonia (tela completa) | `Harmony` (`10`) | Legado compatível (complementa overlay do cabecalho) |

---

## 4. Contrato de Navegacao e Preservacao de Notas

- `UiNavigationManager::unmountCurrent()` ([ui_navigation.cpp](../components/ui_blueprint/src/ui_navigation.cpp)) **nao** chama `backend_->releaseAllUiNotes()`.
- **Garantias verificadas em host com `amy::Runtime` real** ([ui_018c1_integration_test.cpp](../tests/ui_018c1_integration_test.cpp)):
  1. Trocar entre as 5 areas (`TOCAR`, `TIMBRES`, `RITMO`, `FX`, `MAIS`) e abrir/fechar overlays nao envia `NoteOff`, `AllNotesOff`, `Stop` nem reinicia o AMY.
  2. Notas USB MIDI sustentadas com `CC 64` permanecem ativas ao navegar entre areas e ao abrir `FX`.
  3. Retornar a `TOCAR` preserva `Sequencer` e `Arpeggiator` em execucao e mantem `PerformanceLock`.
  4. O teclado Touch (`VirtualKeyboard`) libera **exclusivamente** suas proprias notas (`token`s do widget) no evento `LV_EVENT_DELETE` ao sair de `TECLADO`, mantendo notas USB intactas.
  5. `PANIC` no cabecalho permanece acessivel com overlays abertos e zera todas as vozes (`active_notes == 0`) e transporte de sequenciador/arp conforme contrato do Audio Owner.
  6. Desconexao USB MIDI continua acionando o tratamento de seguranca no firmware/adapter.

---

## 5. Capturas `800×480` Verificadas (`smk-p4/docs/ui_018c1_shots/`)

Todas as 12 capturas abaixo foram geradas a partir de `LocalAmyPerformanceBackend` + `UiBlueprintPerformanceAdapter` (`amy::Runtime` real em host, com cenas `INTRO`/`CHORUS` e slot `BADPATCH` populados via `NvsSceneStore` em RAM) e convertidas para PNG `800×480`:

| # | Arquivo | Cenario | Observacao |
|---|---|---|---|
| 1 | `01_tocar_macros_1_4.png` | Tocar — Macros 1–4 | Backend real (`128 DX7 BRASS 1`, `C1 INTRO ACTIVE`, `Q ON`, `CH ON`, `124 BPM`) |
| 2 | `02_tocar_macros_5_8.png` | Tocar — Macros 5–8 | Alternancia de banco de macros sem perda de contexto |
| 3 | `03_tocar_cenas.png` | Tocar — Cenas 1–8 | `C1: INTRO [ACTIVE]`, `C2: CHORUS [SAVED]`, Locks `FX/HARM/ARP/MAC` |
| 4 | `04_tocar_teclado_touch.png` | Tocar — Teclado Touch | Tecla `C4` pressionada em tempo real (`adapter.isNoteHeld(60)`) |
| 5 | `05_header_harmony_overlay.png` | Cabecalho harmonico em uso | Overlay `D NAT MINOR`, `Q`/`CH` independentes, escopo e `PANIC` livre |
| 6 | `06_overlay_bpm.png` | Overlay de BPM | Relogio global unico `124 BPM`, `-5/-1/+1/+5`, `TAP`, `PANIC` livre |
| 7 | `07_fx_delay.png` | FX — Delay | Piloto FX `0.18-B.1` integrado com rodape de 5 areas |
| 8 | `08_fx_master.png` | FX — Master & EQ | `FX BYPASS`, `MASTER VOL`, `TILT TONE`, EQ 3 bandas |
| 9 | `09_timbres_selecao.png` | Timbres — selecao de modulo | Seletor contextual `BIBLIOTECA / SINTESE / CAMADAS [PLAN] / MODULACAO / MIXER [PLAN]` |
| 10 | `10_ritmo_selecao.png` | Ritmo — selecao de modulo | Seletor contextual `SEQUENCIADOR / ARPEJADOR / BATERIA [PLAN]` |
| 11 | `11_mais_selecao.png` | Mais — selecao de modulo | Seletor contextual `MIDI / SMK25 / PREFERENCIAS / DIAGNOSTICOS / HARMONIA` |
| 12 | `12_cena_pendente_erro_recall.png` | Cena pendente e slot invalido | `C1: INTRO [ACTIVE]`, `C2: CHORUS [PENDING]` (`NextBar` c/ `SEQ ON`), `C8: BADPATCH` (patch `UINT16_MAX`) |

---

## 6. Resultados das Regressoes em Host

Legenda estrita: `PASS`, `FAIL`, `NOT RUN`, `BLOCKED`, `MANUAL`.

| Suite / Comando | Status | Observacoes |
|---|---|---|
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_ui_018c1.ps1` | **PASS** | 12 capturas `800×480`, preservacao USB/Sustain/Seq/Arp/Touch/Locks, `PANIC` em overlay, 500 trocas de area, 500 trocas FX, 100 toggles Macros, 100 overlays Harmonia |
| `powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_preview.ps1` | **PASS** | `blueprint_test_suite.exe` `[1/7]..[7/7]` + 23 PNGs `800×480`; 700 montagens `baseline_free=71632 after_free=71632 diff=0 peak_used=105328 frag=1%` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_preview_018a.ps1` | **PASS** | `[T1]..[T7]` + 19 PNGs `800×480`; 500 transicoes `objs 107->107`, `free=69032B diff=0B frag=4%` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_fx_tests.ps1` | **PASS** | 19 suites FX DSP + persistencia NVS + Gate G |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_sound_editor_ui.ps1` | **PASS** | 7 abas editor, Effects, slider 0.18-B.1, Save/Recall/Compare/Revert, SMK25 pickup, 32 ciclos popup (`diff=0B`) |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_harmony_ui.ps1` | **PASS** | 14 telas, contrato diferido, `real key`, `Off`, stepper, channel, timeout |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_step_sequencer_ui_016b.ps1` | **PASS** | 50 trocas de divisao, Pattern Browser 017A, Scenes 017B + Modal Seletivo, Panic |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_performance_lock_0152a.ps1` | **PASS** | Locks de sessao (`Fx`, `Harmony`, `Arp`, `Macros`) e integridade musical |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_blueprint_adapter.ps1` | **PASS** | Linkagem corrigida em host (filtrando `amy_runtime.o` e `ui_blueprint_performance_adapter.o` duplicados); mock + LVGL 1400 telas (`frag=2%`, `second_window_growth=0`) + AMY real |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_amy.ps1` | **PASS** | Suite autonoma AMY runtime / polyphony / retrigger / backend |
| Testes fisicos Guition ESP32-P4 (`A1–A5`, Touch `GT911`, `USB MIDI`, `PCM5102A`) | **MANUAL** | Adiados explicitamente conforme decisao de projeto; nao executados |

---

## 7. Metricas LVGL (Pool TLSF `128 KiB` em Host)

| Cenario (`ui_018c1_integration_test.cpp`) | Objetos LVGL | `free` (B) | `largest` (B) | `diff` (B) | `peak_used` (B) | `frag` | Latencia Host (`500` areas) |
|---|---|---|---|---|---|---|---|
| Baseline apos warmup | `91` | `74.656` | `73.648` | — | — | `2%` | — |
| Pos `3a` (500 trocas entre 5 areas) | `91` | `74.656` | — | `0` | — | `2%` | `median=2944.6 us`, `P95=4960.3 us`, `max=7353.7 us` |
| Pos `3b` (500 trocas de modulos FX) | `93` | `78.448` | — | — | — | `1%` | — |
| Pos `3c` (100 alternancias `MACROS 1-4 <-> 5-8`) | `91` | `74.656` | — | `0` | — | `2%` | — |
| Pos `3d` (100 aberturas/fechamentos Harmonia + retorno) | `91` | `74.656` | `73.648` | `0` | `74.216` | `2%` | — |

> [!WARNING]
> As latencias acima (`median=2944.6 us`, `P95=4960.3 us`) referem-se apenas a execucao em CPU host Windows (`g++ -O2`) com framebuffer em RAM e **nao** medem leitura I2C `GT911`, flush `MIPI-DSI` ou margem real da task `Audio Owner` / `DMA I2S` no ESP32-P4.

---

## 8. Resultados de Build ESP32-P4 (`ESP-IDF 5.5.2`)

Compilacoes executadas via `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e <perfil> -j 8` e auditadas com `check_blueprint_build.py`:

| Perfil | Status | `check_blueprint_build.py` | RAM PIO (B / %) | Flash PIO (B / %) | `firmware.bin` (B) |
|---|---|---|---|---|---|
| `guition-editor-8` | **PASS** | **PASS** | `69.892 / 327.680 (21.3%)` | `1.481.426 / 16.777.216 (8.8%)` | `1.482.142` |
| `guition-editor-8-bench` | **PASS** | **PASS** (`--musical`) | `70.460 / 327.680 (21.5%)` | `1.586.942 / 16.777.216 (9.5%)` | `1.587.658` |

---

## 9. Limitacoes, Riscos e Pendencias Fisicas

### 9.1 Limitacoes e Riscos
- As telas de `TIMBRES`, `RITMO` e `MAIS` (exceto `FX` e `TOCAR`) ainda utilizam os controladores legados de `800 px` acessados pelo seletor contextual de modulos; sua reestilizacao visual fica para `0.18-C.2`.
- Em `04_tocar_teclado_touch.png`, o rotulo auxiliar a direita de `OCT +` (`SAIR LIBERA TOUCH`) sofre leve recorte na borda direita (`w=128 px`); nao afeta interacao nem testes.
- `ENCADEAR [PLAN]` permanece estritamente como proposta visual nao operacional (`NAO IMPLEMENTADO`), sem Chain Engine ou persistencia NVS.

### 9.2 Pendencias Fisicas Explicitamente Adiadas
1. **A1**: Boot e margens reais de RAM interna, PSRAM e pool LVGL `128 KiB` na Guition ESP32-P4.
2. **A2**: Qualificacao de toque capacitivo `GT911` em `800×480` (alvos de `44 px` no cabecalho, sliders de Macros/FX, overlays com `PANIC`).
3. **A3 completo**: USB MIDI Host com SMK25 V2 (Pitch Bend, Mod, Sustain, hot-unplug -> Panic, reconexao sem reboot).
4. **A4**: Audiencia estereo `PCM5102A` via `I2S` e medicao real de `DMA_underruns` durante navegacao.
5. **A5 e A–H**: Recall de cenas com NVS real e estresse prolongado em hardware.

---

## 10. Recomendacao para `0.18-C.2`

1. Executar primeiro a qualificacao fisica de bancada (`A1–A5`) do conjunto `TOCAR + Cabecalho + FX + 5 Areas` na Guition ESP32-P4 antes de iniciar novas telas.
2. Em `0.18-C.2`, migrar os modulos de `RITMO` (`Sequenciador` com separacao `PLAY`/`EDIT` e `Arpejador`) e `TIMBRES` (`Biblioteca` e `Sintese` com 4 subguias contextuais), mantendo `Camadas`, `Bateria` e `Encadear` como `PLANNED` ate haver contrato no Audio Owner.
