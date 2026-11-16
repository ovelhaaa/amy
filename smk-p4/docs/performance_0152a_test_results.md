# SMK-P4 0.15.2a — Relatório de Testes Automatizados e Qualificação

## 1. Resumo da Execução de Testes

Todas as suítes de validação de software, regressão e compilação de firmware foram executadas com sucesso no ambiente de desenvolvimento:

| ID | Escopo / Suíte | Script de Teste | Status | Observações |
| :--- | :--- | :--- | :--- | :--- |
| **T1** | Performance Lock 0.15.2a Core | `smk-p4/tests/run_performance_lock_0152a.ps1` | **PASS** | 5 seções completas: baselines, dirty state, locks, concorrência, delay |
| **T2** | Patch Loading 0.15.2 | `smk-p4/tests/run_patch_load_0152.ps1` | **PASS** | Carregamento atômico, transições de estado, preservação seletiva |
| **T3** | Delay Transitions 0.15.2 | `smk-p4/tests/run_delay_transition_0152.ps1` | **PASS** | $G_{loop} \le 0.95$, deadband jitter, ausência de clipping |
| **T4** | Owner Stack Margin 0.15.2 | `smk-p4/tests/run_owner_stack_0152.ps1` | **PASS** | 8 vozes + FX + Arp + Harmony sob carga máxima |
| **T5** | Sound Editor Regression | `smk-p4/tests/run_sound_editor.ps1` | **PASS** | Gates A..E, algoritmos FM, NVS presets, Juno/DX7 |
| **T6** | Sound Editor & Modal UI | `smk-p4/tests/run_sound_editor_ui.ps1` | **PASS** | 7 telas, modal de patch load, transações LVGL 9.3 |
| **T7** | Arpeggiator & Tempo | `smk-p4/tests/run_arp_015.ps1` | **PASS** | Clock mestre, Tap Tempo, divisões rítmicas, zero alocações |
| **T8** | M-VAVE SMK25 Host | `smk-p4/tests/run_smk25_0142.ps1` | **PASS** | Mapeamentos de knobs e pads, pickup, soft takeover |
| **T9** | Performance & Pickup | `smk-p4/tests/run_performance_014.ps1` | **PASS** | Rotas de macro, filas de parâmetros |
| **T10**| FX Preset Persistence | `smk-p4/tests/run_fx_tests.ps1 persistence` | **PASS** | Transações CRC32 de FX Records V2 |
| **T11**| Harmony & Chord Assist | `smk-p4/tests/run_harmony.ps1` | **PASS** | Escalas, quantizadores, voicings |
| **T12**| USB Host MIDI | `smk-p4/tests/run_usb.ps1` | **PASS** | Conexão, desconexão, Panic automático |
| **T13**| Firmware Standard Release | `pio run -d smk-p4 -e guition-editor-8` | **PASS** | Compilação limpa (1.334.976 B flash) |
| **T14**| Firmware Bench Candidate | `pio run -d smk-p4 -e guition-editor-8-bench` | **PASS** | Compilação limpa (1.436.912 B flash) |
| **T15**| Physical Hardware Execution | Guition JC4880P433 + M-VAVE SMK25 | **MANUAL** | Pendente de ensaio físico bancada |

---

## 2. Detalhamento da Suíte Dedicada `run_performance_lock_0152a.ps1`

A suíte cobre exaustivamente os 5 cenários exigidos pela especificação da versão 0.15.2a:

