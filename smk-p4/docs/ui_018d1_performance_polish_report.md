# SMK-P4 0.18-D.1 — Performance UX Final Polish, Scene Recall Clarity & Navigation Consistency

Execução: 25/11/2026.  
Qualificação física adiada por decisão do usuário; toda validação executada em **HOST SIM** + builds ESP32-P4.

---

## 1. Branch e SHA

- **Branch de trabalho:** `feature/smk-p4-ui-redesign-018d1`
- **Base:** `origin/feature/smk-p4-ui-redesign-018d` @ `ab7de49b1dd292db1f4da4b75896d942962f7085`

---

## 2. Problemas confirmados na auditoria

1. **Banner de contexto musical (`performance_screen.cpp`):** `scene_lbl_` concatenava cena ativa, Pattern ativo/pendente, indicador `LK` e resultado de Scene Recall em uma única linha de 330 px com `LV_LABEL_LONG_MODE_DOTS`, truncando palavras críticas (`FAILED`, `PARTIAL`, `WAIT PATCH`). Além disso, `s_last_recall_slot_` não acompanhava o token do snapshot, podendo atribuir uma falha externa ao slot ativo anterior (inclusive pintando o botão da cena ativa de vermelho).
2. **Semântica do resumo de Scene Recall (`performance_screen.cpp`):** `scenes_domains_lbl_` usava o prefixo genérico `APLICAR:` e chamava `scene::effectiveDomains(..., true)` com `apply_tempo = true`, embora tanto o botão `RECALL` em `CENAS 1-8` quanto o acesso rápido `C1..C8` enviem `SceneOperation` com `apply_tempo = false`. Também não sinalizava `TEMPO(OFF)` nem incompatibilidades de `sceneCompatibility(op)` (`[PAT N/A]`, `[PATCH N/A]`, `[NO DOM]`).
3. **Comportamento de `TOCAR` como HOME musical (`ui_blueprint.cpp`, `ui_navigation.cpp`):** quando `ScreenId::Performance` já estava aberta em um submodo (`CENAS 1-8`, `MACROS 5-8`, `TECLADO`, `ENCADEAR [PLAN]`), tocar em `TOCAR` no rodapé não retornava a `MACROS 1-4`. Além disso, `returnToPerformance()` não redefinia `sub_tabs_[Performance] = 0` e não verificava diálogos de confirmação abertos.
4. **Ergonomia das Quick Actions (`performance_screen.cpp`):** os atalhos `TIMBRES >`, `ARP`, `SEQ`, `FX` e `HARM` tinham `34–36 px` de altura, abaixo da faixa recomendada de `44–48 px`.
5. **Identificação harmônica (`performance_screen.cpp`):** o indicador `-/-` era pouco explícito quando Quantize e Chord Assist estavam desligados, e abria a tela completa `ScreenId::Harmony` (em `MAIS`) em vez do Harmony Overlay compacto.
6. **Descrições antigas da navegação (`status_strip.cpp`):** o seletor de módulos de `TIMBRES` ainda descrevia `SINTESE` como `"Editor FM / Subtrativo (7 abas)"`, embora o `SoundEditor` atual utilize 5 painéis contextuais.

---

## 3. Correções realizadas

- `smk-p4/components/ui_blueprint/screens/performance_screen.cpp`:
  - Separado o banner em **Contexto persistente** (`scene_lbl_`: `SC: <cena> | PAT <...> | LK`) e **Resultado de operação** (`recall_op_lbl_`: `RECALL C<n>: FAILED / PENDING / WAIT PATCH / PARTIAL / CANCELLED` ou `RECALL: FAILED / REJECTED`), sem truncamento de palavras críticas e sem confundir cena ativa com alvo de falha.
  - Corrigido o rastreamento de `s_last_recall_slot_` com `s_last_recall_token_` e `s_last_recall_from_ui_`, além de corrigir `is_op` nos cards/botões para nunca marcar a cena ativa como falha quando o alvo externo é desconhecido.
  - Ampliado o banner para `h = 56 px` (`y = 2..58`) mantendo macros em `80 px` (`y = 102..268`) e acesso rápido em `102 px` (`y = 278..380`). Todos os atalhos (`TIMBRES >`, `ARP`, `SEQ`, `FX`, `HARM`) passaram para **`h = 46 px`** (`w = 82..88 px`, gap `4 px`, zero sobreposição).
  - Substituído `-/-` por `HARM OFF` / `Q ON` / `CH ON` / `Q+CH` / `HARM N/A`; toque em `harm_pill_` abre o **Harmony Overlay compacto** em 1 toque via `UiNavigationManager::requestOpenHarmonyOverlay()` (com fallback para `ScreenId::Harmony`).
  - Atualizado o resumo em `CENAS 1-8` para `RECALL TOTAL: ...`, alinhado exatamente ao `SceneOperation` enviado pelo botão `RECALL` (`apply_tempo = false`), exibindo `TEMPO(OFF)`, `(LK)` e limitações de `backend.sceneCompatibility(op)`.
