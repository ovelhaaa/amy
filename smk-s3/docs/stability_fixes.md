# Correções de estabilidade — ESP32-S3

Data: 2026-09-05. Escopo: primeira etapa de correções de MIDI, Panic e ciclo de
vida do áudio. O ESP32-P4 permanece fora desta alteração. Modificações locais
anteriores de display, perfil do SMK25 e alocação do AMY foram preservadas.

A etapa seguinte de propriedade exclusiva do motor está implementada em
[amy_ownership.md](amy_ownership.md), que atualiza a arquitetura, os custos e
os limites de concorrência relatados abaixo. Este documento registra a primeira etapa.

## Comportamento implementado

### MIDI e Panic

- O parser USB transmite o valor exato de pitch bend, inclusive zero. O filtro
  anterior dependia de novas mensagens para convergir e podia manter a nota
  desafinada depois de soltar o controle. Uma futura suavização deve ocorrer por
  tempo no motor, preservando o valor alvo por canal.
- O EventBus mantém a ordem FIFO de eventos musicais. Não move Note Off para a
  frente de seu Note On. Na capacidade padrão de 256 entradas, reserva 32 para
  liberações e notificações USB.
- Panic ocupa um registro independente da fila. Se uma liberação não couber,
  o barramento contabiliza a perda e solicita Panic como recuperação.
- Panic invalida eventos musicais anteriores por geração e bloqueia novos
  eventos musicais até o consumidor concluir a limpeza e chamar
  `acknowledgePanic()`. Uma nova solicitação durante essa limpeza exige novo
  atendimento; a confirmação antiga não a apaga.
- Desconexão USB solicita Panic mesmo se a notificação não couber. Eventos de
  conexão já enfileirados sobrevivem à invalidação. Uma notificação USB ainda
  pode ser perdida quando a fila inteira está cheia; essa perda é contabilizada.
- O comando `panic` do console usa o mesmo caminho. O tratamento na aplicação
  limpa arpejador, sequenciador, motor e o gesto pendente de salvar por pressão.
- MIDI Learn deixa passar Panic, liberações, Stop de transporte e eventos USB;
  sua entrada solicita Panic. CC1, recebido como `Modulation`, agora é aprendido
  como CC. Stop/Rec mapeados por CC permitem pular/cancelar antes das etapas de
  aprendizado dos próprios botões de transporte.
- O callback do relógio publica ticks no barramento. A geração de notas e as
  transições de cenas passam a ocorrer na tarefa de controle, junto do reset.
  Sob saturação, ticks podem ser descartados; esta etapa não promete clock
  preciso sob sobrecarga.

### Áudio

- Falhas de criação da fila, inicialização do DAC/AMY, habilitação do I²S ou
  criação da tarefa interrompem a inicialização dependente e são reportadas.
- Todos os quatro descritores DMA recebem silêncio antes de habilitar I²S.
  Um fade-in estéreo de 960 frames (20 ms a 48 kHz) é aplicado na partida.
- Sample rate, bloco, DMA, timeout e fade estão em `audio_config.h`. A pilha da
  tarefa usa a configuração existente em `app_config.h`.
- O timeout da API I²S é passado em milissegundos, sem conversão indevida para
  ticks. Buffer inválido, tamanho incompatível ou escrita incompleta/erro
  encerra a tarefa de áudio, tenta silenciar/desabilitar a saída e sinaliza
  falha. Falha ao parar também é sinalizada. Não há laço de renderização sem
  espera após uma falha de escrita.
- A tarefa de controle reporta a falha uma vez, solicita Panic e bloqueia novos
  eventos musicais. A recuperação desta condição requer reinício.

## Arquitetura e custo

Nenhum arquivo do núcleo AMY foi alterado nesta etapa. As correções usam a
camada de aplicação e seus adaptadores. O teste de boot existente ganhou uma
regressão de Note On imediatamente seguido de Panic, antes do atraso de 4 ms
do instrumento.

O novo EventBus aloca suas entradas uma vez, em RAM interna no ESP32. Envio e
recepção copiam no máximo uma entrada por seção crítica; descarte de entradas
antigas usa iteração limitada pela capacidade. Não há alocação no envio, na
recepção ou no fade. Na configuração atual, a geração acrescenta 1.024 bytes
ao armazenamento das 256 entradas, além dos metadados e semáforo; isso não é
uma medição do consumo total de heap em execução.

| Métrica do build PlatformIO | Antes desta etapa | Depois |
| --- | ---: | ---: |
| Flash reportada | 912.803 bytes | 915.539 bytes |
| RAM estática reportada | 28.852 bytes | 28.860 bytes |
| Sample rate | 48.000 Hz | 48.000 Hz |
| Bloco / DMA | 256 / 4 × 256 frames | 256 / 4 × 256 frames |

Um bloco representa 5,33 ms; a capacidade total do DMA representa 21,33 ms.
Esses valores não medem a latência entre tecla e saída analógica. CPU, tempo
máximo real de render, memória livre e latência precisam de medição na placa.

## Validação executada

