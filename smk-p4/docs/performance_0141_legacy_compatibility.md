# Offsets legados na 0.14.1

Abra **Performance → Macro Focus → Legacy Offsets**. O painel mostra as três posições históricas, o neutro 64, a família, o destino e o estado de Compare. Em DX7, Brightness atua na profundidade de FM. Em Juno, Brightness atua no cutoff e Attack/Release no envelope de amplitude.

O primeiro toque em **Reset Legacy Offsets** abre a confirmação. O segundo solicita ao owner uma edição completa que altera somente as três posições legadas. **Back / Cancel** fecha o painel. A confirmação rejeita mudanças de patch ou de offsets desde o primeiro toque. A interface mostra os valores recebidos do owner.

As fórmulas históricas e os codecs V1/V2/V3 permanecem na implementação existente. Compare mantém sua referência ROM e suspende as contribuições de macros. Revert continua voltando à referência de fábrica. Para recuperar offsets já salvos depois de um reset, descarte as edições e faça Recall. A restauração direta dos offsets anteriores ao reset ainda está pendente.

O teste LVGL usa um Juno com Brightness 96 e confirma que o primeiro toque preserva o cutoff, que a UI sinaliza a solicitação antes do ACK e que o owner devolve o cutoff ao valor manual após o reset.
