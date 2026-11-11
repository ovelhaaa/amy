# SMK-P4 0.14 — auditoria de arquitetura

Base: c2978a35b4aa890fc354c6d4a25bd644947bf944.

A solicitação 0.14 define o alvo Guition JC4880P433/ESP32-P4, 800×480, oito vozes, 48 kHz e 256 frames. O specs.md descreve o SMK-S3 e serve de referência para o modelo de controle. As regras elétricas e de build do P4 continuam definidas pelos seus próprios perfis.

## Fluxo e ownership

USB Host → UsbInput (validação de pacotes USB-MIDI) → fila USB de comandos → AMY owner → snapshot copiado → LocalAmyPerformanceBackend → UiBlueprintPerformanceAdapter → LVGL.

A UI é a produtora de Commands, do mailbox de edição e da fila FX. O owner consome as três rotas. A task cliente USB produz somente para UsbInput::commands e para seu trace. O trace é lido pelo diagnóstico e inclui bytes rejeitados e cabos ignorados, portanto seu contrato difere de um barramento de controle.

O ponto de publicação recomendado é a ramificação CC validada em UsbInput. Uma fila independente USB → UI pode carregar CC, cabo, canal, timestamp e geração. O consumo e o mapeamento ficam na task da UI, que continua sendo a única produtora dos comandos de edição. A geração musical também muda em Panic, troca de patch e barreira de armazenamento. É adequada para descartar tráfego pendente, enquanto a identificação da conexão deve ter seu próprio ciclo de vida. Limitar o consumo por ciclo e contabilizar descartes fora da fila musical.

## Comportamento MIDI

Apenas o cabo virtual 0 participa da execução. Note On com velocidade zero vira Note Off. O owner aplica CC1 como modulação, CC64 como sustain e CC120/123 como liberação por canal. Pitch bend de 14 bits ajusta as vozes USB do canal. Reset MIDI e desconexão invalidam a geração USB. Perda de uma liberação na fila musical também aciona Panic USB. CC genéricos chegam à fila musical, onde o owner não tem ação para eles. Encaminhá-los por uma fila dedicada libera capacidade na fila musical. CC reservados continuam em suas rotas existentes.

## Macros e confirmação

Sound Edit tem três posições legadas de 64. No registro da UI correspondem a perf.macro.2, perf.macro.5 e perf.macro.6 (índices 1, 4 e 5). M2 controla cutoff Juno ou profundidade FM. M5 e M6 controlam ataque e release Juno. As oito entradas perf.macro.* já existem no registro, mas a presença no registro não comprova uma rota DSP.

O Sound Editor já mantém valores original, manual e applied. Calcular as contribuições a partir de original/manual permite neutralidade e Compare/Revert sem realimentação. Manter a profundidade FM na rota de moduladores e preservar o seletor discreto de algoritmo e sua barreira de notas/sustain. O perfil de macros do S3 demonstra composição relativa, mas usa IDs, formatos e destinos próprios.

Edição e FX têm tokens e erros independentes. Hoje o recall aguarda as duas confirmações. Para macros com destinos Synth e FX, a política precisa validar todos os destinos e distinguir aceite, confirmação e erro. O estado manual FX precisa ficar separado do resultado aplicado.

## Persistência

V1/V2 usam três bytes de macro nos offsets 49–51. V2 ocupa 160 bytes e o espaço restante é usado por parâmetros, identidade ROM e CRC. Os IDs 0–57 permanecem estáveis. O coordenador publica Synth e FX por marcador que referencia bancos e sequências, após escrita e verificação dos dados. Uma extensão da imagem Synth, validada pelo mesmo marcador, evita que um sidecar independente combine gerações diferentes. Preservar a decodificação V1/V2 e o significado das três posições antigas. Perfis globais MIDI devem usar namespace próprio, independente da transação de preset.

## Verificação por camada

Qualificar composição pura e neutralidade, depois a aplicação em oito vozes com snapshots e a contagem de alocações. Para USB, saturar a fila de CC sem retirar capacidade de Note Off e testar o descarte após mudança de geração. Para persistência, injetar falhas em cada escrita e verificar que o recall recupera uma geração inteira. Executar as regressões do editor, UI, FX, Harmony e USB antes de gerar candidatos.
