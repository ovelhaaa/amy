# SMK-P4 0.18-C.3a — Rhythm UX Hardening

Execução: 25/11/2026. Branch `feature/smk-p4-ui-redesign-018c3a` (baseada em `feature/smk-p4-ui-redesign-018c3` @ `d5b8200ef0663aad612a4fef241135fae8441813`).
Qualificação física adiada por decisão do usuário. Sem alterações em AMY core, Audio Owner, Pattern Engine, Scene Engine, Arp Engine, USB MIDI, NVS ou protocolos de controle.

---

## 1. Problemas encontrados e correções aplicadas

### P0 — Preservação do modo de Pattern Switch (`IMMEDIATE` / `NEXT BAR`)
- **Causa:** `SequencerScreen::browser_switch_` era inicializado localmente como `seq::SwitchMode::Immediate`. Ao sair da tela `Patterns` e retornar, a tela era destruída e reconstruída, perdendo a seleção `NEXT BAR`.
- **Correção:** Adicionado `pattern_switch_mode_` em `UiNavigationManager` (`patternSwitchMode()` / `setPatternSwitchMode()`), inicializado em `Immediate` apenas em `UiNavigationManager::begin()`. `SequencerScreen::build()` restaura `nav.patternSwitchMode()`, e `BrowserAction::Mode` atualiza ambos sem enviar comandos ao Audio Owner e sem persistência NVS.

### P0 — Separação entre página visível, seleção e cursor de gravação
- **Causa:** `Control::Page` (`1-8 / 9-16`) executava `(selected_ + 8) % seq::kSteps`, `saveSelection()` e `selectStep()`, misturando paginação visual com seleção de passo e atualização do cursor de gravação no Audio Owner. Além disso, `refresh()` e `chooseCell()` derivavam a página de `selected_ / 8`.
- **Correção:** Introduzido `sequencer_page_` (0 ou 1) em `UiNavigationManager` (`sequencerPage()` / `setSequencerPage()`), separado de `subTab(ScreenId::Sequencer)`. `Control::Page` alterna apenas `page_` e atualiza `UiNavigationManager`, sem enviar `seq::Action::SelectStep`, sem alterar `selected_` ou `midi.selected`, sem alterar o padrão e sem reiniciar o transporte. A grade, cabeçalhos e playhead em `EDIT MODE` usam `page_ * 8`. Quando a gravação ocorre com o cursor fora da página visível, a UI informa explicitamente na linha MIDI (`REC XX (OFF-PAGE 1-8)` / `(OFF-PAGE 9-16)`) e na linha de feedback (`REC CURSOR STEP XX (PAGE ...)`), sem mudar silenciosamente de página.

### P1 — Captura `24_failed_operation.png` e atualização de estado em Patterns
- **Causa:** Em `UiBlueprint::service()`, `ScreenId::Patterns` não estava incluído na lista de telas atualizadas via `nav_.refreshActiveView()` quando `backend_.model().revision()` não mudava. Assim, quando uma operação de `Patterns` concluía ou era rejeitada sincronamente sem alterar `model().revision()` (ex.: `slot >= seq::kPatternSlots`), `refreshBrowser()` não era chamado e a tela permanecia exibindo o estado `REQUESTED` anterior.
- **Correção:** Adicionado `cur == ScreenId::Patterns` em `UiBlueprint::service()`, garantindo que `SequencerScreen::refreshBrowser()` reflita imediatamente `CONFIRMED`, `REJECTED`, `TIMEOUT`, `READY TO WRITE`, `WAITING` e a reabilitação dos botões.

### P1 — Refinamento dos diálogos de armazenamento
- **Correção:** Em `SequencerScreen::confirmStorage()`, os títulos foram diferenciados por estado do slot e ação:
  - Slot vazio: `SAVE NEW PATTERN - SLOT XX` (`destructive = false`).
  - Slot ocupado: `OVERWRITE PATTERN - SLOT XX: <nome>` (`destructive = true`).
  - Exclusão: `DELETE PATTERN - SLOT XX: <nome>` (`destructive = true`).
  - Save & Load: `SAVE SLOT XX -> LOAD SLOT YY`.
  - Todos preservam `ConfirmDialog` e o aviso de silêncio de áudio e parada de transporte.

### P1 — Indicadores de Step Recording
- **Correção:** Os botões `NORMAL PLAY`, `STEP INPUT` e `STEP RECORD` no painel `MIDI / REC` passam a ser armazenados (`mode_buttons_[3]`) e destacados exclusivamente a partir de `status->midi.mode` do snapshot confirmado (sem confirmação visual otimista ao clicar). `settings_info_` exibe também o resultado `Requested` ou `Rejected` quando presente.

### P1 — Ajustes visuais do Arp
- **Correção:** Em `ui_parameters.cpp`, o rótulo de `arp.restart` foi alterado de `"RESTART ON NEW CHORD"` para `"RESTART"`, mantendo a descrição contextual completa. Em `arpeggiator_screen.cpp`, o espaçamento de `" +OCT"` nas células de 72 px foi ajustado para evitar toque na borda direita.

---

## 2. Contrato final de Pattern Switch e Paginação / Cursor

### Contrato de Pattern Switch
1. `SWITCH MODE` alterna exclusivamente a preferência visual da sessão (`Immediate` ↔ `NextBar`) armazenada em `UiNavigationManager`.
2. Navegar entre `TOCAR`, `TIMBRES`, `RITMO`, `FX` e `MAIS` preserva a preferência.
3. Solicitações já admitidas no Audio Owner mantêm a política enviada no momento da admissão (`operation.mode`); alterar a preferência visual depois não converte nem cancela trocas pendentes.
4. `NEXT BAR` continua usando a fronteira global 4/4 (96 ticks) quando o transporte está ativo e aplicação imediata quando parado.
5. `CANCEL QUEUE` cancela explicitamente a troca pendente.

