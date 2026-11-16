# SMK-P4 0.16A — Testes e desempenho

**PASS** indica execução aprovada. **FAIL** indica execução reprovada.
**PENDING** indica evidência ainda insuficiente.

| Comando executado | Status | Evidência |
| --- | --- | --- |
| `smk-p4/tests/run_step_sequencer_016a.ps1` | PASS | 36 combinações de BPM, divisão e swing e 9.216 inícios agendados no ciclo de 16 passos |
| Mesmo comando, edição e transporte | PASS | Gate, pausas, padrão vazio e inválido, BPM e divisão durante execução, Play/Stop repetidos e Restart |
| Mesmo comando, AMY real | PASS | PCM idêntico à referência MIDI para pitch, velocity, gate, pausa e loop |
| Mesmo comando, Audio Owner | PASS | Stop com fila cheia, notas coincidentes em fontes distintas, oito vozes ocupadas, mudança de escala, Arp/BPM, patch, Panic e geração obsoleta. Probe de heap ativo em apply/render |
| Mesmo comando, mailbox | PASS | 10.000 requests transferidos entre produtor e consumidor com ordem e conteúdo verificados |
| `smk-p4/tests/run_performance_lock_0152a.ps1` | PASS | 22 casos, incluindo 16 máscaras |
| `smk-p4/tests/run_harmony.ps1 -ReuseObjects` | PASS | Harmony, USB musical e retrigger |
| `smk-p4/tests/run_arp_015.ps1 -Runtime -BaselineRoot build/tempo_015_baseline` | PASS | Clock, render AMY e PCM idêntico com Arp desligado |
| `smk-p4/tests/run_usb.ps1` | PASS | Enumeração, recepção e recuperação com IDF simulado |
| `pio run -d smk-p4 -e guition-editor-8 -j 8` | PASS | Build ESP32-P4 com auditoria de perfil |
| `pio run -d smk-p4 -e guition-editor-8-bench -j 8` | PASS | Build ESP32-P4 com auditoria de perfil `--musical` |
| Guition, áudio I²S, toque e carga com SMK25 | PENDING | Medir em bancada |

O maior atraso observado no trace de agendamento host foi de 254 frames
(5,292 ms) para Note On e 255 frames (5,313 ms) para gate. Os testes
compararam os prazos de fase absolutos com as fronteiras dos blocos a 40,
120 e 240 BPM, com swing 50% e 75%. Os eventos permaneceram dentro de
um bloco do prazo ao longo de 256 inícios por combinação. O teste não
mede latência I²S nem jitter físico.

## Comparação com a 0.15.2a

O host passou sete rodadas por modo, com 64 blocos de aquecimento e
2.048 blocos de render medidos por rodada. Mesmo GCC, objetos AMY, perfil
de oito vozes e bloco de 256 frames nos dois commits. O comparativo
desligado usa as mesmas oito notas sustentadas. A execução ativa usa sete
notas sustentadas e uma voz sequenciada com gate de 90%, como cenário
adicional separado.

| Modo host | Média de render | Menor / maior média por rodada |
| --- | ---: | ---: |
| 0.15.2a, oito notas MIDI | 57,84 µs | 57,24 / 58,70 µs |
| 0.16A, oito notas MIDI, sequenciador parado | 58,24 µs | 57,23 / 58,83 µs |
| 0.16A, sete notas MIDI e sequenciador ativo | 59,49 µs | 58,34 / 62,63 µs |

O objeto Runtime passou de 11.800 B para 12.408 B no host. O snapshot
passou de 2.384 B para 2.408 B. A engine usa 120 B e o mailbox usa 452 B
no host. A alteração de RAM estática regular no firmware foi de
+632 B nos dois perfis, pelo relatório PlatformIO. O arquivo binário
cresceu 3.600 B em ambos. Os logs de build da 0.16A não registraram
warnings de compilação.

**PENDING** para render no ESP32-P4, stack, deadlines, continuidade I²S,
jitter de saída e avaliação auditiva. As médias host não qualificam os
deadlines do dispositivo.
