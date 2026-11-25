# SMK-P4 0.18-C.3 — Rhythm Performance UX

Execução: 24–25/11/2026. Branch `feature/smk-p4-ui-redesign-018c3`.
Base local e remota conferida por `git ls-remote origin`: `d81561fa821ccf498ea4f83863959ca8c9ef9965`.
SHA de código/testes congelado para a validação limpa: `9f809c18ccd2427115f768b89be468118bf9b5bf`.
Os commits posteriores de relatório/capturas não alteram os fontes testados.
Nenhum commit posterior encontrado na referência C.2a. Os 20 PNGs locais já modificados em
`ui_018c2_shots` foram preservados e não integram esta entrega. Sem merge em main, flash ou acesso serial.

## Arquitetura e auditoria

O pedido explícito P4/800×480 prevalece sobre a especificação raiz S3/284×76.
Foram consultados os documentos de navegação 0.18A, C.2a, sequenciador 0.16A/B/C,
Patterns 0.17A e Scenes 0.17B, build/CI, telas e adaptador musical existentes.
A documentação antiga que menciona oito padrões não representa o contrato atual:
`seq::kPatternSlots == 16`. Scenes mantém seus oito slots independentes.

```text
Rodapé TOCAR | TIMBRES | RITMO | FX | MAIS
                          |
            SEQUENCER | ARP | PATTERNS | DRUMS
                |
           PLAY | EDIT
                |
IUiBackend -> UiBlueprintPerformanceAdapter -> mailbox -> Audio Owner
```

A sidebar ocupa 160 px; o conteúdo contextual ocupa 640×388. Apenas o módulo
visitado permanece montado. `ScreenId::Patterns` foi acrescentado ao final do enum,
preservando os valores anteriores. `area_screens_` mantém o último módulo de Ritmo.
`sequencer_edit_` é estado de navegação independente de `subTab(Sequencer)`;
o campo antigo continua codificando oitava e passo. O slot pré-selecionado da
biblioteca também pertence à navegação, sem se confundir com ativo ou pendente.
Todos os pedidos de navegação dos novos botões são diferidos.

Não houve mudanças no AMY core, Audio Owner, algoritmos Arp/Seq, USB Host,
SMK25, DSP, filas, relógio, políticas de notas, NVS, CRC, versão ou tombstones.
Nenhuma dependência nova. Novas alocações são exclusivamente da montagem LVGL,
fora do áudio; os sliders de Seq enviam somente ao soltar.

## Layouts e controles reais

| Vista | Ações / contrato |
|---|---|
| Seq PLAY | PLAY, STOP SEQ, RESTART usam `seq::Action::Play/Stop/Restart`. Live usa `SetLive`; divisão, quantização, swing, comprimento e transpose usam suas ações existentes. |
| Seq PLAY monitor | Nome/slot ativo, MODIFIED/CLEAN, NEXT, referência de cena confirmada, modo MIDI, offset efetivo em semitons e 16 passos compactos. BPM permanece no cabeçalho global. |
| Seq EDIT | Duas páginas de oito cabeçalhos de 71 px, piano roll de 12 alturas, seleção e playhead distintos. Oitava, nota selecionada, velocity e gate ficam no rodapé. |
| Inspector de passo | Toque no nome do passo: semitom ±, oitava ±, REST, Enable/Rest, Prev/Next, Demo e Clear. Demo/Clear conservam confirmação existente. |
| MIDI / REC | NORMAL PLAY / STEP INPUT / STEP RECORD, fonte, canais de entrada e LT, record gate, referência e AUDITION. Fechar o painel não sai da captura. |
| Arp | Enable/Latch/Restart por parâmetros existentes; PLAY ARP/STOP ARP por `requestArpTransport`; Clear por `requestClearLatch`; direção/rate/octaves/gate/swing existentes e Tap global. |
| Drums | PLANNED / NAO IMPLEMENTADO. Retirados controles demonstrativos de bateria que não representavam engine P4 operacional. |