### Contrato de Página Visível / Seleção / Cursor
1. **Visible Page (`page_`, 0=1–8, 1=9–16):** controla apenas quais 8 passos são exibidos no piano roll e onde o playhead aparece; é preservada durante a sessão em `UiNavigationManager::sequencerPage()`.
2. **Selected Step (`selected_`):** passo selecionado explicitamente pelo usuário (cabeçalho ou célula), persistido em `subTab(ScreenId::Sequencer)` junto com a oitava.
3. **Record Cursor (`status->midi.selected`):** cursor confirmado do Audio Owner usado por `StepInput` e `StepRecord`. Durante `StepRecord`, notas USB gravam no cursor confirmado e avançam/fazem wrap pelo comprimento do padrão independentemente da página visível.

---

## 3. Regressão legada 0.16B (`step_sequencer_ui_016b_test.cpp`)

A suíte 0.16B foi adaptada ao layout 0.18-C.3/C.3a preservando todas as asserções musicais, de ownership, timeout, fila, `Panic`, `017A` e `017B`:
- **Expectativas espaciais invalidadas:** coordenadas fixas antigas da grade (`61 + 45*step`, `126 + row*17`) e acesso direto aos botões de demo/clear/enable na tela principal.
- **Adaptação:** o helper `cell(step, note)` entra em `EDIT MODE`, seleciona a página correta via `1-8 / 9-16` e calcula as coordenadas a partir dos bounds reais dos cabeçalhos (`01` / `09`); `LOAD DEMO PATTERN`, `CLEAR PATTERN`, `ENABLE / REST` e `INSERT REST` são acionados dentro do editor contextual de passo (`STEP ` + `CLOSE`); `STOP` foi atualizado para `STOP SEQ`, `PLAYING` para `PENDING`/`RUNNING`, `PATTERNS`/`SEQUENCER` pela sidebar e `SAVE`/`DELETE` com confirmação no `ConfirmDialog`; `LOAD` em slot vazio verifica `LV_STATE_DISABLED` antes de testar a rejeição de backend.

---

## 4. Validação (Host)

| Verificação | Status | Observações |
|---|---|---|
| `run_ui_018c3a.ps1 -Clean -Regressions` | PASS | Suíte C.3a + regressões `ui_018c3_test`, `ui_018c2_test`, `ui_018c1_integration_test`, `ui_018c1a_hardening_test`, `sound_editor_ui_test`, `smk25_0142_ui_test`. |
| `run_step_sequencer_ui_016b.ps1` (adaptado) | PASS | `PASS 017A`, `PASS 017B`, `PASS real LVGL/AMY sequencer controls...` |
| `run_arp_015.ps1` | PASS | `PASS arp_015_engine_test` |
| `run_step_sequencer_016a.ps1` | PASS | `PASS owner commands...` |
| `run_step_sequencer_midi_016c.ps1` | PASS | `PASS real AMY MIDI ownership...` |
| `run_pattern_library_017a.ps1` | PASS | `PASS all 16 NVS slots...` |
| `guition-editor-8` + `check_blueprint_build.py` | PASS | Sem erros; RAM estática 74.200 B, Flash 1.527.776 B (`firmware.bin` 1.528.496 B). |
| `guition-editor-8-bench` + `check_blueprint_build.py` | PASS | Sem erros; RAM estática 71.864 B, Flash 1.633.188 B (`firmware.bin` 1.633.920 B). |
| Hardware físico (áudio, USB, touch, flash) | MANUAL | Adiado por decisão do usuário; não executado. |

### Métricas LVGL (0.18-C.3a Stress)
- Cenário: Arp latched + Sequencer rodando simultaneamente; 500 trocas de área, 500 alternâncias PLAY/EDIT, 500 mudanças de página visual, 200 entradas/saídas de Patterns, 100 alternâncias `SWITCH MODE` + `SAVE`/`CANCEL`, e 100 gravações em Step Record.
- Objetos LVGL: `92 -> 92` (delta 0).
- Memória livre: `73.136 -> 73.136 B` (delta 0).
- Maior bloco livre: `72.040 -> 72.040 B`.
- Pico do pool LVGL: `66.296 B` (orçamento: `131.072 B` / 128 KiB).
- Fragmentação TLSF: `2 -> 2%`.
- Latência agregada por ciclo (500 ciclos): mediana `10.650,5 µs`, P95 `22.274,1 µs`, máximo `62.256,2 µs`.
- Continuidade musical: `generation` e `command.token` inalterados durante toda a navegação/paginação; `arp.tempo.phase` avançando continuamente.

---

## 5. Riscos residuais e pendências físicas

- Todos os testes executados são simulações em host (`HOST SIM`) com NVS fake e USB MIDI injetado; não substituem medição de deadlines, CPU, DMA/I²S, USB Host físico ou resposta touch no ESP32-P4.
- **Veredito técnico:** **APPROVED (Host Software)** — todos os defeitos P0 e P1 da auditoria 0.18-C.3 corrigidos, regressão 0.16B restaurada com cobertura musical preservada, métricas LVGL com crescimento líquido zero e builds ESP32-P4 aprovados. Pronto para auditoria antes da 0.18-D.
