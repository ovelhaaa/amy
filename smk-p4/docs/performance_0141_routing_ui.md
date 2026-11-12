# SMK-P4 0.14.1 — Routing Editor

Abra **Performance → Routing Editor**. Escolha a macro, um dos seis slots e um destino. O catálogo é filtrado pela família e pela topologia materializada no patch. Os nomes indicam o grupo e o painel mostra a unidade e o domínio do destino. Restaurar uma rota de fábrica pode trazer de volta uma rota que só atua na outra família.

Os campos mínimo, máximo e profundidade aceitam edição numérica pelo teclado touchscreen. A curva, a polaridade, a habilitação e a inversão ficam na mesma tela. Para adicionar, selecione o destino de um slot vazio. Para substituir, escolha outro destino no slot atual. Delete, Restore Slot, Reset Routes, Reset Position e Reset Both modificam o rascunho. **Apply Routes** confirma e envia o conjunto. **Cancel / Back** descarta o rascunho.

A UI informa **Waiting for Owner** até receber confirmação do token, da geração e do patch. Uma alteração concorrente de patch, geração ou configuração invalida o rascunho. Compare suspende a aplicação do rascunho. **Save Preset** persiste as rotas confirmadas.

O teste LVGL usa os callbacks reais para editar o slot 6 de M7, selecionar Reverb Mix e S-Curve, rejeitar profundidade fora do domínio e verificar a alteração no barramento AMY após o ACK. O mesmo cenário cobre cancelamento, remoção, reentrada e geração obsoleta.
