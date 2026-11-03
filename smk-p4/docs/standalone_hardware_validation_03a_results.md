# SMK-P4 Standalone 0.3A — hardware validation

2026-11-02–03. Entrega de software e validação incremental de bancada. **Não aprovado para ensaio.**
PASS = executado com sucesso; FAIL = executado com falha; BLOCKED = dependência
identificada; NOT RUN = não executado; MANUAL = exige verificação física.

## Referência, escopo e identidade

HEAD inicial `0f2598f3fb5f57afd2e63a1c36fb380648c87e1d`, branch de referência
`feature/smk-p4-standalone-usb-midi`, working tree inicialmente limpa.
Nova branch `feature/smk-p4-standalone-hardware-validation`; commits desta
entrega consultáveis com `git log 0f2598f3..HEAD --oneline`.
Código e regressões: commit `d789677d` (`fix(p4): gate USB startup on confirmed
owner snapshot`) e `68f5faf4` (`fix(p4): trace rejected USB MIDI packets before validation`).
O commit documental posterior consolida os resultados. Sem reset/rebase,
alteração de main ou eFuses. Commit/push solicitado pelo operador em03/11.
Após a confirmação do
operador foram iniciadas captura serial, identificação ROM e backup integral
da Flash antes de qualquer gravação. O inventário inicial mostrou COM6/COM7
Bluetooth e COM13 USB VID:PID303A:1001, serial80:F1:B2:D3:CC:86.

O operador declarou "PCM e USB qualificados", confirmou P4 na COM13, DAC
"conforme indicado" e SMK25 conectado ao conector **HS** (corrigindo H5).
Isso registra a qualificação declarada e a ligação candidata GPIO50/51/52;
não é medição elétrica independente. VCC/straps e detalhes do circuito VBUS
não foram numericamente fornecidos; não são inferidos deste relato.
Controle0/HS é a seleção correspondente ao conector declarado.

`flash-id` confirmou novamente ES1.3, MAC80:f1:b2:d3:cc:86 e Flash16MB.
Captura pré-gravação de12s: firmware anterior em RAM_ONLY, ready1, frames0,
render_max274µs, service_max462µs, write_errors/timeouts/deadlines0,
heap interno275959/largest196608, PSRAM33554432/free31178568; sem nova
confirmação musical. Backup integral real: `build/p403a/flash_before_03a.bin`,
16777216bytes, SHA256
`aca97cc1fc41f8e497fe68b7f8038d993db8df8ae803803164401dd443bc994f`.
Esptool read-flash concluiu com exit0 em126,2s, incluindo verificação de digest
do protocolo. Esse backup inclui firmware, partições e dados da Flash anterior.

Lidos AGENTS.md, specs.md, README raiz/P4, relatórios 0.2/0.3, wiring, builds,
dependências, CI, integração AMY S3/P4, runtime, filas/parser/Host, PCM, LVGL e
testes existentes. Conflito explícito preservado: specs/AGENTS descrevem
S3/284×76; este pedido determina P4/800×480. Mudanças isoladas no standalone
P4; ambientes anteriores preservados. Marcadores de conflito preexistentes no
README raiz não foram editados.

## Entrega A — boot seguro

`Snapshot initial{}` explícito e resultado de `xQueuePeek()==pdTRUE` capturado
por polling limitado e testável. O mailbox estático é criado vazio, uma única
vez neste boot, e tem somente o owner como escritor. `owner_initialized` só
é publicado depois de concluir a tentativa de inicialização AMY/I²S. Não há
reutilização de mailbox entre owners nesta arquitetura.

O Host só inicia com gate elétrico/configuração USB habilitado, mailbox válido,
tarefa criada, snapshot recebido, inicialização confirmada, erro ESP_OK,
`running=true` e `engine.ready=true`. O Kconfig existente
`SMKP4_USB_WIRING_CONFIRMED` reúne opt-in e registro da qualificação do operador;
não foi criado um segundo switch nem inventada uma aprovação elétrica.

Timeout não consulta campos do payload para autorizar USB. Registra motivo,
mantém USB desligado até o próximo boot e prossegue para diagnóstico/UI; não
reinicia automaticamente. Criação de tarefa malsucedida não aguarda polling.
Mailbox indisponível não cria owner nem chama APIs de fila nula; tenta manter
display/UI e logs. Um owner que publique tardiamente pode atualizar o estado
AMY da UI, mas não inicia automaticamente USB. MIDI continua OFF nesse caso.
Falha real de `Host::begin` continua sendo ERROR, nunca ON.

Precisão da auditoria: as structs atuais já possuem inicializadores padrão de
membros, inclusive `running` e `engine.ready`; não foi comprovada leitura
indeterminada desses campos no HEAD inicial. A falha confirmada era ignorar o
resultado final do mailbox e não explicitar timeout/identidade de inicialização.
Os demais snapshots locais agora também usam `{}`; o snapshot de diagnóstico
fora do bloco de peek começa seguro. Nenhum novo acesso cross-task ao runtime.

Regressão cobre sucesso no último poll, timeout com payload plausível antigo,
mailbox ausente, tarefa ausente, snapshot sem confirmação, owner com erro,
owner parado, engine não pronto, USB desabilitado e falha publicada imediata.
Não testa FreeRTOS real nem certifica boot físico. A regressão da UI também
verifica explicitamente AUDIO RAM sem saída física e AUDIO RUN apenas com
`output_live=true`, usando o backend e widgets existentes com mock LVGL.

## Arquitetura e revisão P1

Mantidos USB → parser → fila USB → owner AMY → PCM e Touch → fila Touch →
mesmo owner. Sem alterações no core AMY, driver PCM, parser, lifecycle Host,
LVGL, layout, patches, efeitos ou dependências. A política nova é usada apenas
no boot; não aloca heap, adiciona locks ou trabalho DSP por bloco. O marcador
é copiado no mailbox existente. Custo/latência no P4: MANUAL.

