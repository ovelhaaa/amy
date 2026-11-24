# SMK-P4 0.18-C.2 — Timbres & System UX: Contextual Editors, Patch Workflow e Musical Usability

**Status da etapa:** Implementação em host (`LVGL 9.3.0` + `amy::Runtime` real) e qualificação de build `ESP32-P4` (`ESP-IDF 5.5.2`) **CONCLUÍDOS** na branch `feature/smk-p4-ui-redesign-018c2`.  
**Veredito de governança:** **APROVADO EM HOST** | **NO-GO para prontidão de palco** (qualificação física em placa Guition `JC-ESP32P4-M3`, `GT911`, `PCM5102A` e `USB MIDI Host` SMK25 V2 adiada por decisão do usuário; sem merge em `main`).

---

## 1. Identificação, Branch e Escopo Preservado

| Item | Evidência verificada |
|---|---|
| Repositório / Branch | `https://github.com/ovelhaaa/amy` · `feature/smk-p4-ui-redesign-018c2` |
| Commit-base verificado (`0.18-C.1a`) | `78a6c47254e867a0933d4aaa3a1d8cecb97be4a6` |
| Motor DSP / Audio / USB / NVS preservados | Zero alterações em `src/*`, `smk-p4/components/amy_runtime/*`, `smk-p4/components/audio_pcm5102/*`, `smk-p4/components/usb_midi_host/*`, `smk-p4/main/ui_blueprint_performance_adapter.cpp` |
| Ações físicas proibidas não executadas | Nenhum flash (`-t upload`), nenhum monitor serial, nenhum teste físico, nenhum merge em `main` |

---

## 2. Correções P0 da 0.18-C.1a (`C.2-A`)

### 2.1 Separação de Seleção e Recall de Cenas (`PerformanceScreen::buildScenesStage`)
- Tocar em um cartão de cena (`C1..C8`) em `TOCAR > CENAS 1-8` agora **apenas seleciona** o slot (`selected_scene_slot_`), atualizando o destaque visual sem disparar `requestScene()`.
- `RECALL` executa a recuperação explícita do slot selecionado; `SALVAR` e `LIMPAR` abrem `ConfirmDialog` de sobrescrita/salvamento ou exclusão.
- A faixa rápida de cenas (`quick_[s]`) na vista `MACROS 1-4` / `5-8` preserva o **recall em um toque**.

### 2.2 Indicadores Harmônicos `Q` e `CH` por Parâmetro (`StatusStrip`)
- `StatusStrip` passou a rastrear o parâmetro específico pendente (`harm_pending_param_`) e o parâmetro retornado por `lastCommandParameterId()`.
- `Q ON/OFF` reage exclusivamente a comandos de `harmony.scale_enable` e `harmony.preset`.
- `CH ON/OFF` reage exclusivamente a comandos de `harmony.chord_enable` e `harmony.preset`.
- Comandos relativos a `harmony.root_note`, `harmony.scale_type`, `harmony.quant_mode`, `harmony.chord_type`, `harmony.voicing` ou `harmony.apply_touch` não alteram `Q` nem `CH` com `WAIT`/`REJ`, e o overlay identifica qual parâmetro está `PENDING`, `CONFIRMADO` ou `REJEITADO` (`[ROOT]`, `[VOICING]`, `[Q]`, `[CH]`, etc.).

### 2.3 Separação entre Cena Ativa e Resultado da Última Operação
- `PerformanceScreen::refresh()` exibe `ATIVA: <nome>` quando em repouso/confirmado e `ATIVA: <nome> | RECALL C<n>: <FALHOU|PARCIAL|PENDENTE|WAIT PATCH|CANCELADO>` quando uma tentativa de recall em outro slot falha ou fica pendente, preservando a identidade da cena ativa e destacando o slot alvo no grid sem modificar a Scene Engine.

---

## 3. Arquitetura de Painéis (`800×480` = Shell `44 px` Topo + `388 px` Conteúdo + `48 px` Rodapé)

Todas as telas remodeladas adotam o padrão do piloto FX (`sidebar 156 px` à esquerda + painel contextual `628 px` à direita, ou 3 colunas em `Biblioteca`), montando exclusivamente os widgets do módulo ativo por vez:

