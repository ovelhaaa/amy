# SMK-P4 0.14.1 — Manual e Applied

O Routing Editor mostra o valor manual e o valor aplicado do destino selecionado. Ambos vêm do snapshot do owner. O valor manual é a base persistente. O valor aplicado é o controle entregue ao DSP depois da composição.

**LEGACY** indica que o algoritmo histórico alterou a base daquele destino. **M1** a **M8** indicam macros com contribuição não nula após o limite individual. A soma final ainda pode sofrer clamping ou cancelamento entre contribuições. **COMPARE** indica a referência ROM e **FX BYPASS** sinaliza o bypass do barramento. A leitura mostra o controle de parâmetros. O sinal modulado por LFO, CC1 e outros processos do DSP varia dentro do bloco e está fora dessa leitura.

A edição manual conserva as posições de macros. O owner recompõe o resultado a partir da nova base. A UI rearma Pickup após a confirmação de rotas. Para MIDI, a posição de captura é calculada a partir do valor manual.
