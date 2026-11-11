# SMK-P4 0.14 — perfis e bancos

O perfil padrão tem oito fontes não atribuídas. O perfil salvo registra cabo, canal, número de CC e banco ativo. A versão 1 usa um blob de 36 bytes com CRC no namespace NVS perf014midi. SAVE PROFILE solicita a barreira de silêncio existente e confirma o readback antes de informar sucesso. Registro ausente, inválido ou incompatível resulta no perfil padrão em RAM.

| Banco | Destinos |
|---|---|
| A | Oito macros |
| B | Osciladores e níveis no Juno; profundidade, feedback, nível e ratio do operador selecionado no DX7 |
| C | Filtro e envelopes no Juno; envelope e sensibilidade do operador selecionado e LFO no DX7 |
| D | Chorus Depth, Delay Time/Feedback/Mix, Reverb Size/Mix e Tilt Tone |

Slots sem destino compatível mostram N/A. Os bancos mudam pela touchscreen. A mensagem do KNOB-B e a identificação física do SMK25 V2 continuam pendentes de captura. As fontes são configuráveis por Learn e os destinos dos bancos seguem a tabela de fábrica.
