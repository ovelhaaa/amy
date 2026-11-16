# SMK-P4 0.15.2a — Auditoria de Arquitetura e Estabilização do Performance Lock

## 1. Visão Geral

A versão 0.15.2a foca na estabilização rigorosa do subsistema **Performance Lock**, correção das referências de **Dirty State**, transacionalidade no carregamento de patches na interface gráfica e garantia da integridade musical em tempo real para o sintetizador standalone SMK-P4 (Guition JC4880P433, ESP32-P4, AMY, 8 vozes, 48 kHz).

Esta auditoria documenta o fluxo de dados entre UI, backend, Audio Owner, a separação conceitual das quatro referências de FX, o tratamento de concorrência e a resolução da divergência de stack livre físico.

---

## 2. Mapeamento do Fluxo de Parâmetros e Ownership

```text
       Touchscreen / UI (Patch Selector Modal)
                          │
                          │ requestSelectPatch(patch, locks)
                          ▼
            LocalAmyPerformanceBackend
                          │
                          │ setPatch(patch, now, locks)
                          │   - Constrói AmyCommand::Type::Patch
                          │   - Preenche patch_target e locks
                          ▼
                  Command Mailbox
                          │
                          │ popCommand() (lock-free)
                          ▼
             Audio Owner Task (Core 0)
                          │
                          │ processPatchLoad(cmd)
                          │   - Silencia vozes ativas (Panic / NoteOff)
                          │   - Limpa buffers de held notes e arpejador
                          │   - Carrega novo instrumento AMY
                          │   - Se Keep FX: restaura FxConfig anterior e marca dirty
                          │   - Se Keep Harmony: restaura escala/acorde
                          │   - Se Keep Arp: restaura rate/gate/latch (limpa notas)
                          │   - Se Keep Macros: valida compatibilidade de síntese
                          ▼
                Snapshot Publicado
                          │
                          ▼
            LocalAmyPerformanceBackend::publish()
                          │
                          │ Confirma patch_target == target_
                          │ Atualiza snapshot publicado para UI
                          ▼
              UI Event Refresh / Dialog Dismiss
```

---

## 3. Problema Prioritário — FX Dirty State e Falso Clean

### 3.1 Problema Identificado na 0.15.2
Na versão 0.15.2, o método `LocalAmyPerformanceBackend::publish()` continha:
```cpp
desired_fx_ = original_fx_ = snapshot.fx.config;
```
Quando o **Keep FX** estava ativado e o usuário trocava de patch, o `snapshot.fx.config` continha a configuração herdada do patch anterior (com delay/reverb personalizados). Ao sobrescrever `original_fx_` com o snapshot, o backend assumia que os FX customizados eram o "padrão original de fábrica" do novo patch.

Consequências:
1. **Falso CLEAN:** O patch recém-carregado era considerado limpo (`fxUnsaved() == false`), embora estivesse soando com FX modificados e não salvos em relação ao timbre base de fábrica.
2. **Quebra do Compare / Revert:** A ação de Compare (`editAction(0)`) carregava `original_fx_`. Como `original_fx_` havia sido contaminado pelos FX preservados, o Compare comparava o som consigo mesmo em vez de alternar com o dry/baseline original de fábrica.

### 3.2 Correção Arquitetural — As Quatro Referências de FX

Para eliminar definitivamente o problema sem introduzir estruturas redundantes ou custos no caminho crítico de áudio, definiram-se 4 referências conceituais:

1. **Factory / Patch FX Baseline (`original_fx_`):**
   - Configuração de referência original do patch ROM ativo.
   - Para patches de fábrica AMY (que não possuem FX embutidos na ROM), a baseline neutra é `fx::FxConfig{}` (Dry, Delay 350 ms bypass, Reverb level 0, Chorus level 0, EQ flat).
   - **Regra:** Nunca é sobrescrita pela herança de locks. Ao carregar um patch de fábrica com Keep FX, `original_fx_` permanece `fx::FxConfig{}`.
