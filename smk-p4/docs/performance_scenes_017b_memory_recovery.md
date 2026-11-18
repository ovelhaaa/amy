# 0.17B — recuperação de RAM interna

**MEMORY RECOVERY CANDIDATE — HARDWARE RETEST REQUIRED.** Release **NO-GO** até medir as margens na Guition. O alvo deste trabalho é o perfil P4 da [PR #47](https://github.com/ovelhaaa/amy/pull/47). A especificação geral do projeto descreve o S3. A mudança fica no port P4.

**Atualização física de 18/11/2026. A1 PASS** em repouso. **A3 parcial PASS** para enumeração e notas MIDI. A qualificação musical e sob carga segue pendente. Veja as capturas registradas abaixo.

## Baseline e orçamento

O HEAD remoto inicial era `8677c7589c43ce1078eaf41375240ce736346b21`. A captura física preservada do firmware bench anterior mostrou `ADMISSION free=337619 largest=245760 required=352256 admitted=0`, `AUDIO FAIL ... ESP_ERR_NO_MEM` e `ENGINE ready=0`. O déficit de heap interno era 14.637 B. O maior bloco excedia o piso por 49.152 B. O proprietário de áudio bloqueou a chamada a `Runtime::begin()`. Consulte o [registro físico original](performance_scenes_017b_first_hardware_session.md).

Em `runtime_config.h` e `Runtime::bootFreeInternalBytes()`, a base histórica P4 é 196.608 + (8 − 4) × 18.432 = 270.336 B. A capacidade de reserva é 8 × (16 × 32 + 33 × 7 + 2 × 8) = 6.072 deltas. São três blocos de 2.048 deltas. A base histórica já inclui o primeiro bloco, e os outros dois acrescentam 2 × 2.048 × 20 = 81.920 B no ABI RV32. O outro termo soma o orçamento de engine de 240 KiB e a reserva posterior de 64 KiB, totalizando 311.296 B. O máximo entre 352.256 e 311.296 B determina o piso de admissão. Os 196.608 B contíguos vêm da guarda histórica de quatro vozes. O código preserva ambos os pisos e, após iniciar o AMY, verifica 65.536 B livres e 32.768 B contíguos antes de permitir a saída.

O linker map do bench anterior associa `.bss.work_mem_int.0` ao alocador TLSF do LVGL. Ele ocupava 131.072 B em DRAM1 desde o carregamento do firmware, mesmo antes de `lv_init()`. O ESP-IDF reserva DRAM1 até `_heap_start_high`, portanto esse consumidor reduz o heap disponível para a admissão. O crescimento de Scenes também pressiona DRAM0, especialmente pelo adaptador, cujo catálogo e cópias de captura fazem parte do isolamento existente. A contribuição de cada alocação dinâmica ao valor físico de 337.619 B segue sem decomposição.

## Inventário do bench

Os tamanhos estáticos vêm do map do firmware anterior. As capacidades configuradas de stacks e buffers excluem overhead de alocador, TCBs e descritores.

| Consumidor | Tamanho | Memória anterior | Momento | Migração |
| --- | ---: | --- | --- | --- |
| Pool LVGL TLSF | 131.072 B | DRAM1 BSS | Boot | PSRAM no port de display |
| Runtime, incluindo filas MIDI, sequenciador, estado de Scene e editor | 14.920 B | DRAM0 data | Boot | Acesso no owner e no render. Mantido |
| Mailbox snapshot | 2.896 B + 84 B de controle | DRAM0 BSS | Boot | Cópia do contrato entre tarefas. Mantida |
| Snapshot de trabalho | 2.896 B | PSRAM com fallback interno | Início do owner | Já prefere PSRAM |
| Adaptador Blueprint, incluindo catálogos, captura e comparação | 19.416 B | DRAM0 BSS | Boot | Candidato futuro no controle. Mantido |
| Objeto Blueprint | 276 B | DRAM0 BSS | Boot | Mantido |
| Handles NVS de Scenes, Patterns, FX e presets | 12 + 12 + 12 + 16 B | DRAM0 data | Boot. NVS aberta após o owner | Dinâmica NVS interna a medir |
| Backend local com estado de controle e presets | `sizeof(Backend)` no serial | PSRAM com fallback interno | Antes do owner | Já prefere PSRAM |
| Tarefas IDF preexistentes | main 24.576 B, esp_timer 3.584 B, idle 1.536 B por core. Timer FreeRTOS configurado em 2.048 B | RAM interna | Antes de app_main | Mantidas. Quantidade e pilhas reais a confirmar na captura |
| Audio Owner | Stack 12.800 B, incluindo handoff PCM de 3 × 1.024 B de payload | RAM interna | Antes da admissão | Mantido |
| AMY, instrumentos e FIFOs | 8 × (252 + 2 × 40) = 2.656 B, mais tabelas para 9 IDs de synth | Heap interno pelas caps de synth | `Runtime::begin()` | Mantidos |
| AMY, estado dos osciladores | 64 osciladores de voz × (356 + 52) = 26.112 B antes dos breakpoints. Cada breakpoint soma 8 B. Chorus e loader têm alocações adicionais | Heap interno pelas caps de events | `Runtime::begin()` e loader existente | Mantido |
| AMY, bancos e temporários DSP | Quatro buffers PCM de 1.024 B. FBL e feedback somam 24.576 B em quatro buses e dois cores. Outros estados e o pico ficam para a placa | Heap interno pelas caps de block, fbl e synth | `Runtime::begin()` | Mantidos |
| AMY, fila e blocos de deltas | Bloco de 40.960 B, mínimo de três blocos para a reserva explícita | Heap interno 8-bit | `Runtime::begin()` | Mantido. Fallback AMY para deltas desabilitado neste alvo |
| AMY, delays, samples e SysEx | Echo usa 2 × 65.536 amostras, mais chorus/reverb e overhead. Total a medir | Payload PSRAM. Metadados internos conforme a cap de cada alocação | `Runtime::begin()` | Já separados pelas caps |
| Saída PCM | Stack 4.096 B, 4 × 1.024 B de payload DMA | RAM interna e DMA | Após a reserva pós-AMY | Mantida |
| USB Host | Objeto 100 B e stack 6.144 B. Fila de 8 mensagens, EP0 até 256 B e RX dimensionado pelo endpoint. Overhead IDF a medir | Objeto DRAM0, stack interna e transferências DMA. USB DMA em PSRAM desabilitado | Objeto no boot. Driver e task após NVS se áudio pronto. RX na enumeração | Mantido |
| Display | Draw de 51.200 B, três framebuffers de 768.000 B e metadados de driver | Draw e framebuffers em PSRAM | Após USB | Mantidos |
| Fontes e tabelas constantes | Mapeadas em Flash via DROM | Flash | Link | Mantidas |
| Bench | Contadores em snapshots e filas existentes. Novo contador atômico de 4 B | Interna | Boot | Mantido |

## Mudança e observação

O LVGL 9.3 conserva o mesmo pool TLSF. O port aloca 128 KiB alinhados em PSRAM 8-bit antes de iniciar o display, entrega o ponteiro pelo hook `LV_MEM_POOL_ALLOC` e mantém o pool durante a vida da UI. Se não houver um bloco externo suficiente, o display retorna falha com log. Se o hardware de display falhar antes de `lv_init()`, o pool é liberado. O pool serve aos objetos da UI. DMA, draw buffers, dados de render, stacks, reservas e partições seguem a arquitetura existente. Remover cópias de Scene teria alterado o isolamento entre tarefas. Reduzir stacks, DMA ou limites de admissão dependeria de margens que a primeira captura ainda não mediu.

Sequência real. ESP-IDF inicializa PSRAM e tarefas de serviço. `app_main` registra o hardware, instancia o backend e configura a mailbox. O owner cria o snapshot de trabalho, avalia a admissão, inicia AMY e verifica a reserva após AMY. A saída PCM e seus recursos são iniciados antes de publicar o primeiro snapshot. O controle abre NVS e instancia a UI, inicia USB se a condição de áudio permitir, e então inicializa display/LVGL. Depois, o controle atende a UI e publica a telemetria. USB pode enumerar em paralelo após sua criação.

O bench registra `BOOT_HEAP` em `app_main`, `before_owner`, `before_amy`, `after_amy` ou `admission_rejected`, `after_owner`, `before_usb`, `before_display`, `after_display`, `steady` e quando o owner termina. O registro mostra livre, maior bloco, mínimo e contagem de blocos conforme as capacidades. O DMA interno é um subconjunto e não deve ser somado ao total interno. Heap interno 8-bit e PSRAM 8-bit são reportados separadamente. `BOOT_STACK`, `AUDIO`, `OUTPUT08` e `USB` registram high-water em bytes. `HEAP_INTEGRITY` verifica o heap antes de AMY e após a chamada de boot, antes do I²S. Se detectar corrupção, o bench entra no tratamento de falha do ESP-IDF. A rejeição de admissão ou de reserva segue o encerramento normal do owner. O callback de alocação conta falhas desde `app_main` sem logar no ponto da falha. As medições periódicas saem do controle, e o teste global de integridade fica fora do render. Nenhuma exceção de admissão foi necessária.

## Builds e memória recuperada

**PASS** nos dois builds sequenciais com `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e <perfil> -j 8`, ESP-IDF 5.5.2. A primeira configuração do editor falhou na resolução do alias CMake `lvgl`. A referência ao nome gerenciado `lvgl__lvgl` resolveu o problema. As imagens abaixo correspondem ao commit de código `b5bcb06add90352e5a072bd7fd8a6b9fc9968f46`.

| Perfil | DRAM0 data/BSS anterior → atual (B) | DRAM1 data/BSS anterior → atual (B) | Flash PIO / firmware.bin (B) |
| --- | --- | --- | --- |
| `guition-editor-8` | 37.908 / 23.724 → 37.904 / 31.616 | 0 / 138.944 → 0 / 0 | 1.451.018 / 1.451.744 |
| `guition-editor-8-bench` | 38.332 / 23.812 → 38.332 / 31.728 | 0 / 138.976 → 0 / 0 | 1.556.494 / 1.557.216 |

Os mapas deixaram de conter `.bss.work_mem_int.0` e passaram a ligar `smkp4_lvgl_pool`. Parte do BSS restante se acomodou em DRAM0, por isso a linha RAM do PIO subiu. A redução líquida de data/BSS é de **131.072 B no editor** e **131.060 B no bench**. No bench, `_heap_start_high` recua 138.976 B e `_heap_start_low` avança 7.904 B, liberando nominalmente 131.072 B de intervalo endereçável antes do overhead do heap. Frente ao log antigo, a projeção com as demais alocações iguais é cerca de 469 kB internos livres antes de AMY, cerca de 116 kB acima do piso. Livre, maior bloco e fragmentação reais requerem captura. O pool novo usa 131.072 B de PSRAM dinâmica mais overhead, sem reserva PSRAM estática adicional.

Os `sdkconfig` efetivos são iguais aos de seus respectivos baselines. PSRAM hex a 200 MHz, uso por malloc, limiar preferencial interno de 16 KiB, reserva interna de 32 KiB, LVGL 128 KiB e heap poisoning desativado. Os perfis seguem separados pelo opt-in de USB/I²S e telemetria do bench. A tabela de partições dos quatro bundles tem SHA-256 `73c0b5c3e5fcba3a151cc70c453c93dd5f4798899e7f2f8cca76da1f32ffc501`.

| Perfil | SHA-256 de firmware.bin | SHA-256 ELF no descritor |
| --- | --- | --- |
| `guition-editor-8` | `238a50dfe3ac264f650104cceaa488029f472fb85dad06bbb2ec109f325baab8` | `837dc2cb4eacada5a8cc424a7b2371ef2fff7691209bf57d1a3a6e9191dd7db9` |
| `guition-editor-8-bench` | `ea832dfcaa513f880e77c9bc4537ebe06a840fc1b3b9323c6d5015d6441ab7ff` | `40087f524ef3164a615fb0b3f3cb2df5991b84bbc20f30dcb494c0048ff447f1` |

Os bundles e respectivos manifestos estão em `build/release-017b-memory-recovery/<perfil>/`. Logs e mapas anterior/atual estão em `build/hardware-017b/memory-recovery-builds/`. Os bundles originais passaram novamente em `firmware.py check`.

## Regressões

| Comando | Resultado |
| --- | --- |
| `run_performance_scenes_017b.ps1 -ReuseObjects -RuntimeOnly` | **PASS**, 1.000 recalls, alocações protegidas em `apply()` e `render()`, OOM e Panic injetados, heap rastreado estável por topologia e retorno no teardown |
| `run_pattern_library_017a.ps1`, `run_step_sequencer_016a.ps1`, `run_step_sequencer_midi_016c.ps1` | **PASS**, Patterns, estado e persistência simulada, sequenciador, MIDI e ownership |
| `run_patch_load_0152.ps1`, `run_sound_editor.ps1` | **PASS**, loader, editor, AMY real com oito vozes, Harmony, NVS simulada e recuperação de OOM no loader |
| `run_arp_015.ps1 -Runtime -BaselineRoot build/arp_015_baseline`, `run_fx_tests.ps1 all` | **PASS**, Arp, PCM com Arp desligado, 19 testes FX, Global Tempo e persistência |
| `run_usb.ps1`, `run_smk25_0142.ps1` | **PASS**, USB IDF simulado e roteamento SMK25 |
| `run_step_sequencer_ui_016b.ps1 -LvglRoot build/scene-lvgl-locked` | **PASS**, LVGL 9.3 com AMY, Scenes e sequenciador. Browser vazio e modal deixaram 66.376 e 52.280 B livres no pool em host |
| `run_host.ps1`, `python -m unittest discover -s smk-p4/tests/hardware_017b -p test_bench.py` | **PASS**, gates de boot, PCM/I²S simulado, 34 testes do host e 32 do kit |
| `check_blueprint_build.py` com editor e bench, `firmware.py package` e `firmware.py check` | **PASS**, perfis e flags esperados, pool de 128 KiB, hook ligado no map, tabela e hashes |
| A1, A2–A5 e A–H na Guition | **MANUAL / NOT RUN** para a nova imagem |

Os scripts PowerShell acima foram executados a partir de `smk-p4/tests/` por caminho relativo à raiz. No stress, o loader fez até 24 alocações e 2.880 B alocados por tentativa, com média/máximo de 1.714,6/5.623,6 µs e uma das 500 tentativas acima de 5.333 µs em tempo de host. O loader continua alocando durante a barreira silenciosa. Pico, margem contígua e duração do silêncio na Guition seguem pendentes. A coleta de heap em regime ocorre na tarefa de controle e sua sobrecarga também deve ser observada no A1.

## A1 na Guition

1. Conferir o SHA-256 do novo bundle bench, a unidade de referência, o cabo, VBUS, DAC, straps e a tabela de partições. Usar o backup e o procedimento de gravação exclusiva da partição de aplicação descritos no [candidato de hardware](performance_scenes_017b_hardware_candidate.md), após autorização para esse hash.
2. Iniciar uma captura passiva do boot completo de pelo menos 30 segundos, com ID de sessão e diretório novos, conforme o [roteiro de captura](performance_scenes_017b_first_hardware_session.md). Selecionar o novo bundle no parâmetro `-Bundle` e conferir o ELF em `BENCH_ID`.
3. Exigir `ADMISSION admitted=1`, `ENGINE ready=1`, `AUDIO I2S_PRODUCING` e `OUTPUT08 frames` crescentes em duas ou mais amostras. Interromper A1 se algum indicador falhar.
4. Comparar `before_amy` com `after_amy` e `after_owner` para medir consumo do engine e da saída. Conferir a reserva pós-AMY, `HEAP_INTEGRITY ok=1` em ambos os marcos, contagem de falhas de alocação, OOM e ausência de erros e timeouts I²S. Comparar antes/depois de USB e display, confirmar `LVGL_POOL`, verificar stack high-water e registrar o mínimo e o maior bloco em repouso após estabilização. O marco pré-display pode coincidir com enumeração. Repetir com e sem o controlador conectado para atribuir o custo do USB.
5. Somente após A1 aprovado, executar A2–A5. Em seguida, medir `SWAP08` e mínimos durante trocas Juno/DX7, repetir no mesmo patch e tela para verificar queda progressiva, e medir heap e high-water com oito vozes, reconexão, Scenes, NVS e LVGL carregados. Usar o stress físico A–H somente após o smoke.

Na raiz do projeto, com a COM13 ainda confirmada para a unidade de referência, valide o bundle e prepare a captura com um ID inédito.

```powershell
$kit = 'smk-p4/tests/hardware_017b'
$bundle = 'build/release-017b-memory-recovery/guition-editor-8-bench'
$raw = 'build/hardware-017b/memory-recovery-a1-01/serial.log'
python "$kit/firmware.py" check --profile guition-editor-8-bench --bundle $bundle
& "$kit/capture.ps1" -Port COM13 -BoardId 'Guition-P4-rev1.3-80F1B2D3CC86' -Bundle $bundle -Output $raw -SessionId 'memory-recovery-a1-01' -Seconds 30
Select-String -Path $raw -Pattern 'BENCH_ID|ADMISSION|ENGINE |AUDIO |OUTPUT08|BOOT_HEAP|BOOT_STACK|HEAP_INTEGRITY|LVGL_POOL|USB '
```

O registro da placa continua sendo o ensaio anterior. Antes do reteste, a economia estática vem do linker, e as margens dinâmicas e a integridade na Guition continuam **MANUAL**. A montagem mantém as referências elétricas existentes. A revisão da PCB, alimentação/VBUS e straps do DAC ainda exigem conferência na unidade.

**Decisão de bancada.** GO para propor a gravação exclusiva da aplicação bench identificada acima e executar A1 após autorização para essa imagem. Release permanece **NO-GO**. Nenhuma gravação foi realizada nesta rodada.

## Gravação autorizada e primeiro A1 físico

**PASS** em 18/11/2026 na COM13, ESP32-P4 rev. 1.3, MAC `80:F1:B2:D3:CC:86`, flash JEDEC `68:4018` de 16 MB. O esptool 5.1.0 gravou somente a aplicação bench em `0x10000`. A leitura independente confirmou todos os bytes do firmware e do prefixo `0x0000–0xFFFF`, que inclui bootloader, tabela e NVS. A primeira leitura do backup perdeu o fluxo serial. A tentativa a 460800 com `--no-progress` concluiu e foi verificada antes da gravação. Backup completo da área até o fim de factory em `build/hardware-017b/memory-recovery-flash-01/before-stub-retry.bin`, SHA-256 `2bb471b20d8a3a16a5e2d4136e253af54ab35a3875e9fdf30080c54de9d81987`.

A primeira captura com RTS simples ficou vazia. O hard reset USB com a sequência do esptool para Windows produziu a captura completa de 30 s, com ELF igual ao bundle. Bruto em `build/hardware-017b/memory-recovery-flash-01/a1-boot-win-reset.log`, SHA-256 `4e6d3b91f8ec084fa062d930d8fa0a6893ba178967fde7d9ebc828d380633006`.

| Marco | Heap interno livre | Maior bloco interno |
| --- | ---: | ---: |
| Antes de AMY | 467.859 B | 352.256 B |
| Após AMY | 264.919 B | 192.512 B |
| Após Audio Owner e saída I²S | 255.735 B | 180.224 B |
| Antes de USB, após NVS | 253.951 B | 180.224 B |
| Após USB e antes de display | 245.523 B | 172.032 B |
| Após display e durante repouso | 243.283 B | 167.936 B |

O heap de admissão subiu **130.240 B** frente ao A1 anterior. O AMY consumiu 202.940 B internos no boot. A saída custou 9.184 B entre os marcos registrados, o USB sem dispositivo 8.428 B e display/UI 2.240 B internos. Em repouso ficaram 203.671 B internos aptos a DMA, 30.347.784 B livres em PSRAM e 83.192 B livres no pool LVGL. Integridade antes e após AMY, sem OOM, falha de alocação ou queda de heap nas cinco amostras. High-water observado em bytes livres: owner 3.840, saída 1.316, controle 10.964 e USB 5.524.

`ADMISSION admitted=1`, `ENGINE ready=1` e `AUDIO I2S_PRODUCING` nos cinco relatórios. `OUTPUT08 frames` passou de 266.240 a 1.235.712, com zero erros, timeouts e riscos de refill. Houve um gap classificado como boot, sem novos gaps. O indicador de underruns permanece `UNMEASURED`. O USB Host iniciou com o SMK25 desligado durante essa janela. Margens sob carga e política de release continuam pendentes. Nenhum stress A–H foi executado.

## A3 parcial com SMK25 ligado

**PASS** para enumeração após ligar o SMK25 V2, sem reiniciar a Guition. A captura passiva de 30 s na COM13 confirmou `USB state=2 connects=1` e Note On/Off no canal 1, com velocidades diferentes. O contador cumulativo chegou a 538 eventos, com zero erros de parsing, perdas ou overflow na fila MIDI. Duplicatas no cabo auxiliar 1 foram ignoradas. `trace_dropped` chegou a 1.020 registros de diagnóstico durante as rajadas.

O AMY respondeu às notas com sinal digital e manteve o I²S produzindo frames, sem novos gaps, erros ou timeouts. O heap interno ficou estável em 239.603 B, com maior bloco de 163.840 B e zero falhas de alocação. Pitch bend, modulação, desconexão/reconexão, touchscreen, escuta analógica e recalls seguem **MANUAL / NOT RUN**. Release **NO-GO**.

Bruto e sidecar em `build/hardware-017b/memory-recovery-flash-01/a3-smk-powered-01.log` e `a3-smk-powered-01.log.metadata.json`. SHA-256 do bruto `e3fc7ef9a9c259a4463bb8c386eeabe62de54641300ae79e73d4c2bc1ec26ee0`.
