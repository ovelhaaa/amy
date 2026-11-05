# SMK-P4 0.6 — qualificação física de oito vozes

2026-11-05. PASS na mesma Guition JC-ESP32P4-M3 qualificada em seis vozes.
Imagem instalada: `887cee5de9c0f2b6da71caa691e0c5ed34a7e563ba9cde0916cb809eae5b7af7`.
Fonte do binário `74a586f4`, ambiente guition-standalone-blueprint-8-bench.
Gravação explicitamente autorizada, somente aplicação em `0x10000`; hash verificado
pelo esptool. PCM5102A BCLK=50/WS=51/DATA=52, USB Host HS0, VBUS/proteções
reconfirmados sem mudanças. COM13/serial80:F1:B2:D3:CC:86.

## Validação e comportamento musical

- PASS: processo de captura concluído, arquivo bruto com 439906 bytes.
  Todas as séries com pelo menos 1874,48 s contínuos;
  maior intervalo de 5,41 s, sem reboot/relógio regressivo.
- PASS: oito vozes amostradas nos dois patches (6 amostras em 128,
  6 em 135); nenhuma acima de oito, nenhuma ativa ao final.
- PASS: 2383 novos eventos MIDI, quatro desconexões/reconexões,
  host conectado no fim, 39 transições de tela e rejeições
  de capacidade novas. Filas musicais sem overflow/drops/parse errors.
- MANUAL/PASS: operador confirmou oito notas, nona rejeitada sem cortar as oito,
  soltura/Panic, VX0/8 e VX8/8, patches 128/135, sustain, bend, modulação,
  acordes/retrigger, troca de patch com notas ativas, hot-plug só SMK25,
  coexistência Touch/USB e navegação. Confirmou nova rodada de reataque antes
  de acabar release com sustain/modulação, sem novos estalos, distorção ou
  notas presas. Confirmações em operator_confirmations.json.
- PASS: render/compute deadlines0, refill risks0, write errors0, timeouts0,
  clipping0, OOM0, USB errors0 e touch/flush errors0 durante toda a janela.
- PASS: sem queda progressiva observada de memória; heap/PSRAM voltaram aos
  valores iniciais após USB/carga. A captura inclui repouso e carga musical,
  não representa 30 min de acorde cheio nem todos os estímulos possíveis.

## Tempos e memória observados

|Métrica|Oito vozes|
|---|---:|
|Período por bloco256/48000|5333,33us|
|Render máximo acumulado|4234us|
|Margem do render máximo|20.6%|
|Render médio acumulado final (inclui repouso)|324us|
|Comandos média/máximo|6/1708us|
|Troca de patch máxima|401us|
|DMA espera média/máximo|4889/5198us|
|Serviço máximo incluindo espera DMA|5689us|
|Idade máxima de evento na fila|17ms|
|Interno livre mínimo/final|169075/169075B|
|Maior bloco interno mínimo/final|94208/94208B|
|PSRAM livre mínima|31176952B|
|Maior bloco PSRAM mínimo|30932992B|
|Stack livre mínimo áudio/USB/UI|7440/4144/3332B|
|LVGL livre mínimo/final|79412/95432B|
|LVGL maior bloco mínimo/final|75004/90328B|
|LVGL pico usado/fragmentação máxima|50184B/19%|
|DRAM0+DRAM1 data/BSS / IRAM|169904/101888B|
|Aplicação|1202400B|

Máximos de comandos e DSP são separados e não devem ser somados como se fossem
uma mesma amostra. O prazo compute inclui housekeeping e exclui só a espera I²S;
zero violações foi observado, mas o máximo combinado não é exposto. A margem
numérica acima é de render, não CPU total. Serviço com DMA pode exceder o
período sem ser violação compute. Média acumulada inclui períodos de repouso.

## Resultados negativos preservados e limites

O teste do analisador com dois prefixos DISPLAY_TEST concatenados falhou antes
da correção `4d2d69db`. Uma linha parcial antiga retida no USB atribuía horário
errado ao próximo registro completo. O log bruto foi preservado. O parser agora
isola cada payload e conserva todos os registros completos, inclusive erros.
PASS nos 10 testes após correção, incluindo erro seguido de registro limpo;
relatório de seis anterior permaneceu idêntico. Nenhum firmware foi mudado por isso.

Descartes MIDI_TRACE são do diagnóstico limitado; dropped musical ficou0.
NOT RUN/não medidos: underruns físicos, CPU total, latência tecla→saída e
máximo combinado de computação. Teste de fila cheia host não equivale à sua
injeção física na placa; não declarar aprovação universal de qualquer pressão
de controles. As guardas de memória não corrigem todas as alocações legadas
sem NULL check de amy_start. Audição é a confirmação do operador, não captura
analógica nem A/B físico controlado. Estímulos manuais6/8 não são idênticos;
não atribuir diferenças de máximo a otimização ou calcular escalabilidade CPU.

## Decisão e próximo menor passo

Oito vozes: PASS nos gates musicais e de hardware para esta montagem, dois
patches e configuração congelada. Recomendação musical nesta bancada: oito,
com seis como rollback já qualificado e de maior margem observada. O pico de
render de 4234 us deixa cerca de 20,6% do período; a recomendação considera também os
reataques repetidos, audição, memória, UI/USB e ausência de deadlines, não só
ficar abaixo de 5333 us. Não é prova de margem para adicionar DSP/FX/patches.
O default genérico seguro continua quatro; o candidato audível selecionado
explicitamente é oito, e não houve mudança dinâmica de capacidade.
Próximo menor passo: uso musical prolongado da imagem congelada; novas cargas
ou funcionalidades exigem nova medição. Não há trabalho de firmware pendente
para os critérios executados desta etapa.

## Comandos e artefatos

PASS: build nativo, auditoria musical --voices8 e esptool image-info, registrados
em [candidato8](polyphony_06_eight_bench_candidate.md). PASS:
`python -m unittest discover -s smk-p4/tests -p test_standalone_capture_report.py -v` (10 testes).
PASS: `python smk-p4/tests/standalone_capture_report.py build/polyphony06_flash8_94617e5a297a/eight_voice_qualification_window.log --output build/polyphony06_flash8_94617e5a297a/eight_voice_qualification_report.json --min-seconds 1800 --hotplugs 4 --voices 8`.
Raw/JSON/flash/autorizações: build/polyphony06_flash8_94617e5a297a/.
Pacote final: build/polyphony06_qualified8_887cee5de9c0.zip, com manifesto,
ELF/mapa/configuração/logs/WAVs em bundle de software, evidência6/8, diff e
rollback6/0.5. Backup integral16MiB é distinto da aplicação em0x10000.