- `smk-p4/components/ui_blueprint/include/ui_navigation.h`, `src/ui_navigation.cpp`, `src/ui_blueprint.cpp`, `screens/sequencer_screen.cpp`, `screens/settings_screen.cpp`, `screens/sound_editor_screen.cpp`, `widgets/status_strip.cpp`:
  - Adicionado `ScreenView::hasBlockingConfirmation()` para proteger `ConfirmDialog` abertos contra fechamento silencioso via `TOCAR`.
  - `UiNavigationManager::returnToPerformance()` e `UiBlueprint::onAreaClick(UiArea::Tocar)` agora retornam sempre a `Performance` em `MACROS 1-4` (`subTab == 0`), sem reconstrução quando já está em `MACROS 1-4`, e liberando apenas notas Touch via `LV_EVENT_DELETE` ao sair de `TECLADO` (preservando USB MIDI, CC64, Arp, Sequencer e Locks).
  - Atualizadas as descrições do seletor de módulos em `StatusStrip::openAreaModules()` (removida referência a `"7 abas"`, usando `"Editor contextual FM 6-OP / Subtrativo"`).

---

## 4. Contrato definitivo do botão TOCAR

1. **De qualquer outra área (`TIMBRES`, `RITMO`, `FX`, `MAIS`):** fecha overlays não destrutivos e retorna a `ScreenId::Performance` em **`MACROS 1-4`** (`subTab == 0`).
2. **De `Performance` em `CENAS 1-8`, `MACROS 5-8` ou `ENCADEAR [PLAN]`:** retorna a `MACROS 1-4`.
3. **De `Performance` em `TECLADO`:** retorna a `MACROS 1-4`, liberando exclusivamente as notas Touch do teclado desmontado via `LV_EVENT_DELETE`.
4. **De `Performance` já em `MACROS 1-4`:** permanece na mesma tela sem reconstrução (`transitionCount()` inalterado).
5. **Com diálogo destrutivo/confirmação (`ConfirmDialog`) aberto:** `TOCAR` não fecha silenciosamente o modal; exige `CANCELAR`, `CONFIRMAR` ou `PANIC`.
6. **Preservação musical:** nunca envia `Panic`, `Stop Arp`, `Stop Sequencer`, nem reinicia o clock global ou interrompe notas USB MIDI / Sustain CC64 / Performance Locks.

---

## 5. Política de exibição de Scene Recall

- **Banner da Performance:** `scene_lbl_` exibe exclusivamente o contexto ativo confirmado (`SC: <nome> | PAT ... | LK`). `recall_op_lbl_` exibe separadamente apenas o estado de operação em curso ou terminal não-confirmado (`PENDING`, `WAIT PATCH`, `FAILED`, `PARTIAL`, `CANCELLED`, `REJECTED`).
- **Aba `CENAS 1-8` (`RECALL TOTAL:`):** reflete exatamente o `SceneOperation` enviado pelo botão `RECALL` (`action = Load`, `mode = Immediate`, `locks = sessionLockMask()`, `apply_tempo = false`).
- **Modal `SELETIVO...` (`ScenesScreen`):** mantém sua própria seleção de domínios, `mode_` (`IMMEDIATE`/`NEXT BAR`), `tempo_` (`SCENE TEMPO OFF/ON`) e `discard_`, exibindo `EFFECTIVE ...` e `sceneCompatibility()` de forma independente.

---

## 6. Resumo dos domínios efetivos

