# Decisão técnica sobre o loader da 0.17B

Baseline remoto da branch e da PR #47: `1aea0a5c1e555c74b99938e024845be5aac3fe62`. O escopo explícito de ESP32-P4 e Guition prevalece nesta auditoria sobre o alvo ESP32-S3 de `specs.md`.

## Mapa do caminho de execução

| Fase e proprietário | Estado musical e saída | Memória, bloqueios e recuperação |
| --- | --- | --- |
| Boot de `amy_owner`, `Runtime::begin()` / `amy_start()` | Inicializa AMY, reserva de deltas, instrumentos, osciladores e FX antes de iniciar a saída física. | A configuração põe estruturas de síntese, eventos e PCM em RAM interna e delays/amostras/SysEx em PSRAM. O motor e a fila de deltas são globais. A configuração desliga renderização multicore do AMY. |
| `apply()` → `requestPatch()` em `amy_owner` | Libera notas e sustain, baixa ganho e invalida gerações UI/USB. O handoff de PCM descarta blocos obsoletos. | Mailboxes fixas, gerações atômicas e deltas da reserva. PCM já entregue ao DMA pode sair antes do silêncio. |
| `loadPendingPatch()` → `loadReusable()` em `amy_owner` | O worker para de produzir PCM. Libera os oito instrumentos e reconstrói a topologia; executa deltas, reaplica FX e aquece DSP antes de verificar. | `patches_load_patch_checked()` libera instrumentos/FIFOs, prepara novas estruturas e materializa deltas. `src/instrument.c` aloca um instrumento e dois FIFOs por voz em heap interno. `src/amy.c` também pode alocar, substituir ou liberar buffers de oscilador conforme os eventos e configurações. Os buffers de FX são reservados no boot. Não há reinicialização integral do motor em cada troca. |
| `pcm_output08`, prioridade 6, durante todas as fases físicas | Tarefa independente do `amy_owner` (prioridade 4). Entrega PCM de geração atual ou um bloco com rampa limitada ao zero e depois zeros. DMA configurado com quatro descritores; o handoff tem três blocos. | Sem chamadas a AMY ou heap no laço de saída. `i2s_channel_write()` pode esperar pelo DMA e retorna erro/timeout. O driver termina a saída em erro. Qualificação física de escalonamento e de fluxo DMA continua necessária. |
| Finalização em `amy_owner` | Segunda mudança de geração descarta entrada de durante a carga. Estado Ready só permite áudio da cena após verificação; falha rende PCM silencioso. | Erro de admissão, topologia incompleta, OOM observado ou geração concorrente resulta em Failed. O estado anterior pode já ter sido destruído. Selecionar patch válido novamente reconstrói o conjunto. |
| `Runtime::end()` → `amy_stop()` | Encerra ciclo do motor. | Libera pools, buffers e osciladores. A igualdade de heap rastreado antes e após o ciclo foi verificada no host. |

No IDF, a fila de deltas usa mutex com `portMAX_DELAY` nos pontos de acesso do AMY. A integração mantém execução em um único proprietário e a tarefa I²S não entra nessa fila. O alocador do IDF pode disputar locks de heap com outras tarefas; dois proprietários do estado AMY introduziriam riscos de concorrência mesmo com renderização silenciada. NVS e LVGL são atendidos na tarefa de controle. O handoff pode pausar o worker quando cheio; a espera de I²S ocorre na tarefa de saída.

A 48 kHz, 256 frames dão 5,333 ms por bloco e quatro descritores representam 21,333 ms de capacidade nominal. O tempo de ausência de PCM musical pode exceder um bloco, mesmo com I²S contínuo. Intervalos entre inícios de writes e contadores de rampa/silêncio são proxies; a continuidade física depende da ocupação real do DMA, do escalonamento e do DAC. Ver os resultados host e aplicar o roteiro na Guition.

Panic ou desconexão antes da carga invalida o pedido. Durante a carga, gerações são observadas entre instrumentos e na conclusão; o loader síncrono não é preemptível no meio de uma chamada C. Cancelamento por token, Stop e Storage Quiet podem ser processados após a carga. Se PATCH já foi aplicado, a cena publica PARTIAL_APPLY. O timeout lógico de 2 s é verificado quando o owner volta a atender a cena. Não é watchdog nem garantia contra travamento dentro do loader. Tráfego pode entrar nas filas limitadas durante Loading; o owner não aplica essas notas e invalida sua geração antes de voltar a admitir notas.

## Alternativas avaliadas

| Alternativa | Concorrência, memória e prazo | Compatibilidade e decisão |
| --- | --- | --- |
| A. Barreira existente | Único owner de AMY. Alocações e latência de recarga variam com o patch e o heap; a tarefa de saída mantém seu próprio prazo. | Menor alteração. Manter como candidato de bancada sujeito a prova física. |
| B. Preparar na tarefa de controle | Metadados e resolução de slots já podem ficar no controle. Separar a materialização exige protocolo de propriedade para instrumentos, pools, mapas e DSP globais. | Os pontos de preparação isolados no C não cobrem reconstrução, aplicação de deltas e aquecimento do patch. Descartada para a 0.17B. |
| C. Pré-alocar/reusar | Poderia reduzir contenção e fragmentação. Reservar o maior formato por oscilador/instrumento aumenta RAM interna comprometida. | Depende de contrato de capacidade e ciclo de vida no AMY e de medir todas as topologias. Avaliar se a bancada mostrar falta de margem. |
| D. Carga incremental | Poderia limitar o trabalho do worker por passagem. O custo de cada etapa ainda dependeria do heap. Estados intermediários precisam de regras de cancelamento e exposição de topologia. | Aumenta duração potencial da barreira e superfície de regressão. Considerar apenas se evidência física justificar. |

**EXCEPTION PROPOSED, condicionada.** Restringir a exceção às alocações dentro de `loadPendingPatch()` com a barreira ativa e saída independente. A evidência host cobre estabilidade de heap e recuperação de falhas injetadas; a verificação de buffers físicos e a aceitação da exceção continuam pendentes. A exceção perde fundamento se houver admissão de notas obsoletas, falha de recuperação, crescimento de heap, falta de margem de stack ou de bloco contíguo, pausa no I²S, dropout fora da janela ou silêncio acima do limite de bancada acordado. As regras normais de `apply()` e `render()` seguem vigentes.

## Estados da UI

Proposta para o release gate: aceitar estados assíncronos de solicitação e espera e o resultado terminal vinculado ao token, geração e máscara aplicada. VALIDATING, PREPARING, APPLYING e VERIFYING são passos síncronos na mesma passagem. A mudança verifica novamente geração UI/USB, Storage Quiet e cancelamento antes de publicar confirmação. Se a transação foi interrompida após aplicar domínios, o snapshot preserva a máscara e a referência anterior. Uma invalidação posterior à decisão de commit é tratada como um evento posterior.
