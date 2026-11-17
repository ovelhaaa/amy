# SMK-P4 0.17A — candidato de bancada Guition JC4880P433

Qualificação física **PENDING / MANUAL**. Referência elétrica e roteiro
de boot na [0.16C](step_sequencer_016c_hardware_candidate.md). Confirmar
revisão Guition, alimentação VBUS, cabo do SMK25 e PCM5102A da unidade.

## Perfis

O perfil guition-editor-8 mantém os gates elétricos de revisão.
Guition-editor-8-bench é o perfil de bancada. A imagem de aplicação
fica em smk-p4/.pio/build/<perfil>/firmware.bin.

Para validação autorizada na placa, preparar backup dos dados e da
aplicação anterior. Verificar endereço da aplicação, hash da imagem e
mapa da placa. Preservar NVS, bootloader e partições.

## Ensaio

1. Abrir SEQ > PATTERNS. Confirmar 16 slots e seus estados. Gravar dois
   padrões distintos, renomear um, reiniciar e carregar ambos. Conferir
   nome, passos, gate, velocity, comprimento, divisão, swing,
   quantização e transposição manual. Verificar presets V4 existentes.
2. Em execução, tentar SAVE, RENAME e DELETE. A UI deve pedir STOP e
   liberação de notas. Repetir parado e conferir REQUESTED, WRITING e
   CONFIRMED. Editar um passo, tentar carregar e cancelar para confirmar
   que a edição permanece. Repetir com DISCARD & LOAD e SAVE & LOAD.
3. Repetir IMMEDIATE e NEXT BAR com 16, 8 e 7 passos em 1/16, 12 em 1/8T,
   swing e mudança de BPM. No próximo limite 4/4, monitorar troca no
   passo 1 e liberação do grupo antigo. Com substituição de fila,
   confirmar que só o último pedido válido é executado.
4. Repetir com duas notas diretas, sustain, Arp, FX e Performance Lock.
   Conferir que a troca preserva os outros domínios. Durante NEXT BAR,
   testar STOP, Panic, troca de patch e desconexão USB. Conferir
   cancelamento e ausência de vozes sequenciadas presas.
5. Em MIDI / REC, tocar LIVE com canal ANY e verificar o aviso.
   Selecionar um canal dedicado e manter uma tecla comum pressionada
   durante a transição. Conferir sua liberação original e ausência de
   voz direta nas teclas capturadas para transposição.
6. Em unidade de ensaio, simular interrupção elétrica durante atualização
   de slot. Após reiniciar, conferir registro completo anterior ou novo,
   outros slots e namespaces intactos. Repetir com NVS perto da capacidade.
7. Comparar com 0.16C em repouso e sob carga musical. Registrar heap
   LVGL, heap NVS, stack mínimo das tarefas, tempo de comando/render,
   riscos de refill, falhas I2S e impacto audível de troca e escrita.
   O prazo por bloco é 5,33 ms.

## Binários verificados

PlatformIO 6.1.18, ESP-IDF 5.5.2. Comparação de RAM e Flash com
a [0.16C](step_sequencer_016c_hardware_candidate.md).

| Perfil | Build | Arquivo (B) | RAM PIO (B) | Flash PIO (B) | SHA-256 |
| --- | --- | ---: | ---: | ---: | --- |
| guition-editor-8 | PASS | 1.405.392 | 45.424 (+1.412) | 1.404.670 (+14.554) | `692A3AD5FFC0B5DE7BE86D731BA4C3894F2307D43C47A015A4AF5D4B3D6D2EBA` |
| guition-editor-8-bench | PASS | 1.507.312 | 46.360 (+2.508) | 1.506.588 (+14.560) | `790A76718915AAA9A00B7A9A19B6E8C81FC87BEB071AAFAD9574004130C61706` |

As cópias locais e seu manifesto SHA256SUMS estão em
build/pattern017a-release/. Nenhuma gravação de placa foi executada.