- `RECALL TOTAL: PATCH* | PATTERN | FX | HARMONY | ARP | TEMPO(OFF)` — quando todos os domínios estão destravados e `apply_tempo == false`.
- Domínios protegidos por Performance Lock aparecem como `FX(LK)`, `HARMONY(LK)`, `ARP(LK)`, `MACROS(LK)`.
- Quando `eff == 0`, exibe `RECALL TOTAL: NENHUM (LOCKED)` ou `RECALL TOTAL: NENHUM (TEMPO OFF)`.
- Limitações detectadas por `backend.sceneCompatibility(op)` são anexadas explicitamente: `[PAT N/A]`, `[PATCH N/A]`, `[MAC INCOMP]`, `[COMPARE]`, `[NO DOM]`.

---

## 7. Revisão de ergonomia dos atalhos

Medição geométrica automatizada em LVGL (`800×480`):

| Controle | Posição (`x, y`) | Dimensão (`w × h`) | Gap esquerdo | Sobreposição |
|---|---|---|---|---|
| `TIMBRES >` | `(334, 3)` | `88 × 46 px` | — | `0` |
| `ARP` (`arp_pill_`) | `(426, 3)` | `84 × 46 px` | `4 px` | `0` |
| `SEQ` (`seq_pill_`) | `(514, 3)` | `86 × 46 px` | `4 px` | `0` |
| `FX` (`fx_pill_`) | `(604, 3)` | `82 × 46 px` | `4 px` | `0` |
| `HARM` (`harm_pill_`) | `(690, 3)` | `88 × 46 px` | `4 px` | `0` |

Macros `1-4` / `5-8` permanecem em `390 × 80 px` e `PANIC` permanece permanentemente acessível no topo-direito (`74 × 36 px`).

---

## 8. Capturas (HOST SIM 800×480)

Índice completo em [smk-p4/docs/ui_018d1_shots/README.md](ui_018d1_shots/README.md) com as 15 capturas obrigatórias (`01_perf_stage_refined.png` a `15_long_patch_simultaneous_states.png`).

---

## 9. Resultados dos testes

| Suíte | Status | Observações |
|---|---|---|
| `run_ui_018d1.ps1 -Regressions` (`ui_018d1_test`) | `PASS` | Home musical, 9 casos de Scene Recall, hitboxes 46 px, Harmony OFF/Q/CH/Q+CH + Overlay, descrições, 15 PNGs e estresse (1000 retornos `TOCAR`, 500 overlays, 500 transições, 200 Scenes, 200 Patterns, 100 Harmony, 100 Touch KB). |
| `ui_018c3a_test`, `ui_018c3_test`, `ui_018c2_test`, `ui_018c1_integration_test`, `ui_018c1a_hardening_test`, `sound_editor_ui_test`, `smk25_0142_ui_test` | `PASS` | Todas aprovadas dentro de `run_ui_018d1.ps1 -Regressions` (`ui_018c2_test` e `ui_018c1a_hardening_test` atualizados para o novo contrato `SC:` / `TOCAR` Home). |
| `guition-editor-8` + `check_blueprint_build.py` | `PASS` | Compilação limpa ESP32-P4 + auditoria de flags/alvos. |
| `guition-editor-8-bench` + `check_blueprint_build.py` | `PASS` | Compilação limpa ESP32-P4 + auditoria de flags/alvos. |
| Qualificação física (áudio, USB, touch, flash) | `MANUAL` | Adiada por decisão do usuário; não executada. |

---

## 10. Métricas LVGL (0.18-D.1 Stress — 1.000 ciclos)

- **Objetos LVGL:** `93 -> 93` (delta `0`).
- **Heap livre (`128 KiB` TLSF):** `74.192 -> 74.192 B` (delta `0`).
- **Maior bloco livre:** `73.184 -> 73.184 B`.
- **Pico do pool LVGL:** `68.736 B` (orçamento `131.072 B`).
- **Fragmentação TLSF:** `2 -> 2%`.
- **Latência agregada por ciclo (HOST):** mediana `3.714,4 µs`, P95 `4.976,6 µs`, máximo `8.457,8 µs`.

---

## 11. Comparação dos builds ESP32-P4 e auditoria de memória

