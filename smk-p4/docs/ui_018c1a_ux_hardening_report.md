# SMK-P4 0.18-C.1a — Relatorio de UX Hardening, Scene Safety e Harmony Overlay

**Status da etapa:** Hardening em host (`LVGL 9.3.0`) e qualificacao de build `ESP32-P4` (`ESP-IDF 5.5.2`) **CONCLUIDOS** na branch `feature/smk-p4-ui-redesign-018c1`.  
**Veredito de governanca:** **APROVADO EM HOST** | **CONDITIONAL GO** para ensaio em bancada | **NO-GO** para prontidao de palco ou merge em `main` (qualificacao fisica explicitamente adiada pelo usuario).

---

## 1. Identificacao, Branch e Escopo Preservado

| Item | Evidencia verificada |
|---|---|
| Repositorio / Branch | `https://github.com/ovelhaaa/amy` · `feature/smk-p4-ui-redesign-018c1` |
| Commit-base verificado (`0.18-C.1`) | `a897c5856189fcdac5772fb836a76594b69192fb` |
| Motor DSP / Audio / USB / NVS preservados | Zero alteracoes em `src/*`, `smk-p4/components/amy_runtime/*`, `smk-p4/components/audio_pcm5102/*`, `smk-p4/components/usb_midi_host/*`, formatos NVS de cenas/padroes |
| Alteracao pontual fora de `ui_blueprint` | `smk-p4/main/ui_blueprint_performance_adapter.cpp`: em `result()`, preserva `"panic"` em `last_id_` (sem reter ponteiro externo) para deteccao de borda de Panic em modais abertos |
| Acoes fisicas proibidas nao executadas | Nenhum flash (`-t upload`), nenhum monitor serial, nenhum teste fisico, nenhum merge em `main` |

---

## 2. Problemas Identificados e Correcoes Implementadas

### 2.1 P0 — Protecao de Acoes Destrutivas de Cenas (`ConfirmDialog`)
- **Problema:** Em [performance_screen.cpp](../components/ui_blueprint/screens/performance_screen.cpp), os botoes `SALVAR` e `LIMPAR` de `CENAS 1-8` disparavam imediatamente `SceneAction::Save` e `SceneAction::Delete` via `requestScene()`, permitindo sobrescrita ou exclusao acidental em apresentacao, alem de deixar `op.domains = scene::kAll` ao salvar pela tela de Performance.
- **Correcao:**
  - Criado componente modal reutilizavel `ConfirmDialog` em [ui_widgets.h](../components/ui_blueprint/include/ui_widgets.h) e [confirm_dialog.cpp](../components/ui_blueprint/widgets/confirm_dialog.cpp), cobrindo apenas `content_` (`800×388`) para manter `StatusStrip` e `PANIC` 100% acessiveis acima do modal.
  - `ConfirmDialog::close(true)` oculta o backdrop, desabilita cliques e usa `lv_obj_delete_async()` para nunca destruir o botao/modal dentro do proprio callback `LV_EVENT_CLICKED`. `PerformanceScreen::onDestroy()` chama `confirm_dialog_.close(false)` antes da desmontagem.
  - **Salvar (`SALVAR`):** slot vazio exibe `"CONFIRMAR SALVAMENTO"` (`"SALVAR CENA"`); slot valido/corrompido/incompativel exibe `"CONFIRMAR SOBRESCRITA"` identificando numero e nome da cena (`"SOBRESCREVER"`). Ao confirmar, envia `op.domains = backend_->sceneCaptureDomains()` e reporta o estado real do backend (ex.: `"RELEASE NOTES TO WRITE SCENE"`, `"SCENE RECALL BUSY"`), sem simular sucesso.
  - **Limpar (`LIMPAR`):** nunca apaga no primeiro toque; exige `"CONFIRMAR EXCLUSAO"` identificando o slot, com `"CANCELAR"` (sem enviar `SceneAction::Cancel` nem alterar armazenamento) e `"APAGAR CENA"`.
  - **PANIC com modal aberto:** `PerformanceScreen::refresh()` detecta borda de subida de `PANIC` e fecha o modal sem executar a acao pendente.

