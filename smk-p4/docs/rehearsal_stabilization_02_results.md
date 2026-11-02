# SMK-P4 Rehearsal Stabilization 0.2 — investigação e qualificação

Data: 2026-11-02. **Versão 0.2 NÃO aprovada para ensaio.** Correções e
diagnóstico em desenvolvimento; aprovação física/auditiva não pode ser deduzida
das regressões host, builds ou uptime. Não há par rotulado estável.

## 1. Branch, versões e baseline

Branch: `feature/smk-p4-rehearsal-stabilization`, criada de
`86ae835d7594e750124d59ed71e53233af5b681a`
(`feature/smk-p4-rehearsal-mvp`). Árvore inicialmente limpa. Alterações desta
rodada versionadas a pedido do usuário; nenhum reset/rebase/force-push.
O único commit do MVP sobre `614a3eb5` foi inspecionado, incluindo o relatório
das gravações e três ciclos USB anteriores.

Commits desta rodada:

- `70fcf994` — `fix(smk-link): prioritize touch leases and urgent releases`.
- `fix(smk-audio): publish owner diagnostics and record rehearsal evidence` —
  instrumentação, correção NoteRefs, coletor e relatório (commit que contém
  esta atualização).

Os firmwares cujos hashes estão abaixo foram compilados/gravados antes desses
commits, a partir do working tree baseado em `86ae835d`. A metadata `dirty`
registrada identifica essas imagens efetivamente testadas. Commit/push não
recompilam, regravam nem promovem as imagens para release estável.

Lidos AGENTS, specs, README raiz, relatórios MVP, READMEs P4/protocolo,
ligação UART, configuração/dependências, integração AMY/ESP-IDF, testes e CI.
Conflitos preexistentes: specs prevê LCD S3 284×76 e bloco inicial128;
o pedido atual usa P4 800×480 e bloco256. Prevaleceu o pedido, sem alterar
os ambientes originais. README raiz já contém marcadores AMY/voxP4;
não foram resolvidos nesta tarefa.

Firmware previamente aprovado no escopo limitado do MVP, conforme relatório
e hashes dos arquivos encontrados antes dos builds desta rodada:

| Dispositivo/perfil | SHA-256 da aplicação `.bin` anterior |
|---|---|
| S3 headless `esp32-s3-link`, 1.104.336 bytes | `ee4116a07162c89ae8732eecd407caf7239c4735d1fef45f6c5fe77e8d5ea9e8` |
| P4 `guition-link`, 669.328 bytes | `82eec9d3d630a0e1f2893e7b82c8e2c4eccd2d60bf0e8a2a17319f4a89889c79` |

Portas identificadas novamente: S3 COM8, CH343 VID:PID1A86:55D3,
serial5A4E121397; P4 COM13, VID:PID303A:1001, serial80:F1:B2:D3:CC:86.
Não se presume que os números COM sejam permanentes.

**PASS — identidade do firmware efetivamente instalado:** aplicações lidas da
Flash, offset0x10000, antes da substituição, coincidiram integralmente com os
dois hashes acima. Backups locais em `build/stab02_baseline_images/`:
`s3-running.bin` e `p4-running.bin`. São aplicações, não backups de toda Flash/NVS.
S3 ROM identificou ESP32-S3 rev0.2, MACb8:f8:62:e3:e4:a4; P4 rev1.3,
MAC80:f1:b2:d3:cc:86. Leitura reiniciou as placas de modo documentado;
não foi reset para consultar contadores. A primeira tentativa S3 falhou por
encoding de stdout no Windows; repetida com `PYTHONIOENCODING=utf-8`, sem
escrita Flash, completou. Logs `stab02_backup_s3*.log`/`stab02_backup_p4.log`.

**PASS — baseline host:** todas as oito suítes abaixo executadas antes de
alterar implementação. **PASS — baseline builds:** original S3
106,58s/link82,51s; original P4 51,90s/link31,08s.
IDF5.5.2/plataforma55.03.36-1 preservados. Logs locais ignorados:
`build/stab02_baseline_*`. Nenhuma CI remota executada.

## 2. Medição real antes da correção

**PASS, limitado — S3 em execução, consultas somente `status`/`audio_status`,
sem reset:** `build/stab02_baseline_live_s3.log`.
Mesmo fluxo de captura, timestamps de log677,5→689,6s; firmware legado não
expõe boot ID/uptime explícitos, portanto não atribuir um ID inventado a estas observações.

