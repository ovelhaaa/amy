# SMK-P4 0.16C — candidato de bancada Guition JC4880P433

## Perfis e instalação

`guition-editor-8` é a revisão com gates elétricos desabilitados.
`guition-editor-8-bench` usa o perfil elétrico já documentado na
[0.16B](step_sequencer_016b_hardware_candidate.md). A Guition de referência
é ESP32-P4 rev. 1.3, com USB HS0 e PCM5102A em BCLK/WS/DATA 50/51/52.
Confirmar placa, alimentação VBUS, tela e saída estéreo na unidade sob
ensaio. Esta versão não muda o pinout.

As imagens são geradas em `smk-p4/.pio/build/<perfil>/firmware.bin`.
Os hashes abaixo identificam as aplicações de revisão.

| Perfil | Build | Arquivo (B) | Flash PIO (B) | RAM PIO (B) | SHA-256 |
| --- | --- | ---: | ---: | ---: | --- |
| `guition-editor-8` | PASS | 1.390.848 | 1.390.116 | 44.012 | `1657AA4ED397F498EBBBFC1543237F18741DC615D779D45AC0AE374A65131FE2` |
| `guition-editor-8-bench` | PASS | 1.492.752 | 1.492.028 | 43.852 | `57697280AB71AD87464A6CEBAB2569DC5C6CDCA376027A080B7E9AE01EA9D573` |

O firmware e o manifesto SHA-256 foram preparados em
`build/step016c-release/` no checkout de desenvolvimento.

Qualificação física **PENDING**. Após autorização de gravação para o
hash de bancada, registrar revisão da placa, cabo USB, DAC e ligação de
áudio. Gravar somente a aplicação na partição que o mapa atual confirmar
para esta unidade. A referência 0.16B usou `0x10000`; preservar NVS,
presets, bootloader e tabela de partições. Validar o hash por leitura de
volta e manter disponível a imagem de referência do banco antes do ensaio.

## Roteiro

1. Abrir o log serial, ligar a Guition com o SMK25 e conectar a saída
   estéreo. Confirmar admissão de RAM, identificação do dispositivo,
   inicialização de áudio e execução I²S. Na aba SEQ, abrir MIDI / REC,
   selecionar NORMAL PLAY e manter LIVE TRANSPOSE OFF. Tocar e soltar
   acordes, variar a força, usar bend, modulação e sustain. Conferir
   silêncio após soltar o pedal e nenhuma alteração na grade.
2. Selecionar o passo 4 e escolher STEP INPUT. Tocar C4 com velocity
   suave e depois D4 mais forte. Confirmar substituição no passo 4, sem
   avanço. Com AUDITION OFF, só o padrão é editado. Ativar AUDITION,
   repetir e confirmar que a tecla também soa pelo fluxo musical.
3. Ajustar Length para 4, escolher o passo 3, selecionar STEP RECORD e
   ajustar RECORD GATE para 65%. Tocar quatro notas e confirmar a ordem
   3, 4, 1, 2. Manter uma tecla pressionada, tocar outra e confirmar um
   passo por Note On novo. Inserir REST / NEXT e conferir o avanço.
   Abrir o editor pelo nome do passo e testar semitom, oitava, velocity,
   gate e ENABLE / REST.
4. Pressionar PLAY. Conferir o áudio e o playhead no primeiro ciclo.
   Manter o padrão em execução e ajustar swing e divisão. O gate em
   curso e o próximo onset já agendado devem manter seus prazos.
5. Em MIDI / REC, selecionar o canal de transposição que o perfil MIDI
   real permitir. Com referência 60 e manual em 0, tocar C4, D4 e A3
   nesse canal. O offset deve mostrar 0, +2 e −3. A próxima nota da
   sequência muda de altura sem reiniciar o playhead. As teclas de
   transposição ficam sem voz direta. Somar transposição manual de +2,
   desativar Live e conferir retorno a +2.
6. Alterar escala e tônica no Harmony Assist com SCALE QUANTIZE ativo.
   Confirmar uma única voz para a nota sequenciada, preservação da nota
   na grade e liberação do gate anterior. Tocar junto em outro canal
   com o fluxo convencional e em seguida com Arpeggiator ativo. Ativar
   Delay SYNC, alterar o BPM global e conferir os três prazos musicais.
7. Enquanto segura uma tecla convencional, alterar modo, ativar Live
   em outro canal e soltar a tecla. Ela deve obedecer à liberação
   original. Repetir com sustain. Executar STOP, mantendo outra tecla
   convencional pressionada. A sequência deve parar imediatamente e
   a voz convencional deve continuar até a liberação.
8. Com sequência, Arp e notas mantidas, executar PANIC em outra tela.
   Confirmar silêncio, retorno a NORMAL PLAY e padrão preservado.
   Repetir durante STEP RECORD, primeiro com Panic e depois com troca
   de patch. Confirmar que o padrão já confirmado continua em RAM.
9. Desconectar o SMK25 durante a execução com sustain e Live ativos.
   Confirmar parada da sequência, liberação de Arp/USB, latch zerado e
   status de desconexão. Reconectar em seguida, confirmar SMK25 OK e
   tocar sem reiniciar a Guition. Conferir os atalhos originais do
   perfil e repeat com conexão e desconexão rápidas.
10. Comparar com a 0.16B sob a mesma carga e mesmo BPM, em repouso e com
    sete vozes diretas mais sequência, lote de acordes, Arp e FX.
    Guardar `AUDIO`, `ENGINE`, `ADMISSION`, `OUTPUT08`, `SEQ016C`,
    `USB` e `UI_BLUEPRINT`. Registrar RAM interna livre e maior bloco,
    PSRAM, heap LVGL, low-water de stack, tempo máximo/médio de comando
    e render, deadlines, timeouts, riscos de refill e erros I²S.
    A 48 kHz/256 frames, o orçamento do bloco é 5,33 ms. Comparar os
    contadores antes e depois e registrar falhas audíveis separadamente.
    O contador de underruns de DMA ainda aparece como `UNMEASURED`.