### 2.2 P0 — Ciclo de Vida do Harmony Overlay (`StatusStrip`)
- **Problema:** Em [status_strip.cpp](../components/ui_blueprint/widgets/status_strip.cpp), `TOUCH HARM`, `PRESET` e as 5 linhas (`Fundamental`, `Escala`, `Modo Quant.`, `Tipo Acorde`, `Voicing`) chamavam `openHarmony()` recursivamente dentro do callback LVGL, destruindo os widgets ativos durante o evento, sem atualizar os controles em `refresh()` nem filtrar valores nao suportados.
- **Correcao:**
  - Eliminada toda reconstrucao de `openHarmony()` dentro de callbacks; armazenados os ponteiros (`harm_touch_btn_`, `harm_preset_btn_`, `harm_row_lbls_[5]`, `harm_row_prev_[5]`, `harm_row_next_[5]`) e buffers estaticos (`harm_preset_buf_`, `harm_row_bufs_[5]`) para atualizacao **in-place** em `StatusStrip::refresh()` com zero realocacoes TLSF.
  - Fechamento de overlays por botoes internos (`FECHAR` e selecao de modulo) usa `closeOverlays(true)` (`lv_obj_delete_async`).
  - Respeitados `isParameterEditable()` (desabilita controles com `LV_STATE_DISABLED`) e `isParameterValueSupported()` (pula opcoes nao suportadas como `ADD9/9TH` e `DROP2/SPREAD/POWER` ao navegar `<` / `>`, sinalizando `[N/A]` se um estado externo nao suportado estiver ativo).
  - Rastreamento de token pendente (`harm_awaiting_token_`, `harm_pending_token_`) impede que uma resposta antiga de token anterior confirme um comando mais recente.

### 2.3 P1 — Separacao Semantica de Estados de FX e Audio
- **Problema:** A pill `FX LIVE` em `PerformanceScreen` inferia audio ativo a partir de `fx.bypass_all == false`.
- **Correcao:** `fx_pill_` passa a exibir exclusivamente o estado da cadeia FX (`FX ON`, `FX BYP` ou `FX N/A` quando desconectado/indisponivel), enquanto o estado real do audio permanece independente no cabecalho (`AUD: RUN` / `AUD: OFF` / `AUD: SIM`).

### 2.4 P1 — Polimento Visual e Ergonomia (`800×480`)
- **Teclado Touch:** substituido `"SAIR LIBERA TOUCH"` (recortado na borda direita) por `"SAIR: SOLTA NOTAS"` em `x=600, w=192`, ajustando `OCT -` / `OCT +` para `w=92` (sem recorte).
- **Cabecalho vs Performance:** cabecalho exibe identificacao compacta (`#%03u %.14s`) e o banner de Performance exibe nome completo e familia com `LV_LABEL_LONG_MODE_DOTS` (`w=330`) para nao invadir `TIMBRES >`.
- **Cenas:** contraste e rotulos explicitos para `EMPTY`, `SAVED`, `ACTIVE`, `PENDING`, `FAILED`, `PARTIAL`, `CORRUPT` e `INCOMP` em badges e cards.

---

## 3. Arquivos Modificados e Criados

- `smk-p4/components/ui_blueprint/include/ui_widgets.h`
- `smk-p4/components/ui_blueprint/widgets/confirm_dialog.cpp` *(novo)*
- `smk-p4/components/ui_blueprint/CMakeLists.txt`
- `smk-p4/components/ui_blueprint/screens/performance_screen.cpp`
- `smk-p4/components/ui_blueprint/widgets/status_strip.cpp`
- `smk-p4/main/ui_blueprint_performance_adapter.cpp`
- `smk-p4/tests/ui_018c1a_hardening_test.cpp` *(novo)*
- `smk-p4/tests/run_ui_018c1a.ps1` *(novo)*
- `smk-p4/docs/ui_018c1a_shots/*.png` *(10 capturas `800×480`)*

