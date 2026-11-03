# SMK-P4 standalone USB + áudio 0.3

2026-11-02. **Implementação de software; gates físicos MANUAL, sem release estável.**

## 1. Referência e auditoria

HEAD inicial `270b05ab`, árvore limpa, branch `feature/smk-p4-standalone-amy`.
Trabalho na nova `feature/smk-p4-standalone-usb-midi`. Consultar
`git log 270b05ab..HEAD --oneline` para os commits pequenos desta entrega.
`69284d0f`: filas/ownership/AMY e teste musical; `c4f25043`: Host nativo e
testes de lifecycle; `8c51f7eb`: ligação do Host/UI/CI e gates de build.
O commit documental posterior consolida este relatório e o link-check isolado.
Sem push, reset, mudanças em main, gravação, reset ou abertura de porta serial.

Lidos AGENTS/specs/README, relatórios 0.1/0.2, wiring, build/dependências/CI,
runtime, driver PCM, backend/UI, testes e USB/parser/ownership S3.
Conflito preservado explicitamente: specs/AGENTS ainda especificam S3/284×76;
este pedido exige P4/800×480. Prevalece o pedido no aplicativo P4 isolado.
Marcadores de conflito preexistentes no README raiz não foram alterados.

## 2. Arquitetura implementada

`USB nativo P4 → parser MIDI1 → fila USB → owner AMY → PCM16 → driver PCM5102A`.
`LVGL → backend local → fila Touch → mesmo owner/AMY/driver`.

Duas filas SPSC independentes,32 comandos por origem. UI é o único produtor
da fila Touch; tarefa USB é o único produtor da fila USB. Um único consumidor
AMY aplica as duas no limite do bloco, com no máximo32 comandos de cada.
Não há produtor USB em `runtime.commands().push()`, mutex compartilhado, heap
por bloco, nem chamadas AMY em callbacks USB/LVGL. As gerações são atômicas e
podem ser invalidadas pelo owner/Panic, sem se tornarem outro produtor de slots.

Um único AMY,64 osciladores, quatro slots musicais totais. Dois bancos ROM
pré-materializados, cada um com quatro instrumentos monofônicos endereçáveis;
o pool continua8 vozes alocadas/64 osciladores, apenas4 tocáveis por vez.
Isto distingue alturas iguais sem usar a alocação por pitch do AMY como
identidade global. A quantidade de instrumentos internos passa de3 para9;
há custo de heap de boot a medir no P4. Nenhum arquivo do core `src/` mudou.
Sem dependência nova, atualização de SDK, segundo engine, S3 ou ES8311.

## 3–4. PCM5102A e áudio físico — MANUAL

Driver anterior preservado integralmente: Philips,48kHz, PCM16 L/R,
256 frames/bloco, DMA4×256, preload zero, MCLK ausente, timeout20ms.
`CONFIG_SMKP4_PCM_WIRING_CONFIRMED` continua off por padrão.

| Sinal | Candidato no P4 / JP1 | Estado |
|---|---|---|
| BCK | GPIO50 /11 | MANUAL: continuidade/orientação |
| LCK/WS | GPIO51 /9 | MANUAL |
| DIN | GPIO52 /7 | MANUAL |
| GND | GND identificado fisicamente | MANUAL |
| VCC/XSMT/FMT/SCK/DEMP/FLT | conforme módulo real | MANUAL: tensão/straps |

Seguir [pcm5102_wiring.md](pcm5102_wiring.md): remover todos os fios S3,
conferir XSMT antes energizado pelo S3, GND comum, nenhum5V em GPIO.
Silêncio → L → R → estéreo → AMY; medir WS48kHz e BCLK1,536MHz, confirmar
canal/tensão/ruído no mixer. Não foi medido sinal I²S ou áudio analógico novo.

## 5–6. USB físico, porta, controlador e VBUS