Os passos fora da página editada continuam acessíveis pela troca 1–8 / 9–16.
O piano roll é uma visão compacta; o inspector fornece os alvos amplos de ajuste
de altura. Notas fora da oitava usam marcadores de direção. A seleção não escreve
uma nota; tocar uma célula diferente é uma edição explícita. Nenhuma montagem,
troca de módulo ou PLAY/EDIT envia Stop, Restart ou Panic.

O modo de captura e o Live Transpose vêm do snapshot, nunca da existência do
painel. Live Transpose e Step Record no mesmo canal conservam a precedência real
do roteamento. A suíte desliga Live explicitamente ao testar gravação no mesmo
canal. USB continua sendo produtor apenas de sua fila; não foi conectado ao
mailbox de UI. Sustain, Note Off, AUDITION e ownership permanecem no motor.

## Patterns, armazenamento e Scenes

Os 16 slots aparecem simultaneamente, em quatro linhas. A seleção altera somente
o slot pré-selecionado. LOAD é explícito; `*` identifica ativo e `>` pendente.
Os detalhes vêm do `seq::Slot` verificado: comprimento, divisão, swing,
transpose, quantização e integridade. O campo de nome abre o teclado LVGL.

IMMEDIATE/NEXT BAR reutilizam `requestPattern`. A fronteira continua em 96 ticks,
4/4 do clock global, independente do comprimento e divisão. O motor preserva a
aplicação em STOP e o início no passo 1. CANCEL QUEUE usa o cancelamento existente.
O backend continua revalidando dirty state, geração, token e revisão na aplicação.

LOAD sobre MODIFIED oferece DISCARD & LOAD e SAVE & LOAD. Overwrite, Delete e
Save & Load abrem `ConfirmDialog` com origem/destino identificados e aviso de
storage quiet. SAVE em slot vazio também pede confirmação de silêncio. Operações
sem slot válido, biblioteca ocupada ou condições musicais inadequadas ficam
indisponíveis. Fechar/confirmar os novos modais não destrói widgets no callback.

A UI publica `patternNotice()` sem inventar confirmação: REQUESTED, espera por
storage quiet, READY TO WRITE (cancelável), espera pelo owner, CONFIRMED,
REJECTED e TIMEOUT / SLOT WRITTEN, OWNER UNCONFIRMED permanecem distintos.
**A escrita e a verificação NVS são síncronas na passagem do controle:** não
existem frames independentes de WRITING/VERIFYING no backend atual; não foram
criadas animações fictícias. SAVE pode silenciar e interromper transporte.

Scenes é somente contexto do snapshot `sceneView().recall.active`. Não há novo
recall, Scene Chain, agendamento, modificação de locks ou seleção de domínios.
A navegação para Patterns e Scenes não carrega padrões implicitamente.

## Capturas

O [índice de capturas](ui_018c3_shots/README.md) reúne os estados.
`ui_018c3_shots/` contém PNGs reais LVGL 9.3, 800×480, gerados pelo harness com
AMY real, NVS fake e pacotes USB injetados. Todos são **HOST SIM**, não evidência
física. Nomes numerados identificam Arp, PLAY, EDIT, captura MIDI, biblioteca,
NEXT BAR, confirmação e falha. `25_fx_while_running.png` mostra navegação com
Arp com nota MIDI retida e Seq em execução; a continuidade é aferida também pelas asserções.

## Validação

| Verificação | Resultado |
|---|---|
| `run_arp_015.ps1` | PASS: relógio global, divisões, latch, wrap e Tap Tempo. |
| `run_step_sequencer_016a.ps1` | PASS: mailbox concorrente, owner, capacidade, notas, overflow/Stop e Panic. |
| `run_step_sequencer_midi_016c.ps1` | PASS: AMY real, gravação, ownership, Harmony e Live Transpose. |
| `run_pattern_library_017a.ps1` | PASS: 16 slots, Rename/Delete, CRC, versão incompatível, falta de espaço, torn write, dirty/discard, limites 4/4 e cancelamento. |
| `run_step_sequencer_ui_016b.ps1` | FAIL: entrada por coordenadas legadas, detalhada abaixo. |
| C.3 e C.2a limpas com regressões | PASS: ambos os manifestos `result=PASS`, `clean=true`, `regressions=true`, mesmo SHA congelado. |
| Dois builds P4 e verificadores | PASS, recursos na seção própria. |
| Áudio/USB/touch físicos | MANUAL: adiados, não executados. |

