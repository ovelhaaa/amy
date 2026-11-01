# Build, flash e smoke test — 2026-11-01

- PASS: build e upload pela USB-UART CH343, COM8, 115200 baud.
- PASS: esptool identificou ESP32-S3 revisão v0.2, 16 MB Flash e PSRAM embutida de 8 MB; hashes de bootloader, partições e aplicação verificados; reset por RTS concluído.
- PASS: firmware respondeu a `status`, `audio_status` e `memory` após a gravação.
- PASS: nas leituras de uptime aproximadamente 12,7 s, 32,9 s e 38,2 s, zero erros/timeouts de escrita I²S e zero comandos descartados.
- Observação: 63 lacunas PCM, três violações de deadline e máximo de render de 241.039 us já estavam presentes na primeira leitura; não aumentaram nas leituras posteriores. A captura não cobriu diretamente a partida; a origem desses valores não foi determinada.
- MANUAL: áudio analógico, resposta musical, enumeração/reconexão do controlador, VBUS, mute elétrico e soak sob carga. Nenhum evento MIDI foi recebido na janela; zero vozes e saída PCM silenciosa.

Comando executado na raiz:

```powershell
$env:PYTHONIOENCODING='utf-8'
$env:PYTHONUTF8='1'
C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-devkitc-1 -t upload --upload-port COM8
```

PlatformIO 55.3.36, ESP-IDF 5.5.2; 48 kHz, bloco de 256 frames.
RAM estática reportada: 29.716 bytes; Flash reportada: 1.122.339 bytes.
Arquivo da aplicação: 1.122.736 bytes.
SHA-256 do firmware.bin: `974ede802fcd73a62f1889184d035b020460974e065a7b1a5633823740aa34e8`.
As alterações automáticas do sdkconfig geradas pelo SDK foram removidas do diff.
Não houve alteração de código, arquitetura, pinagem ou formatos persistidos nesta gravação.

O console foi aberto a 115200 baud com DTR/RTS desativados, sem reset adicional;
foram enviados apenas os três comandos de consulta acima. Captura local:
`build/esp32_upload_smoke_20261101.log`. A porta foi fechada após a leitura.

Em repouso, entre 32,9 s e 38,2 s: média de render 831 → 816 us,
RAM interna livre 38.479 bytes, maior bloco interno 21.504 bytes,
PSRAM livre 8.242.372 bytes. Os contadores permaneceram estáveis nessa janela.
Esses dados não certificam continuidade DMA, latência ou uso em palco.

Revisão exata da DevKitC, VBUS, ligação XSMT e saída analógica continuam sujeitos
à confirmação física. Próximo menor passo: executar o procedimento de
[qualificação da Fase 1](phase1_stability_audit.md) com teclado e captura de áudio.
