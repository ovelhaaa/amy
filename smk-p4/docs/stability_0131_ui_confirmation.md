# SMK-P4 0.13.1 — Confirmação Robusta de Comandos e Interface Touchscreen

The FM selector reads its dedicated result and checks the owner token. The adapter checks current and snapshot generation, patch, token, applied algorithm and error count. User Presets keeps one modal. Overwrite expires after 10 seconds and rechecks the name, source patch and synth sequence. The modal tracks the operation revision to distinguish consecutive completions.

## 1. Princípios de Confirmação na UI

Na versão 0.13.1, nenhuma alteração crítica de parâmetros na interface gráfica assume sucesso de forma otimista ou meramente local em RAM:

1. **Despacho Assíncrono com Token:** Cada solicitação recebe um token monotônico gerado pela camada de controle (`newEditToken()`).
2. **Confirmação pelo Owner DSP:** A UI mantém o estado visual solicitado e só transiciona para confirmado quando o snapshot do motor AMY publica o token e os valores efetivos aplicados.
3. **Rejeição Transparente:** Em caso de erro, timeout ou rejeição pelo motor, os valores confirmados anteriores são restaurados instantaneamente sem reconstrução disruptiva do layout LVGL.

---

## 2. Confirmação do Seletor de Algoritmo FM

### 2.1 Requisitos Implementados

- **Correlacionamento por Token e Geração:** O widget `fm_algorithm_selector` armazena o token de requisição e compara diretamente com `lastCommandToken()` do adapter.
- **Proteção contra Notas Ativas:** Mudanças na topologia de algoritmo FM são rejeitadas categoricamente enquanto houver notas ativas ou pedal de sustain mantido, com aviso na tela: `"RELEASE NOTES TO CHANGE FM ALGORITHM"`. Edições pontuais em níveis e envelopes de operadores continuam permitidas.
- **Coalescência de Toques Rápidos:** Taps rápidos repetidos enquanto uma troca de algoritmo está pendente são coalescidos ou protegidos com `"ALGORITHM CHANGE PENDING"`.
- **Cancelamento por Troca Concorrente de Patch:** Se um comando de troca de patch (e.g. Program Change externo) ocorrer enquanto um algoritmo estava pendente, o adapter cancela a requisição com `"PATCH CHANGED; ALGORITHM CANCELED"` e alinha os controles ao novo patch.

### 2.2 Telemetria Serial Estruturada

O seletor FM emite telemetria determinística no formato:

```text
FM_ALGO_DIAG requested=<REQ> confirmed=<CONF> token=<TOK> generation=<GEN> state=<STATE>
```

Exemplos capturados durante os testes:
```text
FM_ALGO_DIAG requested=16 confirmed=19 token=11 generation=16 state=PENDING
FM_ALGO_DIAG requested=16 confirmed=16 token=11 generation=16 state=CONFIRMED
FM_ALGO_DIAG requested=22 confirmed=16 token=13 generation=16 state=REJECTED
```

---

## 3. Modal de Presets: Atualização Não-Bloqueante

O modal de User Presets (`sound_editor_screen.cpp`) foi otimizado para atualizar de forma não-bloqueante via eventos `LV_EVENT_REFRESH`:

- **Preservação de Foco e Entrada:** O slot selecionado, o texto digitado na caixa de nome e o estado do teclado virtual não são destruídos durante o ciclo assíncrono de gravação.
- **Desabilitação Seletiva de Botões:** Durante estados intermediários (`WaitingForQuiet`, `Writing`, `Verifying`), botões conflitantes (como Recall e Save As simultâneos) ficam desabilitados, prevenindo disparos múltiplos concorrentes.
- **Liberação Automática:** Após o recebimento de `Confirmed` ou `Rejected`, os botões são reabilitados e o dropdown de slots é atualizado in-place.
