# SMK-P4 0.15.2a — Auditoria de hardening

## Patch e referências de FX

O Audio Owner confirma cada carga concluída ao publicar `PatchState::Ready` e
incrementar `patch_changes`. O backend compara o contador com o valor anterior
à publicação. Isso também identifica o recarregamento do mesmo ID. Snapshots de
gerações anteriores são descartados. A solicitação enfileirada e o snapshot
de falha não redefinem as referências de FX.

- `original_fx_` contém a configuração de fábrica usada por Compare. O
  carregamento padrão do Owner usa `FxConfig{}` como referência, com o FX
  preservado no estado confirmado quando Keep FX está ativo.
- `desired_fx_` contém os FX de edição, inclusive durante Compare. Após a
  carga confirmada do patch, ele recebe a configuração publicada.
- `saved_fx_` contém a configuração confirmada no Save As ou Recall. Revert
  e dirty state usam essa referência quando válida, inclusive em presets
  com alterações somente de FX. A mudança de época invalida o preset anterior.
- Recall V4 mantém seu próprio fluxo até confirmar os tokens de synth e FX.
  O carregamento intermediário não reinicializa as referências no backend.
  O Recall só estabelece a nova referência de preset depois da confirmação.

## Popup

O ciclo existente de `UiBlueprint::service()` roda a cada 10 ms nominais no
firmware. Após o serviço do adapter, ele verifica somente o modal aberto.
A exclusão do objeto raiz limpa o contexto, inclusive quando a navegação
desmonta a tela. Não há timer adicional nem dependência de uma mudança na
revisão visual para fechar o modal ou mostrar falha.

O adapter usa a mudança no contador de sucesso e a correspondência do patch
alvo para confirmar. O contador de falhas diferencia uma falha nova da
publicação ainda visível de uma tentativa anterior. Uma transação pendente
bloqueia novos pedidos de patch. Durante a espera, CANCEL e os dois botões de
carga ficam desabilitados. O PANIC global no status strip continua acessível
e encerra a espera com uma indicação de interrupção.

O timeout é de **2.000 ms** a partir da solicitação, observado no próximo
ciclo da UI. Falha ou timeout reabilitam CANCEL. Após timeout, o Owner ainda
pode concluir a operação. Um novo pedido continua sujeito ao estado ocupado
do Owner, e a confirmação tardia não reabre o modal fechado.

## Performance Locks e tempo real

A matriz host testa as 16 máscaras de FX, Harmony, Arp e Macros em uma troca
Juno → DX7. Rotas compatíveis atravessam a troca. O teste de Patch Loading
também cobre o descarte de uma rota incompatível entre famílias. Macros
preservadas fora do padrão permanecem editadas e o backend detecta esse
dirty state. A mudança conserva o BPM global e libera notas antigas.
O DSP e a alocação de vozes do AMY permanecem upstream.

## Stack e RAM

A 0.15.2 registrou **3.584 B** livres na stack do Audio Owner na Guition.
A 0.15.2a configura **12.800 B** de stack, um aumento de 512 B sobre
12.288 B. A extrapolação de **4.096 B** livres assume o mesmo consumo
máximo e precisa de medição física para comprovar o piso de 4.000 B.
O mesmo cálculo projeta 357.251 B de RAM interna livre na admissão,
a partir dos 357.763 B registrados na 0.15.2, com piso de 352.256 B.
O relatório de bancada contém os critérios e o estado de qualificação.
