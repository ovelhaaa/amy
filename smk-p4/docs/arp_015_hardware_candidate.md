# Candidatos Guition SMK-P4 0.15

## Imagens compiladas — PASS

Fonte de firmware no commit `5d0edb0df0e34269fef71cad37fa681a8266de58`.
Imagens locais em `smk-p4/.pio/build/<perfil>/firmware.bin`.

| Perfil | Tamanho | Flash reportada | RAM estática reportada | SHA-256 |
|---|---:|---:|---:|---|
| guition-editor-8 | 1.319.376 B | 1.318.650 B | 41.352 B | `494d006498b256bddc89c201c5ed7d5eb82158536def7f3ec36647522af0191c` |
| guition-editor-8-bench | 1.421.312 B | 1.420.600 B | 41.740 B | `a11c3a0cd71b989677412bd5c39a01149b1453fac987bf88d5d982f31002b3fc` |

Em relação aos builds documentados da 0.14.2, as imagens cresceram
9.280 B e 9.440 B. A RAM estática total do PlatformIO variou −44 B no
editor e −40 B no bench. A contribuição específica da estrutura do
owner foi medida no host. A margem de RAM interna na admissão exige
medição física. Flash de 16 MB, PSRAM Hex a 200 MHz e a guarda de
352.256 B continuam na configuração compilada.

## Procedimento físico — MANUAL

Usar a montagem Guition JC4880P433 / PCM5102A / SMK25 V2 já confirmada
na [qualificação 0.14.2](smk25_0142_hardware_candidate.md). O perfil
`guition-editor-8-bench` usa I²S 50/51/52 e USB Host 0. O perfil
`guition-editor-8` mantém os gates elétricos desabilitados.

1. Registrar placa, revisão, porta, imagem instalada, backups e SHA-256.
   A autorização de gravação deve indicar o SHA-256 exato do binário bench.
2. Capturar o boot serial. Conferir versão 0.15, oito vozes, 48 kHz e 256
   frames. Registrar RAM interna livre antes e depois da admissão, maior
   bloco interno, PSRAM e stack. Esperar ao menos 352.256 B livres e maior
   bloco de 196.608 B antes da admissão. Conferir AMY, I²S estéreo e USB VID/PID
   4353:4B4D.
3. Com ARP desligado, tocar notas, acordes e pedal. Conferir velocity,
   pitch bend, modulação e Note Off nos timbres Juno e DX7.
4. Ativar ARP pelo Touch e tocar C–E–G. Conferir a ordem audível e o passo
   exibido. Soltar E e conferir a remoção do padrão. Ativar Latch, soltar
   as outras teclas e confirmar que o padrão continua. Tocar um acorde
   novo e confirmar a substituição.
5. Alterar direção, divisão, oitavas, gate, swing e BPM enquanto toca.
   Usar Tap Tempo. Confirmar o estado aplicado na tela. Na combinação
   mais rápida, capturar em PCM as bordas de passos e o gate mínimo.
6. Alternar ARP pelo B4. Registrar Note On repetido e release. Testar
   Play com padrão vazio e Stop durante um gate e com Latch ativo.
   Sustentar uma nota pelo Touch durante Stop USB e verificar que ela
   permanece audível. Confirmar CC114/115 e Rec reservado no monitor.
7. Usar quantização de escala e Chord Assist. Habilitar Harmony também
   para Touch e sustentar um acorde de quatro vozes pelo Touch enquanto
   o SMK25 dispara outro. Conferir oito vozes no máximo. A rejeição integral
   quando há apenas três slots livres é verificada pela suíte host.
8. Testar CC64 com ARP ativo. Esperar os gates terminarem no prazo.
   Testar Panic, Patch Change e desconexão durante um padrão latched.
   Confirmar silêncio após a cauda do timbre. Reconectar e tocar um
   padrão novo sem reiniciar.
9. Sustentar carga de até oito vozes com FX e mudanças de tempo por
   60 segundos. Registrar máximos do `ARP015`, render médio/máximo,
   deadlines, RAM, stack, clipping, OOM, grupos rejeitados, filas e drops.
   Medir jitter e interrupções na saída estéreo com captura externa.

## Rollback

Se houver falha de admissão, áudio, segurança de notas ou transporte,
registrar o log e voltar à imagem bench 0.14.2 conhecida, com autorização
para seu SHA-256 e preservação da NVS. A referência é o binário
`aa4d6fdaee8043d2fe438281d5babb254c47e4e3011a4f1d9a6b7e46340ea3db`
do commit-base `f4eeb0d8d450a9bd06a3553415c151d28ed3d9f3`.