- **TIMBRES > Biblioteca (`PatchBrowserScreen`):**
  - Sidebar esquerda (`156 px`): filtros de banco (`ALL BANKS`, `JUNO A` / `P4 ROM`, `JUNO B` / `S3/AMY`, `AMY DX7` / `USER DEMO`, `FAVORITES *`), stepper de categoria (`<` / `>`), `USER PRESETS / SAVE AS`, `SINTESE >`, `TOCAR >`.
  - Lista paginada central (`374 px`, 7 linhas/página): distingue visualmente patch ativo (`*`, borda verde) da pré-seleção (destaque âmbar). Primeiro toque apenas pré-seleciona.
  - Inspetor direito (`248 px`): exibe família, banco, categoria, macros `M1-4`, estado (`PATCH ATIVO`, `PRE-SELECAO`, `CARREGANDO PATCH...`, `EDICOES NAO SALVAS`), botões `CARREGAR` (com diálogo de Performance Lock / descarte) e `CARREGAR JA` (carregamento direto seguro, acionando diálogo se houver edições não salvas), `FAVORITE: YES/NO`, `LAYER A/B` (`Layer B PLANNED` sem DSP).
- **TIMBRES > Síntese (`SoundEditorScreen`):**
  - Consolidação das 7 abas legadas em 5 módulos contextuais adaptativos à família (`patch.family`):
    1. `GERAL`: `amp.level`, `amp.pan`, `amp.vel_sens`, `perf.master_volume` + `fm.depth`/`fm.feedback` (FM) ou `osc.1.level`/`filter.cutoff_hz` (Subtrativo).
    2. `FM / OPS` (quando FM) ou `OSCILADORES` (quando Subtrativo): FM mostra `fm.depth`, `fm.selected_op`, `fm.op.ratio`, `fm.op.level`, `fm.feedback`, `fm.op.attack`, `fm.op.decay`, `fm.op.vel_sens`, `FmAlgorithmSelector` e `FmTopologyDiagram`; Subtrativo mostra `osc.1.level`, `osc.sub_level`, `osc.noise_level`, `osc.1.detune_ct`, `osc.1.pulse_width` (e controles Demo quando em Demo).
    3. `FILTRO / ENV`: Subtrativo exibe `filter.cutoff_hz`, `filter.resonance`, `filter.env_amount`, `filter.keytrack`, `EnvelopeEditor` e ADSR; FM sinaliza explicitamente `FILTRO ANALOGICO N/A NO DX7` e amplia o `EnvelopeEditor` + ADSR.
    4. `LFO / MOD`: `lfo.1.wave`, `lfo.1.rate_hz`, `lfo.1.depth`, `lfo.1.dest` + `ModulationMatrixWidget` responsivo em `628 px`.
    5. `AVANCADO`: preserva todos os parâmetros secundários/planejados (`filter.type`, `filter.drive`, `amp.portamento_ms`, `env.amp.curve`, `lfo.1.phase_deg`, `lfo.1.sync`, `osc.1.wave`, `osc.1.pitch_st`, `osc.1.pan`, `osc.1.unison`, `osc.1.pulse_width`, `osc.1.detune_ct`).
  - Barra inferior fixa na área de edição: `COMPARE`, `REVERT` (com `ConfirmDialog` quando há edições não salvas), `USER / SAVE AS` (modal com 16 slots NVS, teclado e confirmação de sobrescrita) e `MOD >`.
- **TIMBRES > Modulação (`ModulationScreen`):**
  - Sidebar contextual: `MATRIZ 1-4`, `MATRIZ 5-8`, `ROUTING EDITOR` (navegação integral para `RoutingEditor`), `LEGACY OFFSETS` (separado dos controles modernos de Macro Routing), `SINTESE >`, `TOCAR >`.
  - Painel direito: matriz responsiva (`628 px`) + strip rápido de `LFO 1`.
- **MAIS > MIDI / SMK25 (`MidiScreen`):**
  - Sidebar contextual: `CONEXAO`, `CONTROLES / CC`, `SMK25 / PERFIL` (quando `hasMidiPerformance()`), `TOCAR >`.
  - Fluxo completo de MIDI Learn (`LEARN`, `LEARN + CC1`, `CONFIRM`, `REPLACE`, `CANCEL`, `SAVE PROFILE`), seleção de `DESTINATION` com takeover `PICKUP`, e monitoramento de 16 Pads + transporte (`CC114/115/117`).
- **MAIS > Preferências (`SettingsScreen`):**
  - Sidebar contextual: `SISTEMA` (`sys.brightness`, `sys.theme`, `sys.boot_mode`, Display/Touch HAL), `AUDIO` (`sys.sample_rate_hz`, `sys.master_tuning_hz`, PCM5102A), `PERFORMANCE` (Session Locks `LK FX/HARM/ARP/MAC`), `ARMAZENAMENTO` (política NVS/Demo, `RESET DEMO RAM DEFAULTS`, e `EXPORT/IMPORT BACKUP (PLANNED)` explicitamente desabilitados).
