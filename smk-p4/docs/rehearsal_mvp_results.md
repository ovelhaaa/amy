# SMK-P4 rehearsal MVP 0.1 — resultados incrementais

Execução iniciada: 2026-11-01. Atualização física: 2026-11-02.
**Entrada/controles P4 e SMK25 após boot conectado aprovados pelo operador;
conexão posterior do SMK25 falhou na versão anterior. Após a atualização,
três reconexões sem reinício foram confirmadas pelo operador e em captura
contínua. Sustain, isolamento do enlace P4 e soak pendentes.
Há crescimento dos contadores de falha de áudio
entre leituras, ainda sem causa isolada.**
Gate 0/1 compilado e gravado na COM13, com aplicação e refreshes capturados
em Serial. **PASS físico informado pelo operador:** nenhuma falha; imagem,
toque, botão e slider funcionam. A seleção anterior de falha foi corrigida
explicitamente pelo usuário. Soak prolongado permanece MANUAL.
Performance foi posteriormente compilada e gravada no P4; execução offline
capturada em Serial com heap estável nas amostras. **PASS visual informado
pelo operador:** nova tela Performance aparece corretamente. Após trocar o
cabo, S3 voltou à COM8 e seus logs foram recebidos. O operador confirmou a
montagem UART; backend gravado e HELLO/snapshots reais confirmados em ambas
as placas. Em2026-11-02 o operador confirmou LINK ON/nome real do patch,
som pelo teclado virtual e notas parando ao soltar. A resposta inicial ampla
foi esclarecida posteriormente: testou somente P4, não o SMK25 físico. Esse PASS
foi seguido pela aprovação de macros habilitadas alterando o som, MASTER
reduzindo/recuperando volume, troca de patch atualizando nome/timbre e PANIC
cortando notas e permitindo tocar novamente. Em teste posterior, SMK25 físico
também teve som e release confirmados, com eventos USB nos logs, após o usuário
reiniciar S3 com o teclado já conectado. A conexão após boot havia falhado com
`ESP_ERR_NO_MEM`; não registrar hot-plug como aprovado. Sustain/reconexão/soak pendentes.

## Arquitetura e escopo efetivo

Escolhida a opção B: P4 como interface, S3 como sintetizador Single com AMY,
USB MIDI Host e PCM5102A. Há teste independente Gate 0/1 e uma aplicação
Performance LVGL conectada ao cliente UART. Som/release pela entrada P4 e
os controles básicos foram confirmados pelo operador. SMK25 físico também
confirmado após reinício com o teclado conectado. Hot-plug USB, sustain, perda de
enlace/reconexão e estabilidade prolongada ainda exigem bancada.

`src/` permanece sem alterações. O incremento da ponte adiciona ao SMK-S3
um componente `remote_link` opcional e eventos RemoteUi, com notas Touch
independentes do pedal físico. Não há Layer B, migração AMY para P4, mudança
de sample rate, bloco, formato persistente ou novas APIs de materialização.
A branch nova
`feature/smk-p4-rehearsal-mvp` parte de `614a3eb5`, árvore inicialmente limpa,
na branch `feature/smk-s3-dual-amy-runtime`. Esse HEAD tem regressões Single
documentadas anteriormente; as verificações executadas agora estão abaixo.
Não se fez checkout retroativo, reset de Git, merge ou push. O teste Gate1 foi
gravado somente no P4, depois substituído pela Performance; o firmware S3
foi posteriormente atualizado com o backend opcional após confirmar a montagem
UART e a porta COM8. Não houve mudança no core AMY.

Conflitos de especificação documentados: `specs.md`/AGENTS pedem display S3
284×76 e specs sugere inicialmente bloco128; o pedido atual prioriza interface
P4 800×480 isolada e bloco256. Seguido o pedido explícito, preservando o S3.
O README raiz já contém marcadores de conflito AMY/voxP4; não foram introduzidos
nem resolvidos por esta etapa.

## Gate 0 — evidência de hardware

O usuário informou que `C:/progs/P4SDM` usa a mesma placa conectada. A referência
foi inspecionada no commit `307a8190152b12bba3e3a65fa81e17647d7c1cfd`, sem
alterá-la. Ela identifica **JC4880P443C_I_W / JC-ESP32P4-M3** e contém registros
anteriores de inicialização/qualificação de display, touch e PSRAM. Não se
afirma equivalência elétrica universal com o nome JC4880P433 do pedido inicial.

