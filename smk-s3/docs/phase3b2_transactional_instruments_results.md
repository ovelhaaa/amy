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

## Implementação: limite exato da garantia

`patches_load_patch_checked` retorna um `amy_patch_result` com:

| Resultado | Semântica |
| --- | --- |
| `AMY_PATCH_RESOURCES_ASSIGNED` | Mapas de vozes/oscs e container atribuídos (ou deleção explícita executada). **Não significa patch materializado ou Ready.** |
| `INVALID_REQUEST` | Synth, número de vozes ou bus inválido |
| `UNKNOWN_PATCH` | Gap ROM, patch inexistente ou índice de user patch fora dos limites |
| `INVALID_COST` | Custo zero ou não representável/maior que pool |
| `NO_VOICES` | Não há slots de voz suficientes |
| `NO_OSCS` | Não existe plano de blocos contíguos completo |
| `NO_INSTRUMENT_MEMORY` | Não foi possível preparar container e duas FIFOs |

O caminho novo valida o patch antes de liberar vozes, calcula os blocos reais
usando os mesmos pontos de início/paridade do alocador anterior (inclui TULIP),
considera os recursos reutilizáveis do instrumento substituído e os blocos já
escolhidos para todas as novas vozes. Não é uma soma aritmética de oscs.
Só depois de achar todos os blocos prepara o container e duas FIFOs reais.
Falhas nessa admissão não alteram mapas, container ativo, níveis ou deltas.
Não foi implementado compactador e um plano inviável pode ser recusado mesmo
que uma estratégia de reposicionamento diferente pudesse caber.

`instrument_prepare/commit/discard` separa container destacado de publicação.
Prepare não escreve níveis por voz nem ativa MIDI; discard libera somente seu
container/FIFOs. Commit publica e reinicializa níveis depois de liberar o
container anterior. O wrapper legado `instrument_add_new` agora conserva o
container anterior se prepare falhar. IDs/custos/vozes/bus são validados.
Essas funções **não reservam mapas ou slot de instrumento** enquanto um token
destacado é retido: só podem ser usadas pelo mesmo owner, com validação externa
de recursos; não são um backend assíncrono. O carregador prepara e faz commit
na mesma chamada, sem yield entre plano e atribuição.

Na admissão recusada, não há recursos DSP anteriores a restaurar: apenas
alocações temporárias de container/FIFOs são liberadas. Isso é cancelamento
de preparação de container, não rollback de patch. Após a atribuição de mapas,
reset/configuração ainda podem falhar. Rollback de materialização, falha de
rollback e estado Disabled após falha dupla continuam **BLOCKED**.

A API void `patches_load_patch` foi preservada. A adoção automática é opt-in
por `AMY_PATCH_CHECKED_RESOURCE_ADMISSION=1`, definido no componente SMK e nas
suítes host SMK com AMY real. Sem esse define, loader/eventos seguem o código
legado upstream preservado. A API checked explícita existe nos dois casos.
No SMK, o caminho normal de eventos usa o resultado e não encaminha Note On/controles
do restante de um load recusado para o instrumento anterior. Atenção: eventos
AMY que combinam FX/global e load podem ter efeitos independentes processados
**antes** da admissão, conforme a ordem legada; o evento inteiro não é atômico.
O parser também admite comandos imediatos. A garantia sem side effects é da
admissão direta, não de wire arbitrário nem de `amy_add_event` completo.
`patches_voices_for_load_synth` legado permanece no core por compatibilidade,
mas o novo carregador não chama seu caminho destrutivo.

## ACKs, gerações, estados musicais e switching

Não existe ACK Ready novo nem request ID nesta entrega. O enum é síncrono,
owner-only e limitado à admissão; não substitui o contrato solicitado.
Preparação completa, configuração aplicada confirmada, substituição de requests,
ACK obsoleto e consulta concorrente: **BLOCKED**, dependem de materialização
falível verificável e reserva completa. Não publicar PerformanceState como
aplicada com base neste enum.

Geração por instrumento e cancelamento de deltas seletivo: **BLOCKED**.
Barreiras global/por fonte, Panic, SourceStop, coalescência e PCM geracional
existentes foram preservados. Nenhuma substituição de `amy_deltas_reset`.
Não há scheduler adicional, engine adicional ou tarefa nova.

Mono/pilha/legato do adapter continuam pertencendo a A; referências continuam
A/drums, sustain lógico por canal físico e sustain poly por instrumento AMY.
Não há estado musical independente B integrado: **BLOCKED**. FM por destino
da 3B.1 permanece e suas regressões são executadas. Não apresentar testes de
sustain de A como prova de sustain seletivo de layers.

