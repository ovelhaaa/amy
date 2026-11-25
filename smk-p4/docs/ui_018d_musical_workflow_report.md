# SMK-P4 0.18-D — Musical Workflow Refinement Report

Execução: 25/11/2026.  
Branch: `feature/smk-p4-ui-redesign-018d` (a partir de `feature/smk-p4-ui-redesign-018c3a` @ `1ddd8e73b46842c260f93bda322f7d44bcb17181`).  
Qualificação física adiada por decisão do usuário; toda validação executada em **HOST SIM** + builds ESP32-P4.

---

## 1. Matriz de fluxos antes / depois (Auditoria 0.18-D.A)

| Fluxo | Antes (0.18-C.3a) | Depois (0.18-D) | Redução / Ganho |
|---|---|---|---|
| **A. Trocar patch e voltar** | 4 toques, 2 trocas de área (`TIMBRES >` → linha → `CARREGAR` → `VOLTAR P/ TOCAR >`). De outras áreas exige abrir `TIMBRES` / `MODS`. | 4 toques na Performance (agora também tocando direto no nome do patch no banner ou no `StatusStrip` de qualquer tela) + visibilidade de `LOCKS`. | Acesso universal em 1 toque de qualquer tela via `StatusStrip` + transparência de Locks. |
| **B. Ajustar FX e voltar** | 4+ toques (`FX` rodapé → módulo → ajustar → `< TOCAR` / `TOCAR`). | `fx_pill_` (`FX ON`/`FX BYP`) no banner da Performance abre `Effects` direto no último módulo editado; `< TOCAR` ou `TOCAR` retorna em 1 toque. | 1 toque direto no módulo contextual; preservação de módulo e transporte. |
| **C. Agendar Pattern (NEXT BAR)** | Visível apenas dentro de `Patterns` / `Sequencer`. | Banner da Performance exibe `PAT 01->02` e pill `SEQ P>2`; toque em `seq_pill_` abre `Sequencer`; `NEXT BAR` preservado. | Visibilidade contínua do padrão ativo e pendente fora da biblioteca. |
| **D. Recuperar Cena com Locks** | `CENAS 1-8` exibia apenas `SEL: C1 \| ATIVA: -- \| IDLE`, sem detalhar quais domínios seriam aplicados. | `CENAS 1-8` exibe resumo real `APLICAR: PATCH* \| PATTERN \| FX(LK) \| HARMONY(LK) \| TEMPO` via `scene::effectiveDomains()`. | Previsibilidade completa dos domínios efetivos antes do `RECALL`. |
| **E. Alterar tonalidade / Harmonia** | `Harmony Overlay` não mostrava `SEQ Q` nem `LIVE TR` confirmados. | `Harmony Overlay` exibe escopo explícito (`Q/CH: USB Ch+Arp+Touch` vs `SEQ Q: ON/OFF` vs `LIVE TR: ON/OFF (+N st)`). | Elimina ambiguidade de escopo entre Harmonia global, Sequencer Quantize e Live Transpose. |
| **F. Retornar à Performance** | Se o usuário estivesse em `Scenes` (área `Tocar`) e fosse para `FX`, tocar `TOCAR` no rodapé voltava para `Scenes`, não para `Performance`. | `TOCAR` no rodapé fecha overlays não destrutivos e retorna sempre para `ScreenId::Performance` em **1 toque** de qualquer tela. | Retorno 100% determinístico em 1 toque de qualquer área. |

---

## 2. Alterações realizadas por subetapa

- **0.18-D.B — Navegação e Quick Actions:**
  - `smk-p4/components/ui_blueprint/src/ui_blueprint.cpp`: `onAreaClick(UiArea::Tocar)` fecha overlays e chama `requestReturnToPerformance()` sempre que a tela atual não for `Performance`.
  - `smk-p4/components/ui_blueprint/widgets/status_strip.cpp`: `info_box` (`#001 PATCH / MIDI / AUD`) clicável para abrir `ScreenId::PatchBrowser` de qualquer tela.
  - `smk-p4/components/ui_blueprint/screens/performance_screen.cpp`: `patch_lbl_` (→ `PatchBrowser`), `scene_lbl_` (→ `CENAS 1-8`), `arp_pill_` (→ `Arpeggiator`), `seq_pill_` (→ `Sequencer`), `fx_pill_` (→ `Effects`), `harm_pill_` (→ `Harmony`) como Quick Actions contextuais.
- **0.18-D.C — Performance Stage V4:**
  - `performance_screen.cpp`: banner contextual exibe `ATIVA: <cena> | PAT 01->02 | LK` (preservando o prefixo `ATIVA: ` para compatibilidade), pills `ARP RUN/LAT/ARM/ON/OFF` e `SEQ P>X / Sx / RUN / OFF`.
  - Aba `CENAS 1-8` adiciona `scenes_domains_lbl_` (`APLICAR: ...`) calculado com `scene::effectiveDomains()` e `sessionLockMask()`.