- **MAIS > Diagnósticos (`DiagnosticsScreen`):**
  - Sidebar contextual: `AUDIO`, `MEMORIA`, `USB / MIDI`, `SISTEMA`.
  - Atualização **in-place** (`setDiagLine`) sem reconstruir a tela, exibindo `N/A` honesto para métricas não medidas em host/bancada (ex.: `DMA UNDERRUNS`).

---

## 4. Arquivos Modificados e Criados

- `smk-p4/components/ui_blueprint/include/ui_widgets.h`
- `smk-p4/components/ui_blueprint/widgets/status_strip.cpp`
- `smk-p4/components/ui_blueprint/widgets/modulation_matrix.cpp`
- `smk-p4/components/ui_blueprint/screens/performance_screen.cpp`
- `smk-p4/components/ui_blueprint/screens/patch_browser_screen.cpp`
- `smk-p4/components/ui_blueprint/screens/sound_editor_screen.cpp`
- `smk-p4/components/ui_blueprint/screens/modulation_screen.cpp`
- `smk-p4/components/ui_blueprint/screens/midi_screen.cpp`
- `smk-p4/components/ui_blueprint/screens/settings_screen.cpp`
- `smk-p4/components/ui_blueprint/screens/diagnostics_screen.cpp`
- `smk-p4/tests/ui_018c1a_hardening_test.cpp` *(atualizado para seleção sem recall implícito)*
- `smk-p4/tests/sound_editor_ui_test.cpp` *(atualizado para módulos contextuais `GERAL`/`AVANCADO` e família FM)*
- `smk-p4/tests/ui_blueprint_adapter_test.cpp` *(atualizado para `GlobalSettings` tab `3` `ARMAZENAMENTO`)*
- `smk-p4/tests/ui_018c2_test.cpp` *(novo)*
- `smk-p4/tests/run_ui_018c2.ps1` *(novo)*
- `smk-p4/docs/ui_018c2_shots/*.png` *(20 capturas `800×480`)*

---

## 5. Testes Executados e Métricas LVGL (`128 KiB` TLSF)

| Suite / Comando | Status | Escopo e Resultados Observados |
|---|---|---|
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_ui_018c2.ps1` | **PASS** | P0 (`C.2-A`), Biblioteca (`C.2-B`), Editor FM/Subtrativo (`C.2-C`), Modulação/MIDI/Preferências/Diagnósticos (`C.2-D`), preservação USB/Sustain/Seq/Arp/PANIC em overlay, 20 PNGs `800×480`, estresse (`500` áreas, `500` módulos, `200` síntese, `100` MIDI Learn, `100` diálogos, `50` ticks diag.) |
| `powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_preview.ps1` | **PASS** | `blueprint_test_suite.exe` `[1/7]..[7/7]` + 23 PNGs; 700 montagens `baseline_free=71632 after_free=71632 diff=0 peak_used=105128 frag=0%` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_sound_editor_ui.ps1` | **PASS** | Módulos do editor, Effects, slider `0.18-B.1`, Save/Recall/Compare/Revert, SMK25 pickup, 32 ciclos popup |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_ui_018c1.ps1` | **PASS** | 12 capturas + preservação USB/Sustain/Seq/Arp/Touch/Locks + `STRESS_018C1 diff=0` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_ui_018c1a.ps1` | **PASS** | 8 cenários Scene Safety + Harmony Overlay + `STRESS_018C1A diff=0` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_fx_tests.ps1` | **PASS** | Suites FX DSP + NVS + Gate A/B/C/G |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_harmony_ui.ps1` | **PASS** | 14 telas + contrato diferido Harmony |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_performance_lock_0152a.ps1` | **PASS** | Locks `0x00..0x0F`, integridade musical, concorrência |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_blueprint_adapter.ps1` | **PASS** | Mock + LVGL 1400 telas (`frag=2%`, `second_window_growth=0`) + AMY real |
| Testes físicos Guition ESP32-P4 (`GT911`, `USB MIDI`, `PCM5102A`) | **MANUAL** | Adiados por decisão do usuário |

**Estresse `STRESS_018C2` (`128 KiB` TLSF em Host):**
- `objs = 91 -> 91`
- `free = 74.632 -> 74.632 B` (`diff = 0 B`)
- `largest = 73.624 -> 73.624 B`
- `peak_used = 89.712 B` (abaixo do limite de `128 KiB`)
- `frag = 2% -> 2%`

