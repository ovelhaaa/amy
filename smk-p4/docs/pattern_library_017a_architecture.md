# SMK-P4 0.17A — arquitetura da biblioteca de padrões

Base verificada no checkout e no remoto: 0.16C, 2ddef04ee8a4b385b6abe797e342fc372171c3ca.
O pedido define o alvo Guition ESP32-P4. O specs.md descreve o produto S3.
Esta etapa se limita à aplicação P4 monofônica.

## Propriedade

O Audio Owner aplica comandos do mailbox SPSC e é dono da configuração
ativa e da troca pendente. A UI lê snapshots e executa operações NVS
na tarefa de controle, sob a barreira de storage quiet já usada por
presets e perfil MIDI. Essa barreira silencia e interrompe o transporte.
A leitura e preparação da transação precedem o pedido de silêncio.

## Registro e referências

16 IDs estáveis, independentes de nome, para referência em cenas futuras.
O namespace de padrões usa dois bancos por slot, sequência crescente,
versão e CRC no registro serializado. Após interrupção, o leitor escolhe
o registro válido mais recente. Exclusão grava um tombstone para impedir
a recuperação acidental de um padrão excluído. Presets V4 ficam em seu
namespace atual. O registro contém configuração musical, excluindo
transporte, teclas, latch e ownership.

## Troca e edição

O padrão confirmado serve de referência para comparação estrutural de
dirty state. A troca imediata libera o grupo antigo e reinicia no passo
1 no bloco de aplicação. NEXT BAR usa 96 ticks do clock global (4/4);
o comprimento, swing e divisão do padrão não definem a fronteira.
A última solicitação admitida substitui a pendente. Edições posteriores
invalidam a autorização anterior de descarte. Stop, Panic, desconexão,
patch e entrada em storage quiet cancelam trocas pendentes.

A troca preserva BPM, FX, timbre, Arp, Harmony, macros, Performance Lock
e o latch de Live Transpose. O MIDI existente conserva ownership de cada
tecla até o release. A UI pede confirmação antes de habilitar ANY.
O perfil SMK25 já usa pads 36–51 no canal 10 e CC 114/115/117 no canal 1;
uma futura ação de MIDI Learn pode enviar o mesmo comando de UI por um
produtor de controle com ownership definido.

## Layout V1

Registro de 104 bytes, little endian. Chaves p00a/p00b a p15a/p15b;
a UI e a documentação de palco mostram 01–16.

| Offset | Campo |
| --- | --- |
| 0–3 | Magic PAT1 |
| 4–7 | Versão 1 |
| 8–11 | Sequência do banco |
| 12 | 0 para exclusão, 1 para padrão |
| 13–17 | Comprimento, divisão, swing, transposição com bias 24, quantização |
| 18–34 | Nome ASCII terminado em zero, máximo 16 caracteres |
| 35 | Reservado |
| 36–99 | 16 células, cada uma com nota, velocity, gate e enabled |
| 100–103 | CRC-32 sobre bytes 0–99 |

A escrita usa o banco inativo, commit do ESP-IDF e releitura byte a byte.
Registros incompatíveis bloqueiam a escrita do slot. Se só um banco
estiver íntegro, ele pode ser lido. Em caso de espaço insuficiente,
a gravação é rejeitada com reserva de uma página de GC. O ID do slot
permanece estável ao renomear ou substituir. Uma cena futura deve validar
o estado do slot antes de usá-lo, especialmente após exclusão.

## Fluxo do navegador

LOAD sobre um padrão editado pede SAVE & LOAD, DISCARD & LOAD ou CLOSE.
SAVE & LOAD usa o slot ativo como origem e salva antes de carregar o
destino. Se a edição estiver apenas em RAM, escolher um slot e executar
SAVE primeiro. Caso haja novas edições enquanto a troca aguarda o
compasso, a troca é cancelada e a edição permanece no padrão ativo.

IMMEDIATE dispara no próximo bloco do Audio Owner. NEXT BAR seleciona
o primeiro limite de 4/4 estritamente posterior à solicitação. O padrão
novo começa no passo 1, mesmo se a fronteira cair no meio do padrão
antigo. A aplicação é quantizada ao bloco de 256 frames. Com transporte
parado, NEXT BAR aplica a configuração imediatamente e mantém STOP.
A UI pode cancelar a seleção pendente sem parar o sequenciador.

WRITING só começa após o snapshot confirmar o silêncio. Antes disso,
o usuário pode cancelar. SAVE confirma o arquivo e depois a referência
CLEAN no owner. Se a confirmação do owner expirar após a gravação, a UI
mostra o estado de timeout e o slot verificado continua disponível.
Ao abrir um namespace novo, a primeira gravação também ocorre em quiet.

Após excluir o slot ativo, os seus passos permanecem em RAM marcados
como modificados. Um novo SAVE restabelece uma referência persistida.

A interface de controle separa a solicitação de slot, modo e
cancelamento. Na 0.17B, MIDI Learn pode usar esse contrato para
NEXT/PREVIOUS PATTERN, QUEUE PATTERN e CANCEL QUEUED PATTERN. O produtor
de controle deve resolver o slot no catálogo e preservar o produtor
único do mailbox. O perfil persistido conserva seus mapeamentos atuais.