| Métrica | Antes | Depois / delta |
|---|---:|---:|
| Configuração | 48kHz / 256 frames | igual |
| Frames produzidos | 32.441.088 | 33.022.208 / +581.120 |
| EWMA serviço do owner | 631µs | 632µs |
| Máximo histórico | 262.672µs | igual |
| Deadline misses | 3 | 3 / +0 |
| PCM starvation | 76 | 76 / +0 |
| I²S erros/timeouts | 0/0 | 0/0 |
| Comandos descartados | 0 | 0 |
| RAM interna livre/maior bloco | 45.763 / 28.672 bytes | igual |
| PSRAM livre/maior bloco | 7.558.780 / 7.471.104 bytes | igual |
| Notas / peak / eventos USB | 0 / 0 / 0 | sem execução musical confirmada |
| Enlace P4 | connected1 | sem erros/rejeições/drops observados |

Isso é uma janela de repouso, sem confirmação auditiva ou SMK25 enumerado.
Não reproduziu o crescimento sob controles do relatório anterior.
**BLOCKED — captura serial P4:** COM13 abriu, mas forneceu zero bytes em20s
usando o capturador existente sem reset. Isso não comprova falha de display,
touch ou UART: o S3 reportava a sessão conectada. Não houve PASS visual novo.

## 3. Semântica auditada: respostas às sete perguntas de áudio

1. `max_render_us` no modo AMY buffered vem de `serviceBlock()` em
   `smk-s3/components/audio/amy_commands.cpp`. Não mede somente `amy_render()`.
2. Começa depois de verificar espaço na fila PCM e antes de aplicar até16
   comandos; termina depois de `renderEngine()` e da cópia estéreo para a fila.
   Inclui comandos, patch/Panic, deltas, DSP/FX, ganho e telemetria de amostras,
   cópia e qualquer preempção/espera ocorrida dentro dessas chamadas.
   A espera explícita do owner por buffers/semaphore fica fora da região.
   A escrita bloqueante I²S também fica fora, em outra tarefa.
3. `beginEngine()` fica fora, mas a primeira materialização de deltas/FX e os
   presets enviados após iniciar o worker podem entrar. Troca de patch entra.
4. Sim, o relógio monotônico continua durante suspensão/preempção da tarefa;
   o máximo observado pode atravessá-las. Não existe medição de tempo de CPU
   DSP que permita excluir essa hipótese na captura legada.
5. Relógio `esp_timer_get_time()` em microssegundos, subtração64bits e duração
   armazenada32bits.262ms não é explicado por unidades ou wrap aparente;
   durações superiores a~71min poderiam truncar32bits, sem evidência disso.
6. `synth_pcm_starvations` conta toda saída zerada por falta de bloco válido,
   incluindo partida, fila vazia e descarte por geração após Panic/preset.
   Não é sensor de DMA underrun. Os dois blocos PCM e quatro DMA permanecem.
7. Existe risco de silêncio audível se o owner deixar de produzir PCM por
   mais tempo que a reserva de buffers. I²S pode receber zeros sem erro de
   escrita. O máximo262ms e o crescimento anterior não foram absolvidos.
   A origem física exata exige a medição instrumentada abaixo.

AMY usa lock de fila na plataforma ESP; o adapter mantém único owner.
Comandos de patch podem alocar/materializar recursos e FX podem inicializar
memória. Flash/USB/preempção são hipóteses, não causas comprovadas nesta rodada.
Não foram alteradas prioridades, afinidade, heap, polifonia, DMA ou reservas
para tentar ocultar o problema. Nenhuma otimização especulativa do core.

## 4. Causas comprovadas e correções de comunicação

**FAIL reproduzido antes da correção / PASS depois:** teste determinístico com
nota Touch e novos controles em todo ciclo10ms. `Client::request()` aceitava
o próximo controle antes de `next()` reservar GET_STATE; a nota expirava apesar
de enlace saudável. Log `build/stab02_heartbeat_reproducer.log` registra a
asserção de nota mantida falhando na versão anterior.

Correção: comandos visuais/contínuos aguardam quando a sondagem200ms está
devida. O protocolo, lease600ms e prioridades musicais permanecem.