Política de troca no produto continua a legada, com barreira global/Panic.
Troca seletiva de A/B, desativação seletiva e Note Off tardio por geração de
instrumento não foram habilitados. Não há promessa de continuidade de outra
parte em troca bem-sucedida. Recusa de admissão direta preserva notas e recursos
da parte ativa; o produtor legado pode já ter solicitado Panic antes da recusa.

## Validação reproduzível

Comandos da raiz em PowerShell:

```powershell
./tests/run_smk_safety.ps1
./tests/run_smk_amy_boot.ps1
./tests/run_smk_performance.ps1
./tests/run_smk_patch_integrity.ps1
./tests/run_smk_synth_expansion.ps1
./tests/run_smk_ui.ps1
./tests/run_smk_amy_boot.ps1 -ResourcesOnly -BlockSize 128
./tests/run_smk_amy_boot.ps1 -Legacy
& C:/.platformio/penv/Scripts/python.exe -m platformio run -d smk-s3 -e esp32-s3-devkitc-1 -e esp32-s3-devkitc-1-128
git diff --check
```

Baseline: **PASS**, seis suítes, logs `build/phase3b2_baseline_*.log`. O baseline
AMY reproduziu B8 declarada com 48/64 oscs (A4 e drums intactos). Na versão
corrigida, o probe negativo exige B ausente após recusa e B4 completa após novo
pedido. O modo `-Legacy` compila sem o opt-in em diretório separado para verificar
a compatibilidade upstream, inclusive o comportamento parcial antigo. Não é
teste positivo de segurança nem configuração do produto.

### Falhas determinísticas e AMY real

`test_smk_instrument_resources.cpp` usa os mesmos fontes do componente AMY.
Somente o alocador de plataforma do executável é substituído para falhar no
malloc número N; o core e FIFOs são reais. `--wrap=free` rastreia e exige a
liberação dos temporários. O objeto amy.c exclusivo desse executável usa
`MALLOC_CAPS_DEFINED`, evitando que inlining contorne o ponto de injeção.
Os objetos de boot/owner usam o alocador normal. Nenhuma injeção no firmware.

| Status | Evidência |
| --- | --- |
| PASS | A4 patch128 + B4 patch135 + drums: 96 oscs completos, contíguos e exclusivos |
| PASS | OOM determinístico na alocação do instrumento, FIFO released e FIFO active; B ausente permanece ausente; substituições de A/B preservam instrumentos anteriores |
| PASS | Comparação de mapas bidirecionais, ponteiros/estado synthinfo, patch/vozes/nível, cabeça/qsize/quantidade de deltas livres, sustain/FIFO de A e delta futuro de A nas recusas |
| PASS | Patch ROM inexistente, user patch fora dos limites, custo zero/maior que pool, vozes/bus inválidos; falta de vozes e falta de oscs |
| PASS | Prepare/discard destacado conserva níveis e instrumento ativo |
| PASS | Load recusado combinado com Note On/nível não aplica o restante ao B anterior (não abrange FX anteriores à admissão) |
| PASS | 100 trocas alternadas A/B, vértices completos após cada troca; teardown/recriação B a cada dez trocas (Single/Dual apenas no harness de recursos) |
| PASS | Fragmentação induzida pelo alocador/release reais: 120 vozes de um osc na fixture host, remove posições pares, recompõe containers sobreviventes; 60 livres, maior trecho 1, pedido de oito oscs recusado sem editar os mapas diretamente |
| PASS | OOM pós-admissão em configuração real por delta deixa synthinfo ausente com mapas completos; prova explícita da limitação de Ready. Fixture incompleta não é renderizada |
| BLOCKED | Reserva completa de SRAM de osciladores, crescimento de envelopes, buffers auxiliares e deltas antes de commit; rollback DSP verificado |
| NOT RUN | Cancelamento/substituição de requests, ACK obsoleto, Panic/Stop durante preparação assíncrona (não há preparação assíncrona implementada) |
| BLOCKED | Mono A/poly B e inverso, legato/sustain dual seletivo, stealing B/Off tardio após substituição, troca seletiva durante música |

A fixture com 120 slots de voz é apenas para fabricar fragmentação verificável;
o limite de oscs continua 120 e o limite de vozes de produção não foi alterado.
As falhas são de chamada SRAM simulada, não de exaustão do heap do computador.
Não simula topologia/fragmentação física da SRAM do S3.

### Regressões e limitações da execução

