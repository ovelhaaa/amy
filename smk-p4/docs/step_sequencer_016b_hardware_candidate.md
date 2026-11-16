# SMK-P4 0.16B — Candidato Guition JC4880P433

O build `guition-editor-8` serve para revisão do perfil com as saídas elétricas
desabilitadas. O build `guition-editor-8-bench` usa a configuração de
bancada existente para PCM5102A e USB HS0. O padrão e as configurações do
sequenciador ficam em RAM. A qualificação física permanece **PENDING**.

## Imagens de firmware

| Perfil | Build | Arquivo (B) | Flash PIO (B) | RAM PIO (B) | SHA-256 |
| --- | --- | ---: | ---: | ---: | --- |
| `guition-editor-8` | PASS | 1.345.952 | 1.345.222 | 43.000 | `9E5E89934D8661241255D333172AED830DA950004E9F7AFE9802F8B53ADBDFF2` |
| `guition-editor-8-bench` | PASS | 1.447.856 | 1.447.144 | 43.388 | `FC9EFF61E0ED1415031C1493B8CF8B4697E45BB921BADF2FB7ADE017924AC011` |

Os binários estão em `smk-p4/.pio/build/<perfil>/firmware.bin`. Em
relação à 0.16A, o arquivo cresceu 7.952 B no editor e 7.920 B no bench.
A auditoria de configuração Blueprint passou nos dois perfis.

## Roteiro de bancada

Após autorização explícita para a imagem de bancada identificada pelo
hash, confirmar a revisão da placa e o cabeamento de referência da 0.16A.
Registrar o hash da imagem, a placa, o módulo DAC e a ligação USB antes de
iniciar. Preservar a NVS e a tabela de partições.

1. Inicializar a Guition com o SMK25 e a saída estéreo conectados. Conferir
   admissão de RAM e inicialização de USB, áudio e display pelo log.
2. Abrir **SEQ** no banco MAIN. Conferir transporte parado, 16 pausas e
   operação de toque nas bordas da grade. Carregar **LOAD DEMO PATTERN** e
   conferir o preenchimento e as pausas.
3. Pressionar **PLAY**. Conferir início audível na fronteira do clock e
   deslocamento do playhead, inclusive nas pausas.
4. Editar uma nota durante a reprodução. Alterar velocity e gate de um
   passo ativo e escutar a mudança quando ele voltar. Alternar Enabled.
5. Alterar o BPM global. Alterar Length, Division e Swing durante a
   execução. Testar 1/4 → 1/16, 1/16 → 1/32 e binária ↔ tercina em swing
   50% e 75%. Conferir a duração do passo já em curso e os intervalos seguintes.
6. Ativar Scale Quantize com uma nota fora da escala. Transpor ±12
   semitons e conferir a altura soada e a nota armazenada na grade.
7. Manter notas do SMK25 enquanto o padrão toca. Ativar Arpeggiator e
   Delay SYNC. Navegar para FX e voltar a SEQ durante a reprodução.
8. Executar **RESTART** com a sequência ativa e conferir retorno ao passo
   1. Executar **STOP** com notas do SMK25 mantidas e conferir que elas
   continuam. Repetir com vozes ocupadas, observando `voice_drops`.
9. Trocar de patch com a sequência ativa. Conferir que o transporte para,
   o padrão continua em RAM e o novo patch pode tocá-lo. Executar Panic em
   outra tela com arpejo, sequência e notas manuais ativos e conferir silêncio.
10. Repetir em 40, 120 e 240 BPM. Capturar `AUDIO`, `ENGINE`, `OUTPUT08`,
    `ADMISSION`, `SEQ016B` e `UI_BLUEPRINT` com a sequência parada e ativa.
    Registrar stack, heap LVGL, RAM, PSRAM, deadlines, erros de escrita
    I²S e timeouts nos dois casos. Comparar com a 0.16A sob a mesma carga.
11. Gravar a saída estéreo durante as transições para verificar continuidade
    I²S, onsets, gates e ausência de notas presas. A contagem de underruns
    DMA continua marcada como `UNMEASURED` pelo firmware.

A aceitação de bancada exige toque utilizável, áudio contínuo e
reproduzível nos dois canais, recuperação após Panic e contadores sem
crescimento inesperado durante edição. Registrar os resultados físicos
separadamente da evidência host.
