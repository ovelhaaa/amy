# PCM5102A no P4 — Gate 1 / bancada

Estado em 2026-11-02: **MANUAL**, montagem ainda não conferida. O operador
localizou os pinos e informou que transferirá o mesmo módulo PCM5102A que
já funciona no S3. Isso não confirma a tensão nominal de módulos comerciais
em geral, nem constitui teste elétrico/analógico no P4.

## Perfil e ligação candidata

Unidade de referência: JC4880P443C_I_W / JC-ESP32P4-M3; revisão de silício
ES 1.3 e Flash 16 MB registradas anteriormente. Perfil ES <3.0, IDF 5.5.2.
Não estender essa identificação a qualquer placa vendida como Guition P4.

| P4 | JP1 no esquema | PCM5102A |
|---|---|---|
| GPIO50 / BCLK | 11 | BCK |
| GPIO51 / WS | 9 | LCK / LRCK |
| GPIO52 / DOUT | 7 | DIN |
| GND | posição marcada GND | GND |
| Alimentação validada do módulo | não definida pelo firmware | VCC |

Conferência documental: [folha 03 do esquema extraído do pacote Guition](https://github.com/ultramcu/guition-jc4880p443c-i-w/blob/master/schematic/03-expand-io_usb-fs-hs_lcd-backlight.webp).
Os números acima foram lidos no desenho, não medidos na unidade. Confirmar
face do conector, marca de pino 1 e continuidade antes de ligar. GPIO50–52
não aparecem no perfil LCD/touch nem nas atribuições SDIO/USB do esquema
consultado. O driver `i2s_std` usa a GPIO matrix; o build não prova sinal físico.
GPIO51/52 eram RX/TX da ponte: **desconectar completamente o enlace P4/S3**.

1. Desligar as placas e o DAC; baixar o ganho do mixer.
2. Registrar/fotografar a montagem S3 que funciona, inclusive alimentação,
   GND e straps. O firmware novo não aciona XSMT por GPIO.
3. Conferir regulador/tensão de entrada e FMT, SCK, XSMT, DEMP e FLT do módulo.
   Manter a configuração de I²S/PLL comprovada na montagem existente.
   Não deduzir VCC pelo nome PCM5102A nem alimentar por GPIO.
   Se XSMT vinha do mute GPIO18 do S3, não basta deixar esse fio solto:
   confirmar a polarização de unmute apropriada ao módulo. O P4 não substitui
   esse quarto sinal nesta etapa; não deixar entrada de controle flutuante.
4. Retirar UART e sinais I²S provenientes do S3; evitar duas saídas no mesmo fio.
5. Ligar os três sinais acima e GND comum. Não conectar 5 V a GPIO.
   Não unir fontes/VBUS pela placa sem validação elétrica.
6. MCLK não é gerado. Conferir a configuração SCK do módulo para PLL de BCK
   no [datasheet TI](https://www.ti.com/lit/ds/symlink/pcm5102a.pdf).
7. Usar as saídas de linha L/R do DAC no mixer. Não usar speaker da Guition.

## Configuração e gravação

Desde a entrega 0.2, `guition-standalone` usa Performance e AMY locais.
O padrão não inicializa I²S: renderiza em RAM e mostra `AUDIO RAM`.
Ver [resultados 0.2](standalone_amy_performance_02_results.md).
O modo diagnóstico anterior continua disponível como fonte separada.

```powershell
$env:PYTHONIOENCODING='utf-8'
$env:PYTHONUTF8='1'
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone -t menuconfig
```

Em `Component config / SMK-P4 standalone PCM5102A bring-up`, somente após
conferir a ligação, habilitar `SMKP4_PCM_WIRING_CONFIRMED`. Para o primeiro teste elétrico,
habilitar também `SMKP4_PCM_DIAGNOSTIC` e deixar
`SMKP4_PCM_TEST_SEQUENCE` desligado: transmite zeros. Os pinos estão centralizados
em Kconfig, restritos aos candidatos 50–52 e precisam ser distintos. Outro grupo
de pinos exige novo perfil de placa e verificação, não edição do driver genérico.

Depois habilitar a sequência audível e recompilar, mantendo o modo diagnóstico. A frequência é configurável
por `SMKP4_PCM_TEST_HZ`, padrão 440 Hz. O pico é 0,05 FS (~−26 dBFS), sem volume
automático da UI. Ao finalizar os testes L/R, desligar `SMKP4_PCM_DIAGNOSTIC`
e `SMKP4_PCM_TEST_SEQUENCE` para tocar AMY pela Performance; manter o self-test
RAM desligado. O modo diagnóstico não habilita controles musicais. O sdkconfig gerado fica em `.pio/build/guition-standalone/`:
defaults não sobrescrevem automaticamente um sdkconfig existente. Conferir
`config/sdkconfig.h` antes da gravação.

Identificar a porta real (COM13 era a unidade de referência), fechar monitores
e gravar somente o P4:

```powershell
C:/.platformio/penv/Scripts/python.exe -m serial.tools.list_ports -v
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone -t upload --upload-port COM13
C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port COM13 --output build/standalone_pcm_physical.log --seconds 60
```

## Aceitação física (MANUAL)

1. Com silêncio digital, conferir ausência de sinal musical, DC anormal e pop
   forte; registrar boot frio separadamente. PCM zero não garante mute analógico.
2. Com analisador/osciloscópio, verificar WS 48 kHz, BCLK esperado 1,536 MHz
   (2 slots × 16 bits), Philips com atraso de um bit, DATA MSB primeiro e sem MCLK.
3. Sequência de 10 s: 2 s silêncio, 2 s esquerda, 2 s direita, 2 s ambos,
   2 s silêncio, repetindo. Confirmar no mixer/RCAs canal correto e 440 Hz,
   sem distorção, inversão de bytes ou ruído inesperado. Rampas de 20 ms em
   cada extremidade sonora; a fase continua entre blocos.
4. Mexer no slider/botão diagnóstico e manter touch ativo por pelo menos 10 min.
   Conferir tela responsiva, heap/maior bloco estáveis e stack com margem.
   O slider continua sendo apenas teste de display.
5. Registrar `frames_written`, `blocks_rendered`, médias/máximos de geração, `render_deadlines`,
   `service_max_us`, `refill_risks`, erros/timeouts e memória. Esperado:
   frames avançando; erros/timeouts/deadlines/riscos sem crescimento em regime.
   Orçamento de render: 5333,33 µs; serviço inclui espera DMA e não é DSP puro.
6. Registrar aprovação auditiva e instrumental separadamente dos logs. O
   driver não oferece contagem exata de underruns DMA: `DMA_underruns=UNMEASURED`.
   `refill_risks` conta intervalos entre serviços >21,33 ms; não prova dropout.
7. Se houver falha I²S, o owner para o canal, publica erro e não faz retry em
   loop. Baixar volume antes de reiniciar; não declarar ausência de pops sem ouvir.

Nenhum ES8311, NS4150, USB Host ou enlace S3 é inicializado pelo novo caminho.
As versões híbridas continuam disponíveis para recuperar a montagem anterior.


## Performance AMY local (0.2, MANUAL no caminho analógico)

Depois de aprovar silêncio/L/R, usar o modo musical e começar com master baixo.
Tocar/soltar C4, arrastar para fora, perder contato, segurar até o limite de10s,
mudar oitava/browser/patch com nota ativa e acionar Panic. Esperado: nenhuma
nota antiga reaparece, releases terminam, nova nota funciona após Panic.
Validar ambos ROM128/135; trocar patch pode cortar notas intencionalmente.
Somente master está habilitado nesta rodada; macros estão indisponíveis.

Registrar janelas do mesmo boot: repouso; nota; acordes do diagnóstico; master
rápido; navegação/troca de patch; Panic. Registrar `cmd_avg/max`, `dsp_avg/max`,
`compute_deadlines`, `dma_avg/max`, `patch_max_us`, latência máxima de comando,
peak/clipped, queue high/overflow, memória e stack. Esperado: sem crescimento de
erros/timeouts/deadlines em regime, heap/stack estáveis, touch responsivo e som
sem falhas percebidas/medidas. Espera DMA não é tempo DSP. Não promover RAM ou
mock LVGL a aprovação física. Confirmar 30min inicial e depois ensaio prolongado.
