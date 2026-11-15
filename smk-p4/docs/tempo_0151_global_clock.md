# Clock global

O owner mantém o BPM inteiro em `tempo::Clock`. A cada bloco AMY concluído soma `frames × BPM × 24 × 25` à fase. Um tick corresponde a `60 × 48000 × 25` unidades. O snapshot usa cópias coerentes de fase, BPM e revisão. Mudanças de BPM preservam a fase, e diferenças de fase sem sinal toleram wrap.

`perf.bpm` usa o comando versionado anterior de BPM, com a mesma confirmação pelo owner. `arp.bpm` é alias na entrada. O clock é interno, segue com o ARP desligado e mantém a fase em Stop, Panic e troca de patch. A fase fica estacionária enquanto o runtime devolve silêncio sem renderizar (por exemplo na pausa de NVS), retomando do mesmo ponto.

Tap Tempo usa intervalos monotônicos de 250–1500 ms e a mediana de três a quatro amostras consistentes. Pausas acima de 1500 ms reiniciam a amostragem. O resultado passa pelo mesmo comando de BPM.
