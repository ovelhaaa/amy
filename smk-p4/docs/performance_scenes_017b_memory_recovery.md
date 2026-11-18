# 0.17B — recuperação de RAM interna

**MEMORY RECOVERY CANDIDATE — HARDWARE RETEST REQUIRED.** Release **NO-GO** até medir as margens na Guition. O alvo deste trabalho é o perfil P4 da [PR #47](https://github.com/ovelhaaa/amy/pull/47). A especificação geral do projeto descreve o S3. A mudança fica no port P4.

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
| AMY, tabelas de instrumentos, FIFOs, osciladores, bancos e temporários DSP | 8 instrumentos ativos, 64 osciladores de voz, 9 IDs de synth. Pico ESP32 a medir | Heap interno pelas caps de synth, block e fbl | `Runtime::begin()` e loader existente | Mantidos |
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
