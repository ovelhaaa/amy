# SMK-P4 0.17A — resultados de testes

Base 0.16C: 2ddef04ee8a4b385b6abe797e342fc372171c3ca, conferida no
checkout, na branch remota e no HEAD da PR #45.

| Comando | Resultado |
| --- | --- |
| ./smk-p4/tests/run_pattern_library_017a.ps1 | PASS. 16 slots no mock NVS, releitura, rename, tombstone, CRC, versão incompatível, falha de espaço e recuperação após corte. Troca por fase em 4/4 com edição, swing, divisão tercinada e mudança de BPM. |
| ./smk-p4/tests/run_step_sequencer_016a.ps1 | PASS. 36 casos de clock, 9.216 onsets, PCM de referência e 10.000 requests SPSC. |
| ./smk-p4/tests/run_step_sequencer_midi_016c.ps1 | PASS. Captura MIDI, ownership, releitura, transposição e desconexão simulada. |
| Placa Guition | PENDING / MANUAL. |

## Auditoria FX

As 12 repetições iniciais da asserção antiga passaram sob carga de host.
Entretanto, um Panic com a fila drenada pode produzir zero rejeições
obsoletas. O teste passou a contabilizar todas as entradas aceitas pela
fila, permitir essa ordem e verificar separadamente a rejeição forçada
de uma entrada antiga. A instrumentação também encontrou uma corrida:
o owner podia remover uma mensagem da geração nova antes de processar
o Panic da geração anterior. O consumidor agora observa a publicação
antes de conferir a geração e mantém a entrada na fila até o próximo
apply se o owner ainda estiver na geração anterior. A drenagem por bloco
tem limite igual à capacidade da fila.

## Memória

| Estrutura host | 0.16C | 0.17A |
| --- | ---: | ---: |
| Runtime | 13.112 B | 13.352 B |
| Snapshot | 2.528 B | 2.536 B |
| Engine sequenciador | 168 B | 384 B |
| Mailbox sequenciador | 476 B | 496 B |

LVGL host, firmware, desempenho e medições físicas serão registrados
conforme execução. Métricas host não medem latência de touchscreen ou DMA.

## Interface e concorrência

| Comando | Resultado |
| --- | --- |
| ./smk-p4/tests/run_step_sequencer_ui_016b.ps1 | PASS. LVGL 9.3 com AMY real e ponteiro simulado. Inclui aviso ANY, Save, Load, Rename, Delete, descarte, troca e cancelamento, falha de Load e retorno ao navegador. |
| ./smk-p4/tests/run_fx_tests.ps1 gate_l_concurrency | PASS. Execução inicial instrumentada e 12 repetições, três cargas por execução. A terceira carga confirmou zero rejeições com fila drenada. |

Heap LVGL após aquecimento no navegador fechado estabilizou entre
44.496 e 44.512 B, em estados alternados. A verificação compara o mesmo
ponto do ciclo e repetiu 24 aberturas e fechamentos após aquecimento.
A tela 0.16B sem biblioteca montada ficou em 47.312 B livres. A suite
também verificou 40 alternâncias SEQ/FX.

## Capturas host em 800×480

- [Grade SEQ com nome e estado](pattern_library_017a_shots/pattern-active.png)
- [Biblioteca com slot confirmado](pattern_library_017a_shots/pattern-library.png)
- [Decisão sobre alterações não salvas](pattern_library_017a_shots/pattern-discard.png)
- [Troca agendada](pattern_library_017a_shots/pattern-next-bar.png)
- [Carregamento rejeitado](pattern_library_017a_shots/pattern-error.png)

## Limites de qualificação

O mock NVS exercita o fluxo de banco inativo e a recuperação, mas
capacidade real com presets legados e comportamento sob corte elétrico
dependem da placa. Heap NVS, stack das tarefas ESP32-P4, underruns de
DMA, continuidade audível e toque físico dependem de ensaio de bancada.
