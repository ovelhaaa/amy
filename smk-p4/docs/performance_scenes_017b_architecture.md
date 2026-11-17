# Performance Scenes 0.17B

Base auditada — `cdcc5d0bd68cffe98e967ab120b35c43199e44c1`, HEAD remoto da 0.17A e do [PR #46](https://github.com/ovelhaaa/amy/pull/46).
O `specs.md` descreve o SMK-S3, incluindo layers, pads e outros pontos de troca.
O pedido da 0.17B define o escopo do ESP32-P4 para este trabalho.

## Auditoria da 0.17A

| Domínio | Configuração e persistência | Execução e ownership |
| --- | --- | --- |
| Patch Engine | IDs do catálogo de ROM e parâmetros manuais de `edit::State`. | `Runtime::loadPendingPatch()` roda no worker de síntese. O pedido de patch silencia as vozes e muda a geração das filas. A conclusão muda a geração novamente. |
| Presets V4 | `LocalPerformanceBackend` coordena operações de `NvsUserPresetStoreV2`, incluindo rotas e macros. | O backend espera gerações e revisões específicas após trocar o patch. A configuração de cena usa o ID do patch AMY. |
| Performance Lock | Máscara `perf_lock::Options` reaproveitada no cálculo de seleção. | A troca de patch usa a máscara para preservar domínios, sujeita à disponibilidade de parâmetros do novo patch. |
| FX | Configuração manual, composição de macros, Delay SYNC e armazenamento FX. | O Audio Owner aplica comandos FX e atualiza o DSP. Tails e interpolação pertencem à execução. |
| Harmony Assist e Chord Assist | Ambas as funções usam `harmony::Config`. | Os grupos de notas guardam a origem e notas admitidas. A liberação usa a propriedade do grupo. |
| Arpeggiator | `arp::Config` contém os parâmetros recuperáveis. | Entradas, latch em execução, gate, cursor e saídas são propriedade do Audio Owner. |
| Global Tempo Clock | BPM em `tempo::Clock` com persistência opcional existente. | Arp, Delay SYNC e sequenciador leem a fase global. `setBpm()` altera a taxa sem zerar a fase. |
| Step Sequencer | Padrão, divisão, swing, transpose salvo e quantize. | Playhead, transporte, Live Transpose, notas em voo e troca pendente pertencem à execução. |
| Pattern Library | `patterns017`, 16 slots com duas cópias, CRC, versão e tombstone. | O adaptador resolve o slot e o owner publica confirmação e dirty state próprios do padrão. |
| Storage Quiet e NVS | O backend solicita Storage Quiet e o adaptador faz I/O no contexto de controle. | O owner silencia a saída e invalida eventos anteriores ao retomar. O cancelamento de Pattern antes de chamar NVS aborta a transação. |
| Audio Owner e filas | Mailboxes limitadas, incluindo slot independente para Stop. | O owner aplica comandos e publica snapshots. A fronteira de geração evita aplicar comandos de um patch anterior. |
| UI Blueprint LVGL | O adaptador é a fronteira da UI com o backend local. | O Blueprint consome snapshots publicados. O browser de cenas e o acesso rápido ainda não foram integrados. |
| SMK25 | Roteamento e mapeamento de controles no perfil existente. | Normal Play, Step Input, Step Record e Live Transpose dependem do roteamento de notas. Nenhum destino novo foi instalado. |

## Implementado

- Máscara efetiva = domínios armazenados ∩ seleção solicitada, com exclusão dos domínios protegidos pelo Performance Lock e de TEMPO se Apply Scene Tempo estiver desativado. Validação rejeita a seleção inteira se uma referência solicitada estiver indisponível ou se as rotas selecionadas forem incompatíveis com as capacidades fornecidas para o patch resultante.
- O ID estável é o índice do slot, de 0 a 7. O banco isolado `scenes017b` usa chaves `s00a` a `s07b`. O registro contém magic, versão, sequência, máscara, número de overrides, payload e CRC. O payload guarda apenas o índice de Pattern. A representação persistida de FX usa o codec existente e sua resolução de 1% para os valores normalizados.
- O modelo decodificado mede 1.256 bytes no host; rotas expandidas medem 1.156 bytes. O registro usa 92 bytes sem overrides, 15 bytes por override e até 812 bytes no máximo. O tombstone usa 20 bytes. O store escreve no banco alternativo, lê de volta e verifica os bytes. Um banco de versão desconhecida bloqueia mutações mesmo com um banco antigo válido.
- Pattern usa uma referência para os dados já materializados na mailbox em `applySequencer()`. A stack configurada permanece em 12.800 bytes.

## Integração necessária para recall ao vivo

1. Validar integridade, slot de Pattern, edições pendentes e disponibilidade antes de propor um plano. Fixar a revisão do Pattern resolvido. Se a seleção inclui PATCH, resolver também as capacidades dependentes da topologia do novo patch; a máscara de família por si só é insuficiente.
2. Usar uma proposta limitada em memória estática, associada a token e geração. O owner deve validar novamente na aplicação. Para NEXT BAR, medir a fase global com resolução de bloco e usar múltiplos de 96 ticks do clock 4/4. A alteração de BPM pertence ao ponto de aplicação e deve preservar a fase.
3. Para PATCH, usar uma barreira musical que preserve o plano enquanto passa pelas duas mudanças de geração esperadas. Tratar Panic, Stop, desconexão ou geração inesperada como cancelamento ou falha. O carregamento destrutivo de patch pode falhar depois de liberar vozes; rollback exigiria outra carga, com os mesmos riscos. Reportar falha e manter áudio em estado seguro.
4. Para PATTERN isolado, liberar apenas saídas do sequenciador na aplicação. Para HARMONY, manter ownership das notas antigas até o Note Off correspondente. Para ARP, usar o contrato de liberação do engine. MACROS pode modular FX indiretamente; verificar o estado manual e efetivo e respeitar FX protegido para que a seleção permaneça previsível.
5. Verificar estado publicado antes de promover a cena ativa ou o baseline CLEAN. Dirty state deve observar parâmetros recuperáveis e a referência de Pattern, excluindo playhead, transporte, sustentação e Live Transpose temporário.

O store e o validador são exercitados no host. A máquina de estados de recall, a publicação de dirty state, UI e MIDI, e o fluxo Live Set dependem dessa integração.
