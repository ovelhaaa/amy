# SMK-P4 0.16A — Imagens para revisão

| Perfil | Build | Arquivo (B) | Flash PIO (B) | RAM PIO (B) | SHA-256 |
| --- | --- | ---: | ---: | ---: | --- |
| `guition-editor-8` | PASS | 1.338.000 | 1.337.276 | 42.220 | `BA4384BACED8976D06C9422F888722084A886D60A5938B91BEE6CE359A69A1F0` |
| `guition-editor-8-bench` | PASS | 1.439.936 | 1.439.224 | 43.160 | `53CBF2E8B387C2974333DDA5AC5E063860591AB2464BEF164ABAFE8F60AACDE4` |

Os arquivos estão em `smk-p4/.pio/build/<perfil>/firmware.bin`. O perfil
de bancada mantém o cabeamento conhecido do PCM5102A e USB HS0 na Guition
JC4880P433. A RAM PIO reporta ocupação estática da região DRAM0.
A qualificação física permanece **PENDING** e a gravação exige autorização
explícita para a imagem identificada.

## Ensaio disponível

A 0.16A expõe o motor pelo harness host
`smk-p4/tests/run_step_sequencer_016a.ps1`. Ele configura e toca o padrão
pela fila de comandos do Audio Owner. O build da Guition mantém a UI
existente. O ensaio ativo do sequenciador na placa depende de uma rota
de controle na etapa de integração.

Após autorização para a imagem de bancada, registrar hash e capturar
`AUDIO`, `ENGINE`, `ADMISSION` e `PATCH08` com oito vozes, delay SYNC e
trocas de patch. Confirmar stack livre de pelo menos 4.000 B e admissão
de RAM interna de pelo menos 352.256 B. Registrar deadlines de render,
refill, erros de escrita I²S e timeouts. O firmware identifica underruns
de DMA como `UNMEASURED`. Após a integração de controles, repetir com
o sequenciador ativo em 40, 120 e 240 BPM ao lado do Arpeggiator e SMK25.
Comparar onset e gate da saída gravada com o clock esperado e verificar
Stop, Panic e mudança de patch com notas simultâneas.

**GO** para revisão do motor e da integração no Audio Owner.
**PENDING** para aceitação física e para a interface de performance.