Comandos a partir da raiz do repositório, PowerShell, com GCC/G++ no PATH:

```powershell
./tests/run_smk_safety.ps1
./tests/run_smk_amy_boot.ps1
C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-devkitc-1
```

| Status | Verificação |
| --- | --- |
| PASS | Parser, limites/centro de pitch por canal, velocity zero, reserva e ordem de liberações, Panic sob saturação, caminho ISR simulado, gerações, despertar, dois produtores concorrentes e MIDI Learn. |
| PASS | Implementações reais de AudioTask/PCM5102Output com driver/RTOS simulados: silêncio antes do enable, falhas de I²S e criação de tarefa, saída incompleta, buffer nulo, fade estéreo e propagação de falha ao parar. |
| PASS | AMY e adapter reais no host: alocação de vozes, 256 trocas de patch, CC, Note Off, bateria e Panic, inclusive antes de renderizar a nota agendada. |
| PASS | Teste de UI existente compilado com os drivers de display de host e suas telas; executável `build/test_smk_ui.exe`. |
| PASS | Build ESP32-S3 com plataforma instalada 55.3.35 e ESP-IDF 5.5.1. |
| PASS | Verificação de whitespace do diff dos arquivos desta etapa. |
| NOT RUN | Upload, testes físicos, carga/latência na placa e builds de outras plataformas. |
| MANUAL | Enumeração/reconexão USB real, continuidade DMA, saída analógica e ruído de partida. |

O teste de AMY em Windows emite avisos preexistentes de conversão de ponteiro
para inteiro em código de diagnóstico. Os testes de áudio usam simulações das
falhas do driver; eles não certificam a temporização do FreeRTOS nem do DMA.

## Procedimento na placa — ainda não executado

1. Confirmar revisão N16R8, fiação USB nativa, alimentação VBUS, GPIOs e módulo
   PCM5102A antes de gravar. Os GPIOs existentes não foram alterados. Confirmar
   se XSMT está disponível e ligado ao mute configurado; a garantia elétrica
   de silêncio depende desse circuito e do estágio de saída.
2. Conectar a saída de linha a uma interface/entrada apropriada e capturar
   ambos os canais. Fazer dez partidas e observar a ausência de pulsos de
   grande amplitude; registrar qualquer transiente. Conferir 48 kHz e DMA
   4 × 256 no log. O fade digital não elimina sozinho transientes analógicos.
3. Executar `audio_status`, `status` e `memory`. Tocar acordes por dez minutos,
   alternando velocity, sustain e movimentos rápidos dos controles. Comparar
   contadores antes/depois; esperar nenhuma nota presa e nenhuma falha de
   escrita. Registrar máximo de render, média e memória. O limite temporal de
   um bloco é 5.333 us; medir margem também durante trocas de patch/display.
4. Sustentar uma nota, levar pitch ao máximo/mínimo e soltar. Verificar retorno
   exato à afinação original após uma única mensagem central.
5. Com acorde sustentado, executar `learn_start`; depois soltar teclas/pedal,
   mover modulação e executar `panic`. Esperar ausência de notas presas, avanço
   correto na etapa de modulação e Panic atendido durante o assistente. Repetir
   com `learn_cancel` e voltar a tocar.
6. Desconectar o controlador enquanto toca, inclusive durante Learn e com
   arpejo ativo. Esperar interrupção das notas, sem reinício por eventos antigos.
   Reconectar e tocar novamente. Repetir vinte vezes, registrando contadores.
   Caudas dos efeitos devem ser distinguidas de notas mantidas/reativadas.
7. Se houver gerador USB MIDI de bancada, produzir rajadas de acordes e CCs
   acima do consumo da aplicação, terminando com liberações. Esperar descarte
   contabilizado e recuperação por Panic se uma liberação não couber, sem
   travamento. Depois da recuperação, novas notas devem funcionar.

## Limites e próximo passo

- Ainda existem acessos ao AMY vindos do controle e de outros comandos do
  console enquanto o áudio renderiza. A mudança do callback do relógio reduz
  uma concorrência, mas não implementa propriedade exclusiva do motor.
- Carregamento de patches e Flash ainda precisam ser retirados dos caminhos
  concorrentes sensíveis. O contador chamado `audio_underruns` registra erros
  de escrita; não detecta todos os casos de repetição de descritor DMA.
- Persistência de patches/cenas, parameter locks e transições por comprimento
  de padrão continuam com as pendências identificadas na análise. Não foram
  declarados corrigidos por estes testes.
- Controlador/offsets do display, revisão da placa, VBUS e straps do DAC ainda
  dependem de confirmação física. A aplicação preserva a configuração local.
- O README raiz já continha marcadores de conflito e texto de P4 na entrada
  desta etapa; não foi reescrito para evitar misturar os projetos paralelos.

Próximo passo de implementação: centralizar os comandos de motor em uma fila
limitada com propriedade explícita, separando preparação de patch da aplicação
no áudio; validar prazos no S3 antes de avançar recursos criativos. Executar o
procedimento físico acima em paralelo à preparação dessa mudança.