| Perfil | RAM Estática (`.data` + `.bss`) | DIRAM Total | Flash (`.text` + `.rodata` + ...) | Imagem Total | Status |
|---|---|---|---|---|---|
| `guition-editor-8` (0.18-D) | `70.316 B` (21,5%) | — | `1.522.176 B` (9,1%) | `1.522.892 B` | `PASS` |
| `guition-editor-8` (0.18-D.1) | `70.328 B` (21,5%) | `169.040 B` (29,32%) | `1.523.916 B` (9,1%) | `1.524.632 B` | `PASS` |
| `guition-editor-8-bench` (0.18-D) | `70.852 B` (21,6%) | — | `1.627.664 B` (9,7%) | `1.628.380 B` | `PASS` |
| `guition-editor-8-bench` (0.18-D.1) | `70.876 B` (21,6%) | `174.826 B` (30,33%) | `1.629.406 B` (9,7%) | `1.630.122 B` | `PASS` |

### Origem das diferenças
- **0.18-D → 0.18-D.1:** variação mínima de `+12 B` RAM em `guition-editor-8` (`+24 B` em `bench`) e `+1.740 B` Flash devido ao novo estado `s_last_recall_token_` / `s_last_recall_from_ui_`, callback `on_harmony_overlay_` e strings de `RECALL TOTAL:`.
- **`guition-editor-8` vs `guition-editor-8-bench`:** `+548 B` RAM (`+428 B .data`, `+120 B .bss`) e `+105.490 B` Flash em `bench` decorrem exclusivamente de `sdkconfig.defaults.bench06` (`CONFIG_SMKP4_PCM_WIRING_CONFIRMED=y`, `CONFIG_SMKP4_USB_WIRING_CONFIRMED=y`, `CONFIG_SMKP4_USB_DESCRIPTOR_LOG=y`, `CONFIG_SMKP4_USB_MIDI_TRACE=y`) e `sdkconfig.defaults.hardware017b` (`CONFIG_SMKP4_HARDWARE_017B_TELEMETRY=y`), que habilitam os drivers físicos I²S/USB Host e telemetria.
- **0.18-C.3a (`74.200 B`) → 0.18-D/D.1 (`70.316–70.328 B`):** redução de ~3,8 KiB pela simplificação do shell/widgets introduzida na 0.18-D.
- **Pendência herdada:** a falha de orçamento da suíte `SoundLibrary` permanece documentada separadamente e não foi mascarada.

---

## 12. Riscos residuais e 13. Pendências físicas

- Todos os testes funcionais e capturas foram executados em **HOST SIM** (LVGL 9.3 + AMY real + NVS fake + USB MIDI injetado).
- Qualificação física pendente na Guition ESP32-P4: latência/deadlines DMA I²S, recepção USB Host física com SMK25 V2, precisão real dos alvos de 46 px no touch capacitivo 4,3" e persistência NVS real.

---

## 14. Veredito técnico

**APPROVED (Host Software & ESP32-P4 Builds)** — Não declarado pronto para palco até a conclusão da qualificação física.

---

## 15. Proposta — SMK-P4 0.19: Harmonic Pads (somente planejamento)

1. **Arquitetura e UI:** novo submodo `PADS` em `TOCAR` (4×2 ou 4×3 pads largos sobre a área `y = 102..380`), reutilizando `harmony::Config` (Fundamental, Escala, Tipo de Acorde, Voicing) com seleção rápida de grau diatônico (`I..VII`) e oitava (`OCT -/+`).
2. **Trigger, Hold e Release:** cada pad dispara um grupo de notas associado a um `NoteOrigin::HarmonicPad` + `pad_id` explícito no Audio Owner. Modo `MOMENTARY` libera no `LV_EVENT_RELEASED` / `LV_EVENT_DELETE`; modo `HOLD` mantém até novo toque ou `PANIC`.
3. **Integração MIDI/SMK25 e Arpeggiator:** os pads alimentam a mesma fila única de eventos do Audio Owner, podendo opcionalmente injetar acordes no Arpeggiator (`arp::Engine`) quando `ARP` estiver habilitado, sem criar caminho paralelo de notas.
4. **Performance e Scenes:** `scene::Configuration` pode armazenar o modo/oitava dos pads sem quebrar compatibilidade NVS v2 (extensão opcional), e a polifonia permanece limitada às 8 vozes do engine unificado (`SMKP4_VOICES=8`), com contagem e rejeição determinística quando saturado.