A C.3 exercita seleção dos 16 passos, velocity/gate pelos sliders, troca PLAY/EDIT,
Live Transpose, quantização, REST/NEXT, 100 ciclos de Note On/Off em Step Record
com avanço e wrap, fechamento do painel sem desligar captura, Stop Arp independente,
seleção sem carga, IMMEDIATE/NEXT BAR, cancelamento, confirmação de Save, falha
publicada pelo backend e 100 confirmações canceladas. O stress inclui 500 ciclos
entre áreas e modos, 200 entradas/saídas de Patterns e 200 alternâncias Arp/Seq,
com notas Arp efetivas e Seq em execução. Verifica geração e token de transporte
inalterados, relógio avançando, contagem de objetos e memória sem crescimento.

Não se alterou nenhuma asserção musical herdada para obter PASS. A adaptação
SMK25 apenas aponta seus cliques para os novos nomes explícitos de transporte.
A medição de transições é do ciclo agregado de navegação em host, não latência
de áudio nem tempo individual de cada botão.

### Métricas finais LVGL (host)

| Suíte | Objetos antes→depois | Livre antes→depois | Maior bloco antes→depois | Pico | Fragmentação |
|---|---:|---:|---:|---:|---:|
| C.3 | 92→92 | 73.128→73.128 B | 72.064→72.064 B | 66.328 B | 2→2% |
| C.2a | 91→91 | 74.632→74.632 B | 73.624→73.624 B | 100.680 B | 2→2% |
| C.1 | 91→91 | 74.656→74.656 B | 73.648→73.648 B | 75.472 B | 2→2% |
| C.1a | 91→91 | 75.744→75.744 B | 74.680→74.680 B | 66.816 B | 2→2% |
| Sound Editor / FX | 96→96 | 75.600→75.600 B | 75.536→75.536 B | 81.320 B | 1→1% |

C.3, 500 ciclos agregados: mediana 3.093,7 µs, P95 5.131,3 µs,
máximo 7.940,2 µs. Crescimento líquido: **0 bytes e 0 objetos**.
Maior pico das suítes executadas: 100.680 B de 131.072 B (margem 30.392 B).
Essas métricas não substituem a medição de deadlines no hardware.
Os dois manifestos em `build/ui_018c3/test_manifest.json` e
`build/ui_018c2/test_manifest.json` registram 240 objetos LVGL, 20 AMY e 34 UI,
compilados em cada build limpo, SHA, flags, ferramentas e hashes dos inputs.

Comandos executados da raiz:

```powershell
powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_ui_018c3.ps1 -Clean -Regressions
powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_ui_018c2.ps1 -Clean -Regressions
powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_arp_015.ps1
powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_step_sequencer_016a.ps1
powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_step_sequencer_ui_016b.ps1
powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_step_sequencer_midi_016c.ps1
powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_pattern_library_017a.ps1
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8 -e guition-editor-8-bench -j 8
python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --environment guition-editor-8
python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --musical --environment guition-editor-8-bench
```

A suíte nova deriva da infraestrutura de proveniência C.2a: compila LVGL/AMY/UI
próprios, fingerprint de fontes/headers/flags/ferramentas/script, sem stamp em
falha, manifesto com branch/SHA/dirty state. `-Regressions` inclui C.1, C.1a,
Sound Editor e SMK25, cobrindo Harmony, FX, Performance, Scenes, locks e notas.

FAIL conhecido da suíte antiga 0.16B: `step_sequencer_ui_016b_test.cpp:156` espera
COMMAND PENDING após tocar coordenadas fixas da grade antiga (`61+45*step`,
`126+row*17`) sem entrar no novo EDIT. O teste foi executado sem alterar suas
asserções. Essas coordenadas agora pertencem à navegação/sidebar e não editam
notas. Os contratos musicais puros 0.16A/C têm suíte própria; a suíte C.3 cobre
edição e seleção no layout novo. Este FAIL não foi convertido em PASS.

