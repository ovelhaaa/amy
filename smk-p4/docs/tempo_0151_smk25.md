# SMK25 V2 · Legacy S3

A fábrica continua com os mesmos dois bancos físicos e os mesmos CCs. Em FREE, K2 do banco B controla o tempo manual com Pickup. Em SYNC, K2 usa a faixa de divisões válidas no BPM confirmado. Em 40–44 BPM, começa em 1/8. Em 45–59 BPM, começa em 1/8 pontuada. A partir de 60 BPM, todas as divisões são selecionáveis. Um ajuste de dois valores MIDI além do limiar reduz oscilações na fronteira entre divisões.

O Pickup é rearmado ao trocar o modo ou quando mudar a faixa de divisões válidas. Um destino personalizado conserva precedência sobre o comportamento de fábrica. `perf.bpm` é um destino adicional no fim da tabela V2, sem renumerar IDs antigos e usando o MIDI Learn existente. A filtragem de CCs reservados do perfil continua em vigor.
