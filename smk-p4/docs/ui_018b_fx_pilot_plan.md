# Plano Incremental e Critérios de Passagem — SMK-P4 0.18-B (FX Piloto)

## 1. Objetivo da 0.18-B

Executar a primeira migração de produção da nova arquitetura de UI (`UiShell018A`) **restrita ao subsistema FX** (`Delay`, `Reverb`, `Chorus`, `Master`), usando o protótipo validado da `0.18-A` como referência e **sem alterar comportamento de DSP** nem introduzir controles enganosos.

---

## 2. Pré-requisitos de Entrada (`GO` da 0.18-A + Pendências de Contrato)

1. Aprovação humana dos entregáveis UX e medições host da `0.18-A`.
2. Extensão mínima e auditada do contrato `IUiBackend` / `UiBlueprintPerformanceAdapter` caso se deseje expor controles que já existem no Audio Owner (`FxConfig`), mas hoje não têm ID público em `IUiBackend`:
   - `fx.bypass_all` (Bool) → já existe em `FxConfig::bypass_all`, `applyBusFx()`, `delayStatus().bypassed` e `routingValues().bypass`.
   - `fx.eq.mid_db` e `fx.eq.high_db` (Float `-12..12 dB`) → já existem em `FxConfig` e `performance::FxDestination`.
3. **Decisão explícita para bypass individual (Delay / Reverb)**:
   - Se `0.18-B` continuar sem bypass individual em `FxConfig`, manter o indicador explícito `BYPASS INDIV: N/A (MIX 0% NÃO ANULA MACROS M3/M6/M7)`.
   - Caso seja solicitado bypass individual real de Delay/Reverb em firmware, deve vir como tarefa separada de contrato no `Audio Owner` (`FxConfig` + NVS v3 ou flag em runtime) antes de desenhar o botão na UI.

---

## 3. Etapas Incrementais da 0.18-B

| Etapa | Escopo técnico | Critério de aceitação verificável |
| --- | --- | --- |
| **B.1 · Contrato FX e Adapter** | Adicionar `fx.eq.mid_db`, `fx.eq.high_db` e `fx.bypass_all` em `ParameterRegistry` + `isFxId()`/`setFxParameter()` no adapter (ou manter escopo restrito se optado por não alterar adapter na B.1), mantendo `bypass_all`, SPSC queue e `effectiveFx()` intactos. | Testes `run_fx_tests.ps1` e novos casos no `run_blueprint_adapter.ps1` confirmando sincronização ida e volta no `amy::Runtime::snapshot().fx`. |
| **B.2 · Piloto Visual `FX` no `ui_blueprint`** | Introduzir shell contextual somente para `FX` (ou rota dedicada com retorno 1-toque) mantendo as demais telas da 0.17B intactas e compiláveis sem regressão. | `run_preview.ps1`, `run_step_sequencer_ui_016b.ps1`, `run_sound_editor_ui.ps1`, `run_harmony_ui.ps1` **100% PASS**; política de throttle em drag (`VALUE_CHANGED` local, commit final em `RELEASED` + reversão se `Rejected`). |
| **B.3 · Bloqueios, Confirmação e Notas** | Garantir que `fx.delay.time_ms` bloqueia visualmente sob `SYNC`, `fx.chorus.depth` sinaliza estado quando `ChorusMode::Off`, e nenhuma troca dentro de `FX` libera notas USB/MIDI ou reseta o AMY. | Teste integrado `amy::Runtime` + `LocalAmyPerformanceBackend` + LVGL host validando `active_notes` preservado e 0 vazamento em 500 alternâncias Delay/Reverb/Chorus/Master. |
| **B.4 · Validação Física Autorizada na Guition** | **Somente com aprovação explícita**: validar toque GT911 nos sliders largos/botões `-`/`+` dos módulos FX, audição estéreo na PCM5102A com SMK25 ligado, e telemetria `BOOT_HEAP` / `DMA_underruns` / `LVGL_POOL`. | Captura serial completa sem underruns auditivos, 0 falhas de alocação e registro de áudio estéreo real antes de migrar `Ritmo` ou `Tocar`. |

---

## 4. Riscos e Limites Não Negociáveis

- **Não migrar `Tocar`, `Ritmo`, `Timbres` nem `Encadear` durante a `0.18-B`**: manter o piloto focado para isolar custo de pool LVGL (`128 KiB` em PSRAM) e latência de toque no GT911.
- **Re-avaliar `model_.bumpRevision()` no adapter** antes da expansão completa: eliminar bumps incondicionais por tick evita redraws redundantes em LVGL durante deslizamento contínuo de sliders no ESP32-P4.
- **Gate físico 0.17B permanece pré-requisito para merge em `main`**: `A2` (touch), `A4` (audição estéreo), `A5` (Scenes) e desconexão/reconexão USB continuam obrigatórios na bancada.
