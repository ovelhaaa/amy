# Guition JC4880P433 — qualificação da 0.17B

**Qualificação física PENDING.** O pinout, USB, VBUS, I²S e partições seguem o candidato Guition da [0.17A](pattern_library_017a_hardware_candidate.md). Consulte o [release gate de software](performance_scenes_017b_test_results.md) para o resultado dos testes.

Qualificar na Guition com SMK25 e saída I²S, após autorização de gravação e identificação do binário pelo SHA-256.

1. Preencher os oito slots, reiniciar e confirmar nomes e referências. Repetir com NVS perto da capacidade e verificar que Patterns e presets V4 continuam acessíveis.
2. Alternar duas cenas com PATCH, PATTERN, FX e TEMPO. Testar Immediate e Next Bar a 40, 120 e 240 BPM. Comparar o instante de aplicação com a fase global e testar Apply Scene Tempo desativado.
3. Segurar um acorde com sustain enquanto o sequenciador e o Arp tocam. Trocar só PATTERN e conferir que o teclado permanece audível. Trocar PATCH, ligar e desligar sustain, depois disparar Stop, Panic e desconectar o SMK25 durante uma troca pendente. Conferir que os indicadores de notas e transporte se recuperam e que não sobram notas presas.
4. Sob carga de oito vozes, registrar render médio e máximo, underruns, jitter, tempo de aplicação, RAM interna, heap e high-water do `amy_owner`. Comparar com a 0.17A na mesma placa e configuração.
5. Conferir cartões, máscara efetiva, falha de Pattern ausente, feedback de fila e acesso ao Panic durante o modal na tela física 800×480. Usar os atalhos numerados no modo Macro Focus.
6. Após cada falha, conferir a máscara aplicada e a última referência de cena confirmada. Se o loader falhar, escolher explicitamente um patch válido e testar um novo Note On/Off antes de retomar o ensaio.

## Builds da fundação

| Perfil | Resultado | RAM PIO | Flash PIO | SHA-256 |
| --- | --- | ---: | ---: | --- |
| guition-editor-8 | PASS | 45.968 B (+544 B sobre 0.17A) | 1.404.242 B (−428 B) | `DB5A2196FF398F0B890D142F5E6A3EBC0E02C6070B1731D72E1F91DFE5F5C78B` |
| guition-editor-8-bench | PASS | 45.816 B (−544 B sobre 0.17A) | 1.506.160 B (−428 B) | `F8EA364124F66E87079E375758E03F12517FF6B760F041D6860ACEB9035F0311` |

Os deltas são em relação aos builds publicados da 0.17A. A aprovação dos builds cobre o firmware atual e o hardening de Patterns. A persistência de cenas ainda é exercitada em host. Nenhuma gravação na Guition foi executada.

## Builds do recall parcial

| Perfil | Resultado | RAM PIO | Flash PIO | SHA-256 |
| --- | --- | ---: | ---: | --- |
| guition-editor-8 | PASS | 47.500 B | 1.410.838 B | `538CDAE74D9979698906E86D5002C19355A6E468FEB16AD9C59F19CE18E49ED0` |
| guition-editor-8-bench | PASS | 47.360 B | 1.512.772 B | `5E9CFBEE21166FAC505CB4999351AC5C48EA08476EAB3FC3149BC4C4CC1397CC` |

Frente à fundação da 0.17B, os perfis normal e bench cresceram 1.532 B e 1.544 B em RAM estática e 6.596 B e 6.612 B em Flash. Frente à 0.17A, cresceram 2.076 B e 1.000 B em RAM estática e 6.168 B e 6.184 B em Flash. A qualificação física permanece PENDING.

## Builds da integração final

Comandos executados com `C:/.platformio/penv/Scripts/pio.exe`, usando `run -d smk-p4 -j 8 -e` para cada perfil.

