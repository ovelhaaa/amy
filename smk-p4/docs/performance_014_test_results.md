# SMK-P4 0.14 — qualificação em host

Os runners PowerShell da árvore são executados na raiz do repositório. Harmony prepara os objetos AMY compartilhados. O runner 0.14 também faz parte do job host do workflow P4.

| Comando na raiz | Resultado | Cobertura |
|---|---|---|
| powershell -File smk-p4/tests/run_performance_014.ps1 | PASS | Oito posições, composição, curvas e polaridade, neutralidade, exclusão por família, overflow e geração USB, Learn e Pickup |
| powershell -File smk-p4/tests/run_sound_editor.ps1 | PASS | DSP real Juno/DX7, ausência de deriva, Compare, FX nativo, oito vozes, seletor de 32 algoritmos, PCM e heap |
| powershell -File smk-p4/tests/run_sound_editor_ui.ps1 | PASS | Round-trip de snapshot, MIDI Learn/Pickup com pacotes simulados, perfil, Save/Recall V3 e testes existentes de editor e FX |
| powershell -File smk-p4/tests/run_fx_tests.ps1 all | PASS | FX e transação com NVS simulada, incluindo interrupção após gravar Synth V3 |
| powershell -File smk-p4/tests/run_harmony.ps1 -ReuseObjects | PASS | Harmony, sustentação, CC1, pitch bend, retrigger e perda segura de eventos |
| powershell -File smk-p4/tests/run_usb.ps1 | PASS | Enumeração, descritores, RX, falhas e reconexão com IDF simulado |

A auditoria estática python smk-p4/tests/audit_patch_library.py --check também passou.

## Benchmark de host

O teste PERF014_HOST aplica e renderiza 200 blocos de 256 frames por cenário, com oito notas Juno mantidas. A segunda execução alterna as oito posições entre neutro e uma posição não neutra. O cenário mede serviço e render na máquina host e exercita uma taxa de mudança superior ao coalescimento normal da UI. Ele não mede o deadline no P4.

| Cenário | Média serviço + render | Máximo |
|---|---:|---:|
| Oito vozes, macro parado | 81,71 µs | 359,20 µs |
| Oito vozes, oito macros alternando | 107,35 µs | 779,40 µs |

O teste de heap verificou ausência de alocação durante apply/render nessa sequência. Os números são de uma execução em Windows e incluem variação de agendamento do host.

## Validação física pendente

Captura de CC do SMK25 V2, KNOB-B, operação simultânea de teclado e knobs, tempo máximo de render no ESP32-P4, margem de heap/PSRAM, underruns e artefatos durante edição contínua. O procedimento está no relatório do candidato.