A falha herdada de `run_amy.ps1 -Voices 8 -SoundLibrary` continua separada:
orçamento legado fora do perfil editor, documentado em C.2a. NOT RUN nesta etapa. Sem alteração da
asserção nem atribuição automática à remodelação.

## Builds e recursos P4

| Perfil | Compilação / check_blueprint_build | RAM estática | Flash | Delta vs C.2a |
|---|---|---:|---:|---:|
| guition-editor-8 | PASS / PASS | 70.272 B | 1.518.388 B | RAM 0 B; Flash +3.410 B |
| guition-editor-8-bench | PASS / PASS | 70.828 B | 1.623.878 B | RAM 0 B; Flash +3.410 B |

`firmware.bin`: 1.519.120 / 1.624.608 B, respectivamente. Nenhum warning nos
logs P4 finais. O host conserva warnings de AMY C sobre parâmetros não usados
e casts de ponteiro em diagnósticos de plataformas de 64 bits; não houve alteração
nesses fontes. Pool LVGL de 128 KiB preservado. Não são medições físicas de CPU,
latência ou underruns. Os deltas usam os números publicados em C.2a.

## Arquivos alterados

- `include/ui_rhythm.h`, `ui_navigation.h`, `ui_screens.h`, `src/ui_navigation.cpp`:
  sidebar, destino Patterns e estado visual independente.
- `screens/arpeggiator_screen.cpp`, `sequencer_screen.cpp`, `drums_screen.cpp`:
  layouts funcionais, inspector, biblioteca e estado planejado da bateria.
- `widgets/status_strip.cpp`: lista Ritmo inclui Patterns e preserva o campo
  de seleção/oitava do sequenciador ao entrar pelo menu de módulos.
- `tests/run_ui_018c3.ps1`, `ui_018c3_test.cpp`: build independente e regressões.
- `tests/smk25_0142_ui_test.cpp`: somente os alvos textuais de clique mudaram
  de PLAY/STOP para PLAY ARP/STOP ARP; asserções musicais inalteradas.
- README, relatório e capturas: procedimento, limites e evidência.

## Hardware, riscos e próximos passos

MANUAL: qualificação física adiada pelo usuário. Nenhum pinout novo, flash,
monitor ou teste físico foi executado. Permanecem as qualificações da unidade
Guition e do PCM5102A/USB existentes; este trabalho não certifica palco.
Tempos e memória de host não demonstram deadlines de áudio no P4.

Procedimento físico futuro (não executado): com firmware específico autorizado,
tocar notas/sustain, ligar Arp+Seq, alternar PLAY/EDIT e FX/Timbres; exigir notas
contínuas e ausência de reinício. Verificar Stop por fonte e Panic global;
gravar 16 notas com wrap, alternar páginas e audicionar. Carregar NEXT BAR com
comprimentos 5 e 16; exigir a mesma fronteira 4/4. Salvar com confirmação de
silêncio, reiniciar e conferir nome/passos/integridade. Registrar underruns,
render máximo, heap e resposta touch, sem inferir qualidade de áudio do build.

Próximo passo, somente planejamento: harmonizar indicadores entre Performance,
Ritmo e FX; reduzir toques para retornar ao último padrão e preparar transições
Scenes/Patterns, preservando seletividade e locks. Avaliar atalhos SMK25 somente
sobre o perfil confirmado e seu produtor atual. Harmonic Pads e encadeamento
continuam propostas futuras, sem novo engine nesta etapa.


## Veredito

**CONDITIONAL GO para revisão de software em host.** Nova UI funcional, suítes
limpas C.3/C.2a com regressões aprovadas e ambos os builds P4 aprovados.
O FAIL da automação por coordenadas antigas 0.16B foi classificado e preservado;
a falha de orçamento SoundLibrary legada permanece separada e não foi reexecutada.
As fases síncronas de escrita/verificação e as qualificações físicas continuam
limites explícitos. **Não qualificado para palco.**