Revisados SPSC/acquire-release, gerações, Off/Panic fora da fila Touch,
cancelamento USB por geração em perda de release, sustain por canal/origem,
limites de quatro vozes, descritores limitados e cleanup após callbacks.
Testes existentes exercitam fila cheia, payload parcial/malformado, falha de
claim/alloc/submit, callback em desconexão, retry e limpeza de recursos.
Nenhum outro defeito causal foi confirmado nesta revisão; não houve redesenho.

Limites preservados: `amy_start` upstream não oferece recuperação completa de
OOM em todas as alocações de boot. Reserva de admissão não prova recuperação.
Host retém recursos se um callback nunca retornar; stop pode aguardar sem
limite para evitar use-after-free. TSAN e fault injection no P4: NOT RUN.

## Build e configuração efetiva

PlatformIO existente / ESP-IDF 5.5.2 / LVGL 9.3.0, sem upgrades. Perfil P4 ES
<3.0 para a unidade anteriormente identificada como ES1.3; sem nova consulta
ROM. 48 kHz, PCM16 estéreo, bloco 256, DMA 4×256, timeout write 20 ms,
quatro vozes, ROM128/135, sem FX adicionais. Budget de compute: 5333,33 µs.

Conferir os arquivos gerados, não só defaults:
`smk-p4/.pio/build/guition-standalone/sdkconfig` e `config/sdkconfig.h`.
Estado do build seguro validado: Standalone=y, ESP32P4/ES<3=y; UART link, PCM wiring,
USB wiring, PCM diagnostic, sequência audível e AMY RAM self-test desligados.
GPIO50/51/52 continuam candidatos, sem acionamento de I²S nesse estado.
Controlador USB efetivamente executado: nenhum; seleção 0/HS é apenas padrão
condicional ao gate. Não equivale a seleção aprovada para a unidade.

O checker compila objetos isolados com nenhum gate, só USB, só PCM, ambos,
diagnóstico PCM e self-test RAM, além de trace/dump. Mantém o link ELF separado
com USB habilitado já existente. Não modifica sdkconfig, objetos/ELF/bin normais
nem produz firmware autorizado para bancada. É cobertura de compilação,
não execução dessa matriz em hardware.

## Entrega B — PCM5102A e saída L/R

Driver preservado: Philips 16-bit L/R, preload de zeros antes de enable,
auto-clear DMA, MCLK ausente e falha de escrita encerra owner/canal.
Testes host verificam silêncio, conversão/clipping, canal, rampas, frequência,
restart e falhas I²S injetadas. MANUAL: clocks, ruído, tensão, transientes,
polaridade real, separação L/R e confirmação auditiva.

| P4 / JP1 documental | PCM5102A | Situação real nesta rodada |
|---|---|---|
| GPIO50 / 11 | BCK | MANUAL: orientação/continuidade |
| GPIO51 / 9 | LCK/LRCK | MANUAL |
| GPIO52 / 7 | DIN | MANUAL |
| GND identificado | GND | MANUAL |
| Fonte validada do módulo | VCC | MANUAL: qualificação declarada; valores não registrados |
| Polarização validada | XSMT/FMT/SCK/DEMP/FLT | MANUAL: qualificação declarada; módulo não inspecionado pelo agente |

Pinagem declarada pelo operador como ligada conforme o guia; continuidade e
valores dos straps não foram medidos pelo agente.
`CONFIG_SMKP4_PCM_WIRING_CONFIRMED=n` no build seguro; perfil de bancada habilitado
somente depois da declaração de qualificação do operador.
Seguir [wiring](pcm5102_wiring.md): desenergizar; identificar pino1/face JP1;
medir continuidade; retirar todos os fios UART/I²S S3; confirmar GND, tensão,
regulador e straps. XSMT anteriormente vinha do mute S3: não deixá-lo flutuando.
Não assumir VCC3,3V/5V universal, não conectar 5V a GPIO, não ligar DAC ao speaker.

Após registro explícito do operador, sequência de builds/testes físicos:

1. Somente gate PCM + `PCM_DIAGNOSTIC=y`, `PCM_TEST_SEQUENCE=n`: transmitir
   zeros. Esperar frames crescentes, write_errors/timeouts zero; verificar
   início/parada, DC/ruído/transientes no mixer. Silêncio digital não prova mute.
2. Mesmo perfil, sequência=y, frequência440Hz, amplitude0,05FS: 2s silêncio,
   2s L, 2s R, 2s ambos, 2s silêncio; rampas20ms. Confirmar canais fisicamente.
   Osciloscópio: WS48kHz, BCLK esperado1,536MHz, atraso Philips de um bit.
3. Desligar diagnostic/sequence/self-test; manter PCM qualificado. Master/mixer
   baixos. ROM128 e135: C4, acorde4notas, Off, Panic, nova nota, patch e Master.
   Confirmar origem física P4→DAC com S3 e saídas auxiliares desconectados.
4. Durante notas: Master, patch, abrir/fechar Browser, oitava, Panic, tocar
   novamente. Registrar clicks/pops/interrupções/distorção/travas/notas presas/
   reset/erros I²S e reação touch, sem promover logs a aprovação auditiva.

## Entrega C — USB2/USB3, controlador, VBUS e enumeração