Resultados finais de boot/owner, recurso 128 e build serão registrados abaixo.
Safety inclui clock/timing/ownership; expansion inclui persistência/StorageWorker;
patch integrity cobre FM/patches reais, e UI mantém os perfis existentes.

Sanitizadores: **BLOCKED**. Probe de link Clang 19 com
`'int main(void) { return 0; }' | & clang '-fsanitize=address,undefined' -x c - -o build/phase3b2_sanitizer_probe.exe`
falhou por ausência de `libclang_rt.asan_dynamic.dll.a` e
`libclang_rt.asan_dynamic_runtime_thunk.a`. Log em
`build/phase3b2_sanitizer_probe.log`. Não houve execução sanitizada.

Durante desenvolvimento, o teste de fragmentação inicialmente excedeu os 64
slots de voz padrão da fixture, e o teste de falha pós-admissão inicialmente
supôs que RESET_OSC alocava synthinfo. Ambos foram corrigidos no harness:
120 slots somente na fixture e delta WAVE real, pois reset de osc ausente é
no-op. Asserções intermediárias falharam; não são resultados PASS do produto.

## Arquitetura, custo e hardware

Mudanças upstream restritas a amy.c/h, patches.c e instrument.c: extensão necessária
porque o loader void libera recursos antes de expor qualquer ponto de extensão
à aplicação. Sem esse patch, um adapter só consegue detectar a corrupção após
a liberação. A admissão não destrutiva e a construção destacada são candidatas
a upstream; não foi feita proposta ou push. A API C void continua disponível,
sem tipos ESP-IDF/FreeRTOS nas extensões. A recusa integral substitui deliberadamente
o comportamento defeituoso no alvo SMK. O loader legado foi retido sob guarda:
aplicar a recusa indiscriminadamente ao demo upstream de 120 oscs deixaria seu
Synth 1 ausente (81 atribuídos em vez de 117 declarados/111 atribuídos). O SMK
usa default_synths=0 e não depende desse demo. Não alterar defaults, aumentar
pool ou quebrar outras plataformas para resolver esse conflito. Outros clientes
só adotarão a API estrita após ajustarem seu próprio orçamento de recursos.

Arrays locais do plano: três vetores uint16 de 32 elementos =192 bytes,
mais metadados. Não há heap para o plano proporcional ao número de oscs.
Pico de heap inclui temporariamente container e duas FIFOs novos junto dos
antigos, em vez de liberar o antigo primeiro. Sem novas dependências ou estado
persistente, sem alteração de AMY oscillator pool, FX, pinagem ou formatos.
Memória SRAM/PSRAM real livre, maior bloco, stacks e CPU antes/depois: **MANUAL**.

Planejamento é limitado por max voices/oscs, mas faz nova varredura antes de
atribuir. Não é operação aprovada para deadline de render: no limite S3
(32 vozes de instrumento, pool 120), limite conservador do termo de varredura
é 32×120×120×32 verificações, além de conflitos entre blocos. Casos práticos
curto-circuitam antes; não foi medido WCET físico. Materialização permanece
no owner legado e pode alocar, logar OOM e bloquear por mais de um bloco.
Não ativar troca dual em performance com base nos resultados host.

Nenhuma revisão DevKitC, VBUS, pinagem, controlador de display, strap PCM5102A
ou estágio de saída foi inferido. Hardware, USB, áudio analógico e soak: **MANUAL**.

## Critérios restantes e próximo menor passo

| Entrega | Estado |
| --- | --- |
| A — mapa e plano | PASS documentação e reprodução baseline |
| B — prepare/commit/rollback de patch completo | BLOCKED; implementada apenas admissão/container |
| C — geração/cancelamento por instrumento | BLOCKED; preservadas barreiras globais |
| D — estado musical e switching seletivo | BLOCKED; B continua inacessível no produto |
| E — regressões disponíveis | Resultados finais abaixo; qualificação dual completa BLOCKED |

Entrada na 3B.3 exige concluir a 3B.2: preparar programa de patch com erro
propagado em cada allocation/delta/buffer, reservar recursos reais, testar rollback
e falha dupla com safe Disabled, adicionar request IDs/ACKs e generations por
instrumento cobrindo referências/reset/bus da fila existente; separar ownership,
mono/legato/sustain e só então integrar B ao produto. Medir custo S3 e testar
Panic/Stop/substituição durante preparação antes de permitir troca em música.
O próximo menor passo é a materialização falível e verificável, sem UI/feature nova.

## Commits

- `0662ce8e`: auditoria anterior à implementação.
- Admissão e validação final: referências registradas ao concluir os checks.