Fontes consultadas: [IDF5.5.2 USB Host P4](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32p4/api-reference/peripherals/usb_host.html),
[esquema Guition folha03](https://github.com/ultramcu/guition-jc4880p443c-i-w/blob/master/schematic/03-expand-io_usb-fs-hs_lcd-backlight.webp),
[folha02/alimentação](https://github.com/ultramcu/guition-jc4880p443c-i-w/blob/master/schematic/02-speaker_power-3v3_uart-input_battery.webp),
[Serial/JTAG P4](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32p4/api-guides/usb-serial-jtag-console.html).
Inspecionadas também as imagens locais das folhas02/03 já usadas na fase0.2.

| Item | Conector/sinal documental | Verificado? | Observações |
|---|---|---|---|
| USB Host HS | USB3, `ESP_USB_N/P`, PHY dedicado | MANUAL | esquema diz High speed; posição/silk na unidade e continuidade ainda pendentes |
| USB FS | USB2, `USB1P1_N/P` | MANUAL | esquema diz Full Speed; não deduzir OTG/Serial-JTAG só pelo nome do net |
| VBUS5V | ambos USB-C ligados a `USB5V_IN` na folha03 | MANUAL | rede comum; alimentação de Host não qualificada |
| CC/orientação | ambos conectores com5,1kΩ para GND em CC1/CC2 | MANUAL | indica terminação de sink no desenho; não afirmar source/DRP/OTG pelo formato USB-C |
| D+/D− | USB3 A/B6=P, A/B7=N; USB2 idem nos respectivos nets | MANUAL | confirmar continuidade ao módulo/controlador e ambas orientações |
| Proteção | ESD D4–D7; folha02 mostra AO3401/TVS na entrada VIN e IP5306 | MANUAL | ESD não comprova limitação de corrente nem reverse-blocking VBUS entre portas |
| Fonte | VIN/USB5V_IN/IP5306/VOUT-BAT conforme folha02 | BLOCKED | faltam capacidade real, correntes de pico, tensão do DAC e consumo SMK25 |
| Debug | console configurado USB Serial/JTAG; COM13 na fase0.2 | MANUAL | associação física com USB2/USB3 não foi revalidada nesta fase |

No SDK instalado5.5.2, `usb_dwc_ll.h` e `soc/esp32p4/usb_dwc_periph.c`
confirmam índice0=HS/UTMI dedicado; índice1=FS, GPIO26 D−/27 D+ no mapa
do SDK. O driver usa explicitamente `peripheral_map=1U<<controller`;
o0 padrão evita depender da seleção implícita. Isso **não confirma** o net
do módulo nem a porta da unidade. Serial/JTAG é um terceiro controlador;
há variantes/seleção de PHY: não transplantar a restrição S3 de PHY único.
Não escrever eFuses para adaptar conectores. Console pode desaparecer se
o mesmo caminho físico for ocupado; preparar UART0 qualificada se necessário.

IDF5.5.2 suporta só um Host por vez. HS aceita dispositivos FS diretos;
o driver de hub dessa versão não tem Transaction Translator para FS/LS sob
Host HS. Um hub alimentado genérico não é solução garantida para SMK25 FS.
Usar conexão direta qualificada ou ensaiar hub apropriado no controlador FS
qualificado. Não unir VBUS de computador/fonte/hub sem proteção comprovada.
O firmware não fornece5V nem controla um GPIO VBUS fictício.

ES1.3 permanece no perfil `CONFIG_ESP32P4_SELECTS_REV_LESS_V3`; o SDK contém
ramos PHY específicos pré-v3. Build ES não qualifica USB nesta unidade.

## 7. Parser MIDI

Mantém velocity0–127, canais1–16, NoteOn/Off/velocity0, CC64, CC120/123,
CC1 e bend14bit. CC120/123 adotam release imediato do canal USB, limpando
sustain; não afetam Touch. Bend±2semitons; CC1 mapeia vibrato5Hz até±0,5st
nos slots USB desse canal, com atualização fracionária de nota sem retrigger.
Não usa `amy_event.pitch_bend`, que é global no core atual. A atribuição CC1
é MIDI padrão, não um perfil de knobs presumido para o SMK25.

Transferências com tamanho parcial/status/data inválidos são rejeitadas antes
de publicar qualquer nota e invalidam a origem USB. SysEx/pressure/program/
clock não roteados nesta fase. Uma instância USB e cabo MIDI virtual0;
outros cabos são rejeitados explicitamente, sem misturar identidades.

Reutiliza o predicado S3 `usb_midi_descriptor.h`, sem alterá-lo. O adaptador
P4 constrói views limitadas de interface/endpoint, valida todo o span, aceita
MIDI1 Audio/MIDIStreaming protocolo0/bulkIN, alternates não-zero e MPS
8/16/32/64/512. Prefere alt0; reivindica o alternate selecionado. Não escolhe
VID/PID nem trata qualquer bulk como MIDI. UMP/MIDI2/vendor-specific não
suportados: requer descritores reais antes de criar um quirk.

Logs de enumeração mostram VID/PID/interface/alt/IN/MPS. Opções off por padrão
permitem dump de até256 bytes do descritor e trace bruto SPSC de32 pacotes
com timestamp_ms/cabo/status/data; main imprime no máximo4 por janela5s.
Overflow de trace é contado separadamente e não interfere nas notas.

## 8–10. Router, ownership, sustain

Touch mantém latches Off por pitch/token e lease10s. USB mantém ordem FIFO
por fonte incluindo pedal, sem aplicar sustain fora de ordem antes do Off.
Fila cheia descarta NoteOn/bend/controles contínuos com contador. A perda de
Off, pedal ou CC120/123 incrementa contador e geração USB fora da fila:
cancela todas as notas/sustain USB e invalida gates antigos. É uma degradação
audível deliberada de segurança, não garantia de performance sob overload.
Panic global invalida as duas origens, notas, sustain e controles de pitch/mod.

Owner mantém4 registros: origem, canal, pitch, token Touch/geração USB,
held e instante de início. Duplicado enquanto held é idempotente; retrigger
de nota USB já liberada mas sustentada reutiliza seu slot. Quinto slot é
rejeitado/contado, sem referência extra. Sem voice stealing implícito entre
fontes. Touch C4 e USB C4 ocupam2 slots; Off de um preserva o outro.
USB não usa lease10s: notas/sustain longos permanecem até release/cancel.
Patch silencia todas as origens e invalida comandos USB antigos; requer nova
tecla para retomar uma nota mantida durante a mudança.

## 11. Hot-plug e recursos

Uma tarefa possui install/register/enumeration/claim/transfer/cleanup e ambas
APIs library/client. Cada iteração atende library sem espera e client com
espera de até um tick; completion acorda imediatamente, sem sleep por pacote.
Prioridade4, stack6144, sem afinidade imposta; áudio permanece prioridade5/
stack12288. Não se alterou prioridade/afinidade das tarefas existentes.

DEV_GONE cancela somente USB imediatamente por geração. Cleanup solicita
halt/flush e continua atendendo callbacks até `inflight=false`; depois free,
release, close, nessa ordem. Falhas de cleanup retêm ownership e tentam depois.
Falha de RX/submit/parser solicita cancel e tenta reabrir após1s; desconexão
física cancela retry. Novas conexões são guardadas em bitmap limitado de128
endereços enquanto o dispositivo antigo drena. Stop aguarda drenagem, sem
force-delete de callbacks. Driver que nunca devolve callback pode impedir
shutdown; mantém recursos para evitar use-after-free. Reboot é recuperação
manual final, não solução automática para todos os erros.

## 12. Performance UI

Preservados layout800×480, teclado, octave, master, browser ROM128/135 e Panic.
Macros continuam indisponíveis. `MIDI OFF/ON/*/ERROR` representam desconectado,
conectado, atividade e erro locais; abreviados para caber no widget existente.
Sem snapshots S3 no standalone. Desconectar USB não desabilita Touch; falha
AMY/I²S publica erro pelo mailbox existente. AUDIO RAM não representa saída.

## 13–14. Tempo real, memória e stack

Budget256/48000=5333,33µs. Mantida instrumentação DSP/command/compute/DMA,
latência máxima, deadlines, write_errors/timeouts, RAM interna/maior bloco,
PSRAM, stack, frames/peak/clipping. Novos contadores: connects/disconnects,
USB errors/parse/drop/queue/overflow/stack e polyphony_rejected.
Espera DMA não equivale a DSP; `DMA_underruns=UNMEASURED` permanece explícito.
CC1 acrescenta até4 atualizações AMY/ bloco; custo máximo no target MANUAL.
Não aumentar buffers, vozes ou efeitos para esconder falhas.

Dados0.2 (1732µs máximo em teste RAM pré-LVGL) são históricos, não medição
0.3 com USB/áudio/LVGL. Heap adicional, CPU, maior bloco, PSRAM, latência touch,
stack USB e áudio e estabilidade prolongada0.3 ainda MANUAL. Não extrapolar
resumo estático de linker para heap runtime. OOM de boot completo upstream
continua limitação (`amy_start` void com alocações sem checagem); reserva de
admissão existente e oom_count preservados, sem alegação de recuperação total.

## 15–16. Testes e builds

Logs locais ignorados em `build/p403_*` não são artefatos versionados.

| Estado | Comando executado da raiz | Evidência |
|---|---|---|
| PASS | `./smk-p4/tests/run_host.ps1` | baseline e final; geometria/dirty/PCM/mock I²S e9 testes Python |
| PASS | `./smk-p4/tests/run_amy.ps1` | ROM128/135 reais, PCM/release/Panic/restart/heap; novo musical e stress2produtores; backend/UI |
| PASS | `./smk-p4/tests/run_usb.ps1` | descritores inválidos/truncados/class/endpoint/alt/512MPS; install/register/task/claim/alloc/submit/RX/cleanup faults; disconnect/reconnect in-flight; retry sem desconectar; begin/stop assíncronos |
| PASS | `./build/smk_p4_amy/performance_ui_test.exe` | recompile com flags do runner após ampliar asserts de MIDI OFF/ON/*/ERROR |
| PASS | `./tests/run_smk_safety.ps1` | baseline MIDI/USB/áudio/ownership S3; fontes afetadas permanecem inalteradas |
| PASS | `./tests/run_smk_amy_boot.ps1 -Legacy` | baseline, probes de limitação legacy esperados; warnings de cast de ponteiros Windows preexistentes |
| PASS | `./tests/run_smk_ui.ps1` | baseline S3, todos os testes UI |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone` | após regenerar CMakeCache:256,95s; componente USB compilado; log `p403_final_standalone.log` |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -v -d smk-p4 -e guition-standalone` | verificação incremental35,15s; log `p403_link_verbose.log` |
| FAIL | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone -e guition-display -e guition-link` | primeiro lote: display/link passaram62,27/39,79s; standalone falhou por cache antigo e passou no comando separado acima |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-display -e guition-link` | regeneração CMake completa com novo componente:177,86/182,92s; `p403_final_compat.log` |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-devkitc-1 -e esp32-s3-link` |188,27/181,30s; sem alterar S3/core |
| PASS | `C:/.platformio/penv/Scripts/python.exe smk-p4/tests/check_standalone_build.py` | sdkconfig efetivo ES/Standalone, UART/PCM/USB/self-test off; compila ramos AMY/I²S/diagnóstico/USB/trace/dump e linka ELF separado USB habilitado com libs/flags nativos |
| PASS | `git diff --check` / `git diff --cached --check` | revisão de whitespace e alterações versionadas |
| NOT RUN | CI remota / ThreadSanitizer / outras plataformas upstream | runner USB adicionado à CI; não houve execução remota |
| MANUAL | P4/USB/I²S/analógico/música/carga A–G | sem upload, captura ou teste físico nesta fase |

Os quatro instrumentos monofônicos por banco preservaram os mesmos valores
PCM do caso de referência0.2: ROM128 pico2148/RMS572,0148; ROM135 pico1810/
RMS558,2924; clipping0, release/Panic verdadeiros. Heap interceptado2→2 em
três ciclos; nenhuma alocação/free durante apply/render/Panic/patch/musical.
Teste musical também verifica bend real por oscilador, isolamento Touch,
CC1 e retorno ao centro,16canais, USB mantido além da lease Touch, patch com
eventos antigos pendentes,50.000 comandos por produtor com integridade/ordem
e20.000 iterações musicais por produtor. Não é medição de latência no P4.

Standalone normal: RAM estática29856 / Flash1062614 bytes; versus0.2
28936/1058854, deltas+920/+3760. Bin1063344 bytes, inferior à factory3MiB,
SHA256 `d2af803ab167648c2bf0fe920f7789ef54b198a38e977b631b48e8545ce732fc`.
Imagem normal gerada antes dos commits finais; hash identifica os bytes,
não uma alegação de firmware gravado. O ELF `enabled-usb-check.elf` é só
teste de link, sem gerar/gravar bin e sem substituir objetos/map/ELF normais
ou alterar gates no sdkconfig. Pico de RAM com Host ativo permanece MANUAL.

## 17. Gates físicos

| Gate | Estado | Evidência ainda necessária |
|---|---|---|
| A áudio | MANUAL | silêncio/L/R/AMY físico/mixer/touch |
| B USB | MANUAL | CC/VBUS, descritores reais, enumeração/RX/reconnect no P4 |
| C música | MANUAL | SMK25+Touch, sustain/bend/mod, quatro vozes, Panic |
| D estabilidade | MANUAL | cenários A–G abaixo; nenhum release estável |

Nenhuma execução nova no P4, sinal I²S, áudio analógico, SMK conectado ou
MIDI recebido fisicamente foi presumido a partir de compilação/mock/host.

## 18–20. Problemas, correções e limites

Fila antiga SPSC preservada por origem; colisão de pitch resolvida por slots
monofônicos no adapter; AMY bend global contornado por updates por voz.
Sem alterações no S3, AMY core ou driver PCM. Primeiro baseline display/link
passou64,64/55,98s; o baseline standalone não foi concluído limpo antes das
novas alterações: houve erro do gerenciador de componentes ao verificar um
arquivo LVGL existente. Tentativas incrementais também mantiveram lista CMake
anterior sem o novo componente USB, causando header ausente. Removidos somente
os `CMakeCache.txt` gerados dos perfis, preservando sdkconfigs/dependências/
fontes; regeneração resolveu a descoberta. Nenhuma dependência foi atualizada
ou arquivo LVGL versionado alterado. Um assert de pitch no teste novo consultava
deltas antes do render; corrigido para consultar estado após render, sem
mascarar erro musical.
Não há qualificações elétricas, perfil vendor-specific, múltiplos teclados,
MIDI2, monitor sem perdas, seamless patch ou certificação de ausência de race
por ThreadSanitizer no Windows. Stress e modelo SPSC não substituem TSAN.

## 21–22. Ligação, configuração e gravação

1. Desenergizar, remover S3/UART/I²S antigos e baixar master/mixer.
2. Aprovar o roteiro PCM e registrar XSMT/straps/fonte/continuidade.
3. Identificar USB2/USB3 na unidade, continuidade/net/controlador, CC nas duas
   orientações e caminho VBUS. Medir tensão, corrente/pico e reverse leakage.
   Qualificar switch/fusível/current limit/reverse-blocking externo se faltante.
   Não fabricar cabo em Y nem unir fontes pelo VBUS comum.
4. Usar menuconfig do ambiente existente, ligar separadamente os gates PCM
   e USB somente após qualificações correspondentes. Primeiro diagnóstico PCM
   silêncio/L/R; depois desligar `PCM_DIAGNOSTIC/TEST_SEQUENCE/RAM_SELF_TEST`.
5. Para Host selecionar controlador confirmado, inicialmente0 somente se
   USB3/HS e alimentação forem aprovados. Logs/trace opt-in para identificar
   SMK25; sem firmware vendor quirk antes dos dados.
6. Inspecionar `.pio/build/guition-standalone/config/sdkconfig.h` e `sdkconfig`:
   defaults não sobrescrevem configuração gerada. Gate USB off e PCM off são
   o estado seguro entregue. Verificar perfil ES, UART off e AMY diagnóstico off.

```powershell
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone -t menuconfig
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone
C:/.platformio/penv/Scripts/python.exe -m serial.tools.list_ports -v
# Somente após identificar a porta e conferir configuração/ligação:
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone -t upload --upload-port <P4_PORT>
C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port <P4_PORT> --output build/p403_physical.log --seconds 60
```

## 23. Recuperação

Panic na tela → soltar teclas/pedal → confirmar notes0 → tocar novamente.
Desconexão libera USB; se ERROR persistir, retirar/reconectar teclado e conferir
VBUS/desc/heap. Touch deve continuar. Falha áudio: baixar mixer, guardar logs,
revisar fonte/driver e reiniciar com gates seguros; não há retry I²S em loop.
Se debug desaparecer, remover Host/alimentação externa com tudo desligado,
usar entrada ROM BOOT/reset na placa confirmada ou UART0 qualificada e gravar
imagem segura. Não apagar Flash/NVS nem gravar eFuses como recuperação.

## 24. Pré-requisitos para ensaio / próximo passo mínimo

Após aprovação elétrica, registrar no mesmo boot cenários A repouso, B notas/
acorde4, C MIDI+Touch C4 com ambas ordens de Off, D master/patch128/135, E
sustain por canal, F hot-plug e G estabilidade30min seguida de ensaio prolongado.
Para F: boot com teclado, conectar após boot, retirar sem notas/com notas/com
pedal, reconectar10vezes sem reboot. Esperado: USB libera sozinho, Touch
permanece, notes0 após releases, heap/stack sem tendência, sem resets nem
incrementos persistentes de erro/deadline/timeout. Ouvir e medir separadamente.
Anotar baselines/deltas de contadores por janela; máximos são cumulativos,
não subtrair máximos como máximo da janela. Medir cauda Panic/latência até o
DAC e perda audível com analisador/mixer, não apenas sucesso de I²S write.
Próximo passo mínimo: qualificar fisicamente USB3/VBUS/CC e PCM, então gravar
imagem opt-in e capturar descritores/MIDI/silêncio/L/R antes do ensaio musical.
Fonte única final BLOCKED até medir carga P4+LCD, SMK25 e módulo DAC, quedas,
proteção e backfeed. Não foi prometido alimentar tudo por uma porta USB.