---

## 4. Evidencias dos Testes em Host

Legenda estrita: `PASS`, `FAIL`, `NOT RUN`, `BLOCKED`, `MANUAL`.

| Suite / Comando | Status | Escopo e Resultados Observados |
|---|---|---|
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_ui_018c1a.ps1` | **PASS** | 8 cenarios Scene Safety, ciclo de vida Harmony Overlay (in-place, pending, confirmed, rejected, 2 requests, token fora de ordem, fechamento c/ pendente, navegacao, PANIC, `isParameterEditable`/`isParameterValueSupported`), FX ON/BYP/N/A, `"SAIR: SOLTA NOTAS"` sem recorte, 10 capturas `800×480`, estresse LVGL (`500` areas, `200` overlays, `100` ciclos modal cena) |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_ui_018c1.ps1` | **PASS** | 12 capturas `800×480`, preservacao USB/Sustain/Seq/Arp/Touch/Locks, PANIC em overlay, `objs=91->91`, `free=74656->74656`, `diff=0`, `frag=2%` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_preview.ps1` | **PASS** | `blueprint_test_suite.exe` `[1/7]..[7/7]` + 23 PNGs `800×480`; 700 montagens `baseline_free=71632 after_free=71632 diff=0 peak_used=105328 frag=0%` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_harmony_ui.ps1` | **PASS** | `14` telas, contrato diferido, `real key`, `Off`, `stepper`, `channel`, `timeout` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_step_sequencer_ui_016b.ps1` | **PASS** | `50` trocas de divisao, `017A` Pattern Browser, `017B` Scenes + Modal Seletivo, Panic |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_sound_editor_ui.ps1` | **PASS** | 7 abas editor, Effects, slider `0.18-B.1`, Save/Recall/Compare/Revert, SMK25 pickup, 32 ciclos popup, `diff=0 B` |
| `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_performance_lock_0152a.ps1` | **PASS** | Locks de sessao (`Fx`, `Harmony`, `Arp`, `Macros`) e integridade musical |
| Testes fisicos Guition ESP32-P4 (`A1–A5`, Touch `GT911`, `USB MIDI`, `PCM5102A`) | **MANUAL** | Adiados explicitamente por decisao do usuario; nao executados |

---

## 5. Metricas LVGL (Pool TLSF `128 KiB` em Host)

| Cenario (`ui_018c1a_hardening_test.cpp`) | Objetos LVGL | `free` (B) | `largest` (B) | `diff` (B) | `peak_used` (B) | `frag` |
|---|---|---|---|---|---|---|
| Baseline apos warmup + 500 areas + 200 overlays + 100 modais cena | `91 -> 91` | `75.752 -> 75.752` | `74.688 -> 74.688` | **`0`** | `66.808` | `2% -> 2%` |

---

## 6. Capturas `800×480` Verificadas (`smk-p4/docs/ui_018c1a_shots/`)

Todas as 10 capturas foram geradas com `LocalAmyPerformanceBackend` + `UiBlueprintPerformanceAdapter` (`amy::Runtime` real em host):

