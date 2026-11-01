# Fase 2 — auditoria temporal antes das alterações

Data: 2026-11-01. HEAD auditado: `4ec5fb956f7e9963bfd3c9bd41d9b9c75ae45fc9`. Árvore de trabalho limpa na entrada. Este mapa descreve o código observado; latência e jitter não foram medidos na placa.

## Fluxo real

```text
USB Host/client (core 0, prioridade 5)
  -> MidiParser::emitEvent(timestamp ESP em us, armazenado truncado a 32 bits)
  -> EventBus (FIFO 256, reserva de 32 para críticos, Panic fora da fila)
  -> app_init_task / controle (core 0, prioridade configurada)
       -> perfil, UI, patch, transporte, arpejador, sequenciador
       -> AmyAdapter (fila fixa de comandos)
  -> amy_owner (core 1, prioridade abaixo de AudioTask)
       -> até kSynthCommandsPerBlock comandos antes do próximo render
       -> AMY / ring fixo de PCM
  -> AudioTask (core 1, prioridade máxima)
  -> PCM5102A via I2S, DMA 4 × bloco, 48 kHz
```

O cliente USB é dono do parser e envia ao EventBus. `app_init_task` é o consumidor único e aguarda eventos em períodos de até 10 ms. Ele também processa MIDI Learn, perfil, ações de UI, patches e sistema. O loop pode executar leitura/gravação síncrona de Flash/NVS, além de logs, então o atraso máximo de processamento MIDI não é limitado pelo código.

O AMY é acessado depois do boot somente por `amy_owner`; o adapter copia comandos em fila fixa sob seção crítica curta, coalesce controles contínuos e reserva espaço para releases. A fila AMY não bloqueia o produtor. Quando uma release não cabe, muda a geração e solicita Panic. O owner aplica Panic antes de comandos da geração nova; descarta comandos obsoletos em lotes limitados por bloco, que ainda podem ocupar slots temporariamente. O adapter substitui o timestamp da origem pelo instante de enqueue e processa até `kSynthCommandsPerBlock` comandos antes do render. O limite exato desse lote vem de `synth_config.h`.

O `EventBus` tem 256 entradas e reserva 32 para releases/lifecycle; overflow é contado. Panic usa latch independente e geração. Perda de Note Off pede Panic. USB Disconnect pede Panic independentemente da capacidade de notificação.

## Agenda AMY investigada

AMY já suporta agenda por amostras: `amy_event.time` indica posição na linha de tempo do engine; deltas são inseridos ordenados com comparação wrap-relative e `amy_add_event` usa lock interno. O adapter constrói eventos com `amy_default_event()` e não define `time`, então os comandos de performance são convertidos no tempo corrente quando o owner os atende. Não é necessário criar outro scheduler agora. A conversão entre relógio ESP/MIDI e amostras AMY ainda não existe.

## Clock, arpejador e sequenciador

O relógio interno usa `esp_timer` (dispatch `ESP_TIMER_TASK`). Callback publica cada Clock no EventBus com contador e timestamp de µs truncado. O loop de controle recebe o evento e executa `Arpeggiator::processTick`, `StepSequencer::processTick` e transição de cena. Assim, UI, patch/storage e tráfego na FIFO atrasam triggers. Clock não tem reserva própria e pode perder ticks por overflow. Os geradores não reproduzem todos os limites perdidos; um limite ainda pendente pode gerar evento tardio no tick que finalmente for processado.

MIDI Clock percorre USB → parser → EventBus → controle → `ClockManager::onExternalTick`, portanto o timestamp de recepção não agenda os passos. A escolha de clock externo não é inferida pelo parser; Start chama `onExternalStart`. BPM/fonte/tick/callback do `ClockManager` são campos acessados entre timer e controle sem sincronização uniforme, risco de concorrência. Swing desloca fronteiras lógicas nos geradores, sem alterar o período do timer.

## Propriedade do estado de notas

`app_init_task` roteia Note On/Off diretos. Arpeggiator guarda notas held/latched e último trigger; StepSequencer guarda a última nota por track e tick de release; `AmyAdapter` guarda bitmap canal/nota e stack mono; AMY realiza allocation/voice stealing. Não há ownership por origem para teclado, arpejador e sequenciador. `activeVoices()` é snapshot da contagem de engine, não número de teclas fisicamente pressionadas. Sobreposição de mesma nota/canal por fontes diferentes precisa de teste antes de justificar uma tabela de ownership.

## Mapa de atraso por etapa

