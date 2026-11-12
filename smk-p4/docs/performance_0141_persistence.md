# SMK-P4 0.14.1 — persistência

O incremento de offsets legados usa os codecs e as transações da 0.14. As posições aparecem no snapshot após Recall. O reset altera o estado de trabalho no owner. Uma gravação ainda requer Save explícito e a barreira de silêncio já existente.

| Formato | Leitura neste incremento | Posições | Rotas |
|---|---|---|---|
| V1 | Caminho existente | Três legadas, oito novas inicializadas nos neutros | Perfil de fábrica |
| V2 | Caminho existente | Três legadas, oito novas inicializadas nos neutros | Perfil de fábrica |
| V3 | Caminho existente | Três legadas e oito novas | Perfil de fábrica |
| V4 | Em projeto | A definir | A definir |

O registro Synth V3 de 176 bytes já usa os bytes para magic, V2, oito posições e CRC. A NVS existente tem 24 KiB. Com dois bancos para cada um dos 16 slots, só os registros Synth V3, FX e marcadores somam 7.680 bytes brutos. Mais 48 rotas de 14 bytes por banco acrescentariam 21.504 bytes brutos. O total de 29.184 bytes ultrapassa a partição mesmo antes de incluir metadados NVS, perfis e reserva de garbage collection. A avaliação de V4 precisa medir uma representação esparsa e definir como o Save deve se comportar quando o orçamento real não comportar uma nova geração.

A opção auditada para V4 é incorporar as rotas no próprio registro Synth e usar seu CRC e sua identidade de banco/sequência no marcador. A leitura de V4 por firmware anterior deverá ser tratada como incompatível. Antes de qualquer qualificação futura que grave V4, preservar a imagem e a NVS da unidade. O retorno exato ao estado anterior usa o par de backups da mesma unidade. Qualquer flashing exige autorização explícita para o SHA-256 da imagem selecionada.