Outros casos cobertos por regressão:

- Release não aguarda250ms×retries de um comando sem ACK: nova sessão HELLO
  existente cancela o comando pendente e libera exclusivamente Touch no S3.
  Panic explícito fica latched até poder ser enviado após HELLO_ACK.
- P4 descarta TX ainda não transmitida antes de HELLO; bytes já em FIFO
  precedem a barreira. SLIP END recupera quadros parcialmente transmitidos.
  Descartes intencionais entram no contador TX drops.
- Resposta que chega após1500ms não ressuscita uma sessão antes da checagem
  de timeout. UI limpa estado enquanto offline e solicita snapshot novo.
- Nenhum Note On redundante, replay de patch/controle ou extensão do lease.
  Atrasos não limitados ainda podem expirar uma nota; um heartbeat posterior
  não a religa. Após gesto interrompido, soltar e pressionar novamente.

Só o cliente, a ponte P4 e o diagnóstico do adapter/Console mudaram.
AMY core, protocolo v1/formato persistente e saída PCM5102A preservados.

**Segundo defeito comprovado — NoteRefs atrasado:** dois ciclos automatizados
de acordes via Console exibiram zero referências durante notas e quatro após
release, com som PCM produzido (peak9895) e render voltando ao repouso.
O controle publicava o contador imediatamente depois de enfileirar o comando,
antes de o owner executá-lo. Teste AMY real reproduziu a divergência
(`build/stab02_note_refs_reproducer.log`, asserção de1 referência falha).
Correção: somente o owner publica após aplicar comandos, e zera no shutdown.
Foram removidas as duas publicações antecipadas do controle. Isso corrige
telemetria, não muda note ownership, envelopes ou a política musical.

## 5. Instrumentação e interpretação

Opt-in Kconfig `SMKS3_AUDIO_PHASE_PROBE_AT_BOOT`, defaultn; o perfil de bancada
`sdkconfig.defaults.link` opta pory para medir também os primeiros blocos.
O original S3 continua sem probe ao boot. É obrigatório conferir o cabeçalho
gerado, pois sdkconfig em cache pode manter a opção antiga.
`audio_status probe_on|probe_off` alterna sem resetar histórico. Não se adicionou
log por bloco, alocação, arquivo ou mutex à coleta do áudio.

| Campo Console | Significado |
|---|---|
| `boot_id`, `uptime_us`, `elf_sha256` | Identidade do boot (mesma do HELLO), tempo64bits e identificação ELF; SHA do `.bin` é outra informação |
| `commands_max_us` | Máximo da fase de comandos, incluindo instrumentação/mono snapshot |
| `patch_count`, `patch_max_us`, `panic_max_us` | Comandos Preset executados e máximos individuais de Preset/Panic |
| `engine_max_us` | `renderEngine`: deltas, inicialização FX quando necessária, DSP, mistura/conversão e headroom; NÃO CPU DSP isolada |
| `copy_max_us` | Cópia para slot PCM e marcação da geração |
| `probe_blocks`, `probe_service_us` | Soma32bits e contagem de blocos medidos para média por delta; soma faz wrap em~71min de serviço acumulado |
| `pcm_discarded` | Blocos PCM rejeitados por geração antiga |
| `gap_startup` | Zeros antes da primeira entrega válida |
| `gap_barrier` | Zeros enquanto a geração solicitada difere da última entregue |
| `gap_empty` | Zeros após partida, sem mudança pendente de geração |

As três classes de gap continuam representando falta real de PCM válido;
barreira não significa automaticamente inaudível/aceitável. A classificação
não altera `starvation`. Medidas são amostras atômicas independentes, não uma
transação coerente; pode haver pequeno desvio entre campos enquanto são lidos.
Máximos são históricos desde o boot/reset explícito, e não máximos de janela.
Não há medida de espera por buffer, atraso de escalonamento ou CPU DSP isolada.
Essas lacunas devem ser mantidas explícitas ao atribuir causalidade.

Coletor: `smk-p4/tests/rehearsal_windows.py`, comandos no README P4. Mantém raw
log e JSON, separa boots/configurações, rejeita resets suspeitos e lacunas>60s.
Não envia reset, alteração de patch ou notas. Só envia consultas, com toggle
de probe se `--probe` foi solicitado. Nunca chamar `audio_reset`/`diag_reset`
entre as amostras. Deltas zero não são confirmação auditiva.

