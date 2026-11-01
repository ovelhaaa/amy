# Fase 3B.2 — admissão de recursos de instrumentos (entrega parcial)

**Fase não concluída. Layer B permanece desabilitada no produto.**

## Estado inicial e auditoria antes da implementação

HEAD inicial `2863b7f7`, branch `feature/smk-s3-dual-amy-runtime`, árvore limpa.
`git rev-list --left-right --count main...HEAD`: `0 3`. Main é ancestral;
os commits adicionais são `1f789b41`, `01575aec`, `2863b7f7`. Nenhum merge,
reset, rebase, descarte ou push é necessário. A comparação é entre referências
locais; não afirma atualização do remoto. Relatórios 3A, 3B e 3B.1 consultados.
README tem conflitos antigos AMY/P4; specs pede 284×76 e o perfil local usa
240×240. Nenhum desses conflitos de documentação/hardware é resolvido aqui.

### Mapa destrutivo do carregador original

| Operação | Efeito / falha | Estado que precisaria de restauração |
| --- | --- | --- |
| `patches_voices_for_load_synth` | Libera oscs antigos se muda a contagem, antes de validar patch; seleção de voz pode falhar | Mapas bidirecionais, resets já enfileirados, instrumento antigo |
| `patches_load_patch` | Enfileira resets antes de validar patch; acesso de user patch sem limite superior | DSP ativo, deltas, memória inválida |
| Loop de oscs | Libera todos os antigos e atribui blocos um por vez; falha de contiguidade não aborta | Vozes antigas e novas, dono de cada osc, resets |
| `instrument_add_new` | Libera container anterior antes de alocar o novo | Instrumento, duas FIFOs, notas, sustain, nível por voz, MIDI ativo |
| `instrument_init` / `voice_fifo_init` | Três mallocs SRAM; FIFOs têm armazenamento fixo; OOM parcial é liberado | Container e FIFO sobrevivente; não é necessário recriar voz DSP aqui |
| Publicação do instrumento | Ocorre antes da materialização; contagem declarada não prova integridade | Estado observável e ativação MIDI |
| `parse_patch_string_to_queue` | Parser pode produzir comandos de synth, MIDI e bus; não é uma função pura | Mappings, propriedades de synth, FX/bus globais |
| `amy_event_to_deltas_queue` | `ensure_osc_allocd` aloca synthinfo/mod_synthinfo/envelopes em ram_caps_events; retorno nem sempre propagado | Osciladores reais, vetores e side effects de parâmetros |
| `add_delta_to_queue` / `delta_get` | Pool pode crescer; OOM descarta delta, API void; teto de blocos aborta | Configuração parcial, free-list, qsize, ordem estável por tempo |
| `play_delta` | Cresce vetores de breakpoints; OOM descarta delta. PCM/efeitos têm caminhos adicionais | Materialização efetiva, buffers e estado DSP anterior |

Pool global permanece 120; mapas não reservam SRAM. Configuração SMK usa SRAM
interna para instrumentos e estados de osc, PSRAM para delay/reverb/samples;
pool de deltas permite fallback PSRAM no componente existente. FIFO não usa
PSRAM separadamente. Ausência de bloco SRAM contíguo pode falhar mesmo havendo
oscs livres. Contagem de oscs jamais é prova de memória ou de preparação.

### Bloqueio arquitetural e menor passo seguro

Não é seguro anunciar Ready após `amy_add_event`, após contar vozes ou após
executar deltas: esses caminhos não propagam todos os erros de materialização.
Também não é seguro restaurar apenas patch/contagem após OOM: DSP, envelopes,
MIDI e deltas podem já ter mudado. Uma transação completa precisa preparar um
programa de patch validado, reservar seus deltas/buffers e separar side effects
antes de publicar. Não introduzir outro scheduler nem afirmar rollback completo.

Primeiro incremento: tornar a **admissão de mapas/container** não destrutiva em
falhas de validação, contiguidade e malloc de instrumento/FIFOs. Planejar os
blocos concretos, preparar container destacado e só então atribuir recursos.
Isso não certifica a materialização e não habilita patch switching em performance
dual. A API de resultado terá nome de recursos atribuídos, nunca Ready/ACK.

### Invariantes de eventos a preservar

Mailbox tem geração global e por fonte, não por instrumento; preset avança a
global. NoteOwnership aceita A/drums, mono/pilha do adapter pertence a A;
sustain lógico é por canal físico. Não remover essas barreiras nesta entrega.
Deltas perdem synth ao serem expandidos: `osc` pode significar oscilador ou bus;
RESET_OSC guarda o destino em `data.i`, podendo carregar flags globais.
Referências MOD_SOURCE/CHAINED_OSC/ALGO_SOURCE também têm destinos. Filtrar só
`d.osc` descartaria resets errados e poderia preservar onsets obsoletos. A fila
é lista ordenada estável (tempo relativo ao wrap), reciclada em free-list;
`delta_qsize` é global. `amy_execute_deltas` também avança sequencer/CV.
PCM já misturado não permite subtrair B sem renderizar novamente A/drums/FX.
Panic global precisa continuar limpando deltas e PCM globalmente.

## Validação, implementação e pendências

Resultados finais serão acrescentados depois da implementação e execução.
