# SMK-P4 0.16C — arquitetura MIDI do sequenciador

## Base e escopo

A branch parte de `1010b66c`, HEAD da 0.16B no checkout, no remoto e na PR
[#44](https://github.com/ovelhaaa/amy/pull/44). O pedido 0.16C define o alvo
Guition JC4880P433, ESP32-P4, 800×480. O `specs.md` descreve o produto
ESP32-S3, incluindo múltiplas pistas. Esta etapa mantém a arquitetura P4
da 0.16B com um padrão monofônico de até 16 passos, AMY a 48 kHz, blocos
de 256 frames e oito vozes.

## Fluxo e propriedade

1. O cliente USB Host enumera o dispositivo e entrega pacotes a
   `amy::UsbInput`. O filtro admite o cabo virtual 0, valida CIN e dados e
   converte Note On com velocity zero em Note Off.
2. `UsbInput` faz o roteamento musical do controlador. O perfil SMK25
   reconhece VID/PID `4353:4B4D`. O perfil legado usa pads 36–51 no canal 10
   e transporte por CC 114/115/117 no canal 1. Pads atribuídos a controle,
   knobs e transporte seguem os caminhos existentes de controle e Arp.
   Harmony, Performance Lock, FX e o perfil NVS permanecem com seus
   mecanismos existentes. O novo fluxo recebe somente notas classificadas
   como musicais. A tela MIDI continua sendo o ponto para confirmar canal e
   perfil do controlador. Seleção de passo, pausa, modo e latch já têm
   comandos sem dependência de pads, prontos para um mapeamento futuro.
3. A tarefa USB é a única produtora de `UsbInput::commands`. O Audio
   Owner drena essa fila e decide se a tecla segue para o sequenciador,
   Arpeggiator ou voz direta. Modulação, bend, sustain e CC 120/123
   permanecem no roteamento existente. O Audio Owner admite grupos no AMY.
4. A touchscreen usa `IUiBackend → UiBlueprintPerformanceAdapter →
   LocalAmyPerformanceBackend → seq::Mailbox`. A tarefa UI é a única
   produtora desse mailbox versionado SPSC. O owner aplica os comandos de
   UI antes de drenar o lote USB do bloco, valida geração, valor e prazo,
   e publica o padrão, estado MIDI e confirmação no snapshot existente.
   O USB jamais escreve nesse mailbox.

O adapter tem quatro posições locais para operações ainda não enviadas,
além de um token normal em trânsito. Edições absolutas adjacentes do
mesmo parâmetro e passo podem ser substituídas pela intenção mais
recente. Mudanças relativas de pitch, seleção, modos, transporte e
operações no padrão preservam a ordem. Campos de uma célula são
aplicados sobre o passo atual no owner. Fila cheia, expiração, geração
cancelada e payload inválido geram feedback na UI. Operações normais
expiram em dois segundos. Stop tem slot e confirmação independentes,
retira as travas sonoras da sequência e invalida Play/Restart anteriores
ainda locais na UI. Panic usa a geração global existente.

## Modos e ownership

| Controle | Comportamento |
| --- | --- |
| NORMAL PLAY | As notas musicais seguem para o roteamento convencional. A sequência pode tocar junto. |
| STEP INPUT | Nota e velocity substituem a célula selecionada, ativando-a e mantendo seu gate. A seleção permanece. |
| STEP RECORD | Cada tecla nova grava nota, velocity e o gate configurado. O cursor avança no snapshot e faz wrap no comprimento atual. REST / NEXT desativa o passo corrente e avança. Uma seleção fora do comprimento volta ao primeiro passo. |
| AUDITION | Com On, uma nota capturada para edição também segue pelo caminho musical, incluindo Arp ou Harmony quando elegíveis. O padrão inicial é Off. |
| ANY USB MIDI / SMK25 ONLY | Restringe a captura e a transposição ao SMK25 identificado, se selecionado. O canal de entrada e o canal de transposição aceitam ANY ou 1–16. |

O Audio Owner usa dois bits por canal/pitch para distinguir tecla solta,
captura exclusiva, voz direta e Arpeggiator. Cada Note Off libera a
propriedade da admissão original. Releases musicais usam a identidade
e o sustain existentes. Note Ons repetidos enquanto uma tecla está
pressionada não regravam nem redisparam a nota. Notas simultâneas avançam em
ordem USB. CC 120/123 libera o canal e limpa seu estado de roteamento.

Panic global, troca de patch e barreira de armazenamento retornam a
NORMAL PLAY, limpam o latch e preservam o padrão confirmado em RAM. Uma
mudança de geração USB libera vozes USB e Arp e limpa o estado MIDI do
sequenciador. Uma desconexão física também para o transporte do
sequenciador e libera seu grupo; notas touchscreen continuam com seu
ownership independente. Reconectar devolve o teclado em NORMAL PLAY.

## Transposição e relógio

LIVE TRANSPOSE é um latch, disponível em qualquer modo de entrada. Seu
canal tem precedência sobre a captura de edição. Uma tecla desse canal
só atualiza o latch, mesmo com AUDITION ligado. Com referência MIDI 60,
C4 resulta em 0, D4 em +2 e A3 em −3. O offset efetivo é a soma da
transposição manual e da diferença da última tecla para a referência,
limitada a −127…+127. Desativar Live limpa a diferença. O latch está no
owner e mantém identidade de teclas, de forma que um comportamento
momentâneo pode usar as mesmas entradas e releases no futuro.

A execução parte da nota armazenada, aplica os dois offsets, descarta
resultado fora de 0…127 e usa uma única transformação de escala Harmony
quando SCALE QUANTIZE está ativo. Chord Assist permanece desativado para
a origem Sequencer. O gate libera o grupo admitido no onset, inclusive se
a escala ou o latch mudar depois. O clock global continua governando o
Arp, sequenciador e Delay SYNC. Os modos e o latch não reiniciam a fase.

## Tela

O piano roll mantém as 16 colunas e a edição direta por toque. O painel
MIDI / RECORD SETTINGS ocupa a área da grade quando aberto e reúne
modos, latch, canais, referência, gate, audição e fonte. O status indica
a última nota/velocity, identificação do SMK25 e offsets. O nome do passo
na faixa inferior abre o editor contextual, que adiciona semitons e
oitavas em botões amplos e mantém os controles de velocity, gate e
Enabled/Rest acessíveis na faixa inferior. Os painéis são montados sob
demanda e liberados ao fechar ou sair da tela.

Configurações e padrão residem em RAM. As mudanças aproveitam a aplicação
P4 e seus adaptadores. O núcleo AMY, o perfil persistido, o pinout e as
dependências são os mesmos da base.