## 6. Regressões host, builds e memória

Comandos na raiz, logs em `build/` ignorado. Status host e build são separados
de hardware. As suítes baseline e finais usam os mesmos comandos:

| Status host | Comando |
|---|---|
| PASS | `./tests/run_smk_link.ps1` — protocolo, Touch, ACK/retries, expiração, controles contínuos, UART TX saturada/parcial e barreira de sessão |
| PASS | `./tests/run_smk_safety.ps1` — MIDI, USB, filas, Panic, áudio, clock e ownership |
| PASS | `./tests/run_smk_amy_boot.ps1` — AMY real; owner exclusivo, referências, sustain, Panic e probe com relógio determinístico |
| PASS | `./tests/run_smk_performance.ps1` |
| PASS | `./tests/run_smk_patch_integrity.ps1` |
| PASS | `./tests/run_smk_synth_expansion.ps1` |
| PASS | `./tests/run_smk_ui.ps1` |
| PASS | `./smk-p4/tests/run_host.ps1` — geometria/dirty/fences e quatro testes Python de janelas |
| PASS | `C:/.platformio/penv/Scripts/python.exe -m unittest discover -s smk-p4/tests -p test_rehearsal_windows.py` |
| NOT RUN | CI GitHub e matriz remota IDF5.3/5.5.2, blocos128/256 |

O teste de fase injeta6000µs na aplicação de patch e1200µs no render: observa
7200µs de serviço, incremento do deadline e métricas de cada fase. Não é
benchmark de velocidade da CPU. Inclui probe desligado sem reset, gaps de
partida/fila vazia/barreira e descarte de dois blocos antigos após Panic.
Host AMY tem os warnings preexistentes de casts de ponteiro64→32bits; não se
atribui desempenho target aos tempos host.

Primeira matriz S3 instrumentada: **FAIL**, faltava declarar o componente IDF
`esp_app_format` para `esp_app_desc.h`. Corrigido em CMake; é componente do SDK
existente, não nova biblioteca externa. Resultados finais e hashes abaixo.

## 6a. Bancada instrumentada intermediária — boot1742246591

**PASS — gravação de desenvolvimento:** S3COM8 e P4COM13, ambientes link,
com identificação de chip e hashes verificados por esptool. Logs
`build/stab02_upload_s3.log`/`stab02_upload_p4.log`. Não é release estável.
Esta primeira imagem de diagnóstico usava prefixo ELF `8f0f10b75` (limite do
helper IDF). A versão final imprime os32bytes completos diretamente do app
descriptor, independentemente de `CONFIG_APP_RETRIEVE_LEN_ELF_SHA`.

**PASS, limitado — startup instrumentado:** máximo de serviço261.630µs;
commands_max251.579µs; engine_max10.043µs; copy_max57µs;
patch_count3, patch_max147.237µs, panic_max4292µs. O pico de~262ms é dominado
pela aplicação de comandos, não por uma chamada DSP de262ms. Máximos de fase
são independentes, não devem ser somados como um mesmo bloco.
Permanecem possíveis esperas/preempção dentro da aplicação de patch.

Inspeção confirma três seleções no startup após o worker já iniciar:
`PatchManager::begin()` seleciona0, `SceneManager::begin()` aplica a cena e a
restauração NVS seleciona o patch salvo. Cada seleção passa por `loadPreset()`.
Não se removeu uma dessas seleções sem qualificar a configuração final das
cenas/NVS; isso não é prova de que todas sejam redundantes semanticamente.

Ao primeiro diagnóstico: starvation79 = gap_barrier63 + gap_empty16 + startup0,
quatro blocos antigos descartados, deadlines3, I²S erros/timeouts0.
A classificação cobre o contexto das lacunas; gaps de barreira podem ocorrer
enquanto o carregamento é lento e não foram declarados inaudíveis.

**PASS, limitado — A sem USB:** quatro snapshots de uptime19,57→49,56s;
boot/config constantes, três janelas~10s, deltas starvation/deadline/I²S/drops0.
Média de serviço por delta~640µs. Interna45.915→45.699bytes na primeira consulta
e depois constante, maior bloco28.672; PSRAM7.558.780 constante.
P4 conectado, notas/eventosUSB0. Não é cenário A completo com SMK25 enumerado.
Raw/JSON `build/stab02_window_A.*`.