- **0.18-D.D — Estados musicais e confirmações:**
  - `patch_browser_screen.cpp`: exibe `LOCKS: ...` (ou `LOCKS: NENHUM`) dinamicamente no inspetor.
  - `status_strip.cpp` / `ui_widgets.h`: `harm_status_lbl_` (`harm_status_buf_[160]`) exibe `SEQ Q` e `LIVE TR` confirmados no snapshot ao lado do escopo `Q/CH`.
- **0.18-D.E — Validação:**
  - `smk-p4/tests/ui_018d_test.cpp` e `smk-p4/tests/run_ui_018d.ps1`: 7 cenários musicais integrados, 18 capturas 800×480 e estresse completo.

---

## 3. Política de Quick Actions, Preservação de Contexto e Estados

- **Quick Actions:** reutilizam exclusivamente `nav.requestNavigateTo(...)` e `nav.requestReturnToPerformance()`, sem duplicar lógica de backend ou criar ações destrutivas de 1 toque.
- **Preservação de contexto:** sessão mantém último módulo de Timbres, último módulo FX (`subTab(Effects)`), último módulo de Ritmo, `sequencerEdit()`, `sequencerPage()`, `patternSwitchMode()`, `patternSelection()`, e banco de macros.
- **Consistência de estados:** `ACTIVE`, `PENDING`, `WAITING`, `CONFIRMED`, `FAILED`/`REJECTED`, `PARTIAL`, `MODIFIED`, `LATCHED`, `ARMED`, `RUNNING`, `STOPPED` e `UNAVAILABLE` usam rótulos explícitos e cores consistentes, nunca dependendo apenas de cor.

---

## 4. Capturas LVGL (800×480 HOST SIM)

Índice completo em [smk-p4/docs/ui_018d_shots/README.md](ui_018d_shots/README.md) com as 18 capturas obrigatórias (`01_perf_stage_refined.png` a `18_simultaneous_states.png`).

---

## 5. Resultados das regressões e Métricas LVGL

| Suíte | Status | Observações |
|---|---|---|
| `run_ui_018d.ps1 -Regressions` (`ui_018d_test`) | `PASS` | 7 cenários integrados, 18 capturas, 500 ciclos de estresse (1000 navs de área, 500 overlays, 500 retornos, 200 FX, 200 PLAY/EDIT, 200 Patterns, 100 Scenes, 100 Harmony). |
| `ui_018c3a_test`, `ui_018c3_test`, `ui_018c2_test`, `ui_018c1_integration_test`, `ui_018c1a_hardening_test`, `sound_editor_ui_test`, `smk25_0142_ui_test` | `PASS` | Todas aprovadas dentro de `run_ui_018d.ps1 -Regressions`. |
| `guition-editor-8` + `check_blueprint_build.py` | `PASS` | RAM estática: `70.316 B` (21,5%); Flash: `1.522.176 B` (9,1%); imagem total: `1.522.892 B`. |
| `guition-editor-8-bench` + `check_blueprint_build.py` | `PASS` | RAM estática: `70.852 B` (21,6%); Flash: `1.627.664 B` (9,7%); imagem total: `1.628.380 B`. |
| Qualificação física (áudio, USB, touch, flash) | `MANUAL` | Adiada por decisão do usuário; não executada. |

### Métricas LVGL (0.18-D Stress vs 0.18-C.3a)
- **0.18-D Stress:** objetos `92 -> 92` (delta 0), livre `73.608 -> 73.640 B`, maior bloco `72.544 -> 72.544 B`, pico `66.448 B` (orçamento `131.072 B` / 128 KiB), fragmentação `2 -> 2%`, mediana `12.162,1 µs`, P95 `22.489,3 µs`, máximo `60.269,3 µs`.
- **0.18-C.3a (na mesma execução):** objetos `92 -> 92`, pico `66.408 B`, mediana `10.324,2 µs`.

---

## 6. Riscos residuais, pendências e planejamento futuro (Seção 20)

- **Pendências / Riscos residuais:** validação física em hardware Guition ESP32-P4 (deadlines DMA/I²S, USB Host físico, touch capacitivo, NVS real) permanece pendente.
- **Planejamento futuro (não implementado nesta etapa):**
  1. **Harmonic Pads:** matriz de acordes em Performance reutilizando o produtor MIDI/Harmony existente com ownership explícito no release de toque.
  2. **Scene Chain:** encadeamento no Audio Owner sincronizado ao clock global (4/4), com contador de compassos, desarme manual e preservação de Panic/Stop.
  3. **SMK25 Performance Shortcuts:** atalhos dedicados para `PLAY/STOP SEQ`, `NEXT/PREV SCENE` e `CANCEL QUEUE` sobre o perfil confirmado (`4353:4B4D`) e sua fila única de entrada.
