# SMK-P4 0.7 — registro físico e estado de qualificação

2026-11-05, America/Sao_Paulo. Operador autorizou: “gravação na placa autorizada.
Ao final, faça commit e push”. A autorização abrange a candidata corrente desta
tarefa; cada revisão foi vinculada a hash/arquivo antes da escrita. Nenhum novo
pinout foi inventado: montagem0.6 PCM50/51/52, USB HS0, mesma Guition.

## Preflight e preservação — PASS

COM13,303A:1001, serial/MAC80:F1:B2:D3:CC:86. ROM confirmou ESP32-P4 rev1.3 e
16MB. Backup integral novo16777216B em
`build/library07_preflight_f42086c0c91c460aa448ad1c7adf2575/flash_before_library07_usbreset.bin`.
SHA256 `6615596487e8290c746d234edb86f06a0db2a6aa155af2e85cf72b17fdffc7ff`.
Leitura95,3s; bootloader/NVS/tabela não escritos. Imagem instalada extraída:
`887cee5de9c0f2b6da71caa691e0c5ed34a7e563ba9cde0916cb809eae5b7af7`, igual ao
bundle8 qualificado0.6. Rollback local preservado. Tabela de partições candidata
byte-idêntica à instalada; somente aplicação0x10000/partição0x300000 foi substituída.
Backups/bundles antigos permanecem intocados.

## Primeira candidata — FAIL seguro preservado

SHA256 `0b96997037410ee487d9984b342e40d8f55312a9d964346418d587b2688aa45c`,
1203456B. Escrita app-only com “Hash of data verified”. Boot recusou admissão:
engine ready0, AUDIO FAIL/ESP_ERR_NO_MEM, nenhum frame/engine parcial nem OOM.
A estimativa incremental exigia401408B, acima do disponível já no owner.

Rollback0.6 regravado e verificado; captura confirmou I²S produzindo, ready1,
render máximo289us em repouso, deadlines/OOM0. Uma tentativa do utilitário de
rollback foi interrompida por UnicodeEncodeError/cp1252 ao imprimir progresso;
repetição com PYTHONUTF8=1/PYTHONIOENCODING=utf-8 passou. Essa falha do host é
preservada nos logs, não atribuída ao firmware. Leitura inicial do backup foi
interrompida enquanto stdout estava bufferizado; nova leitura completa passou.

## Revisão do orçamento — startup PASS

SHA256 `37be3adad7dfcac9272328e5bdbf1785bf602af1eabd88cd9f868b6682cb2f38`,
1203696B, arquivo `candidate_07_eight_v2.bin`, autorização_preflight_v2.json.
Piso histórico, reservas pós-begin, maior bloco inicial e stack mantidos; novo
orçamento total300KiB testado contra pico host286635B. Limiar372736B, maior bloco
inicial196608B; pós-begin>=65536B e maior>=32768B. Firmware/drivers/clock/gain/AMY
inalterados fora do orçamento e snapshot de diagnóstico de boot.

Escrita app-only6,4s, hash verificado. Captura28s sem reset extra:

|Métrica|Startup em repouso|
|---|---:|
|Heap antes de begin / maior bloco|388259 /245760B|
|Heap após begin / maior bloco|111027 /38912B|
|Delta do engine observado|277232B|
|Heap após I²S/UI / maior bloco|95479 /31744B|
|Vozes/osciladores/instrumentos|8/256/32|
|Engine / áudio|ready1 / I2S_PRODUCING|
|Render máximo|345us|
|Render/compute deadlines, clipping, OOM, erros I²S|0|
|Stack livre áudio/UI/USB ausente|7348/3424/5520B|
|USB|state1, aguardando controlador|

O maior bloco31KiB depois da UI é uma limitação explícita; as guardas existentes
são aplicadas antes de I²S/USB/UI. É preciso observar a margem com endpoints USB,
trocas e navegação. Não aprovar Gate E nem oito notas por estes dados de repouso.

## Smoke e janela prolongada — PENDENTES

Operador foi solicitado a conectar SMK25 e testar128/135/138/142, oito notas,
rejeição da nona, soltura, sustain, reataque, bend/CC1, troca, Panic e tela/toque.
Captura `smoke07_v2.log` iniciada, sem enviar notas/comandos ou fabricar ações.
Somente após smoke confirmado iniciar>=30min, incluindo quatro hot-plugs,
navegação e oito notas amostradas em cada patch. Analisador agora aceita
`--voices 8 --patches 128 135 138 142 --require-full-patches`, mantendo default
legado128/135. Onze regressões do analisador PASS.

Audição, margem DSP sob carga, endpoints/hot-plug e janela>=30min: MANUAL/NOT RUN
até serem executados e cruzados com ações reais do operador. Não transferir a
aprovação0.6 para esta imagem.