Reinspecionadas as imagens locais das folhas02/03 e consultados os links da
[folha03](https://github.com/ultramcu/guition-jc4880p443c-i-w/blob/master/schematic/03-expand-io_usb-fs-hs_lcd-backlight.webp),
[folha02](https://github.com/ultramcu/guition-jc4880p443c-i-w/blob/master/schematic/02-speaker_power-3v3_uart-input_battery.webp)
e [Host IDF5.5.2](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32p4/api-reference/peripherals/usb_host.html).
Esquema redistribuído no repositório de referência; correspondência à revisão
PCB real continua MANUAL. SDK instalado confirma índice0=HS/UTMI dedicado,
índice1=FS GPIO26D−/27D+, em `components/soc/esp32p4/usb_dwc_periph.c` e
`components/hal/esp32p4/include/hal/usb_dwc_ll.h`.

| Pergunta obrigatória | Evidência documental / resposta |
|---|---|
| Conector para SMK25? | Operador confirmou silk HS e SMK25 conectado; corresponde ao candidato USB3 do esquema |
| FS/HS? | USB2: nets USB1P1_N/P, marcado Full Speed; USB3: ESP_USB_N/P, marcado High Speed |
| Host em ES1.3? | SDK contém suporte pré-v3; operação real/PHY nessa unidade: MANUAL |
| VBUS recebe ou fornece? | Ambos ligados a USB5V_IN; CC1/CC2 com5,1kΩ GND sugerem sink; source não comprovado |
| Chave VBUS? | Nenhuma chave dedicada demonstrada nessas folhas; AO3401 da entrada VIN não prova chave Host |
| Sobrecorrente? | D4–D7 ESD não são limitadores; proteção/corrente disponível não qualificadas |
| Reversão? | Rede comum entre conectores; bloqueio de retorno entre fontes não comprovado |
| 5V teclado? | MANUAL: qualificação declarada; fonte/corrente de pico e circuito não registrados |
| Computador simultâneo? | PASS comunicação COM13 com Host HS ativo; isolamento elétrico/backfeed não medidos pelo agente |
| Adaptador externo? | Possivelmente necessário; proposta abaixo, sem montagem aprovada |

USB2 não é automaticamente "porta debug"; Serial/JTAG é controlador separado,
e a associação física/seleção PHY precisa ser comprovada. Não assumir que COM13
histórica identifica Host. Não gravar eFuses. IDF5.5.2 usa somente um Host por
vez; FS sob hub HS exige atenção à ausência de Transaction Translator no driver
de hub. Hub alimentado genérico não é solução garantida.

**Proposta externa mínima, ainda BLOCKED:** se não houver porta source segura,
projetar uma interface Host USB2 com acesso D+/D− comprovado do P4, GND comum,
saída USB-A para teclado e alimentação5V própria dimensionada. VBUS do teclado
deve passar por chave com limite de corrente/curto e bloqueio reverso, com ESD e
desacoplamento. Separar eletricamente VBUS dessa saída do USB5V_IN/debug da
Guition; alimentação da placa permanece em seu caminho validado. Se a interface
usar USB-C, resolver CC/source e os Rd existentes em projeto elétrico, não por
cabo/adaptador presumido. Acesso aos sinais, integridade de sinal, tensão,
corrente/pico, comportamento desligado e retorno de alimentação devem ser
validados por responsável de bancada. Não é recomendação de cabo Y improvisado,
nem autorização para injetar5V na USB-C; nenhum modelo comercial foi qualificado.
Mantém apenas um P4; não usa S3 nem ponte MIDI.

Só após aprovação: USB wiring=y, controlador correspondente à continuidade
real (0 apenas se HS/USB3 comprovado), descriptor log e MIDI trace temporários.
Capturar boot/controller/erro, VID/PID, bytes descritor, interface/alt/IN/MPS,
claim, primeira transferência válida, heap interno/maior bloco/PSRAM/stack USB.
Claim bem-sucedido é pré-condição do estado Connected; primeiro MIDI válido
exige trace e `events` crescentes, não apenas MIDI ON. Logs trace limitados a
4pacotes/5s; podem perder eventos e não são monitor sem perdas. Não presumir
VID/PID ou CC dos knobs. Capturar descritor completo externamente se >256bytes.

Teste de enumeração: conectar → nota On → Off → outra oitava → velocity →
sustain → desconectar sem notas → reconectar; repetir com nota/sustain ativos.
Esperado: cancelamento das notas USB, Touch preservado, nova enumeração/RX sem
reboot; nenhuma nota antiga deve reaparecer. Descritores reais, enumeração,
MIDI RX e hot-plug nesta rodada: pendentes de execução; qualificação foi
declarada pelo operador, mas não há medição elétrica independente do agente.

## Entregas D/E — música e estabilidade (MANUAL)

Cadeia a aprovar: SMK25→Host P4→AMY local→PCM5102A→mixer. ROM128/135,
C4, acorde4notas, notas rápidas/repetidas, velocity0, sustain, bend, CC1,
Panic, patch e teclado virtual simultâneo. Para identidade: segurar C4 USB,
pressionar/soltar C4 Touch, confirmar USB permanece, soltar USB; repetir ordem
inversa. Com pedal USB, liberar teclas, tocar/soltar Touch, confirmar independência
e liberar pedal. Release Touch não deve acionar Panic global.

Manter layout/macros desabilitadas. Verificar fisicamente MIDI OFF desconectado,
ON após claim/submit, ACTIVITY após eventos válidos e ERROR em falha; AUDIO RAM
sem I²S não é AUDIO RUN. Touch, patch, Master e Panic exigem confirmação musical.

Janelas do mesmo boot, com logs completos antes/depois e observação auditiva:
A repouso USB/display; B mono/acorde USB; C USB+Touch; D Master/patch/Browser;
E sustain/bend4vozes; F hot-plug; G30min contínuos, depois ensaio prolongado
somente se gates anteriores aprovados. Não zerar contadores entre janelas.
Registrar uptime/firmware/config e ações/horários; máximos são cumulativos,
diferença de máximos não é máximo da janela.

| Medida exigida | Campo/evidência disponível; situação nova |
|---|---|
| DSP médio/máximo | dsp_avg/max / render_avg/max_us; MANUAL |
| Aplicação de comandos/patch | cmd_avg/max, patch_max_us; MANUAL |
| Compute e deadlines | compute_deadlines, render_deadlines; MANUAL |
| Serviço/espera DMA | service_max_us, dma_avg/max; MANUAL |
| Refill/erros | refill_risks, write_errors, timeouts; MANUAL |
| Frames/nível/clipping | frames_written, blocks_rendered, peak, clipped; MANUAL |
| MIDI/USB/fila | events, errors, parse_errors, dropped, overflow, queue/high; MANUAL |
| Notas/polifonia | notes, polyphony_rejected; MANUAL |
| RAM/PSRAM | internal free/min/largest; PSRAM total/free/min/largest; MANUAL |
| Stack | stack_free_bytes owner / stack_free USB; MANUAL |
| Integridade heap | self-test RAM verifica separadamente; soak musical não tem contador automático; NOT RUN |
| CPU total/latência física | requer medição adicional no target; NOT RUN |
| Underrun DMA exato | UNMEASURED; refill_risks/write OK não provam áudio analógico limpo |

Reprovar com crash, stuck note, erro persistente USB/áudio, perda de comunicação,
OOM não recuperado, deterioração de heap, deadlines crescentes sem explicação,
interferência do display ou defeito audível sem causa. Métricas históricas0.2/0.3
não são medições0.3A. Nenhuma janela física nova foi executada.

## Recuperação e bancada reproduzível

1. Não gravar até identificar porta física correta, gates efetivos e preservar
   imagem/data funcionais existentes. A cópia local `build/p403a/baseline_firmware.bin`
   é o bin gerado de referência, SHA256
   `d2af803ab167648c2bf0fe920f7789ef54b198a38e977b631b48e8545ce732fc`;
   não é backup lido da Flash nem prova de firmware atualmente instalado.
   Preservar também bootloader, partições, configuração e dados do firmware
   funcional antes de qualquer upload futuro.
2. Identificar porta sem reset/upload:
   `C:/.platformio/penv/Scripts/python.exe -m serial.tools.list_ports -v`.
   Não reutilizar COM13 sem identificar novamente a unidade/conector.
3. Usar `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone -t menuconfig`,
   aplicar apenas gate já qualificado e recompilar. Inspecionar sdkconfig gerado.
   Upload só com porta confirmada e condições acima; nenhum upload nesta entrega.
4. Captura sem reset, com monitor fechado:
   `C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port <P4_PORT> --output build/p403a_physical.log --seconds 60`.
   Para30min usar `--seconds 1800`; anotar cenários e observações separadamente.
5. Se falhar: baixar mixer, preservar logs/config/contadores. Panic, soltar
   teclas/pedal e confirmar notes0; USB ERROR: desconectar, conferir alimentação,
   reconectar e observar enumeração. Não usar reset automático para esconder causa.
   Se I²S parar, guardar erro antes de reboot manual com volume baixo.
6. O backup integral real foi lido de0x0,16MiB. Para recuperação desse backup,
   com COM13 novamente identificada, usar esptool `write-flash 0 build/p403a/flash_before_03a.bin`
   no P4, sem confundir esse arquivo completo com uma aplicação a0x10000.
   Não foi necessário restaurá-lo nesta rodada. Fallback híbrido exige montagem anterior
   aprovada, e não constitui standalone P4 aprovado. Gates podem voltar a n para
   diagnóstico seguro em RAM. Não conectar duas saídas digitais no mesmo fio.

## Validação automatizada e resultados

Logs locais ignorados em `build/p403a/`; mocks não são hardware.

| Status | Comando da raiz | Resultado |
|---|---|---|
| PASS | `./smk-p4/tests/run_host.ps1` | baseline e após alteração; novo boot gate + geometria/PCM/mock I²S +9 testes Python |
| PASS | `./smk-p4/tests/run_amy.ps1` | baseline e final; ROM128/135 reais, musical USB/Touch, sustain/bend/mod, stress2produtores, heap, backend/UI incluindo AUDIO RAM/RUN |
| PASS | `./smk-p4/tests/run_usb.ps1` | baseline; descritores, lifecycle e falhas/cleanup/hot-plug com mocks |
| PASS | `./tests/run_smk_safety.ps1` | baseline; MIDI/USB/áudio/console/clock/ownership S3 |
| PASS | `./tests/run_smk_amy_boot.ps1 -Legacy` | baseline; probes de limitações legacy esperados |
| PASS | `./tests/run_smk_ui.ps1` | baseline; UI S3 |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone -e guition-display -e guition-link` | baseline: standalone56,96s; display43,09s; link35,63s |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-devkitc-1 -e esp32-s3-link` |179,89s /171,71s; fontes S3/core sem alterações |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone -e guition-display -e guition-link` | final completo: display610,28s; link576,62s; standalone573,93s |
| PASS | `C:/.platformio/penv/Scripts/python.exe smk-p4/tests/check_standalone_build.py` | configuração segura efetiva, matriz de compilação e link USB isolado |
| PASS | `C:/.platformio/penv/Scripts/python.exe -m serial.tools.list_ports -v` | inventário, sem abrir porta; não é teste P4 |
| NOT RUN | CI remota / TSAN / demais plataformas upstream | sem execução nesta rodada |
| MANUAL | Sinais digitais, L/R analógico, SMK25 real, janelas A–G | registrar cada etapa física separadamente após a declaração do operador |

Uma primeira invocação em lote de Legacy expandiu incorretamente o argumento
PowerShell e falhou no binding antes de iniciar testes; comando direto acima
executado depois com exit0. Não foi falha de firmware nem PASS do comando
inválido. Warnings host preexistentes de pointer-to-int no AMY foram mantidos.
As execuções de AMY/USB/S3 validam a base preservada; regressão nova de boot usa
o runner host. A alteração adicional na suíte AMY é somente a checagem visual
RAM/RUN; backend/runtime/driver/UI produtivos permanecem preservados.

Binário seguro final gerado antes dos commits: RAM estática29856bytes,
Flash1063756bytes, bin1064480bytes (<factory3MiB), SHA256
`2930c2230617197b5fa4ff77042cf937735bb4eea17032a6549e7d0c8d09a8eb`.
Versus0.3: RAM estática sem aumento; Flash+1142bytes. Isso não mede heap runtime.
Artefatos seguros/configuração preservados em `build/p403a/safe_image/`.
Os nomes PlatformIO reais são `bootloader.bin` e `partitions.bin` na raiz do
build; os caminhos em subpastas do manifesto ESP-IDF não são os arquivos PIO.

## Execução física após declaração do operador

Comandos executados após confirmação COM13/HS/DAC:

```powershell
C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port COM13 --output build/p403a/preflash_runtime.log --seconds 12
C:/.platformio/penv/Scripts/python.exe -m esptool --chip esp32p4 --port COM13 --baud 115200 --no-stub --after hard-reset flash-id
C:/.platformio/penv/Scripts/python.exe -m esptool --chip esp32p4 --port COM13 --baud 460800 --after hard-reset read-flash 0 0x1000000 build/p403a/flash_before_03a.bin --no-progress
```

PASS: captura prévia, ROM/Flash e backup integral. ROM query/read resetaram a
placa; não escreveram Flash nem eFuses. Primeiro perfil de bancada preparado
no sdkconfig gerado: PCM wiring=y, diagnostic=y, sequência=n, USB=n, self-test=n.
Defaults versionados continuam seguros/desligados. Os resultados desse perfil
são registrados separadamente do build seguro acima.

### PCM etapa A — driver real transmitindo zeros

PASS build do perfil de silêncio:440,87s. Primeira tentativa de upload falhou
por `UnicodeEncodeError` cp1252 durante saída do esptool; houve início da etapa
de gravação e ela não foi considerada completa. Encerrada somente a árvore
de processos dessa tentativa, mantido backup e repetido com
`PYTHONIOENCODING=utf-8` e `PYTHONUTF8=1`, conforme README.

PASS upload UTF-8 na COM13 em84,50s, com verificação de hash pelo esptool.
Argumentos efetivos PlatformIO: DIO/80MHz, tamanho detectado16MB, bootloader
0x2000, partitions0x8000, aplicação0x10000. O manifesto ESP-IDF informa40MHz,
mas o comando real PIO e o cabeçalho anterior são80MHz; não foi confundido o
manifesto nativo com o que efetivamente foi gravado. Sem eFuses.

Imagem realmente gravada/preservada em `build/p403a/pcm_silence_image/`, SHA256
`8c357813c72df958f33721150ea099d1186f49adddf49225f4462ec29db078b7`.
Ela difere do hash do primeiro build304b75... porque o upload PIO regenerou a
descrição da aplicação. Fonte permanece no commitd789677d, sem alteração DSP.

PASS escrita I²S real: captura de24s após upload, frames até1939200, blocks7575,
write_errors/timeouts/render_deadlines/compute_deadlines/refill_risks0;
geração média2449µs/máximo2727µs; command_max13µs, DMA médio2742/máximo2821µs,
service_max5446µs incluindo espera DMA. Stack owner9492bytes livres;
heap interno411199/largest245760, PSRAM livre31194424/largest30932992.
Sem erros touch/flush; refresh2395 avançando. ENGINE ready0 é esperado no
diagnóstico, que não inicializa AMY; USB state0 é esperado/desabilitado.
O log contém início parcial de boot e telemetria; não se alegou captura integral
do boot. Arquivo `build/p403a/pcm_silence_runtime.log`.

Sinais BCLK/WS/DATA, DC/ruído/transientes analógicos e aprovação auditiva:
MANUAL, observação solicitada ao operador. DMA_underruns continua UNMEASURED.
PASS da API de escrita não promove a saída analógica a PASS.

PASS janela adicional de silêncio, sem reset: captura120s com amostras de
186795ms/8899072frames a301795ms/14419200frames; delta observado115000ms/
5520128frames. Erros/timeouts/deadlines/refill permaneceram0; heap e stack
inalterados nas amostras. Não é soak de30min do sistema musical completo.
Arquivo `build/p403a/pcm_silence_steady.log`. Observação analógica solicitada
ao operador, sem presumir resposta pela passagem do tempo.

### PCM etapa B — sequência L/R no P4

PASS build de canais e upload na COM13 em87,06s, hashes de gravação verificados.
Perfil: PCM=y, diagnostic=y, sequence=y,440Hz,0,05FS, USB=n. Imagem preservada
em `build/p403a/pcm_channels_image/`, SHA256
`bcdb3ca2b712e6ebbdf691df3d99636d34aa398aa523a19df76f1731ae1c8169`.
PASS captura35s `build/p403a/pcm_channels_runtime.log`: até1459200frames,
5700blocos; geração média2457/máximo2752µs; command_max14µs; DMA médio2735/
máximo2804µs; service_max5459µs; erros/timeouts/deadlines/refill0.
Heap/PSRAM/stack iguais ao silêncio nas amostras, touch_errors/flush_errors0.
ENGINE peak0 é snapshot AMY inativo neste modo, não medição da amplitude do tom.
Separação física L/R, qualidade analógica e touch solicitado ao operador;
nenhuma resposta foi presumida. USB permanece off nessa imagem.

Próximo perfil preparado sem mudar fontes/defaults: diagnóstico/sequence/self-test
off, PCM=y, USB=y, controller0/HS, descriptor log/trace=y. Kconfig oculta/remove
as linhas USB no perfil diagnóstico; configuração musical foi reintroduzida
explicitamente e deve ser conferida no header gerado antes de gravar.

### AMY + I²S + Host HS reais

PASS build255,69s e upload UTF-8 COM13 em88,20s, hashes de gravação verificados.
Configuração efetiva: PCM/USB=y, controller0/HS, descriptor log/trace=y,
diagnostic/sequence/self-test/UART=n. Imagem/configuração preservadas em
`build/p403a/musical_image/`, SHA256
`aa60de7cd83d0b0d1e1fb24eb51bb24fb31d44f9fc58883a2a4527784e9be6e9`.
RAM estática30204bytes, Flash1156798bytes no build inicial desse perfil;
binário gravado1157504bytes (<factory3MiB); imagem de diagnóstico não foi
confundida com o candidato musical.

PASS boot positivo registrado em `build/p403a/musical_boot.log`:
1392ms `USB boot gate: owner ready; wiring qualification enabled` e
`Starting qualified USB Host controller=0`; depois GT911/Performance prontos.
ENGINE ready1, patch128, I2S_PRODUCING. Primeiros60s sem erros I²S/timeouts,
render/compute deadlines ou refill_risks. DSP máximo272µs em repouso, command
máximo64µs, DMA máximo5204µs, service_max5669µs incluindo espera. Owner stack7440,
USB stack5520bytes livres; heap interno254139/largest180224,
PSRAM livre31177688/largest30932992, estáveis nas amostras.

**FAIL primeira enumeração física / causa não determinada:** Host state1=Disconnected, connects0,
disconnects0, errors0, events0. Nenhum VID/PID/descritor/interface/endpoint foi
recebido apesar de o operador informar SMK25 conectado em HS. Não é Connected,
nem PASS de USB MIDI. Solicitados alimentação/estado aceso, tipo de cabo/adaptador
e um ciclo de reconexão com Host já ativo; captura separada sem reset.
Sem troca cega de controlador, eFuses, injeção de5V ou quirk de descritor.

Projeto de referência P4SDM inspecionado somente para leitura: também seleciona
bit0/HS, e seu M23 declara conexão física/USB/VBUS não qualificados; não há
evidência transferível de SMK25 funcionando nele. Não se copiou código.

### Retorno do operador e cabo corrigido

Operador confirmou explicitamente: silêncio normal e tela/touch funcionando;
L/R corretos, som limpo e touch funcionando; C4 na Performance com som pelo DAC
e release normal. **PASS auditivo declarado pelo operador**, separado dos logs.
Não houve medição de BCLK/WS/nível/DC pelo agente. Também informou que o cabo
USB estava mal conectado e foi corrigido.

Nova captura sem reset `build/p403a/usb_after_cable.log`: **PASS enumeração real**
VID4353/PID4b4d, endereço1, configuração165bytes, interface MIDI1 número1,
alternate0, endpoint IN0x84, MPS64. Descritor completo cabe no dump256bytes e
está preservado no log. Claim e primeiro submit ocorreram (`connects` cresceu),
mas o parser rejeitou transferências recebidas: até16 parse_errors/USB errors
e17 aberturas/reaberturas. Isso não representa17 ciclos físicos de hot-plug.
MIDI events permaneceu0, sem aprovação de Note On/Off do teclado.

**FAIL parsing real a investigar:** a primeira implementação de trace só
registrava pacotes após validar a transferência; rejeições ficavam invisíveis.
Alteração isolada em UsbInput para registrar bytes antes do filtro, incluindo
fragmentos de1–3bytes com tamanho explícito e padding zero apenas no log.
Continua rejeitando a transferência inteira antes de publicar qualquer nota.
Mantém32registros SPSC,4logs/5s, sem heap/log em callback ou mudança AMY/DSP.
Trace passa de8 para12bytes por registro (+128bytes fixos); contador de perda
permanece separado de MIDI dropped. USB+AMY host regressions PASS, incluindo
transferência inválida de8bytes e parcial de3bytes sem notas publicadas.
Ainda não foi assumido que a causa é cabo virtual/CIN ou tamanho parcial.

PASS captura adicional150s `build/p403a/usb_hotplug_attempt.log`, sem reset:
última amostra256842ms/12261120frames; notas0, connects/disconnects/events/errors0,
sem crescimento de erros áudio/deadlines/refill, heap/stack estáveis. Nenhuma
ação física de reconexão foi confirmada pelo operador durante essa janela;
portanto não é PASS nem reprovação de recuperação após hot-plug. É somente
registro de Host aguardando dispositivo. A hipótese de cabo/alimentação/CC
permanece hipótese, não diagnóstico elétrico confirmado.

### Comandos de bancada executados

Usados `PYTHONIOENCODING=utf-8` e `PYTHONUTF8=1` após a falha inicial.
Cada build abaixo corresponde à configuração de fase descrita acima.

| Estado | Comando da raiz | Fase |
|---|---|---|
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone` | silêncio440,87s; canais246,80s; musical255,69s |
| FAIL | `C:/.platformio/penv/Scripts/pio.exe run -v -d smk-p4 -e guition-standalone -t upload --upload-port COM13` | primeira tentativa silêncio, cp1252; encerrada árvore de processos dessa tentativa |
| PASS | mesmo comando de upload com UTF-8 | silêncio84,50s, hashes verificados |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone -t upload --upload-port COM13` | canais87,06s; musical88,20s, hashes verificados |
| PASS | `C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port COM13 --output build/p403a/pcm_silence_runtime.log --seconds 24 --reset` | captura real, início parcial e telemetria |
| PASS | `C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port COM13 --output build/p403a/pcm_silence_steady.log --seconds 120` | silêncio, sem reset |
| PASS | `C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port COM13 --output build/p403a/pcm_channels_runtime.log --seconds 35` | canais, boot/telemetria |
| PASS | `C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port COM13 --output build/p403a/musical_boot.log --seconds 60` | boot AMY/I²S/HS e falha de detecção |
| PASS | `C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port COM13 --output build/p403a/usb_hotplug_attempt.log --seconds 150` | aguardando ação física, nenhum dispositivo observado |
| PASS | `git diff --check` / `git diff --cached --check` | revisão final de whitespace/diff |

Capturar bytes com sucesso não promove o conteúdo a aprovação funcional.
Estado instalado ao encerrar esta coleta: **candidato musical AMY+PCM+USB HS**,
descriptor/trace ativos, sem tom automático. As imagens anteriores estão
arquivadas; nada foi gravado no S3 e nenhum codec ES8311 foi inicializado.

### Firmware de trace de rejeições — 03/11

PASS testes `run_usb.ps1` e `run_amy.ps1` após a alteração. Primeiro build
nativo FAIL por disco C: cheio (`Errno28`) ao gerar sections.ld, sem erro C/C++.
Liberados somente caches gerados de compatibilidade com comandos PlatformIO:
`pio run -d smk-p4 -e guition-display -e guition-link -t clean` e
`pio run -d smk-s3 -e esp32-s3-devkitc-1 -e esp32-s3-link -t clean`.
Definições dos ambientes, fontes, imagens P4 arquivadas e backup integral
preservados; não se limpou outro worktree nem dados pessoais.

PASS rebuild242,60s e upload240,45s após liberar espaço. Configuração musical
PCM+USB HS e diagnóstico bruto mantida, parser ainda inalterado. Firmware
SHA256 `19a0786795c49faa7d1c9f3f9cc67919010f9ba429d863b73cc63aa9b1f80ac7`,
RAM estática30332bytes, Flash1157102bytes. Captura120s
`build/p403a/rejected_trace_runtime.log`: Host Connected, connects1, events0,
parse_errors0 em repouso, sem novo input observado. Esses zeros após atualização
não apagam as16 falhas registradas no boot anterior nem provam correção do parser.
Próximo teste precisa de uma tecla real para capturar o pacote causador.

### Checkpoint publicado e mudança posterior na placa

Commit/push solicitado pelo operador: checkpoint documental `fe0071a9` publicado
em `origin/feature/smk-p4-standalone-hardware-validation`, junto aos dois commits
de código. Árvore limpa após publicação.

Na captura seguinte `build/p403a/real_key_rejected.log`, a COM13 passou a emitir
somente DISPLAY_TEST/heap, sem AUDIO/ENGINE/USB: heap interno447115bytes,
LVGL livre87056 e uptime reiniciado. Esse fluxo não corresponde ao candidato
standalone observado em `rejected_trace_runtime.log` (Host Connected e heap
~250KB). A imagem da placa mudou entre as janelas; não se atribuiu isso ao
parser MIDI nem se usou essa captura para aprovar/reprovar uma tecla SMK25.
Nenhuma nova gravação foi iniciada após constatar a diferença. Solicitado ao
operador decidir entre manter a imagem atual ou retomar o standalone, para
não sobrescrever outra tarefa. O diagnóstico de pacote depende dessa definição.

## Gates e próximo passo

Gate1: PASS testes de política e boot positivo real do candidato musical,
com owner confirmado antes do Host; falhas injetadas validadas apenas no host.
Gate2: PASS driver e confirmação auditiva do operador para silêncio/L/R/C4 Touch
com release; sinais elétricos quantitativos MANUAL. Gate3: PASS enumeração e
descritores reais após corrigir cabo mal conectado; FAIL parsing durante input,
em diagnóstico, sem Note On/Off válido ainda. Hot-plug musical permanece MANUAL.
Gate4: BLOCKED cadeia musical completa. Gate5: NOT RUN30min completos sob carga;
apenas janelas de repouso/diagnóstico medidas. Qualificação declarada não
substitui evidência funcional. Não apto para palco, release estável ou ensaio.

Próximo menor passo: capturar tecla real no trace anterior à validação, corrigir
somente a causa comprovada e repetir Note On/Off; depois sustain/bend/mod,
identidade Touch+USB, Panic, hot-plug e30min sob carga.
Não substituir dados elétricos por troca de controlador ou injeção de5V.

## Atualização de bancada — 2026-11-03 (substitui o checkpoint acima)

O operador escolheu retomar standalone e diagnóstico MIDI. Antes da gravação,
a imagem de display encontrada na placa foi preservada integralmente em
`build/p403a/flash_before_resume_trace.bin` (16 MiB), SHA256
`a17653a1cf83758a47fe8011da370da5fa5432793b6046617c8287c5cccf0bc9`.

### MIDI real e testes musicais

A captura contínua mostrou pares de eventos equivalentes nos cabos virtuais 0 e 1:
`09 90 3c 50 / 19 90 3c 50`, depois `08 80 3c 40 / 18 80 3c 40`.
O parser rejeitava o cabo não selecionado e descartava a transferência inteira.
Commit `fe1d44cb` passou a ignorar e contar cabos auxiliares, roteando somente 0,
sem duplicar notas. Pacotes malformados no cabo selecionado continuam rejeitados.
Testes reproduzem esses bytes reais, incluindo isolamento de resets auxiliares.
Cabos virtuais fazem parte do formato USB MIDI 1.0, seção 4:
https://www.usb.org/sites/default/files/midi10.pdf.

MANUAL — operador confirmou som limpo pelo PCM5102A em nota/release, acorde de
quatro notas, velocidades suave/forte, Pitch Bend e Modulation. Confirmou C4 de
USB e Touch com releases independentes nas duas ordens; Master, patches 128/135,
Browser, oitava, Panic seguido de nova nota e sustain independente do Touch.
A captura `build/p403a/cable_fixed_runtime.log` registrou 3974 eventos selecionados
e 3974 pacotes auxiliares ignorados, sem erro de parsing, drop MIDI ou overflow.
`trace_dropped` é perda do diagnóstico limitado, não descarte de eventos musicais.
A polifonia permanece limitada a quatro vozes; rejeições por limite são contadas.

### Correção da idade de eventos

Sob input real apareceu `latency_max_ms=4294967295`: o produtor podia marcar um
evento 1 ms depois do instante amostrado pelo consumidor antes de drenar a fila.
A subtração unsigned também podia expirar uma nota recém-chegada. Commit
`1360fec8` usa idade zero para timestamp futuro dentro da comparação modular
existente. Testes AMY reais cobrem USB, Touch, expiração legítima e wrap de relógio.
PASS `./smk-p4/tests/run_amy.ps1` e build standalone. Imagem arquivada em
`build/p403a/time_fixed_image`, SHA256
`c67b3c56efb4de443dc771319bdfb551261f8a11ef664bd25c17afc150ec57b5`.

### Hot-plug: recuperação funcional, contagem incorreta detectada

O operador confirmou explicitamente um ciclo no HS com nota mantida: a nota
parou ao retirar e voltou a tocar após reconectar. Contudo, a captura contínua
`build/p403a/final_soak_runtime.log` mostrou connects=2, disconnects=0, errors=2.
Esse resultado não aprova hot-plug nem soak. A janela foi preservada e interrompida
para correção; não se apagaram os erros. Até uptime 1221845 ms: 149 eventos,
149 pacotes auxiliares, parsing/drop/overflow=0, latência máxima observada 6 ms,
DSP máximo 1804 us, render/compute deadlines=0, clipping/OOM=0, erros/timeouts
I2S=0. DMA underruns continuam UNMEASURED; latência é idade da fila, não medição
física tecla–saída.

O SDK define NO_DEVICE como transferência cujo dispositivo desapareceu e pode
entregar esse callback antes de DEV_GONE. O Host tratava essa conclusão como erro,
fechava o handle e perdia a contagem posterior; ainda tentava o endereço removido.
A correção centraliza cancelamento e contagem idempotentes nas duas notificações,
aguarda NEW_DEV e mantém o drain antes de liberar recursos. PASS
`./smk-p4/tests/run_usb.ps1`: regressões nas duas ordens de callback, uma única
mudança de geração/Panic, zero erro, ausência de retry após 2 s e reconexão.
Build e confirmação física dessa correção ainda pendentes neste checkpoint.

### Estado atual dos gates

Gate1 PASS. Gate2 PASS funcional por procedimento e confirmação do operador;
dados elétricos quantitativos ainda MANUAL. Gate3 PASS parsing e execução MIDI;
hot-plug corrigido em teste mock, confirmação no novo firmware pendente. Gate4
MANUAL confirmado para os controles e independência descritos. Gate5 NOT RUN:
nenhuma janela contínua de 30 min aceita após as correções. Não declarar release
estável ou aptidão para palco com base nestes checkpoints.

Não houve mudança no core AMY, driver PCM, dependências ou nova UI. A alteração
fica na aplicação/adaptador P4, com operações USB na tarefa proprietária e sem
novo trabalho no render de áudio. Permanecem não fornecidos os valores de VCC,
straps e detalhes quantitativos da origem/proteção VBUS; a qualificação elétrica
foi declarada pelo operador. Não se introduziu ligação ou injeção de 5 V.

PASS build nativo da correção de hot-plug:
`C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone`,
231,89 s; Flash 1157206 bytes. Imagem arquivada em
`build/p403a/hotplug_fixed_image`, SHA256
`eeafaf9b05384189445febc5400358e4e8e43bb5aa4f99f6469647623503325b`.
Configuração preserva PCM GPIO50/51/52 e Host HS controlador0 qualificados pelo
operador. `git diff --check` PASS. A confirmação física da correção e o soak
continuam pendentes; o resultado de compilação não os substitui.

### Janela concluída após hotplug fix — evidência ainda divergente

PASS gravação COM13 da imagem acima com hashes verificados. Captura contínua
`build/p403a/hotplug_fixed_soak_runtime.log`: 372 amostras por subsistema,
1855 s entre primeira e última; 89039872 frames adicionais (~47999,93 frames/s).
PASS repouso por mais de 30 min: sem reset observado, render/compute deadlines,
refill risks, write errors, timeouts, clipping, OOM e overflows todos zero;
DSP máximo 284 us. Entretanto, Host permaneceu Disconnected, connects=0,
disconnects=0, events=0. Portanto FAIL critérios de evidência MIDI/hot-plug do
soak integrado; não equivale a falha auditiva demonstrada nem a aprovação musical.
Análise reproduzível:
`C:/.platformio/penv/Scripts/python.exe build/p403a/analyze_final_soak.py build/p403a/hotplug_fixed_soak_runtime.log --output build/p403a/hotplug_fixed_soak_analysis.json`.

O operador respondeu que os três ciclos funcionaram. Essa confirmação é
registrada como MANUAL, mas diverge dos contadores, ainda zerados em captura
posterior no mesmo boot (~3801 s). Não houve regravação ou reset para esconder
a divergência. Nova captura `build/p403a/hotplug_fixed_followup.log` aberta sem
reset; solicitada verificação do conector/dispositivo e C4 Touch com cabo HS
retirado antes de qualquer nova mudança. Próximo menor passo: resolver essa
divergência e observar enumeração/input/ciclos reais nesta imagem, depois obter
30 min integrados. Gate3 hot-plug e Gate5 permanecem pendentes.
