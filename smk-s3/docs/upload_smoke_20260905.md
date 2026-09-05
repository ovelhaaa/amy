# Gravação e verificação inicial — 2026-09-05

Firmware da etapa [AMY com proprietário único](amy_ownership.md), gravado na
COM8 e observado pelo console UART a 115200 baud.

## Resultado

- PASS: upload, verificação de hash pela ferramenta e reinício por RTS.
- PASS: aplicação responde a `status`, `audio_status` e `memory`.
- PASS: zero erros de escrita I²S e zero descartes de comandos nas leituras.
- PASS: o contador de lacunas PCM permaneceu em 59 entre aproximadamente
  43,3 e 53,4 segundos de uptime; não cresceu nessa janela em repouso.
- MANUAL: som analógico, resposta às teclas, reconexão USB e carga musical.

O esptool identificou ESP32-S3 revisão v0.2, 16 MB de Flash e 8 MB de PSRAM.
Isso não identifica a revisão exata da placa DevKitC nem confirma a fiação.

## Medidas observadas

| Campo | Leitura |
| --- | ---: |
| RAM interna livre | 55.755 bytes |
| Maior bloco interno livre | 31.744 bytes |
| PSRAM livre | 8.315.228 bytes |
| Máximo acumulado de comandos + render | 167.035 us |
| Média observada | 416–417 us |
| Maior espera acumulada de comando | 152.564 us |
| Ocupação máxima da fila | 17 comandos |
| Lacunas PCM acumuladas | 59 |
| Erros I²S / descartes de comandos | 0 / 0 |
| Eventos MIDI recebidos | 0 |

O pico de 167 ms e as 59 lacunas já existiam na primeira leitura. Como a
captura começou depois da partida, sua origem não foi medida diretamente.
Não interpretar o contador estável por dez segundos como certificação de
áudio contínuo sob carga. Nenhuma nota foi injetada pelo console neste teste.

O snapshot geral reportou `PSRAMSize=0`, embora esptool e heap livre confirmem
PSRAM presente. Esse campo do diagnóstico permanece uma inconsistência a
corrigir; não houve falha de detecção de heap PSRAM nesta observação.

## Reprodução

```powershell
$env:PYTHONIOENCODING='utf-8'
$env:PYTHONUTF8='1'
C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-devkitc-1 -t upload
```

A primeira tentativa teve erro `UnicodeEncodeError` na saída de progresso do
PlatformIO e não foi considerada concluída. Seus processos foram encerrados
e o upload completo foi repetido com UTF-8, terminando com sucesso.

Arquivo da aplicação: 919.920 bytes. SHA-256 do `firmware.bin`:
`a22d1ed086d17c8906022df1778b18b3fe8067190ba3bd3957c83fcc3f848fb3`.

Log bruto local: `build/esp32_upload_smoke_20260905.log`. A porta serial foi
fechada após a captura e está disponível para os testes do usuário.
