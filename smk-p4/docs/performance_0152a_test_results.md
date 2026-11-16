# SMK-P4 0.15.2a — Resultados de qualificação

**PASS** indica execução aprovada. **FAIL** indica execução reprovada.
**PENDING** indica evidência ainda insuficiente.

## Performance Lock e modal

| Validação host | Casos | Status |
| --- | ---: | --- |
| Matriz Juno → DX7, máscaras `0x00` a `0x0f` | 16 | PASS, todas as máscaras |
| Mesmo patch, com e sem Keep FX | 2 | PASS |
| Baseline entre patches, integridade musical, pedidos concorrentes e limites do delay | 4 | PASS |
| Ciclo do modal LVGL 9.3 com AMY real | 8 | PASS |

Os 22 casos da suíte dedicada incluem defaults e preservação dos quatro
grupos, BPM de 137, notas antigas de touch/USB, FX e macros marcados como
editados quando aplicável. Os dois recarregamentos verificam dirty state,
Compare, Save As, Revert, Recall V4, snapshot obsoleto e falha de carga.
A matriz usa um backend novo e carga limpa antes de configurar cada caso.

O teste LVGL aciona os widgets reais e roda o ciclo periódico da UI. Cobre
sucesso após confirmação do Owner, falha do Owner, timeout sem mutação
do modelo, CANCEL durante a espera, 32 ciclos de abrir e fechar com heap LVGL
estável, seleção rápida, PANIC global e reabertura com nova tentativa após
falha. O teste de timeout também verifica a conclusão tardia depois de
fechar o modal.

## Comandos e regressões

Scripts executados no Windows a partir da raiz do repositório.

| Comando sob `smk-p4/tests/` | Status | Evidência |
| --- | --- | --- |
| `run_performance_lock_0152a.ps1` | PASS | 22 casos |
| `run_sound_editor_ui.ps1` | PASS | Modal, editor, FX, SMK25 e Save/Recall em LVGL |
| `run_patch_load_0152.ps1` | PASS | Locks e compatibilidade de rotas entre famílias |
| `run_delay_transition_0152.ps1` | PASS | FREE/SYNC, BPM, divisões, bypass e Panic |
| `run_owner_stack_0152.ps1` | PASS | Smoke de render host sob carga musical, sem medição da stack FreeRTOS |
| `run_sound_editor.ps1` | PASS | Gates de edição e persistência, regressões PCM e biblioteca |
| `run_harmony.ps1 -ReuseObjects` | PASS | Harmony, USB musical e retrigger com AMY |
| `run_arp_015.ps1 -Runtime -BaselineRoot build/tempo_015_baseline` | PASS | Engine/Runtime e PCM idêntico com ARP desligado contra a baseline 0.15 |
| `run_smk25_0142.ps1` | PASS | 2 executáveis, perfil e roteamento MIDI |
| `run_performance_014.ps1` | PASS | Rotas, pickup e parâmetros |
| `run_usb.ps1` | PASS | Descritores, enumeração, RX e reconexão com IDF simulado |
| `run_fx_tests.ps1 all` | PASS | 19 executáveis, incluindo persistência, codec, falhas e DSP |

## Builds e métricas

PlatformIO para ESP32-P4, ESP-IDF 5.5.2 e GCC RISC-V 14.2.0.

| Comando | Status | Binário (B) | Flash PIO (B) | RAM PIO (B) | DRAM0+DRAM1 ELF (B) |
| --- | --- | ---: | ---: | ---: | ---: |
| `pio run -d smk-p4 -e guition-editor-8 -j 8` | PASS | 1.334.400 | 1.333.672 | 41.588 | 181.068 |
| `pio run -d smk-p4 -e guition-editor-8-bench -j 8` | PASS | 1.436.336 | 1.435.622 | 42.528 | 181.488 |

`check_blueprint_build.py` passou nos dois ambientes com oito vozes,
AMY 48 kHz/256, Editor, biblioteca completa, Unified Engine, Harmony e os
gates elétricos esperados. O perfil de bancada foi auditado com `--musical`.
A coluna ELF soma `.dram{0,1}.{data,bss}` com
`riscv32-esp-elf-size -A`. RAM PIO inclui somente DRAM0 e não representa
a RAM livre em execução.

A primeira execução paralela dos builds encontrou arquivos de cache
removidos durante a compilação. O rerun sequencial aprovou os dois perfis,
sem warnings de compilação registrados nesses logs. A regressão host do Sound
Editor emitiu o warning preexistente `-Wmisleading-indentation` em
`factory_runtime_test.cpp:32`.

## Separação das evidências de áudio

- **PASS** — Preservação de parâmetros, limites de mix/feedback e mudanças de
  divisão testados em host.
- **PASS** — Estabilidade numérica no ensaio host `delay_sync_0151_test`
  com 869 blocos, pico PCM de 2.115 e zero clipping ou OOM nesse ensaio.
  O render com aplicação de comandos registrou 88,63 µs em média e
  141 µs no máximo nessa execução host.
- **PENDING** — Continuidade dos buffers, deadlines e erros I²S sob carga na
  Guition. O teste host de carga confirma que o render produz PCM.
- **PENDING** — Avaliação auditiva de troca com delay preservado, incluindo
  possíveis cliques, interrupções ou caudas de efeito.

Na 0.15.2, a Guition registrou 3.584 B livres na stack do Audio Owner.
O acréscimo de 512 B na 0.15.2a projeta 4.096 B livres. O requisito de
4.000 B segue **PENDING** até medição física sob carga, assim como
reconexão do SMK25, estabilidade I²S e ensaio de toque.
