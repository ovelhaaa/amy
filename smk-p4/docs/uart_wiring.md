# UART de bancada — proposta a conferir na unidade

**Pinos identificados/livres e montagem confirmados pelo operador; enlace
UART real confirmado nos logs das duas placas.** Os perfis `*.defaults.link` selecionam os GPIOs
abaixo; os ambientes originais continuam com a ponte desativada.
Esta é uma proposta reversível para o MVP híbrido, não pinagem final do produto.

## Evidência consultada

- [Folha Guition: expansãoJP1](https://github.com/ultramcu/guition-jc4880p443c-i-w/blob/master/schematic/03-expand-io_usb-fs-hs_lcd-backlight.webp):
  GPIO52 emJP1-7, GPIO51 emJP1-9; GND emJP1-5/6. Folha do esquema Guition
  reproduzida pelo mesmo projeto que forneceu o BSP já usado pelo P4SDM.
  A imagem foi inspecionada diretamente nesta sessão.
- [DevKitC-1 v1.1: headerJ1](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html#header-block):
  GPIO4 emJ1-4 e GPIO5 emJ1-5. A documentação v1.0 mantém esses mesmos sinais;
  ainda conferir a serigrafia da DevKitC real, inclusive se for uma variante.
- O perfil atual do SMK-S3 não usaGPIO4/5 para I2S/display/USB; o perfil
  P4 recuperado do P4SDM não usaGPIO51/52 para LCD/touch. A GPIO matrix do
  ESP-IDF permite remapear UART1 para esses GPIOs; o SOC P4 os aceita como
  entrada/saída. Não usar os pinos UART1 nativos17/18 do S3: já pertencem ao áudio.

## Três fios, depois de conferir os rótulos

Desligar a alimentação antes de montar. Identificar pin1 e orientação deJP1
pela placa/esquema; não contar pinos a partir de uma orientação presumida.
Conferir que nenhum outro fio/periférico usa os quatro GPIOs selecionados.

| Origem | Destino | Função |
|---|---|---|
|P4 GPIO52 / JP1-7|S3 GPIO5 / J1-5|P4 TX → S3 RX|
|S3 GPIO4 / J1-4|P4 GPIO51 / JP1-9|S3 TX → P4 RX|
|P4 GND / JP1-5 ou6|S3 G/GND|Referência comum|

Usar somente sinaisTTL3,3V. Não ligar5V/3V3/VIN entre as placas; cada uma
mantém sua alimentação USB própria. Não usar o conectorRS485(A/B) comoUART.
Não usar GPIO35..37 do S3 N16R8 para esta ligação: estão associados à memória.
O console USB/JTAG do P4 e USB-UART do S3 permanecem separados do UART1.

## Configuração, após confirmação física

No P4: `CONFIG_SMK_LINK_UART=1`, TX52, RX51.
No S3: `CONFIG_SMK_LINK_UART=1`, TX4, RX5.
115200 baud,8N1, sem flow control. Os ambientes `guition-link` e `esp32-s3-link`
selecionam esse perfil de bancada após a confirmação do operador. O Kconfig
genérico ainda usaGPIO-1 como placeholder se o perfil não for carregado.
O perfil Performance foi gravado no P4 e o backend S3 na COM8, com hashes
verificados. HELLO/snapshots reais confirmados nos logs de ambas as placas.
Após a troca do cabo, sua USB-UART apareceu na COM8 (CH343) e logs reais foram
recebidos; reverificar a porta antes de qualquer novo upload. A aceitação de
som, controles e reconexão sob notas físicas ainda exige o roteiro musical.

Verificar primeiro S3 tocando sozinho. Depois confirmarHELLO/HELLO_ACK,
snapshots aumentando e identificação real de patch/macros no log do P4.
Para o primeiro teste de perda de comunicação, manter ambas alimentadas e
retirar/recolocar apenasTX/RX, mantendoGND comum. Para reinício do P4, usarRESET
com alimentação mantida. O circuito para desligar uma placa mantendo GPIOs
ativos da outra conectados ainda não foi qualificado; isso requer tratamento
de alimentação/isolamento deI/O no hardware final.
Verificar reconexão e continuidade do teclado USB. Diagnósticos devem separar bytes/frame inválido, overflowRX,
filaTX cheia, rejeição semântica e timeout. O roteiro completo de áudio e
notas virtuais permanece no relatório do MVP.