---

## 6. Capturas `800×480` (`smk-p4/docs/ui_018c2_shots/`)

1. `01_biblioteca_lista.png` — Lista paginada com patch ativo (`128`), bancos e categoria.
2. `02_biblioteca_selecionado.png` — Pré-seleção (`135`) sem disparo de carga.
3. `03_biblioteca_carregando.png` — Carregamento pendente (`CARREGANDO PATCH...`).
4. `04_sintese_geral.png` — Síntese `GERAL` em patch Subtrativo (`004 MOVING STRINGS`).
5. `05_sintese_subtrativo.png` — Síntese `OSCILADORES` (Subtrativo).
6. `06_sintese_fm.png` — Síntese `FM / OPS` (`135`, algoritmo + diagrama de topologia).
7. `07_sintese_filtro_env.png` — Síntese `FILTRO / ENV` (Subtrativo).
8. `08_sintese_lfo_mod.png` — Síntese `LFO / MOD` (LFO 1 + matriz).
9. `09_sintese_compare.png` — `COMPARE` ativo.
10. `10_sintese_save_as.png` — Modal `USER PRESETS / SAVE AS`.
11. `11_modulacao_visao_geral.png` — Modulação (Matriz `1-4` + LFO 1).
12. `12_modulacao_routing_editor.png` — `RoutingEditor` em largura integral.
13. `13_midi_conexao.png` — `MIDI > CONEXAO`.
14. `14_midi_controles_cc.png` — `MIDI > CONTROLES / CC`.
15. `15_midi_learn.png` — `MIDI > MIDI Learn`.
16. `16_preferencias.png` — `Preferências > SISTEMA`.
17. `17_diagnosticos.png` — `Diagnósticos > AUDIO`.
18. `18_cena_selecionada_sem_recall.png` — Cena `C2` selecionada mantendo `C1 INTRO` ativa.
19. `19_q_ch_independentes.png` — `Q WAIT` sem afetar `CH ON` após rejeição de `ROOT`.
20. `20_cena_ativa_falha_outro_slot.png` — `ATIVA: INTRO | RECALL C5: FALHOU`.

---

## 7. Builds ESP32-P4 (`ESP-IDF 5.5.2`)

| Perfil | Status | `check_blueprint_build.py` | RAM PIO (B / %) | Flash PIO (B / %) | Warnings/Erros |
|---|---|---|---|---|---|
| `guition-editor-8` | **PASS** | **PASS** | `70.264 / 327.680 (21.4%)` | `1.494.252 / 16.777.216 (8.9%)` | `0` |
| `guition-editor-8-bench` | **PASS** | **PASS** (`--musical`) | `70.820 / 327.680 (21.6%)` | `1.599.742 / 16.777.216 (9.5%)` | `0` |

---

## 8. Funcionalidades Ainda `PLANNED`, Limitações e Riscos

- **Ainda `PLANNED`:** `TIMBRES > Camadas` (`Layer B`), `TIMBRES > Mixer`, `RITMO > Bateria`, `TOCAR > Encadear [PLAN]`, `sys.brightness` / `sys.theme` (em live quando sem controle de backlight), `filter.type` / `filter.drive`, `Export/Import Backup` em `Preferências`, e `DMA_underruns` / `meters` ao vivo.
- **Riscos:** Qualificação física em placa Guition (`GT911`, `PCM5102A`, `USB MIDI Host` SMK25 V2) permanece não executada por decisão do usuário; versão **não está pronta para palco**.

---

## 9. Próxima Fase — Planejamento para `SMK-P4 0.18-C.3 — Rhythm Performance UX`

Escopo planejado (não implementado nesta etapa):
1. **Sequenciador (`PLAY` vs `EDIT`):** separação clara entre execução ao vivo (`PLAY/STOP SEQ`, `DIV`, `SWING`, `TR -/+`, `SEQ QUANT != Q`, `LIVE TR`) e edição de passos (`16 steps`, nota/velocidade/gate, `STEP REC`).
2. **Arpejador:** controles contextuais independentes (`ARP ON/OFF`, `PLAY/STOP ARP`, `CLR LATCH`, direção, taxa, oitavas, gate, swing, latch).
3. **Biblioteca de Padrões:** gestão dos 8 slots (`LOAD`, `SAVE`, `DELETE`) com `ConfirmDialog`.
4. **Transporte, Sincronização e Cenas:** integração rítmica com o relógio global único e transição de cenas em fronteira de compasso.