**PASS, limitado — B Console:** dois ciclos de quatro notas60/64/67/72,
velocity80, com releases; cinco snapshots no mesmo boot, uptime88,20→117,26s.
Todos os deltas de falhas de áudio/comandos permaneceram0; média de serviço
por janela1.664→2.395µs, EWMA observado durante notas~2825µs, peak9895
(-10,4dBFS), clipping0, heaps constantes. Os `note_refs` dessa captura estavam
sujeitos ao bug documentado acima; não servem para aprovar releases físicos.
Raw/JSON `build/stab02_window_B_console.*`. Não é teste SMK25 nem auditivo.

**PASS, limitado — P4 após gravação:** serial voltou a fornecer bytes,
snapshots56→181 e refresh3613→5121 nas observações estáveis;
touch_errors/flush_errors0, heap interno435.435, PSRAM31.191.472,
LVGL free90.520/maior81.176/peak45.244 constantes.
Contadores históricos bad_frames231/client_errors154 cresceram enquanto S3
estava em gravação/reinicialização, e ficaram constantes após reconectar.
Não foram zerados nem classificados como falha elétrica comprovada.
Logs `build/stab02_p4_runtime.log`/`stab02_p4_steady.log`.

## 6b. Última imagem gravada — boot2723531568

**PASS — gravação e identificação:** último S3 `esp32-s3-link` naCOM8,
`build/stab02_upload_s3_owner_final.log`,107,74s, três regiões verificadas pelo
esptool. Aplicação ELF
`7b45c889ec2d1a6f8c784e02f07cd1d3b07eb1f25d0151a29e7123b3e78ff069`
coincide entre `esptool image-info` do arquivo final e `audio_status` no target.
Metadata IDF: `1.2.149-61-g86ae835d-dirty`; diagnóstico da aplicação mantém
`Version=0.1.0`. Nenhuma delas foi renomeada para release estável0.2.

**PASS, limitado — correção NoteRefs no target:** capturados32s, cinco
snapshots de uptime31,53→55,56s, após dois ciclos de notas60/64/67/72,
velocity80. Sequência observada `0 → 4 → 0 → 4 → 0`; estado posterior também0.
Comandos enviados nos tempos relativos0/4/10/17/24s (`audio_status`),1/13s
(quatro `note_on <nota> 80`),7/20s (quatro `note_off <nota>`),28s (`status`).
Um bloco `finally` repetiu os quatro Note Off para limpeza.
Não se usou Console para mutar PatchManager simultaneamente ao P4.

Raw/JSON: `build/stab02_window_B_owner_final.*`. Quatro janelas do MESMO boot:
deltas starvation/deadline/I²S/drops/discardes0, frames+193.024/+287.232/
+337.408/+335.360; médias de serviço~2319/1818/1934/1635µs.
EWMA~2845µs durante notas e~637µs depois; peak10.017, clipping0.
Interna45.951→45.735bytes após primeira consulta e depois constante,
maior bloco28.672, PSRAM7.558.780 constante. USB ausente, eventos0.
Não existe aprovação auditiva, de SMK25 ou soak nessa medição.

Startup deste boot: máximo259.885µs, comandos249.759µs,
patch individual146.087µs (três presets), engine10.117µs, cópia58µs.
Starvation77 = barreira61 + fila vazia16; quatro blocos antigos descartados;
deadlines3. **Não subtrair77 de79 do boot intermediário.** Em ambos os boots
instrumentados a anomalia ocorreu antes da janela musical sintética e foi
dominada pela fase de comandos. Troca de patch sob execução permanece MANUAL.

**PASS, limitado — P4 simultâneo:** seis observações completas da janela
estável, snapshots2690→2815, connected/state1; contadores históricos
bad_frames621/client_errors311 constantes. Cresceram novamente durante a
última gravação/reset S3, não foram ocultados. Erros RX/TX drops/I²S0;
touch/flush0 e mesmos heaps/LVGL da captura anterior. Primeiro fragmento
serial antigo/truncado foi excluído da comparação.

## 6c. Builds finais e hashes de desenvolvimento

