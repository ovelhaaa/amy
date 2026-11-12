# Offsets legados na 0.14.1

Abra **Performance → Macro Focus → Legacy Offsets**. O painel mostra posição, neutro 64, família, destino e estado. Em DX7, Brightness atua na profundidade FM. Em Juno, Brightness atua no cutoff e Attack/Release no envelope de amplitude.

O primeiro toque em **Reset Legacy Offsets** abre a confirmação. O segundo envia ao owner uma edição que altera somente as três posições históricas. **Back / Cancel** fecha o painel. A confirmação rejeita mudanças de patch ou offsets desde o primeiro toque. O painel exibe os valores confirmados pelo owner.

Compare usa a referência ROM e suspende macros legadas e novas. Revert recupera a referência de presets chamados por Recall e de referências salvas com offsets legados ou rotas customizadas. Fora dessas referências, Revert volta à fábrica. Abertura e Recall não regravam o registro.

O teste LVGL usa Brightness 96 e verifica a recuperação do cutoff manual após reset. O teste PCM cobre neutro, Brightness, Attack e Release em fixtures V1/V2. O FM V3 inclui algoritmo editado, posição legada, macros novas e FX. Os nove trechos de PCM renderizados pelos runtimes 0.14 e 0.14.1 resultaram no mesmo arquivo de 1.179.648 bytes. [Hash e resultados](performance_0141_results.md).
