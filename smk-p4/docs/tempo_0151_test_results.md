# SMK-P4 0.15.1 · Qualificação

Base de comparação `2db72a01292f1d93cc25301e294f3697afe97019`. Os ensaios host usam MinGW GCC, AMY C real quando indicado, LVGL 9.3 e NVS/USB simulados. Os testes de 0.15.1 foram integrados aos runners existentes e ao CI.

| Comando host | Estado | Evidência |
|---|---|---|
| `powershell -File smk-p4/tests/run_arp_015.ps1 -Runtime -BaselineRoot build/tempo_015_baseline` | PASS | Seis BPM, seis divisões e três swings. Fase, 1.000 horas simuladas, wrap, Tap, Clear Latch, Restart, PCM e segurança de notas |
| `powershell -File smk-p4/tests/run_smk25_0142.ps1` | PASS | Perfil Legacy S3, MIDI Learn e roteamento |
| `powershell -File smk-p4/tests/run_performance_014.ps1` | PASS | Macros, eventos MIDI, Learn e Pickup |
| `powershell -File smk-p4/tests/run_sound_editor.ps1` | PASS | Juno, DX7, 32 algoritmos, edição, compatibilidade de presets e estabilidade |
| `powershell -File smk-p4/tests/run_sound_editor_ui.ps1` | PASS | LVGL 800×480, Tap compartilhado, salvamento global, painel tempo, ARP, Status Strip, K2, Pickup e BPM mapeável |
| `powershell -File smk-p4/tests/run_fx_tests.ps1 all` | PASS | AMY, FX, CRC V1/V2, transações V4, compare/revert, rollback e NVS em simulação |
| `powershell -File smk-p4/tests/run_fx_tests.ps1 gate_i_e2e` | PASS | Recall V4 de SYNC após reboot simulado com BPM global diferente |
| `powershell -File smk-p4/tests/run_fx_tests.ps1 tempo` | PASS | Mesmo comando de BPM move o próximo passo do ARP e o tap do Delay |
| `powershell -File smk-p4/tests/run_harmony.ps1 -ReuseObjects` | PASS | Harmony e USB com AMY real, sustentação, gerações, polifonia e heap |
| `powershell -File smk-p4/tests/run_usb.ps1` | PASS | Descritores, enumeração, RX, desconexão/reconexão, limpeza e falhas simuladas |

## PCM e desempenho host

- ARP OFF + FX FREE. PCM Juno/DX7 idêntico à base, tanto em estado neutro quanto com Delay, Chorus e Reverb ativos. SHA-256 do fixture combinado `11fab98798fb56ec284f0723d115c829643ed0dc41e46fac926c023761fa6bf5`.
- ARP ON + FX FREE. PCM idêntico ao gerado pela partitura MIDI direta com os mesmos pitch, velocity, onset e gate. A lateness máxima no traço sintético foi de 5.291,666 µs, dentro dos 256 frames da fronteira de render.
- Delay Sync. O ensaio de 869 blocos com até oito vozes e feedback de 95% mediu pico PCM de 2.115, maior diferença entre samples adjacentes de 643, zero clipping e zero OOM. A verificação cobre tap aplicado dentro de um sample, fallback fora da faixa, recuperação automática, bypass e ausência de alocações no owner durante as transições. Sem builds concorrentes, apply + render no host mediu 81,40 µs em média e 142,20 µs no máximo.
- Runtime host de 11.688 B (referência 0.15) para 11.792 B na mesma ABI MinGW. O teste ARP host registrou serviço máximo de 17 µs na execução do fixture final.
- Medição de RAM interna/PSRAM, maior bloco, stack, deadlines e áudio na Guition. **NOT RUN**. A referência em repouso da 0.15 é 358.003 B internos livres na admissão, com margem de 5.747 B, render médio de 214 µs e máximo de 459 µs.

## Gates

| Gate | Resultado |
|---|---|
| A | PASS. Auditoria do owner, DSP e persistência no commit inicial |
| B–D | PASS host. Tempo, integração ARP, comando confirmado e Tap |
| E–H | PASS host. FREE/SYNC, capacidade, transições e tap efetivo. Audição MANUAL |
| I–J | PASS host com NVS simulada. FX V1/V2, transação V4 e salvamento global |
| K–O | PASS host. Status Strip, ARP, FX, Clear Latch, Restart e SMK25 |
| P | PASS. API de divisões compartilhada |
| Q–R | PASS host. Unidades, regressão musical e PCM contra 0.15 e 0.14.2 |
| S | PASS de medições host. RAM de admissão, stacks, deadlines e integridade de áudio físico MANUAL |
| T | PASS. Builds e verificadores da configuração dos dois perfis |
| U | PASS. Procedimento físico preparado |
| Hardware | NOT RUN |

## Builds executados

```powershell
& C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8 -j 8
& C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8-bench -j 8
python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --environment guition-editor-8
python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --musical --environment guition-editor-8-bench
```

O primeiro build paralelo encontrou uma disputa pelo diretório de dependências
LVGL. A resolução e os builds finais sequenciais passaram. Tamanhos e
SHA-256 estão no [procedimento físico](tempo_0151_hardware_candidate.md).
