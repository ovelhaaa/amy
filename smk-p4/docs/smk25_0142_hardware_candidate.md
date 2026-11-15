# Candidatos Guition SMK-P4 0.14.2

Fonte de firmware compilada em 0e34b2584ea966a5b61739ff4c4ab7df287e90ba, sobre a referência solicitada dc5fb44742ba68586478842007aa33075bf52599. Os commits de testes e documentação posteriores não alteram a imagem.

| Perfil | Bytes da imagem | SHA-256 | Flash reportada | RAM estática reportada |
|---|---:|---|---:|---:|
| guition-editor-8 | 1.310.096 | 09b0fc1c13ea3e951cec2d5a0f4bd7b4d760cf67fdd01e7eb7d745a8f6511ce2 | 1.309.364 B | 41.396 B |
| guition-editor-8-bench | 1.411.872 | aa4d6fdaee8043d2fe438281d5babb254c47e4e3011a4f1d9a6b7e46340ea3db | 1.411.154 B | 41.780 B |

Imagens locais em smk-p4/.pio/build/<perfil>/firmware.bin. Contra a imagem corrigida documentada da 0.14.1, a diferença de RAM estática reportada é +296 B em ambos os perfis. As imagens crescem +9.232 B (editor) e +9.968 B (bench). A comparação usa os números publicados no candidato 0.14.1 com correção de RAM. A guarda de admissão de 352.256 B, a alocação do backend em PSRAM e a configuração de 48 kHz, 256 frames e oito vozes continuam no build. Flash de 16 MB e PSRAM Hex a 200 MHz são os parâmetros compilados. O perfil bench usa a configuração elétrica existente, incluindo I²S 50/51/52 e USB Host 0.

## Qualificação física — MANUAL

A instalação do candidato bench depende de autorização explícita para seu SHA-256. Registrar a imagem e a NVS da unidade antes do ensaio. Usar o SMK25 V2 com sua programação atual e a montagem Guition/PCM5102A já confirmada.

1. Capturar o log serial da Guition. Conferir versão 0.14.2, oito vozes, admissão >= 352.256 B livres de RAM interna, maior bloco de admissão >= 196.608 B, owner ativo, I²S produzindo e USB conectado com VID/PID 4353:4B4D. Registrar maiores blocos livres, RAM/PSRAM efetivas, stack livre do owner e da task USB.
2. Tocar teclas, acordes e oitava completa. Testar Note Off, Sustain, Pitch Bend e Mod Wheel. Confirmar saída estéreo e zero notas presas ao soltar sustain, tocar Stop e desconectar com um acorde ativo. Reconectar e tocar novamente sem reiniciar.
3. Mover K1–K8 em A e depois em B. Conferir na tela os CCs, os oito destinos de cada banco e o Pickup. Alternar o banco físico e a visualização da tela. Trocar de patch e confirmar que a primeira posição física distante não altera o parâmetro.
4. Acionar pads A e B com o teclado musical em uso. Confirmar que só as mensagens de pad CH10 são interceptadas. Verificar DOWN/UP, velocity, confirmação de patch sujo, Home, páginas e Learn. Pressionar B8 por 0,6 s e por 1,3 s. O painel deve aparecer após soltar apenas no segundo caso. Repetir desconectando antes de soltar. Registrar a confiabilidade de Note Off.
5. Segurar uma nota de toque enquanto uma nota USB está sustentada. Confirmar que Stop encerra a nota USB e conserva a de toque. Acionar Stop duas vezes com soltura entre pressões. Conferir a sequência de valores de CC115. Conferir Play e Rec como reservados.
6. Selecionar um destino próprio, salvar o perfil explicitamente e reiniciar. Conferir os 16 bindings, os bancos e o destino próprio. Testar o cancelamento da restauração por outra ação e a restauração confirmada. Conferir que o perfil salvo pode ser recuperado até um novo Save Profile.
7. Sustentar oito vozes com FX e alternar pads e knobs por 60 s. Registrar render médio/máximo, latência máxima dos comandos MIDI, stack, heap, maior bloco, filas, drops e deadlines. Esperado em uso normal: zero OOM, deadlines, erros de escrita, overflows e notas presas. Registrar a percepção de transientes e interrupções no áudio estéreo. O contador físico de underrun ainda não é instrumentado.

A admissão, a latência, as condições de áudio físico e a RAM/PSRAM em execução deste candidato permanecem pendentes. A captura histórica de 358.819 B livres na admissão, render médio de 219 µs e máximo de 362 µs pertence à 0.14.1.
