# SMK-P4 0.14 — MIDI Learn

Na aba de mapeamento MIDI, selecione LEARN no knob desejado, mova um controle USB e confira o canal e o CC capturados. CONFIRM instala a associação. Se houver conflito, REPLACE transfere a associação para o knob selecionado e CANCEL mantém o perfil anterior. LEARN + CC1 habilita explicitamente a captura da roda de modulação para aquela operação. A função musical de CC1 continua ativa.

| Entrada | Captura |
|---|---|
| CC genérico válido, cabo 0 | Sim, com Learn armado |
| CC1 | Sim, com LEARN + CC1 |
| Sustain e modos de canal | Bloqueada |
| Notas, bend, clock, reset | Fora da fila de Learn |
| Outro cabo ou conexão obsoleta | Descartada |

A captura expira após 10 segundos e é cancelada na mudança da geração USB. O mesmo conjunto aprendido de oito fontes vale nos quatro bancos. A seleção de destinos é determinada pelo banco e pela família do patch. Bindings adicionais fora dos oito slots e compartilhamento de uma fonte permanecem pendentes.
