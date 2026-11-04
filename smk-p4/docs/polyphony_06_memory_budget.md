# SMK-P4 0.6 — orçamento de memória

## Auditoria da arquitetura

Dois bancos ROM128/135, somente um musicalmente ativo; cada slot aponta para
um instrumento AMY de uma voz. Oito osciladores por instrumento (ALGO, seis
operadores FM e auxiliar); a tabela patch_oscs e a verificação de ownership no
boot devem concordar. Não há segundo banco de notas simultâneas.

| Vozes musicais | Instrumentos/vozes AMY | Osciladores | max_synths (ID0 reservado) |
|---|---|---|---|
| 4 | 8 | 64 | 9 |
| 6 | 12 | 96 | 13 |
| 8 | 16 | 128 | 17 |

AMY_OSCS usa config.max_oscs, não um teto compilado de64. A API suporta
MAX_VOICES_PER_INSTRUMENT=32, mas cada instrumento local pede somente1.
Os IDs de synth cabem em uint8_t. patches_init dimensiona mapas por max_oscs e
max_voices; instruments_init dimensiona a tabela por max_synths. FIFO/estado
instrument_info, envelopes e estado dos osciladores são materializados no boot.
Filtros estão embutidos no estado. Não foram adicionados patches, efeitos ou bancos.

O pool de deltas cresce em blocos de2048, teto16 blocos em src/amy.c. Carregar
ambos bancos antes do serviço reserva a pressão de patch no boot. O teste de
heap intercepta malloc/calloc/realloc/free durante apply/render, incluindo
acordes, sustain, retrigger, patch e fila cheia. Nenhum crescimento nesse caminho
é permitido pelo teste. Scratch FM, buffers de mix/I/O e tabelas são compartilhados
por bloco; não se multiplicam por voz. DMA permanece inalterado.

## Evidência inicial de host

Quatro vozes (antes da configuração nova): Runtime3212 bytes, alocações AMY
vivas116618 bytes, pico rastreado116749 bytes,161 alocações. ABI x86-64 Windows:
esses números não são RAM interna do P4. Medição via heap_probe, sem estimar
fragmentação física a partir da soma de bytes.

Medições finais dos perfis e símbolos do firmware serão registradas no relatório.
Heap interno, maior bloco, PSRAM livre e stacks requerem boot físico; MANUAL.

## Admissão e risco herdado

amy_start retorna void e contém alocações iniciais sem teste de NULL. A guarda
conservadora anterior exige maior bloco interno>=196608 antes de inicializar.
Apenas verificar oom_count depois não torna essa API segura contra OOM inicial.
Qualquer expansão precisa aumentar a admissão, preservar reservas para USB,
LVGL/display e tarefas e recusar inicialização sem reduzir vozes silenciosamente.
A guarda não é uma prova formal contra fragmentação/OOM ou alocadores concorrentes.
Não mudar os bancos para contornar um orçamento sem medições comparáveis.
