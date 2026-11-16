# SMK-P4 0.16B — Step Sequencer touchscreen

A aba **SEQ** no banco MAIN abre o sequenciador monofônico na Guition
JC4880P433, em 800×480. A tela reaproveita a navegação, a faixa global de
status, o Panic, a tipografia e as cores do UI Blueprint.

| Controle | Operação |
| --- | --- |
| PLAY / STOP / RESTART | Play arma o primeiro passo na fronteira da divisão global. Stop libera o grupo do sequenciador. Restart alinha novamente o passo 1 enquanto o transporte está em execução. |
| BPM INT | Abre o controle global de tempo usado também pelo Arpeggiator e Delay SYNC. |
| DIV | Percorre 1/4, 1/8, 1/16, 1/32, 1/8T e 1/16T. |
| LENGTH / SWING / TRANSPOSE | Comprimento 1–16, swing 50–75% e transposição de −24 a +24 semitons. Tercinas têm espaçamento uniforme. |
| SCALE QUANTIZE | Aplica a escala, tonalidade e política de quantização configuradas no Harmony Assist às notas do sequenciador. |
| Célula da grade | Escolhe o passo e programa uma nota. Tocar na nota já ativa apenas seleciona o passo. |
| Cabeçalho do passo | Seleciona o passo. Em um passo com nota ativa, também mostra sua oitava. O sufixo `-` indica pausa e as setas indicam notas fora da oitava exibida. |
| OCT − / OCT + | Navega pelas alturas de MIDI 0 a 127, com C4 em MIDI 60. |
| VELOCITY / GATE | Edita o passo selecionado em 1–127 e 1–100%, respectivamente. |
| ENABLED / REST | Alterna o estado do passo e preserva sua nota, velocity e gate. |
| LOAD DEMO PATTERN / CLEAR PATTERN | Substituem o padrão inteiro em RAM. Quando há passos editados, o segundo toque confirma a substituição. O demo volta a 16 passos e Clear mantém o comprimento atual. |

O padrão começa vazio. A seleção e a oitava continuam disponíveis ao voltar
à tela. O cabeçalho âmbar e a linha vertical marcam a posição do snapshot
do Audio Owner. As notas ativas continuam visíveis quando o playhead as
atravessa. A grade mantém os 16 passos editáveis quando o comprimento
reproduzido é menor.

## Comandos e sincronização

`SequencerScreen → IUiBackend → UiBlueprintPerformanceAdapter → LocalAmyPerformanceBackend → Commands → Audio Owner`

A tarefa da UI é a produtora do mailbox SPSC. O owner valida a geração e o
payload e publica o padrão aplicado, transporte, playhead e confirmações.
Uma edição normal fica pendente por vez. Sliders mostram o valor local
durante o arraste e enviam somente o valor final ao soltar. A fila SPSC de
quatro entradas mantém um slot independente para Stop, com confirmação
própria. Panic usa o caminho global de geração. A tarefa de áudio preserva
a posse das notas por origem.

A tela distingue comandos pendentes, fila cheia, rejeição de valor,
indisponibilidade, cancelamento por geração e timeout. O snapshot também
registra notas sequenciadas descartadas por falta de vozes. A troca de
divisão mantém o gate atual e o próximo onset já agendados. A partir
desse onset, a nova divisão determina os próximos prazos de gate e passo.
A transposição e a quantização afetam a próxima nota executada. O gate
libera o grupo de vozes admitido; o pitch original continua no padrão.

O refresh da tela usa o ciclo existente do LVGL. A mudança de playhead
atualiza os cabeçalhos afetados e move o indicador. O Audio Owner não
chama LVGL. Load Demo, Clear e os demais controles mantêm o padrão em
RAM.

## Capturas host em 800×480

| Estado | Captura |
| --- | --- |
| Parado e vazio | [empty.png](step_sequencer_016b_shots/empty.png) |
| Demo carregado | [demo.png](step_sequencer_016b_shots/demo.png) |
| Reproduzindo | [playing.png](step_sequencer_016b_shots/playing.png) |
| Passo selecionado | [selected.png](step_sequencer_016b_shots/selected.png) |
| Quantização ativa | [quantize.png](step_sequencer_016b_shots/quantize.png) |
| Comando rejeitado | [rejected.png](step_sequencer_016b_shots/rejected.png) |

## Próxima etapa

Qualificar a edição ao toque e a continuidade de áudio em bancada antes de
avaliar na 0.16C a gravação de passos pelo SMK25. Qualquer integração deve
preservar a propriedade de cada fila e a prioridade do Stop e do Panic.