| Etapa | Tarefa/core | Timestamp | Fila | Espera/operação | Overflow/Panic |
| --- | --- | --- | --- | --- | --- |
| USB/parser | cliente USB, core 0 | ESP µs truncado em 32 bits | transfer USB fixo | parse síncrono e envio não bloqueante | erro de transferência limpa conexão e Panic; MIDI drops contam |
| EventBus | produtor USB/timer; consumidor controle | timestamp retido no evento | 256, reserva 32 | seção crítica limitada, receive até 10 ms | releases não cabem → Panic; Clock pode cair |
| Controle | `app_init_task`, core 0 | timestamp existe, mas não usado para execução | evento individual | UI, mapeamento, patch, Flash/NVS e logs síncronos | Panic limpa geradores/engine e reconhece latch após enqueue |
| Adapter | vários produtores → amy_owner, core 1 | timestamp de enqueue substitui origem | capacidade fixa, release reserve | dequeue limitado por bloco; preset pode materializar deltas synchronously no owner | nova geração + Panic; comandos antigos descartados aos poucos |
| Áudio | owner → ring PCM → AudioTask → I2S | tempo AMY por amostra; não correlacionado ao MIDI | ring fixo; DMA 4 × 128/256 | starvation dá silêncio; escrita timeout 20 ms | PCM antigo filtrado por geração; falha interrompe áudio |

## Tabela de riscos pré-implementação

| ID | Evidência e severidade | Efeito possível | Tratamento proposto |
| --- | --- | --- | --- |
| T1 — Clock na FIFO geral (alta) | `onClockTick` envia Clock ao EventBus; loop único faz geração musical | jitter/omissão sob carga | separar ticks de eventos comuns, avançar por tick mais recente e descartar triggers vencidos, preservando releases |
| T2 — timestamp inconsistente (média/alta) | ESP µs 64 bits truncado a 32; adapter grava novo timestamp | latência firmware não pode ser calculada ponta a ponta | timestamps 64 bits em diagnóstico; preservar origem e medir etapas distintas |
| T3 — Flash no controle (alta, máximo não limitado) | saves/loads de patch, perfil e cenas ocorrem no loop durante playback | Note On/Off e controle atrasam | mapear cada chamada e medir; não mover para outro core sem demonstrar impacto/sincronização |
| T4 — AMY sem tempo de origem (alta) | `amy_event.time` existe, mas unset no adapter | evento espera owner/fila/bloco | conversão com âncora coerente, usando scheduler AMY já existente |
| T5 — troca de patch em partes (média/alta) | Preset e parâmetros são comandos separados; preset materializa no owner | som intermediário entre blocos | tratar como barreira/geração após teste de regressão |
| T6 — Stop e releases (média) | `StepSequencer::stop()` sem EventBus apaga playback; app também envia All Notes Off | segurança depende do segundo comando | validar todos caminhos Stop e releases independentes de tick futuro |
| T7 — concorrência ClockManager (alta) | campos compartilhados entre timer e controle sem proteção uniforme | BPM/fonte/fase inconsistentes | sincronizar estado ou serializar alterações e testar relógio simulado |
| T8 — notas sem origem (média) | aplicações não mantêm ownership por origem | Note Off de fonte pode encerrar nota concorrente coincidente | validar AMY/application semantics; tabela compacta só se demonstrar necessidade |
| T9 — fila obsoleta após Panic (média) | entries velhas permanecem até dequeue em lotes limitados | novas notas podem esperar limpeza | medir saturação; qualquer compactação deve preservar ordem crítica |

## Configuração e limites

48 kHz, bloco padrão 256 frames (5,333 ms) ou 128 frames (2,667 ms), quatro buffers DMA. Capacidade nominal de DMA: 21,333 ms ou 10,667 ms de frames. É configuração de buffer, não medição de latência acústica. Existe uma fila de comandos e ring de PCM adicionais. Firmware possui `synth_max_command_wait_us`, tempo médio/máximo de render, deadline misses, starvation, HWM/overflow e erros I2S. Não há métricas de recepção→controle, jitter de clock ou evento sequenciado→owner. `audio_underruns` ainda conta erros/short writes, não underrun DMA. Nada disso prova latência tecla→saída, CPU, jitter ou áudio limpo.

## Plano de teste observado

Host: `tests/run_smk_safety.ps1`, `tests/run_smk_amy_boot.ps1`, `tests/run_smk_patch_integrity.ps1`; adicionar teste lógico determinístico de clock/ticks/release. Firmware: workflow `.github/workflows/smk-s3-build.yml`, ESP-IDF 5.3, bloco 128 e 256. Hardware: procedimento em `phase1_test_procedure.md`, ampliado com captura de MIDI e saída analógica; contadores do firmware não substituem medição elétrica.

## Política antes de implementar

- Releases vencidas são atendidas antes de triggers. Triggers atrasados não são recuperados em rajada.
- Panic/Stop invalida geração anterior e limpa eventos pendentes.
- Preservar timestamp de origem para diagnóstico; usar relógio 64-bit em intervalos.
- Usar scheduler de amostras AMY se a conversão entre clocks puder ser ancorada e monotônica. Não afirmar sample-accuracy enquanto isso não estiver implementado e medido.
- Latência de firmware e latência analógica são resultados separados.