| Perfil | Resultado | RAM PIO | Flash PIO | SHA-256 de firmware.bin |
| --- | --- | ---: | ---: | --- |
| guition-editor-8 | PASS | 61.608 B | 1.449.948 B | `BE6601DE3C2F088264D59497973007975C27CA32A35B091DACAD2E1A7BC64629` |
| guition-editor-8-bench | PASS | 61.992 B | 1.551.848 B | `042AC632749D0694835CA47F036FF0A9FC85041F60FF84C8C930EA7415B05AA2` |

Em relação ao HEAD inicial da PR, editor e bench cresceram 14.108 B e 14.632 B em RAM e 39.110 B e 39.076 B em Flash. Em relação à 0.17A, os deltas foram +16.184 B e +15.632 B de RAM e +45.278 B e +45.260 B de Flash. A maior parte do aumento de RAM vem do catálogo de oito Scenes e dos buffers fixos de captura e comparação.

O código de integração não altera o esquema de partições nem as configurações de pinos. MANUAL na Guition com SMK25: duração de silêncio na troca, underruns e refill DMA sob oito vozes, headroom de NVS, heap LVGL físico e high-water real da tarefa.

## Procedimento de qualificação pós-hardening

1. Identificar revisão da Guition, porta, DAC, straps do módulo, BCLK/WS/DATA, terra comum e alimentação. Reconferir VBUS e o perfil USB HS0 com a montagem documentada da 0.17A.
2. Conferir o SHA-256 da imagem bench e comparar a tabela efetiva da unidade com `smk-p4/partitions.csv` (aplicação `factory` em `0x10000`, NVS em `0x9000`). Guardar um backup legível do firmware e da NVS existentes, com hashes e plano de restauração. Gravar somente a partição de aplicação após autorização para a imagem identificada.
3. Capturar boot e janelas seriais em repouso, com oito vozes sustentadas e durante rajadas de recall. A captura passiva pode usar `smk-p4/tests/capture_serial.py` sem `--reset`. Capturar simultaneamente a saída analógica estéreo. Para investigar continuidade, capturar BCLK, WS e DATA com analisador lógico.
4. Preencher oito Scenes, renomear, salvar e reiniciar. Ler os oito slots, conferir CRC e integridade de Patterns e presets V4. Testar FULL, SELECTIVE, FX-only, Pattern-only, navegação e NEXT BAR com 40/120/240 BPM.
5. Selecionar patches Juno e DX7 alternadamente sob carga. Medir do último áudio do patch antigo ao primeiro áudio utilizável do novo com uma nota de prova após a confirmação. Registrar tempos do loader e quantidade de rampas/blocos de silêncio em separado. Repetir com Arp e sequenciador, sustain, Stop, Panic, desconexão e reconexão. Testar falha simulada somente em imagem aprovada que tenha injeção controlada; evitar exaurir a memória global da placa.
6. Comparar com a 0.17A na mesma montagem, com as mesmas sequências e captura. Anotar build, quantidade de trocas, nível do sinal, estado terminal, contadores antes/depois e resultado GO/NO-GO.

| Critério | Aceitação proposta para bancada |
| --- | --- |
| Áudio e DMA | Zero erros I²S, timeouts, gaps inesperados com notas ativas, lacunas ou transientes fora da barreira. Confirmar continuidade de WS/BCLK na captura. Underruns no firmware permanecem UNMEASURED. Um intervalo de refill acima de 21,333 ms sinaliza risco pela capacidade nominal dos quatro descritores. |
| Latência | Render de até 5,333 ms sem incremento de deadlines fora da barreira. NEXT BAR dentro de um bloco. Em 100 trocas alternadas, comparar com troca de patch direta na 0.17A. Limite proposto de regressão do maior silêncio é um bloco adicional, equivalente a uma passagem de confirmação da cena. Medir separadamente tempo de comando MIDI em regime normal e adotar o mesmo limite de regressão. A tolerância musical do silêncio absoluto exige escuta. |
| Stack e RAM | Margem proposta de pelo menos 2 KiB no `amy_owner` após todas as cargas e falhas, para variação de caminhos descendentes. Zero falhas de alocação espontâneas e nenhuma queda progressiva de heap livre ou de maior bloco após aquecimento, ao comparar no mesmo patch e tela. Registrar RAM interna e PSRAM separadamente. |
| Recuperação e USB | Cada tentativa termina com o token correto e a máscara correspondente. Injeção controlada deixa Failed silencioso e a próxima seleção válida deve voltar a tocar; 10 ciclos de desconexão/reconexão sem reboot e sem notas presas. |
| NVS e UI | Oito slots íntegros após reinício e operações de edição. Presets V4 e Patterns íntegros. Todos os controles e o Panic acessíveis no touchscreen 800×480, com estado de espera e desfecho coerentes. |

