# SMK-P4 0.16A — Arquitetura do Step Sequencer

Baseline de software: 0.15.2a, commit `3b7f5769`.

## Clock e transporte

O Audio Owner avança `tempo::Clock` a cada bloco de 256 frames renderizado
a 48 kHz. Arpeggiator, Delay SYNC e Step Sequencer usam o mesmo BPM. O
sequenciador guarda os próximos prazos de início e gate em unidades inteiras
da fase publicada pelo clock. Cada prazo parte do início agendado e
preserva a fração de bloco acumulada. Uma alteração de BPM muda a taxa
de avanço da fase, inclusive durante um gate.

Play arma o passo 0 na próxima fronteira da divisão no clock global.
Play repetido durante a execução mantém a posição. Restart reinicia o
passo 0 na próxima fronteira durante a reprodução e libera a nota em curso.
Stop e Disable liberam a nota do sequenciador e deixam o transporte parado.
Panic, troca de patch e transição para storage quiet também param o
transporte. O padrão e as configurações do sequenciador ficam em RAM.

O padrão inicial tem 16 passos vazios. Cada passo tem nota MIDI, velocity,
gate de 1 a 100% e flag de execução. Pausas ocupam a mesma grade que passos
ativos. A capacidade fixa permite comprimentos de 1 a 16. Qualquer edição
vale a partir de um próximo início de passo e preserva o gate e a fronteira
já agendados. A mudança de comprimento reposiciona o cursor para o início
na próxima fronteira se ele estiver fora do novo intervalo.

As seis divisões iniciais usam `tempo::Division`. Swing de 50–75% alterna a
duração dos passos em pares nas divisões binárias. Tercinas usam duração
uniforme. A paridade do swing continua ao dar a volta em um padrão de
comprimento ímpar. O menor intervalo é cerca de 15,625 ms a 240 BPM com
swing máximo em 1/32. O Owner processa no máximo um novo passo por bloco.
A resolução dos comandos musicais e gates é um bloco de 5,333 ms.
A medição host do agendamento está no relatório de testes.

## Comandos, snapshot e propriedade de notas

`Commands::pushSequencer()` aceita requests versionados de um produtor.
O mailbox SPSC tem quatro entradas e um slot separado para Stop. O Owner
lê no máximo quatro entradas normais por ciclo e aplica Stop por último.
Assim, Stop prevalece sobre Play já enfileirado para o mesmo ciclo. O
Owner valida token, versão, geração, disponibilidade do patch e dados do
comando. O snapshot publica o token mais recente, rejeição, revisão, BPM,
divisão, posição e estado do transporte.

O Audio Owner aplica os eventos de touch e USB, as notas do Arpeggiator
e depois a nota do Step Sequencer. Os quatro compartilham a admissão de
vozes existente. O Step Sequencer usa `NoteOrigin::Sequencer` e guarda o
grupo admitido até o fim do gate ou Stop. Assim, notas da mesma altura
em fontes diferentes têm liberações independentes. Se as oito vozes
estiverem ocupadas, a admissão rejeita o disparo e o cursor continua.
A geração de troca de patch ou Panic invalida requests pendentes.
A desconexão USB mantém o escopo de cancelamento da origem USB.

A transposição de −24 a +24 semitons precede a quantização opcional
do Harmony Assist. Um resultado transposto fora de 0–127 vira pausa.
A quantização usa a escala e tonalidade atuais com a política
de quantização selecionada. O Chord Assist fica desativado para o
sequenciador. O gate libera o grupo admitido, mesmo que a escala mude
durante a nota.

## Integração futura

O harness host usa a mesma API de comandos e snapshots do firmware.
O acesso é SPSC. UI e uma futura integração com SMK25 devem encaminhar
pedidos por um produtor comum ou por uma rota USB com ownership próprio.
A 0.16B deve mapear Play, Stop e edição touchscreen após revisão da
engine. A grade deve desenhar a posição com o snapshot publicado e
tratar fila cheia, respostas rejeitadas e coalescência de observações.
Formatos de preset, gravação, múltiplas pistas e UI touchscreen ficam
fora desta etapa.