| # | Arquivo | Cenario |
|---|---|---|
| 1 | `01_tocar_macros_1_4.png` | Tocar — Macros 1–4 apos polimento (`FX ON` + `AUD: OFF` separados, cabecalho compacto) |
| 2 | `02_tocar_cenas_variados.png` | Tocar — Cenas com estados variados (`ACTIVE`, `PENDING`, `SAVED`, `EMPTY`, locks) |
| 3 | `03_confirmacao_sobrescrita.png` | Modal de confirmacao de sobrescrita (`CENA 1: INTRO`, `PANIC` livre no topo) |
| 4 | `04_confirmacao_exclusao.png` | Modal de confirmacao de exclusao (`CENA 2: CHORUS`, `CANCELAR` / `APAGAR CENA`) |
| 5 | `05_cena_recall_failed.png` | Cena com recall rejeitado (`FAILED` real via `RecallError::FxConflict` com `LK FX`) |
| 6 | `06_cena_recall_partial.png` | Cena com aplicacao parcial (`PARTIAL` real via `WaitingPatch` + `Panic` no `amy::Runtime`) |
| 7 | `07_harmony_overlay_pending.png` | Harmony Overlay com comando pendente (`Estado: PENDING`, controles in-place) |
| 8 | `08_harmony_overlay_confirmed.png` | Harmony Overlay apos confirmacao (`Estado: CONFIRMADO`) |
| 9 | `09_harmony_overlay_rejected.png` | Harmony Overlay com rejeicao (`Estado: REJEITADO`, snapshot preservado) |
| 10 | `10_teclado_touch_sem_recorte.png` | Teclado Touch com `"SAIR: SOLTA NOTAS"` sem recorte |

---

## 7. Builds ESP32-P4 (`ESP-IDF 5.5.2`)

Compilados via `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e <perfil> -j 8` e auditados com `check_blueprint_build.py`:

| Perfil | Status | `check_blueprint_build.py` | RAM PIO (B / %) | Flash PIO (B / %) | `firmware.bin` (B) |
|---|---|---|---|---|---|
| `guition-editor-8` | **PASS** | **PASS** | `70.256 / 327.680 (21.4%)` | `1.483.886 / 16.777.216 (8.8%)` | `1.484.608` |
| `guition-editor-8-bench` | **PASS** | **PASS** (`--musical`) | `70.796 / 327.680 (21.6%)` | `1.589.400 / 16.777.216 (9.5%)` | `1.590.112` |

Nenhum erro ou warning nos arquivos alterados.

---

## 8. Limitacoes, Riscos Residuais e Planejamento da Proxima Fase (`0.18-C.2`)

### 8.1 Limitacoes e Riscos Residuais
- A qualificacao fisica na Guition ESP32-P4 (`A1–A5`, Touch `GT911`, `USB MIDI Host` SMK25 V2, audio estereo `PCM5102A` e `DMA_underruns`) permanece pendente por decisao explicita do usuario.
- Os modulos de `TIMBRES`, `RITMO` e `MAIS` continuam utilizando os controladores legados de `800 px` ate a etapa `0.18-C.2`.

### 8.2 Planejamento Tecnico — SMK-P4 0.18-C.2 (Somente Planejamento)
Escopo planejado para a remodelacao de **Timbres** e **Mais** sem comprometer o orcamento LVGL (`128 KiB` TLSF) nem os contratos do `Audio Owner`:
1. **Timbres > Biblioteca (`PatchBrowser`):** lista paginada em `800 px` com filtro por familia (`FM` / `JUNO` / `USER`), preview rapido, `ConfirmDialog` reutilizavel para descarte de edicoes nao salvas (`patchNeedsDiscardConfirmation`) e preservacao de `PerformanceLock`.
2. **Timbres > Sintese (`SoundEditor`):** consolidacao das subguias por familia (`GERAL`, `FM ALGO/OP` ou `OSC/SUB`, `FILTRO/ENV`, `LFO/MOD`) reutilizando `StageStacked` (`parameter_slider.cpp` com coalescencia `35 ms` e `lv_obj_remove_style_all`), desmontando paineis inativos para manter pico TLSF abaixo de `110 KiB`.
3. **Timbres > Modulacao (`Modulation` + `RoutingEditor`):** matriz de rotas Macro `1–8` e offsets legados em layout de largura integral com ajuste fino `-`/`+` e bloqueio explicito em `CompareActive`.
4. **Mais > MIDI / SMK25, Preferencias e Diagnosticos:** reorganizacao em paineis contextuais `800 px` preservando `MidiLearn`, pickup de knobs, `PerformanceLock`, seletor de variante de cabecalho e telemetria live (`free`, `largest`, `frag%`, `active_voices`, `DMA_underruns`).
