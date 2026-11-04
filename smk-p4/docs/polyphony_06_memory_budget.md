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

## Pré-alocação do serviço

FAIL antes da reserva:32 CC1 no mesmo canal com8 vozes fizeram delta_get
crescer o pool no serviço (assert do heap_probe). Acordes comuns não revelavam
essa pressão. Com Touch e USB simultâneos,6 também requer margem além do pool
inicial. O limite conservador em runtime_config.h é:

`8 * (4*32 + 33*(N-1) + 2*(2*N))`

Touch retrigger emite Off+On, quatro campos por oscilador. Se há Touch, no máximo
N-1 slots são USB; cada CC/bend pode atualizar todos, seguido por vibrato. Soma-se
reserva para comando de segurança nos bancos. Sem Touch, o limite também cobre
33*N atualizações por oscilador. Valores1944/2536/3128 para4/6/8.

Runtime::begin segura esse número de deltas via delta_get_checked e devolve a
lista antes de ready. É um laço limitado de boot, sem lista extra no heap. O pool
retém1/2/2 blocos de2048 deltas. Quatro mantém a alocação anterior. Não se mexeu
na arquitetura de bancos nem se alocam deltas no serviço dos casos medidos.
Falha da expansão é injetada nos perfis6/8: begin retornafalse, ready permanece
false, render retornaNULL, teardown sem leak e reinício bem-sucedido (PASS).

## Memória rastreada final — host x86-64 UCRT

| Vozes | Runtime estático | Bytes vivos AMY | Pico rastreado | Alocações vivas | Blocos delta |
|---|---|---|---|---|---|
|4|3212|116618|116749|161|1|
|6|3244|185890|186023|206|2|
|8|3276|206010|206143|250|2|

Antes da reserva,6/8 tinham136738/156858 bytes e205/249 alocações. A diferença
final49152 bytes é o bloco adicional de2048 deltas de24 bytes nesta ABI. Soma
de bytes não mede fragmentação nem heap interno da placa.

## ABI e admissão P4

Probe C compilado usando o comando real de amy.c no compile_commands.json,
inspecionado com riscv32-esp-elf-nm -S: sizeof(delta)=20, synthinfo=356,
mod_synthinfo=52. Artefatos build/p4_abi_probe.c/.o. Portanto o bloco adicional
P4 é40960 bytes, não os49152 do host. Runtime::bootFreeInternalBytes calcula
a reserva extra com sizeof(delta) da plataforma; não fixa uma ABI do host.

| Perfil | Total interno livre mínimo antes de AMY | Piso interno depois de AMY (expandido) |
|---|---|---|
|4|196608 B (mesma guarda anterior)|sem novo piso, preserva baseline|
|6|286720 B|65536 B livres e maior bloco32768 B|
|8|335872 B|65536 B livres e maior bloco32768 B|

O maior bloco deve ser>=196608 em todos os perfis, preservando a guarda original.
A demanda agregada não é uma alocação contígua. O histórico0.3A em diagnóstico
sem AMY tinha411199 bytes livres e maior bloco245760: exigir todo o orçamento
expandido em um único bloco poderia rejeitar memória utilizável. hasBootMemory
verifica total e maior bloco separadamente; teste automatizado cobre essa forma
de heap, total insuficiente e região contígua insuficiente. É evidência histórica,
não medição do boot0.6 nem aprovação de hardware.

A base196608 mais24576 por voz adicional é margem conservadora, não tamanho
medido nem garantia formal. O incremento do pool usa o ABI real. Os pisos
preservam margem para USB/LVGL/I²S/tarefas, mas não certificam que ela basta para
a montagem. Ausência da reserva recusa a configuração, não reduz vozes sozinha.
O P4 pode recusar8 mesmo com host aprovado: medir boot, total e maior bloco antes de
qualificar. Não diminuir o piso sem evidência comparável.

amy_start ainda retorna void e contém alocações iniciais sem teste de NULL.
A guarda anterior a ele reduz exposição; o teste de OOM cobre a nova expansão
checked, não todas as falhas possíveis do core legado. Não afirmar segurança
formal contra fragmentação/OOM ou alocação concorrente. Heap interno real,
maior bloco, PSRAM e stacks: MANUAL. Estática/Flash dos builds finais constam
no relatório. Nenhum consumo PSRAM adicional por voz foi solicitado pelo adapter.