**PASS — consulta ROM executada nesta sessão:** COM13, USB VID:PID303A:1001,
ESP32-P4 revisão1.3, cristal40MHz, MAC80:f1:b2:d3:cc:86, Flash16MB,
fabricante0x68/dispositivo0x4018. A informação de 400MHz do ROM não muda o
perfil: a referência P4SDM documenta360MHz aceitos pelo SDK para esta revisão.
Log local ignorado: `build/rehearsal_gate0_p4_identity.log`.

**MANUAL — revisão da PCB e inscrição. PASS informado pelo operador — pinos
UART identificados e livres em ambas as placas.**
A consulta ROM não identifica esses itens. O firmware novo também reportou
heap PSRAM inicializado de33.554.432 bytes durante a captura real. Esse valor
é memória disponibilizada ao heap; não identifica a revisãoPCB.

Fontes primárias consultadas:

- [Guition registry0.2.1](https://components.espressif.com/components/guition/guition_boards/versions/0.2.1/readme?language=en):
  identifica jc4880p443; exige IDF>=6.1.0. Não foi adicionado ao SDK5.5.2 atual.
- [BSP ultramcu fixado](https://github.com/ultramcu/guition-jc4880p4-bsp/tree/324970bade0d1f4e52880fe8016580368bc1e06e):
  origem MIT da tabela do painel, preservada pelo P4SDM e nesta aplicação.
- [LVGL9.3.0](https://components.espressif.com/components/lvgl/lvgl/versions/9.3.0):
  dependência MIT fixada, exclusiva do P4.

## Display e touch implementados

Perfil recuperado do P4SDM: ST7701S, DSI2 lanes500Mbps, DPI34MHz,
RGB565480×800 nativo; resetGPIO5, backlightGPIO23, LDO3/2500mV.
GT911, I2C0 SDA7/SCL8, resetGPIO3, polling, probe0x5d/0x14 e verificação
do product ID/faixa480×800. Transformação única para800×480:
`logical=(raw_y,479-raw_x)`; LVGL sem rotação adicional.

Um owner executa LVGL/display/touch. Buffer parcial LVGL51.200 bytes em PSRAM,
três framebuffers nativos totalizando2.304.000 bytes e pool LVGL128KiB.
Dirty tiles limitados com reparação do backbuffer e fence de dois refreshes.
Nenhum desenho ou I2C é executado no S3. Falhas são reportadas, sem afirmar
que a tela funciona apenas por compilação. Não há animações pesadas.

Teste: fundo sólido, textos, marcadores dos cantos, botão com contador, slider
explicitamente rotulado teste visual, cursor de toque e diagnóstico serial a
cada5s. Touch com erro ou atualização perdida resulta em release defensivo.
Não há suporte multitoque anunciado. Nenhum periférico de áudio P4 é ligado.

## UART, protocolo e controles musicais

**Implementado no incremento da ponte:** codec SLIP/CRC16 explícito, sessões,
sequência, duplicatas, retries, timeout, snapshot, catálogo de patches,
diagnósticos e comandos de patch/macro/volume/Panic/nota virtual. Backend S3
publica eventos; não chama AMY pela UART. Cliente P4 negocia HELLO e recebe
estado real do controle, com flag REQUESTED e ACK somente RECEIVED.
Detalhes e payloads em [protocolo v1](../../shared/smk_protocol/README.md).

**Implementado em código, validação física MANUAL:** Performance800×480,
oito macros com nomes/valores/suporte do backend, master, anterior/próximo,
browser de256 patches factory em páginas de oito e Panic acessível nas duas
telas. Não envia parâmetros musicais ao iniciar. Offline desabilita controles
e reconectar recebe estado novo, sem replay de movimentos anteriores.

Teclado mono de13 teclas, inicialmenteC4–C5, oitavas configuráveis, velocity100;
release/saída da tecla, troca de página/patch/oitava e hold máximo10s liberam
a fonte Touch. Lease600ms no S3 cobre perda de heartbeat. O pedal físico não
retém Touch. Há indicação de conexão, USB MIDI real, frames de áudio, notas,
max-render e gapsPCM. AUDIO RUN não certifica qualidade auditiva.
O slider do ambiente diagnóstico continua apenas visual.

Pinagem de bancada conferida nos esquemas e identificada/confirmada livre
pelo operador: P4 TX52/RX51, S3 TX4/RX5, UART1 em ambas, 115200 baud, 8N1.
Ver [guia de ligação](uart_wiring.md). Os ambientes originais deixam a ponte
desativada; os novos ambientes `guition-link` e `esp32-s3-link` selecionam
esse perfil de bancada. Os defaults genéricos Kconfig permanecem GPIO-1.
Montagem confirmada pelo operador e comunicação UART real executada. Impacto
musical sob notas/controles e reconexão continuam MANUAL.

RX usa ring2048 bytes e trabalho limitado a64 bytes por service; TX tem4 slots
fixos e usa FIFO não bloqueante. Timeout/reconexão liberam só Touch. Reservas
fora das FIFOs no EventBus e adapter preservam essa liberação mesmo com filas
cheias; a confirmação em AMY real preservou também notas físicas na mesma
altura. Pedal físico não retém Touch. Panic explícito segue o caminho global
existente. Macros, números/nome de patch e diagnósticos vêm do backend real.
Troca Single poderá interromper notas; não se promete seamless ou APPLIED.

## Validação executada

Comandos na raiz; logs em `build/` são locais e ignorados pelo Git:

| Status | Comando / resultado |
|---|---|
| PASS | `C:/.platformio/penv/Scripts/python.exe -m serial.tools.list_ports -v`: P4 em COM13; COM6/7 Bluetooth |
| PASS | `C:/.platformio/penv/Scripts/python.exe -m esptool --chip esp32p4 --port COM13 --baud 115200 --no-stub --after hard-reset flash-id`: identificação acima; sem escrita Flash |
| PASS | `./tests/run_smk_safety.ps1`: parser/MIDI, USB, filas/releases/Panic, áudio, console, clock e NoteOwnership; `build/rehearsal_baseline_safety.log` |
| PASS | `./tests/run_smk_performance.ps1`: rotas, comandos, releases e codecCRC; `build/rehearsal_baseline_performance.log` |
| PASS | `./tests/run_smk_patch_integrity.ps1`: integração AMY/PatchManager e freeze; `build/rehearsal_baseline_patch.log` |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-devkitc-1`: IDF5.5.2, 48kHz/bloco256, RAM30.116 bytes, Flash1.141.383 bytes; `build/rehearsal_baseline_s3.log` |
| PASS | `./smk-p4/tests/run_host.ps1`: todos os pixels/cantos, clipping, dirty history2/3 buffers, page changes, fence/wrap; `build/rehearsal_gate1_host.log` |
| FAIL, resolvido | Primeira tentativa P4 sem espaço ao extrair LVGL; `build/rehearsal_gate1_p4.log`. O usuário liberou espaço e o build foi retomado |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-display`: `build/rehearsal_gate1_p4_final.log`,251,67s; firmware inicial desta rodada compilado |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-display -t upload --upload-port COM13`: últimos ajustes de GPIO compilados, bootloader/partições/app gravados e hashes verificados; `build/rehearsal_gate1_p4_upload.log`,102,75s |
| PASS | `C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port COM13 --output build/rehearsal_gate1_boot.log --seconds 30 --reset`:1226 bytes reais; aplicação executa, refreshes583→2095, zero touch/flush errors reportados e heap interno447.235 bytes estável nas amostras |
| PASS | `C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port COM13 --output build/rehearsal_gate1_interaction.log --seconds 60`:2410 bytes reais de execução, zero erros reportados e heap interno447.235 constante. Botões=0 em todas as amostras: não é prova de interação física |
| PASS | `./tests/run_smk_link.ps1`: SLIP/CRC, limites, truncamento, timeout, resync, sessão, sequência/wrap, duplicatas, snapshot explícito, ACK RECEIVED, lease Touch, Panic e retries do cliente |
| PASS | `./tests/run_smk_safety.ps1`: regressões com a nova liberação seletiva fora da fila; `build/rehearsal_link_safety_final.log` |
| PASS | `./tests/run_smk_amy_boot.ps1`: AMY real e owner, incluindo Touch com pedal/fila cheia/mesma altura de nota física; `build/rehearsal_link_amy_final.log` |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-devkitc-1 -e esp32-s3-link`: matriz local 48 kHz/256 frames; `build/rehearsal_link_s3_matrix.log` |
| PASS | `./tests/run_smk_link.ps1`: inclui modelo real Touch, release/saída, ACK perdido, press cancelado enquanto ocupado, hold10s, offline/audio fault e ausência de replay; `build/rehearsal_performance_link_tests.log` |
| PASS | `./tests/run_smk_safety.ps1`: regressão final, incluindo bloqueio de Remote NoteOn durante falha de áudio; `build/rehearsal_performance_safety.log` |
| PASS | `./smk-p4/tests/run_host.ps1`: geometria/dirty/fences após integração Performance; `build/rehearsal_performance_display_host.log` |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-devkitc-1 -e esp32-s3-link`: matriz final, 43,91s/39,64s; `build/rehearsal_performance_s3_matrix.log` |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-link -e guition-display`: ponte anterior à Performance, ambos passaram; `build/rehearsal_link_p4_matrix_final.log` |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-link -e guition-display`: Performance e diagnóstico,327,02s/301,38s; `build/rehearsal_performance_p4_matrix.log` |
| PASS | Mesmo comando após os últimos ajustes de reconexão e diagnósticos,29,85s/15,92s; `build/rehearsal_performance_p4_final.log` |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-link -t upload --upload-port COM13`: Performance gravada, hashes verificados; `build/rehearsal_performance_p4_upload.log`,23,47s |
| PASS | Mesmo upload após distinguir timeouts do cliente de quadros inválidos; recompilado/gravado e hashes verificados; `build/rehearsal_performance_p4_upload_final.log`,26,32s |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-display`: diagnóstico recompilado após último ajuste de log,17,50s; `build/rehearsal_performance_p4_diagnostic_final.log` |
| PASS | `C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port COM13 --output build/rehearsal_performance_p4_final_runtime.log --seconds 20`:2008 bytes; refresh1793→2700, zero erros de driver/quadros/touch/flush e mesmos valores de heap offline. Timeouts HELLO esperados sem S3 |
| PASS | `C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port COM13 --output build/rehearsal_performance_p4_boot.log --seconds 30 --reset`:2800 bytes reais; execução offline, refresh2700→4212, sem touch/flush errors, heap estável nas amostras. Captura parcial do boot, sem comprovar cold boot completo |
| BLOCKED, resolvido | Porta S3 ausente inicialmente; após troca do cabo apareceu COM8, CH343 VID:PID1A86:55D3, serial5A4E121397. Montagem UART posteriormente confirmada e backend gravado |
| PASS | `C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port COM8 --output build/rehearsal_s3_cable_check.log --seconds 8`:234 bytes reais de logs S3, patch000 A11 Brass Set1, BPM120, sem enviar comandos/reset/gravar firmware. Não comprova saída de áudio |
| PASS | Operador confirmou que a nova tela Performance aparece corretamente; comunicação e controles musicais continuam MANUAL |
| NOT RUN | Workflows no GitHub; somente builds/testes locais executados |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-link -t upload --upload-port COM8`: build/upload, hashes verificados e reset normal; `build/rehearsal_link_s3_upload.log`,93,11s; bin1.149.536 bytes, SHA256 `4d5fa513b7eb6b972ce647a3b328ab12a9ca48f4366342110b90b930b7af5115` |
| PASS | Captura COM13 por20s: `build/rehearsal_uart_p4_connected.log`, snapshots91→166, connected=1/state=1, patch0/engine0. Sem novos erros; contadores históricos P4 bad_frames107/client_errors1842 constantes, não são zero total |
| PASS | Consulta somente leitura `audio_status` na COM8 e logs de execução: `build/rehearsal_uart_s3_audio_status.log`, S3 connected=1, rx_errors/bad_frames/tx_drops/rejected=0, reconnects=1 |
| PASS limitado | Duas consultas `audio_status` separadas por20s, sem notas: `build/rehearsal_uart_s3_steady.log`. Frames2.865.408→3.830.528; média689→676us, carga12,9→12,7%; sem crescimento dos contadores de falha na janela |
| PASS limitado | Janela adicional55s, `build/rehearsal_uart_s3_interaction.log`: leituras em155,6s/203,6s, frames7.386.112→9.690.112, starvations78/deadlines3 sem crescimento; NoteRefs/peak=0 e patch0. Não houve interação musical confirmada durante a captura |
| PASS diagnóstico | Consulta somente leitura `status`, `build/rehearsal_uart_s3_system_status.log`: EventBus/USB drops/parse errors/queue overflows=0, Panics=0 e MIDI EventsReceived=0. Não comprova enumeração ou funcionamento do SMK25 |
| PASS físico informado, escopo corrigido | Em2026-11-02, operador confirmou LINK ON/patch real, nota virtual P4 com som e encerramento ao soltar. Depois esclareceu que testou somente P4; não registrar PASS de SMK25 físico. Sem medição de latência durante a confirmação |
| PASS físico informado | Em2026-11-02, operador confirmou macros habilitadas alterando o som, MASTER reduzindo/recuperando volume, troca de patch mudando nome/timbre e PANIC cortando notas com execução posterior normal. Confirmação verbal; sem nova captura simultânea de contadores |
| PASS diagnóstico / limitação | Após os controles, `build/rehearsal_uart_s3_after_controls.log`: patch006 A17 Choir, Panics3, peak11291 (-9,3dBFS), NoteRefs0, cmd_drop/I2S errors/timeouts/clipping0. Starvations196 (+118) e deadline misses42 (+39) contra a primeira leitura; max-render262.709us. Leituras distantes cerca de51min, sem isolar qual interação provocou o aumento. MIDI EventsReceived/reconnects0; operador confirmou teste somente P4 |
| PASS limitado | Janela adicional após controles, `build/rehearsal_uart_s3_post_controls_window.log`: leituras separadas por20s, patch006/NoteRefs0, starvations196/deadlines42 constantes, média908→897us. Não isola a origem do crescimento anterior nem substitui teste sob carga |
| FAIL | Primeira conexão SMK25 após boot: `build/rehearsal_smk25_usb_connect.log`, Root port reset failed e claim de endpoint `ESP_ERR_NO_MEM`, VID4353/PID4B4D, Panic de recuperação. USB reconnects0; eventos2 são insuficientes para certificar notas |
| PASS físico informado + diagnóstico | Após reiniciar S3 com teclado conectado, operador confirmou acende/toca/solta e confirmou que precisou reiniciar. `build/rehearsal_smk25_usb_verify.log`: uptime46s, USB reconnects1, eventos869, sem parsing/drop/overflow; refs1/2 durante execução, peak10967. Boot novo: starvations82/deadlines101/max-render262.920us, média3149us durante notas. Não calcular delta contra o boot anterior nem confundir com recuperação sem reboot |
| PASS físico + captura | Atualização headless: operador confirmou três ciclos USB sem reiniciar e repetiu o roteiro durante captura contínua. `build/rehearsal_headless_usb_hotplug_continuous.log`: três disconnect/claim/connect, contadores USB disconnects3/reconnects4, sem boot ou timestamps regressivos durante os ciclos. Nenhum ESP_ERR_NO_MEM; P4 conectado. Ver detalhes de métricas abaixo |
| MANUAL | Sustain, liberação seletiva durante perda do enlace P4 e soak. A aprovação dos controles e de três hot-plugs não qualifica estabilidade prolongada |
| PASS | Operador confirmou imagem e interação corretas, sem falhas, após corrigir a resposta anterior. Não é medição de latência ou soak |
| MANUAL | Soak prolongado, áudio/MIDI físico S3 e coexistência. A captura inicial de boot foi parcial; não contém todas as mensagens de inicialização do GT911 |

Warnings preexistentes do harness AMY em host64-bit incluem conversões de
ponteiros para `long`32-bit. Não foram ocultados nem atribuídos ao P4.
Sem medição nova de latência ponta a ponta. A primeira leitura física S3
reportou max-render262.709us,3 deadline misses e78 starvationsPCM; esses valores
permaneceram constantes nas consultas em61,4s e81,5s de uptime. Não atribuir
automaticamente ao boot nem declarar áudio limpo: a janela em repouso não
qualifica notas, troca de patch ou controles. I2S errors/timeouts e cmd_drop=0;
DMA underrun continua não observado pelo driver (`dma_underrun=unobserved`).
Heap interno livre15.627/maior bloco7.680 e PSRAM7.558.780 livres na janela.

O usuário dispensou explicitamente a recuperação do firmware anterior do P4;
as tentativas de backup completo foram encerradas. O upload acima substituiu
o firmware P4, sem alterar eFuses, C6 ou o projeto P4SDM em disco.

Imagem histórica do Gate1 gravado (não é a nova Performance):576.784 bytes no arquivo `.bin`, SHA-256
`93ffdd1f79fc20afb7a63e183af766412cda9af5fad426c52cc7fede674e17ef`.
O mapa detalhado ESP-IDF reporta DIRAM224.200 bytes, incluindo `.bss`137.332
e1.036 bytes; imagem útil576.766 bytes. O resumo PlatformIO informa
Flash576.054 e RAM12.968 bytes, mas **esse resumo de RAM não inclui toda a
ocupação mostrada no mapa IDF**; não o usar como pico de memória.
Runtime interno:447.235 livres, maior bloco253.952; heap PSRAM total33.554.432.
Pico durante init e soak longo continuam separados da amostragem de repouso.

Primeira captura Performance offline: heap interno435.655 livres/maior bloco
253.952; PSRAM31.191.472 livres/maior bloco30.932.992; pool LVGL90.244 livres,
maior bloco81.176, pico45.244 e fragmentação11%. Valores constantes nas amostras
de30s; não comprovam soak nem estabilidade com comandos musicais. O contador
do cliente aumenta pelas tentativas HELLO sem resposta; o log final separa
`client_errors_including_timeouts` de erros de driver e quadros inválidos.

Defaults finais: tarefa main8KiB, LVGL9.3.0 com exemplos desativados e pool
interno128KiB, buffer parcial emPSRAM. Captura Serial não equivale a aprovação
visual, auditiva ou de coordenadas físicas.
O runner host foi repetido com sucesso após os ajustes finais:
`./smk-p4/tests/run_host.ps1`, log `build/rehearsal_gate1_host_final.log`.

## Flashing e aceitação física

Build/flash/monitor e teste dos cantos estão no [README](../README.md).
**Build e upload Gate1 passaram nesta unidade.**
Não gravar na COM8 do S3 por engano. Não alterar firmware do C6.

Após build aprovado, registrar separadamente boot, LCD visível e touch:
quatro cantos/centro, clique, arraste/saída/release, ausência de rotação dupla,
ghost touches e tela preta; heap estável em30min, refresh avançando e zero
erros de touch/flush. A aprovação visual exige observação do operador.

O roteiro completo para primeiro ensaio continua **MANUAL e condicionado à
ponte/Performance verificadas na bancada**, nesta ordem:

1. Ligar apenas S3+SMK25+PCM5102A; confirmar som e MIDI antes do P4.
2. Ligar P4; confirmar tela/touch, HELLO e snapshot real do S3.
3. Tocar acordes, velocity, pitch bend e modulation no SMK25; ajustar macros
   disponíveis e master; trocar patches anotando interrupções de notas.
4. Acionar Panic; tocar/soltar nota virtual e sair da região da tecla. Com
   sustain físico pressionado, soltar Touch deve encerrá-la. Repetir mantendo
   a mesma altura no SMK25: soltar Touch deve preservar a nota física.
5. Com ambas alimentadas, retirar TX/RX e manter GND durante nota virtual;
   confirmar sua liberação em até600ms após último heartbeat e continuidade
   de notas físicas/sustain. Repor TX/RX e usar RESET no P4: recuperar snapshot
   sem Panic global nem replay de notas. Reiniciar S3: P4 sinaliza desconexão.
   Testar também saída do dedo da tecla, hold10s e troca de oitava/patch/browser.
6. Repetir com arp/drums existentes; manter duas horas sob carga.
7. Registrar notas presas, resets, falhasUART, latência de controle medida,
   max-render/deadlines/gapsPCM, errosI2S, heap/mínimo interno/PSRAM, captura
   de áudio e observação auditiva. Nenhum erro pode ser omitido.

No P4 registrar também `LVGL free/largest/peak_used/fragmentation`, heap interno
e PSRAM livre/mínima/maior bloco, além de refresh/touch/flush. O pool estático
LVGL não aparece como variação do heap ESP-IDF; verificar os dois separadamente.
Só declarar PASS de estabilidade após interação contínua e soak, não em repouso.

## Arquivos e riscos

Arquivos da entrega A: `smk-p4/`, projeto/board/LVGL e testes. Ponte:
`shared/smk_protocol/`, `shared/smk_uart/`, `smk-s3/components/remote_link/`,
adaptações pequenas no EventBus/NoteOwnership/adapter/app_main do S3,
`smk-p4/main/p4_link.*`, `performance_ui.*`, `touch_note.h`, perfis de build,
testes e workflows. AMY core intacto.

Host: NoteOwnership3344→3472 bytes (+128); SynthEvent continua32 bytes.
S3 final, resumo PlatformIO: ponte desativada RAM30.096/Flash1.142.099 bytes;
ponte habilitada RAM33.072/Flash1.149.127 bytes. Diferença entre esses builds:
RAM estática+2.976 e Flash+7.028 bytes. Ring/event queue UART são alocações de
inicialização adicionais; não são pico de heap medido no target.
WCET/heap/latência/underruns sob tráfego UART físico continuam MANUAL.

P4 Performance final: PlatformIO RAM17.624/Flash668.608 bytes; mapa IDF
DIRAM235.452 bytes e imagem útil669.320 bytes. Arquivo `.bin`669.328 bytes,
SHA-256 `82eec9d3d630a0e1f2893e7b82c8e2c4eccd2d60bf0e8a2a17319f4a89889c79`.
A limitação do resumo de RAM
continua válida. O sdkconfig ES efetivo informa Flash40MHz e o cabeçalho de
upload DIO, apesar do pedido QIO/80MHz nos defaults; não foi forçado outro
modo/frequência para esta revisão. CPU360MHz e PSRAM200MHz selecionados.

O bloqueio de disco foi resolvido e imagem/touch foram confirmados pelo usuário.
Uma tentativa de limitar componentes IDF foi incompatível com a integração
PlatformIO e foi revertida; não entrou no firmware gravado. UART e NoteOn/Off
físicos e controles básicos aprovados no escopo acima; sustain/reconexão/soak
ainda pendentes.
O Console legado contém mutadores diretos de
PatchManager; edição simultânea por Console/P4 não está qualificada. Para os
testes da ponte usar apenas consultas de diagnóstico no Console.
A pinagem do codec/speaker do P4 não é
saída de linha para mixer e não participa deste MVP.

## Próximo menor passo e standalone

Isolar o crescimento de starvations/deadline misses em janelas controladas de
repouso/notas/macros/troca de patch. Validar sustain e perda de enlace P4/reconexão
preservando o teclado físico,
seguido de soak sob carga. Não anunciar
ensaio aprovado antes da aceitação musical física.

Incremento em validação para a falha USB: `CONFIG_SMKS3_P4_HEADLESS`, opt-in e
selecionado pelo perfil `esp32-s3-link`, usa DummyDisplay em PSRAM e mantém
UIManager/modelo para os bindings existentes, sem iniciar LCD/SPI/DMA nem
tarefa local de render8192 bytes. O perfil original mantém ST7789 e sua tarefa.
Não muda AMY, vozes, fila de áudio, sample rate ou bloco. Diagnóstico USB antes
do claim registra heap DMA livre/maior bloco e heap interno; não atribuir a
causa a uma alocação específica antes de medir e repetir hot-plug no target.
`./tests/run_smk_safety.ps1` e `./tests/run_smk_ui.ps1`: PASS, logs
`build/rehearsal_headless_safety.log` e `build/rehearsal_headless_ui.log`.
Build/upload, leitura de heap e três ciclos físicos de hot-plug executados abaixo.
O primeiro build manteve a opção ausente no sdkconfig em cache: não foi gravado
como headless. Configurações geradas anteriores foram copiadas para `build/`,
aplicada a nova opção y somente no perfil link e n no original, e executada
nova matriz. Verificado no cabeçalho gerado link o define=1 e no sdkconfig
original a opção desativada; CI recebeu build do perfil link e essa verificação.
Execução remota da CI permanece NOT RUN.
Nova matriz configurada: PASS, `build/rehearsal_headless_s3_configured.log`,
original99,24s/link379,27s. Resumos PlatformIO: originalRAM30.116/Flash1.142.507;
link headlessRAM32.024/Flash1.103.843 bytes. A diferença estática não mede os
buffers/tarefas evitados em runtime. `.bin` headless1.104.240 bytes, SHA256
`9bcb35ac0dbea9bee2ab204fa329b0607539957d7ef263937a7bc75c81b6c9b3`.
Comparação do sdkconfig anterior/novo: somente `CONFIG_SMKS3_P4_HEADLESS`
mudou; sem alteração incidental de áudio, GPIOs ou perfil de memória.
Upload headless: PASS, mesmo comando PIO de upload COM8,
`build/rehearsal_headless_s3_upload.log`,198,68s, hashes verificados. Esse upload
recompilou a imagem: RAM32.032/Flash1.103.935 bytes, `.bin`1.104.336 bytes,
SHA256 `ee4116a07162c89ae8732eecd407caf7239c4735d1fef45f6c5fe77e8d5ea9e8`.
Leitura real `status`/`audio_status`, `build/rehearsal_headless_s3_runtime.log`,
uptime66s, USB enumerado/reconnects1 e refs0: interno41.543 livres, maior bloco
28.672, PSRAM7.558.036 livres. Antes, com USB e notas,11.355 livres/maior7.680;
as cargas não são idênticas, mas a margem disponível aumentou nas leituras.
Boot novo: starvations80/deadlines3/max-render262.657us, cmd_drop/I2S errors/
timeouts0. Não calcular delta contra o boot anterior. Uma enumeração não
certifica hot-plug. P4 reconectou e snapshots23.479→23.529, sem novos erros de
quadros/driver/TX nas amostras; `build/rehearsal_headless_p4_reconnected.log`.
Primeira janela de monitoramento do reteste,
`build/rehearsal_headless_usb_hotplug.log`,55s: USB reconnects1/disconnects0,
EventsReceived1, refs0, peak0; nenhuma ação de hot-plug observada nessa janela.
Starvations80/deadlines3 e heap41.543/maior28.672 constantes entre leituras
separadas por48s. Não registrar PASS das três reconexões solicitadas sem
confirmar ação do operador e novos eventos/counters.
Posteriormente o operador informou que as três reconexões funcionaram sem
reiniciar: **PASS informado pelo operador** para esse roteiro. A consulta
seguinte, `build/rehearsal_headless_usb_hotplug_verified.log`, já mostrou
uptime3,975s, USB reconnects1/disconnects0/events1 e peak0: é outro boot e não
corrobora os contadores das três ações anteriores. Não atribuir o reinício
ao operador, alimentação ou abertura serial sem evidência. Iniciada captura
com a COM8 mantida aberta durante uma repetição, para observar timestamps,
enumerações, desconexões e eventual boot espontâneo.
**PASS da repetição com captura contínua:** três retiradas nos uptimes196,0s,
202,8s e209,0s; claims/reconexões em200,3s,205,7s e211,2s. VID4353/PID4B4D,
interface1, IN84. Cada claim: DMA36.679 livres/maior28.672 e interno44.467.
Depois: USB disconnects3/reconnects4, EventsReceived22, peak6610, sem parse/
drop/overflow/I2S errors/timeouts/cmd_drop; heap41.543/maior28.672 mantido.
Uptime avançou95→275s sem boot/timestamps regressivos; sem ESP_ERR_NO_MEM.
Deadline misses3 mantidas; starvations79→88 durante ciclos com Panic/reset de
perfil. O contador marca ausência de PCM válido, inclusive descarte por geração
após Panic, e não mede DMA underrun. Não atribuir todo aumento a falha auditiva
nem ocultar os zeros produzidos. No fim de uma leitura havia NoteRefs1; o
operador depois confirmou que soltou tudo e o som terminou. Não houve leitura
contemporânea de refs0 nessa confirmação. O perfil original com LCD não recebe
essa reserva de memória por padrão; o PASS de hot-plug vale para o perfil
headless medido, não para todas as configurações S3.
Captura P4 adicional180s, `build/rehearsal_p4_uart_loss_physical.log`:18.208
bytes, amostras connected1/state1, snapshots avançando, sem novos erros UART/
quadros/touch/flush, heap interno435.435, PSRAM31.191.472 e pool LVGL90.452
livres constantes. Uptime P4 chegou a124min; isso não comprova duas horas de
ensaio musical contínuo. Nenhuma amostra LINK OFF nessa janela; os dois casos
de perda UART foram solicitados ao operador e ainda não estão aprovados.

Standalone fica como investigação futura isolada: provar AMY/RISC-V/PCM,
I2S/saída de linha, USB MIDI Host e coexistência com tela/touch, medindo memória
e deadlines. O build genérico de upstream não comprova essas integrações.
Qualquer bloqueio mantém S3 como fonte de áudio; não substituir seu caminho
tocável por um port P4 não validado.