Registrar `AUDIO` (tempo, deadlines e high-water), `ENGINE` e `ADMISSION` (vozes, OOM e latência de comandos), `OUTPUT08` e `GAP09` (writes, erros, refill e silêncio), `PATCH_LOAD` (último/máximo e gerações), `SWAP08` (heap livre e maior bloco antes/depois), USB, Scene, NVS e diagnósticos de UI incluindo heap LVGL. Máximos são cumulativos desde o boot. Registrar ocupação de NVS e LVGL para o conjunto de dados real. O tamanho de bloco contíguo amostrado antes/depois não mede fragmentação transitória intra-loader.

`SCENE phase` usa a ordem `0 Idle, 1 WaitingBoundary, 2 Confirmed, 3 Failed, 4 Cancelled, 5 PartialApply, 6 WaitingPatch`. As linhas são snapshots periódicos. Para atribuir métricas a um token específico, isolar uma operação e aguardar coleta do seu resultado antes da próxima. Para stress de rajadas, usar as diferenças dos contadores cumulativos e a captura de áudio.

### Identidade dos builds de hardening

Firmware do commit de código `5651ca1c9b6448d1939095cc5ffd4c051072327d`, compilado com `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e <perfil> -j 8`. Deltas de RAM e Flash PIO contra a integração final em `1aea0a5c`.

| Perfil | Resultado | RAM PIO | Flash PIO | Binário | SHA-256 de firmware.bin |
| --- | --- | ---: | ---: | ---: | --- |
| guition-editor-8 | PASS | 61.632 B (+24 B) | 1.450.606 B (+658 B) | 1.451.328 B | `0F6CC4B80E2BFCC20A6EDB07B5CA8D54997CC6E5594D8E41A32AE91982562722` |
| guition-editor-8-bench | PASS | 61.472 B (−520 B) | 1.552.768 B (+920 B) | 1.553.488 B | `79433C649879C401A38F8111BC40B8AF244D8A6C588FFFE42EE04771E7BEA975` |

`riscv32-esp-elf-size -A` reporta, no editor, DRAM0 data/bss de 37.908/23.724 B e DRAM1 bss de 138.944 B. No bench, DRAM0 data/bss de 38.292/23.180 B e DRAM1 bss de 139.528 B. A soma de data/bss das duas regiões é 200.576 B e 201.000 B respectivamente. A linha RAM do PIO corresponde à DRAM0 e não representa heap livre.

**PASS** em `smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --environment <perfil>`, com `--musical` no bench. Foram verificados oito vozes, 48 kHz/256 frames, LVGL único com pool de 128 KiB, e gates elétricos corretos por perfil.

## Kit de qualificação 0.17B

A captura, o inventário de telemetria e o protocolo A–H estão no [guia do kit](performance_scenes_017b_bench_guide.md). Os hashes da construção anterior permanecem como histórico. Use o manifesto novo do perfil bench e confirme `BENCH_ID` antes de associar medições à imagem. Continuidade I²S, RAM física, recuperação USB e persistência continuam PENDING sem evidência desta unidade.