2. **Effective FX (`fx_state_.config` no Audio Owner e Snapshot):**
   - Configuração efetivamente aplicada aos blocos DSP do AMY em tempo real.
   - Reflete o estado auditivo atual.
3. **Desired FX (`desired_fx_` no Backend):**
   - Configuração editada ou intencionada pela interface / knobs.
   - Na troca com Keep FX, `desired_fx_ = snapshot.fx.config`.
4. **Saved FX (`saved_fx_` ou `saved_edit_` nos Presets V4):**
   - Configuração armazenada no slot de preset do usuário (NVS).
   - Ao carregar um User Preset via Recall V4, `original_fx_ = fx::FxConfig{}` e `saved_fx_` armazena a versão salva para Revert.

### 3.3 Regras de Dirty State e Equivalência

A detecção de alterações não salvas é avaliada através de `fxEquivalent(const FxConfig& a, const FxConfig& b)`:
- Avalia delay time (tolerância 1.0 ms), feedback ($\pm 0.01$), mix ($\pm 0.01$), divisão rítmica e modo FREE/SYNC.
- Avalia reverb level ($\pm 0.01$), decay ($\pm 0.05$), damping ($\pm 0.01$).
- Avalia chorus level ($\pm 0.01$), rate ($\pm 0.05$), depth ($\pm 0.01$).
- Avalia bandas de EQ ($\pm 0.2$ dB).

Com isso:
- Se os FX preservados forem idênticos à baseline de fábrica (ex: delay desligado e reverb zero), o estado é **CLEAN**.
- Se houver qualquer parâmetro audível diferente da baseline, o estado é **UNSAVED (DIRTY)**.
- O salvamento (`saveCurrentPatchAsPreset`) captura a configuração atual e atualiza `saved_fx_`, transicionando o estado para **CLEAN**.
- Revert (`editAction(1)`) restaura a referência adequada: baseline de fábrica para patches de fábrica (`edit_.user_slot < 0`) ou a configuração salva para presets de usuário (`edit_.user_slot >= 0`).

---

## 4. Confirmação Transacional de Patch Loading na UI

### 4.1 Ciclo de Vida Auditado

O ciclo de carregamento foi unificado na máquina de estados:
$$\text{IDLE} \xrightarrow{\text{seleção}} \text{REQUESTED} \xrightarrow{\text{áudio}} \text{LOADING} \xrightarrow{\text{validação}} \text{READY / FAILED}$$

1. **REQUESTED:**
   - O popup na UI exibe o estado de carregamento e desabilita temporariamente os botões de ação para evitar duplicação de toques.
   - O backend registra `loading_patch_ = true` e armazena o token do patch alvo.
2. **LOADING:**
   - O comando `AmyCommand::Type::Patch` é despachado ao Audio Owner via mailbox atômica.
3. **VERIFYING:**
   - O Audio Owner aplica o novo patch, inicializa osciladores, silencia vozes, preserva os locks solicitados e publica um novo snapshot contendo o `patch_target` confirmado.
4. **READY / SUCCESS:**
   - O backend observa a publicação confirmada (`snapshot.patch_target == target_`), atualiza os metadados do patch e limpa `loading_patch_`.
   - O callback de refresh (`LV_EVENT_REFRESH`) no modal fecha o popup automaticamente.
5. **TIMEOUT / FAILURE:**
   - Se o Audio Owner não confirmar o patch dentro da janela limite (1.000 ms), o backend marca falha, exibe aviso visual no popup e reabilita os botões para cancelamento ou nova tentativa.

### 4.2 Política de Concorrência e PREV / NEXT Rápido
- Se o usuário acionar **PREV / NEXT** ou tocar em outro patch enquanto um carregamento estiver pendente (`backend.isBusy() == true` ou `isPatchLoading() == true`), o novo pedido é **rejeitado deterministicamente**.
- Não existem filas concorrentes infinitas de carregamento.
- O Panic (`panic()`) permanece 100% prioritário e acessível em qualquer fase, inclusive com popup aberto ou durante carregamento pendente.