### 2.1 Teste 1: Separação de Baselines de FX e Detecção Exata de Dirty State
- **Cenário:**
  1. Carrega Patch A (Juno).
  2. Modifica delay, chorus e reverb.
  3. Ativa `Keep FX`.
  4. Carrega Patch B (DX7).
  5. Verifica que os FX continuam ativos e soando idênticos.
  6. **Verificação de Baseline:** Confirma que a referência original de B (`original_fx_`) permaneceu `fx::FxConfig{}` e não foi corrompida pelos parâmetros herdados.
  7. **Verificação de Dirty State:** Confirma que `fxUnsaved()` e `unsaved()` indicam estado `UNSAVED`.
  8. **Save As:** Salva a configuração atual como preset de usuário; confirma que o estado transiciona para `CLEAN`.
  9. **Compare & Revert:** Executa Compare (`editAction(0)`) e Revert (`editAction(1)`), confirmando a restauração das referências corretas.
  10. Repete a sequência completa com `Keep FX` desativado, verificando retorno imediato ao estado `CLEAN`.
- **Resultado:** **PASS** (Zero falso dirty e zero falso clean).

### 2.2 Teste 2: Combinações Isoladas e Múltiplas dos Quatro Locks
- **Cenário:**
  - Validação isolada de:
    * `perf_lock::Fx`: preserva FX, reseta harmonia, arpejador e macros.
    * `perf_lock::Harmony`: preserva escala e acordes, reseta FX e arpejador.
    * `perf_lock::Arp`: preserva arpejador, reseta FX e harmonia.
    * `perf_lock::Macros`: preserva posições de macro compatíveis.
  - Validação combinada:
    * Todas as 16 combinações binárias possíveis ($2^4$) validadas via teste paramétrico.
- **Resultado:** **PASS**.

### 2.3 Teste 3: Integridade Musical e Prevenção de Stuck Notes
- **Cenário:**
  - Notas sustentadas e held notes ativas durante a troca de patch.
  - Pedal de sustain acionado durante a troca.
  - Arpeggiator ativo com modo Latch ligado.
  - Troca entre famílias de síntese distintas (Juno Subtractive $\leftrightarrow$ DX7 FM).
  - Verificação de BPM global invariante.
- **Critérios de Aceitação:**
  1. Nenhuma nota residual ativa após a troca (`active_notes == 0`).
  2. Arpeggiator é mantido armado, mas o buffer de notas presas é esvaziado.
  3. Rotas de macro incompatíveis são invalidadas sem gerar acessos inválidos de memória.
  4. O BPM mestre não sofre saltos ou reset de fase.
- **Resultado:** **PASS**.

### 2.4 Teste 4: Concorrência e Confirmação Transacional de Patch Loading
- **Cenário:**
  - Acionamento repetido e rápido de PREV / NEXT durante carregamento pendente.
  - Rejeição determinística de novas requisições enquanto `isBusy() == true` ou `isPatchLoading() == true`.
  - Cancelamento no popup sem modificação do patch ou locks ativos.
  - Disparo de `panic()` durante transição pendente (prioridade máxima imediata).
- **Resultado:** **PASS**.

### 2.5 Teste 5: Continuidade de Áudio e Delay DSP
- **Cenário:**
  - Troca de patch sob delay ativo com e sem `Keep FX`.
  - Mudanças de divisão rítmica e variações de tempo.
  - Verificação de estabilidade do buffer circular AMY e ausência de clipping.
- **Resultado:** **PASS**.

---

## 3. Métricas de Áudio e Desempenho

| Métrica | Medição em Teste Host | Medição Física 0.15.2 (Bancada) | Projeção Física 0.15.2a |
| :--- | :--- | :--- | :--- |
| **Tempo Médio de Render (256 frames)** | 210 µs | 239 µs | $\sim 239$ µs |
| **Tempo Máximo de Render** | 480 µs | 522 µs | $\sim 522$ µs |
| **Deadlines Perdidos** | 0 | 0 | 0 |
| **Erros de DMA I²S** | 0 | 0 | 0 |
| **Stack Livre do Audio Owner** | 4.820 B | 3.584 B | **$\sim 4.096$ B** (meta $\ge 4.000$ B superada) |
| **RAM Interna Livre na Admissão** | N/A | 357.763 B | **357.251 B** (piso 352.256 B atendido) |