| Status | Comando | Resultado |
|---|---|---|
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-link -e esp32-s3-devkitc-1` | Matriz após correção owner: original66,04s/link61,51s; `build/stab02_s3_owner_matrix_final.log` |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-link -e guition-display` | Matriz pós-correção inicial54,46s/69,20s; `build/stab02_final_p4.log` |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-display` | Recompilação original após último ajuste do cliente48,59s; `build/stab02_original_p4_final.log` |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-link -t upload --upload-port COM13` | Último cliente recompilado/gravado com hashes verificados62,10s; `build/stab02_upload_p4.log` |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-link -t upload --upload-port COM8` | Último diagnóstico/owner recompilado e gravado107,74s; `build/stab02_upload_s3_owner_final.log` |
| PASS | `git diff --check` | Sem erros de whitespace |

Cabeçalhos gerados conferidos: link S3 HEADLESS1/PHASE_PROBE1, bloco256,
CPU240MHz, UARTTX4/RX5; original HEADLESS e PHASE_PROBE desativados, bloco256.
Sem warnings novos relevantes nos builds target finais. Nenhum ambiente
original removido, upgrade de SDK ou mudança na pinagem.

| Imagem efetivamente gravada | Tamanho `.bin` | RAM/Flash reportadas no build de upload | SHA-256 `.bin` |
|---|---:|---|---|
| `smk-s3/.pio/build/esp32-s3-link/firmware.bin` | 1.106.672 | 32.072 / 1.106.263 bytes | `64120f0edb285aad9fa355db25cf174329da88e4197e2572c0b4c88b05648b91` |
| `smk-p4/.pio/build/guition-link/firmware.bin` | 669.504 | 17.624 / 668.788 bytes | `52adee1eda7e38296dce54a12ac18a13e9e67ce6e615d42ecf63e229a0663f41` |

Original S3 final: RAM30.196/Flash1.145.579bytes; original P4:
RAM12.968/Flash576.804bytes. Esses resumos não equivalem à memória runtime
total nem medem heap DMA. Contra os arquivos anteriores, as aplicações link
cresceram2336bytes (S3) e176bytes (P4); não é uma medição de latência.
Binários de builds intermediários diferem por código/metadata; os hashes desta
tabela foram lidos APÓS o último upload. Não congelar esses arquivos como par
de recuperação estável sem aprovação física.

Comandos de backup executados com `PYTHONIOENCODING=utf-8`, Python PlatformIO:
`-m esptool --chip esp32s3 --port COM8 --baud 460800 --after hard-reset read-flash 0x10000 1104336 build/stab02_baseline_images/s3-running.bin`
e `-m esptool --chip esp32p4 --port COM13 --baud 460800 --after hard-reset read-flash 0x10000 669328 build/stab02_baseline_images/p4-running.bin`.
Foram executados também `-m serial.tools.list_ports -v`, capturador P4 sem
`--reset`, `-m esptool image-info` e `Get-FileHash -Algorithm SHA256`.
Esses arquivos/JSON/raw logs são locais e ignorados pelo Git; as observações
principais e identidades estão registradas neste relatório versionável.

## 6d. SMK25 conectado — confirmação posterior na mesma sessão

O operador informou PCM5102A na saída e SMK25 conectado ao USB Host.
Foi feita nova consulta, sem reset, gravação, notas sintéticas ou mutações de
patch. Boot2723531568/ELF final permanecem os mesmos. Logs novos:
`build/stab02_usb_connected_check.log` e `build/stab02_window_USB_live.*`.

**PASS, diagnóstico USB/MIDI:** `usb_connected=1`, reconnects1/disconnects0;
eventos recebidos1→421 entre os dois `status`, depois517. Notas observadas
durante execução, retornando a0. Parsing/queue overflow/drops/submit failures/
transfer errors0, clipping0, I²S erros/timeouts0; P4 conectado sem erros novos
do backend. Isso atualiza a condição anterior, que era USB ausente nas capturas
antigas; não altera retroativamente seu resultado.

**FAIL de orçamento temporal sob execução recebida:** no mesmo fluxo de
captura, timestamps1141,3→1153,3s (~12,05s), deadline misses3→17 (+14).
Na leitura seguinte o total era22, com mais96 eventos USB recebidos.
Starvation79 e gaps de fila vazia16 permaneceram constantes nas leituras;
patch_count3 e máximos históricos não mudaram. EWMA chegou a4034µs (75,6%
do orçamento) durante notas. Os novos deadlines não são explicados pelo pico
histórico do boot, e não houve incremento concomitante de zeros PCM observado.
Não equivalem automaticamente a underrun DMA ou defeito audível.
Máximos acumulados ainda não isolam a fase desses novos misses individualmente.

**PASS, limitado — janela posterior em repouso com USB enumerado:** quatro
snapshots, notas0, eventos517 estáveis, deadlines22/starvation79 constantes;
deltas de falhas0. Interna41.515/maior28.672bytes e PSRAM7.558.036 estáveis.
Sem reset ou troca de firmware entre as duas capturas. Uptime não comprova
execução musical durante essa janela de repouso.

**PASS físico/auditivo informado pelo operador, 2026-11-02:** confirmou
"Som e releases normais, sem falhas audíveis" nessa execução do SMK25.
Essa confirmação cobre som e encerramento das notas no trecho observado;
não transforma os deadlines crescentes em falsos positivos nem qualifica soak.
**MANUAL — tipo/carga exata das notas, sustain, hot-plug e perda UART:** ainda
sem aprovação nova. Próximo teste controlado deve
separar notas isoladas, acordes de4 e8 notas e trocas/controles, mantendo o patch
e anotando o que o operador ouve. P0 temporal continua aberto.

## 7. MIDI, sustain, ownership e UI

**PASS host AMY real:** C4 física + C4 Touch; soltar Touch preserva referência
física; pedal físico mantém somente a física após releases; soltar pedal limpa.
Perda Touch com FIFO saturada usa latch seletivo, sem Panic global. Panic limpa
pedal/ownership e permite novo Note On/Off. Testes existentes preservam
arpejador/sequenciador, fan-out, referências, releases e rejeição de replay.

**PASS host USB existente:** enumeração simulada, falhas de alocação/submit,
disconnect/flush/reconnect e Panic com fila cheia. Não substitui hot-plug físico.
A política USB existente segue Panic global; a correção de UART não muda isso.

**MANUAL:** testes físicos A–E de sustain, UART/USB e mesma altura entre fontes;
latência auditiva, responsividade LVGL, sliders, browser e duas horas de música.
Nenhum redesign, animação, ajuste de refresh/buffer/display ou reconhecimento
ACK APPLIED foi introduzido. ACK continua RECEIVED; AUDIO RUN indica produção,
não som analógico comprovado. Três hot-plugs anteriores continuam evidência do
MVP, não aprovação desta rodada sob carga.

## 8. Procedimento de bancada e janelas A–F

Manter48kHz/256frames, AMY Single, S3 headless, USB Host e PCM5102A.
Conferir o perfil gerado e identificar portas/placas antes de cada gravação.
Depois de aquecer, capturar32s por cenário, consultas a cada10s; coletar status
USB/P4 antes/depois em capturas separadas, sem outros monitores na mesma porta.
Anotar patch, ações, fontes, pedal, volume, tempo musical efetivo e pausas.
Preservar o raw log completo, boot ID, uptime e observação auditiva do operador.

- **A — repouso:** P4 atualizando, SMK25 enumerado, sem notas; distinguir deste
  baseline que não tinha USB enumerado. Esperado: frames avançam e nenhum
  crescimento inexplicado de gaps/deadlines, I²S erros/timeouts ou drops.
- **B — notas:** isoladas, acordes, oitavas e repetição rápida pelo SMK25;
  incluir release, velocity0, bend/mod. Confirmar eventos USB crescendo,
  referências encerrando após envelopes/pedal e nenhuma nota presa.
- **C — controles:** cutoff, resonance, envelopes, reverb, chorus, master,
  primeiro separados, depois simultâneos com notas Touch por até10s e USB.
  Esperado: nenhum release prematuro Touch com enlace saudável, sem duplicar
  Note On. O limite defensivo10s continua intencional.
- **D — patch:** sem notas, com notas, cauda reverb e novas notas imediatas.
  Correlacionar patch_count/max, comandos/engine max e gaps de barreira;
  não tratar silêncio mensurável como aprovado sem avaliação auditiva.
- **E — geradores:** arp, sequencer e bateria existentes, isolados/combinados
  e com teclado físico. Conferir Stop/Panic e ausência de notas antigas.
- **F — falhas:** seguir exatamente os passos abaixo; não simular intervenção
  física com scripts nem usar uptime como evidência de música executada.

## 9. Falhas e recuperação sem ferramentas de desenvolvimento

Conexão/montagem inicial: com alimentação desligada, conferir os três fios da
[ligação validada](uart_wiring.md), DAC e saída de linha. Não unir alimentação
das placas. Baixar volume do mixer; alimentar S3 e P4 pelas próprias fontes,
aguardar inicialização, conectar SMK25 no USB Host com VBUS da montagem já
qualificada. Confirmar LINK ON/nome do patch e áudio em volume baixo; selecionar
patch inicial conhecido e elevar MASTER/mixer gradualmente. Não há sequência
de alimentação eletricamente certificada além da montagem de bancada existente.

1. **Notas/estado inconsistentes:** Panic na tela, verificar silêncio e tocar/
   soltar novamente. Não executar salvamento ou reset automático durante música.
2. **UART:** com ambas alimentadas e GND comum mantido, segurar C4 física e
   C4 Touch; retirar só TX/RX. Em~600ms a Touch perde lease; em~1500ms o estado
   conectado deve expirar. A física/pedal e geradores permanecem válidos.
   Recolocar TX/RX: HELLO/snapshot novos, nenhum replay de nota/patch/slider.
   Soltar e pressionar Touch novamente; soltar física conforme sustain.
3. **P4 reset:** manter alimentação S3/P4 e GND, usar RESET do P4. Repetir
   isolamento anterior e confirmar física contínua/snapshot e toque novo.
   Não desligar uma placa mantendo GPIOs da outra ativos: comportamento
   elétrico não qualificado.
4. **USB:** conectar antes/depois do boot; desligar/reconectar USB com notas/
   sustain; repetir após bateria/patch e com P4 ativo. Panic de disconnect deve
   limpar notas e pedal; nova enumeração permite tocar sem reboot. Registrar
   pelo menos10 ciclos sob carga, heap DMA/interno e maior bloco do log pré-claim,
   falhas de claim/submit, contadores USB, MIDI e áudio. Se falhar, Panic e
   reconectar; reboot S3 é último recurso, registrar a falha como tal.
5. **S3 reset manual:** baixar volume do mixer, parar geradores, reiniciar S3,
   aguardar USB/patch/enlace, confirmar notas/releases e recuperar volume.
   Novo boot inicia série separada; não subtrair seus contadores dos anteriores.

**MANUAL — smoke:** boot, tela, touch, link, patch real, áudio USB/Touch,
macros/master/patch/Panic e releases. **MANUAL — falhas:** testes acima e
sustain A–E do pedido. Só depois, **MANUAL — até2h sob carga musical real**, com
pausas anotadas, acordes, pedal, bend/mod, macros, bateria/arp/seq e touch ativa.
Não aprovar se houver crash, nota presa, perda MIDI, recuperação falha, alocação
USB/I²S com erro, defeito audível não explicado, crescimento inexplicado de
gaps/deadlines em regime ou tendência de vazamento.

## 10. Hardware, riscos e próximo menor passo

Pinagem preservada: UART P4 TX52/RX51 e S3 TX4/RX5; I²S15/16/17 com mute18;
painel ST7701S/GT911 existente. Sem novas suposições sobre revisão da PCB,
VBUS, straps PCM5102A, saída analógica ou isolamento de I/O.
Sem dependência externa nova, formato persistente ou mudança no core AMY.

P0 de áudio permanece aberto: o pico foi localizado na fase de comandos do
startup, mas falta decompor suas esperas e explicar os deltas sob controles e
música. Frequência/severidade durante execução e impacto auditivo ainda não
medidos; reprodução anterior envolve controles/patch/Panic sem isolamento. Recuperação conservadora:
Panic, verificar patch/estado e reinício manual se necessário. Essa incerteza
impede declarar aptidão para ensaio. Comunicação corrigida e sustain coberto
em host ainda precisam de aprovação física.

Próximo menor passo: executar janelas C–D, UART/sustain e A–B com SMK25 junto
ao operador, usando o perfil já instrumentado. Se uma causa
for comprovada, adicionar regressão e fazer correção localizada; só depois
hot-plug sob carga e ensaio prolongado. Firmware pareado de recuperação
`SMK-S3-Rehearsal-0.2`/`SMK-P4-Rehearsal-0.2` somente após gates P0 físicos.