---

## 5. Validação Visual e Ergonomia da Interface (800×480)

O popup de carregamento de patch foi projetado para a resolução física de 800×480 da placa Guition JC4880P433:

| Elemento | Dimensões / Especificações | Conformidade de Toque |
| :--- | :--- | :--- |
| **Janela Modal** | $620 \times 276$ px centralizada | Visibilidade panorâmica sem cortes |
| **Checkboxes (Locks)** | 4 colunas horizontais (FX, Harm, Arp, Macro) | Áreas de toque espaçadas |
| **Botão CANCEL** | $100 \times 44$ px | Mínimo 44 px de altura (padrão físico) |
| **Botão Clean Load** | $164 \times 44$ px ("LOAD PATCH" / "DISCARD & LOAD") | Rótulo explícito e legível |
| **Botão Locked Load**| $300 \times 44$ px ("LOAD WITH LOCKS" / "DISCARD & LOAD LOCKS") | Destaque primário |

- As lambdas de evento LVGL foram desacopladas para funções estáticas capture-free com passagem de ponteiro de contexto `BtnContext`, garantindo total compatibilidade com C++17 e callbacks C da LVGL 9.3.

---

## 6. Integridade Musical e Prevenção de Stuck Notes

1. **Flush Inegociável de Vozes:**
   - Ao executar a troca de patch, o Audio Owner executa `silence()`, corta todas as 8 vozes ativas e incrementa a geração de comandos.
   - Nenhuma nota antiga é mantida soando após a troca.
2. **Sustain Pedal & Latch:**
   - Pedais de sustain físicos e estados de latch do arpejador são esvaziados no momento da troca de patch para evitar notas fantasmas contínuas.
3. **Cross-Family Macro Safety:**
   - Na alternância entre famílias de síntese incompatíveis (ex: Juno Subtractive $\leftrightarrow$ DX7 FM):
     * Rotas direcionadas a parâmetros inexistentes (ex: Cutoff no DX7 ou OP Feedback no Juno) são invalidadas com diagnóstico seguro.
     * Rotas universais (FX Mix, FX Decay, etc.) permanecem funcionais.
4. **BPM Global:**
   - O clock mestre (`tempo::Clock`) permanece invariante e contínuo durante toda a operação de troca de patch.

---

## 7. Análise da Margem de Stack do Audio Owner

### 7.1 Divergência Observada
- **Simulação Host / Cálculo Teórico 0.15.2:** 4.564 B de stack livre.
- **Hardware Físico (ESP32-P4 v1.3):** 3.584 B de stack livre.
- **Requisito de Qualificação de Hardware:** Mínimo de 4.000 B livres.

**Causa-raiz:**
A arquitetura RISC-V de 32 bits no ESP32-P4 reserva quadros adicionais para trocas de contexto FreeRTOS, registros de interrupção (FPU/DSP callee-saved registers) e alinhamento de 16 bytes que não são simulados na pilha plana do host Linux/Windows.

### 7.2 Correção Implementada
Em `smk-p4/components/amy_runtime/runtime_config.h`:
- `kOwnerStackBytes` foi aumentado de **12.288 B** (12 KiB) para **12.800 B** (+512 B).
- **Projeção física:** A margem livre no ESP32-P4 passa de $3.584 \text{ B} \to \sim 4.096 \text{ B}$, superando com folga o piso de 4.000 B.
- **Impacto na Admissão de RAM Interna:**
  * RAM livre na admissão em 0.15.2: 357.763 B.
  * Com acréscimo de 512 B: **357.251 B** livres.
  * O piso obrigatório de admissão é 352.256 B.
  * **Margem preservada:** $\sim 5.000$ B de folga segura acima do piso de admissão de memória.
