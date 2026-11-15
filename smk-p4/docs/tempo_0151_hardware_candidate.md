# Qualificação Guition SMK-P4 0.15.1

## Imagens compiladas e auditadas · PASS

Fonte do firmware no commit `9e6cd7bc4773ffc64931961de2c708b4dbf8599d`.

| Perfil | firmware.bin | Flash PlatformIO | RAM estática PlatformIO | SHA-256 |
|---|---:|---:|---:|---|
| guition-editor-8 | 1.329.968 B | 1.329.244 B | 42.112 B | `63fd6a021092e1a3ffa3b92ebde1fbe76492b24569b50f32688faf94e314ad23` |
| guition-editor-8-bench | 1.431.904 B | 1.431.182 B | 42.496 B | `5ae85ed002e8291685ebfa6e583becc53246ee67bb17d1bc6425c2a660ca54b8` |

Arquivos locais em `smk-p4/.pio/build/<perfil>/firmware.bin`. Flash de
16 MB, PSRAM Hex a 200 MHz e I²S Philips de 16 bits, estéreo, com DMA de
4 × 256 frames. As guardas de admissão e as partições mantêm os valores 0.15.
Os dois binários cresceram 10.592 B. A RAM reportada pelo PlatformIO cresceu
760 B no editor e 756 B no bench. A seção adicional `.dram1.bss` mede
138.928 B e 138.968 B, respectivamente, e está fora desse resumo do
PlatformIO. O ELF de cada perfil reserva 11.792 B para o Runtime e
2.608 B para o mailbox da telemetria de áudio.

## Gravação e teste em repouso · 2026-11-15

- **PASS — gravação e leitura de volta.** Guition ESP32-P4 rev. 1.3, COM13. O esptool 5.1.0 gravou os 1.431.904 B do candidato bench em `0x10000`. A leitura de volta confirmou o SHA-256 da tabela. Os bytes anteriores à aplicação, incluindo bootloader, tabela de partições e NVS, coincidiram com o backup integral de 16 MiB.
- **PASS — admissão e boot.** Havia 357.779 B internos livres, com maior bloco de 245.760 B e margem de 5.523 B acima da guarda de 352.256 B. O clock avançou com o ARP desligado, e o USB enumerou um dispositivo.
- **PASS — produção I²S em repouso.** Na captura serial de 30 s, o último snapshot registrou 1.247.488 frames, render médio de 232 µs e máximo de 392 µs, zero deadlines perdidos, erros de escrita, timeouts ou OOM. O menor espaço livre observado na stack do owner foi de 932 B. Medir novamente sob carga. Underruns de DMA e saída analógica permanecem sem medição.
- Evidência local em `build/backups/p4-0151-com13-20261115T165548597/` (`flash-before.bin`, `verify.log` e `boot.log`).

## Procedimento físico · MANUAL

**Ensaio musical e captura de áudio analógico — NOT RUN.** Referência elétrica na [qualificação 0.15](arp_015_hardware_candidate.md). Usar a Guition JC4880P433 com a montagem PCM5102A I²S 50/51/52 e USB Host 0 já aprovada. O perfil bench habilita essas saídas.

1. Antes de gravar, identificar a unidade, a porta, o firmware instalado e o backup. A autorização deve identificar o SHA-256 exato do candidato bench. Preservar a NVS e o conteúdo original da flash.
2. Após a gravação autorizada, capturar o boot serial. Confirmar AMY e LVGL funcionais, USB Host pronto, 48 kHz, 256 frames, PCM estéreo, oito vozes e ausência de notas de boot. Registrar memória interna livre e maior bloco antes/depois da admissão, PSRAM e stack disponível de owner/USB/UI. Esperar ao menos 352.256 B livres e maior bloco de 196.608 B na admissão.
3. Com ARP desligado, ajustar BPM no painel global e entrar na página ARP. Conferir que ambos mostram o valor confirmado. Tocar quatro taps regulares de 500 ms e verificar 120 BPM. Abrir o painel sobre Performance e FX, confirmar que PANIC e navegação continuam acessíveis e verificar o pulso.
4. Com Delay FREE, ajustar 350 ms e tocar notas curtas. Testar SYNC 1/8 e 1/8 pontuada em 120 BPM. Uma captura estéreo deve mostrar repetições com espaçamento de 250 e 375 ms, respectivamente. Testar 120 → 100 → 140 → 60 BPM com uma nota sustentada. Em 40 BPM, selecionar 1/4 e conferir SYNC OUT OF RANGE, alvo de 1.500 ms e tap seguro retido. Subir para 60 BPM e conferir recuperação para 1.000 ms. Voltar para FREE e conferir 350 ms com macros neutras.
5. Testar cinco direções, seis Rates, Octaves até 4, Swing, Gate, Latch, Clear Latch e Restart. Com uma nota Touch sustentada, executar Clear Latch, Stop e desconectar o SMK25 em testes separados. A nota Touch deve permanecer com Clear Latch e Stop USB. Confirmar soltura dos grupos USB, tecla com velocity zero, pedal, Play/Stop no controlador, Pad B4 e caminhamento do visualizador. Reconectar e tocar sem reiniciar.
6. Testar os bancos A e B do SMK25 nos perfis de fábrica. Em FREE, K2 só deve alterar o tempo após Pickup. Em SYNC a 40 BPM, testar a primeira divisão disponível (1/8), pequenas oscilações de K2 próximas do limiar e saltos controlados para tercinas. Mapear `perf.bpm` num destino livre, confirmar Pickup, salvar o perfil e verificar a carga após reiniciar.
7. Salvar um V4 com Delay SYNC e tempo manual, salvar BPM global explicitamente e reiniciar. Confirmar BPM sem notas de boot e fazer Recall do V4. Usar Compare e Revert. Carregar um V4 com registro FX V1 de backup e verificar que o tempo manual permanece em FREE.
8. Executar 60 s de carga com até oito vozes, Harmony, ARP, Chorus/Delay/Reverb, feedback alto, mudança de divisão, taps rápidos e navegação. Capturar PCM e telemetria `TEMPO0151`, `ARP015`, `ENGINE`, `OUTPUT08`, USB e memória. Esperar zero OOM, deadlines perdidos, erros I²S e notas presas. Registrar pico, clipping e duração de descontinuidades audíveis. O teste em repouso e a captura musical são critérios distintos.

Se houver perda de áudio, falha de memória ou notas presas, interromper o teste, capturar o diagnóstico e usar o backup e a política de rollback da 0.15. Um downgrade pode encontrar registros FX V2 se já houve salvamento.

**GO para prosseguir com o ensaio musical de bancada. NO-GO para uso musical
até concluir a qualificação sob carga.**
