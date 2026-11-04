# SMK-P4 Integration 0.4 — resultados

2026-11-03. Trabalho em `feature/smk-p4-integration-04`, worktree separado
`build/worktrees/smk-p4-integration-04`. O pedido explícito Guition P4/800×480
prevalece sobre specs.md/AGENTS gerais S3/284×76; nenhuma migração do core AMY.

**Resultado final:** integração0.4 concluída no escopo aprovado. Firmware
`414624d9…8259c7` instalado novamente na COM13 após comparação A/B autorizada.
PASS builds/host e estabilidade inicial no P4:30min51,7s contínuos, sem falhas
instrumentadas. Operador confirmou UI/controles/USB; cliques de retrigger foram
reproduzidos da mesma forma no0.3A e explicitamente adiados para a próxima etapa.
Não se afirma ausência desses cliques, baixa latência física ou qualificação de palco.

## Identidade e preservação

- Base musical: `adf2e95a` / feature/smk-p4-firmware-integration-ready.
- Origem UI: `7b2ef093` / feature/smk-p4-ui-integration-ready.
- Merge controlado, sem conflitos: `55563d59`, com os dois históricos preservados.
- Adaptador, boot, segurança e testes: `18d96e93`.
- Cobertura ampliada e checker nativo: `9ebde965`.
- Status Host independente da readiness: `df9a9f26`.
- Watermark da pilha UI no diagnóstico a5s: `e2b583d3`.
- Análise versionada de janelas seriais e8 testes: `d1b7d420`.
- Branches de origem e worktrees começaram limpos; nenhum reset Git ou force-push nas origens.
- PASS verificador do bundle físico original, fornecido explicitamente em
  `build/p403a/removal_grace_image` no checkout original. SHA firmware0.3A:
  `89fc42a19b0d588970bf6bd18ded139af1bc3843ae398d0cc2404fa1cfa2e9a7`.
  Não foi regravado, renomeado nem substituído por recompilação.
- Enumeração sem reset encontrou COM13 / USB303A:1001 / serial80:F1:B2:D3:CC:86.
  O firmware instalado foi identificado posteriormente por leitura da Flash,
  conforme seção de preparação física abaixo.

## Implementação e arquivos

Adaptador [header](../main/ui_blueprint_performance_adapter.h) /
[implementação](../main/ui_blueprint_performance_adapter.cpp): usa a API real
PerformanceBackend, sem extensão pública. Contrato corrigido em
[ui_backend_port_contract.md](ui_backend_port_contract.md); links file:///C:
substituídos por caminhos relativos nos documentos UI.

Arquivos de integração: main/CMakeLists.txt, main/Kconfig.projbuild,
main/standalone_main.cpp, platformio.ini, sdkconfig.defaults.blueprint,
components/ui_blueprint/CMakeLists.txt e include/ui_model.h.
Correções de apresentação/ciclo de vida: widgets virtual_keyboard, status_strip,
parameter_slider, parameter_knob, patch_selector, signal_meter; screens
performance, patch_browser, diagnostics, harmony, midi, settings, sequencer;
src/ui_navigation.cpp. Testes: tests/ui_blueprint_adapter_test.cpp,
run_blueprint_adapter.ps1, check_blueprint_build.py, standalone_capture_report.py
e test_standalone_capture_report.py; CI P4 ampliada para o
ambiente Blueprint e teste host integrado. README e documentos UI atualizados.
Os64 arquivos originais da UI foram incorporados pelo merge, sem reescrever seu
histórico. Lista exata reproduzível: `git diff --name-only adf2e95a HEAD`.

Arquitetura: uma única tarefa UI e instância musical. O boot conserva admissão
AMY e snapshot inicial antes das alocações LVGL. Nenhuma segunda tarefa/driver
LVGL/display/touch. Nenhum widget chama AMY/I²S/USB. Owner e mailbox preservados.
Nenhuma alteração em src/, shared/, smk-s3/, amy_runtime, audio_pcm5102 ou
usb_midi_host. Sem nova dependência, aumento de pool, polifonia, patches ou FX.

## Controles e confirmação

| Conectado realmente | Sem backend musical nesta rodada |
|---|---|
| ROM128/135 Layer A | Macros1–8, Layer B, split, Mod Matrix, FX |
| Master0..127 | Arp, sequencer, drums, MIDI Learn |
| Note On/Off e Panic | Scale/Chord aplicados ao MIDI, persistência |
| Oitava local -2..2 | Medidores DSP/amplitude |
| MIDI/áudio/gates/render do snapshot | Diagnóstico sem fonte não é simulado |

setPatch/setMaster true → Requested; modelo só muda no snapshot com valor
aplicado, em revisão diferente da observada no pedido. false → Rejected.
Parâmetros planejados → Unsupported; controles desabilitados/PLAN. Oitava é
transformação de entrada local, não comando DSP. Note/Panic ficam Requested:
não há ACK individual na API. active_notes agregado não é ACK nem caudas DSP.
Uma publicação nova com dados antigos e mesmo valor é indistinguível pela API;
confirmação prova valor observado, não causalidade do pedido.

MIDI ON usa flag de conexão Host; engine ready não significa USB conectado.
AUD RUN exige saída física ativa. DMA underruns e amplitude permanecem N/A.
Memória só exibe números após amostragem real a cada5s, fora do áudio.
As métricas completas já existentes continuam nos logs a5s; o novo log
UI_BLUEPRINT acrescenta watermark da pilha UI e contagem de transições, fora do áudio.

UI antiga: `guition-standalone` continua padrão. Nova UI:
`guition-standalone-blueprint` / CONFIG_SMKP4_UI_BLUEPRINT=y. Nunca simultâneas.
Ambos mantêm gates elétricos desabilitados nos defaults. Preview isolado conserva
UiDemoBackend sem comunicação silenciosa com AMY. Harmony/Split são visuais no
live; audition demo não envia notas live. Reset global demo foi bloqueado no
live para não sobrescrever estado confirmado; grids/favoritos continuam RAM.

## Segurança do teclado e testes host

Ledger fixo32 gestos com identidades monotônicas UI distintas dos tokens internos.
Primeiro On/último Off por pitch, releases antigos ignorados após cancelamento,
Panic, patch, oitava e navegação. Note Off nunca depende da fila normal.
Lease10s começa no primeiro On, compartilhado por gestos do pitch, sem renovação
por heldNote ou On periódico. Expiração emite Off e encerra todos os gestos;
soltar/pressionar rearma. GT911 continua single-touch, sem promessa de multitouch.

Corrigido risco comprovado na ordem de DELETE LVGL9.3: pai recebia DELETE antes
dos filhos e liberava KeySlot ainda referenciado pelos callbacks filhos. Agora
release no pai não acessa visuais, callbacks filhos são removidos antes de liberar
estado; DELETE isolado de tecla libera gesto e zera ponteiros. Testado com
adaptador real, não apenas MockPerformanceBackendAdapter da demo original.

PASS adaptador de produção + mock PerformanceBackend: catálogo128/135,
indisponíveis/Layer B, Master, rejeição, revisão antiga/valor anterior, confirmação
assíncrona, dois gestos por pitch, release antigo após Panic, oitava, lease de
segundo gesto após release do primeiro, engine offline, USB flags sem revisão,
status Host independente de falha da engine,
backend sem primeiro snapshot, saturação ledger e controles planejados.
PASS LVGL real: PRESSED duplicado, CLICKED ignorado, PRESS_LOST, exclusão isolada
de tecla/teclado, navegação, shutdown ativo, octave-held, previews sem live MIDI,
reset demonstrativo sem mudar patch/master live e nenhuma escrita DSP de startup.
PASS AMY real + adaptador: USB/Touch mesma altura nas duas ordens de release,
patch aplicado, Panic com USB/sustain/Touch, gerações, mailbox antigo, lease/rearm, desconexão USB
preservando Touch, fila cheia rejeitando Master/On e preservando Off; patch rejeitado ainda cancela.

## Memória e capturas

Pool LVGL host131072bytes, sem aumento. Demo original rerodada:700 mounts,
free64672→64672, pico99040, frag1%. Live adapter: primeiro bloco700 transitions
free72280→72248 (-32bytes); segundo bloco700 transitions manteve72248,
pico79928, maior bloco72248 e frag0%. Não foi chamado de zero bytes de mudança:
32bytes de diferença estável no primeiro bloco, sem crescimento no segundo.
Cenário live tem mais controles desabilitados e Diagnostics diferente; esse pico
menor não demonstra otimização do DSP. Mede somente heap LVGL do host, não o heap
C++ completo nem heap interno/PSRAM do P4.

Capturas800×480 geradas e inspecionadas em LVGL host com adaptador/AMY em RAM:
[Stage](integration_04_shots/real_amy_host_stage.png),
[Keys](integration_04_shots/real_amy_host_keys.png),
[Browser](integration_04_shots/real_amy_host_browser.png),
[Diagnostics](integration_04_shots/real_amy_host_diagnostics.png).
Não são fotografias da unidade. Render100us é fixture do mock de publicação;
memória de hardware mostra N/A. Não usar capturas para aprovar GateE.

## Execução e artefatos

Logs locais ignorados em `build/p404/` deste worktree. Comandos executados da raiz:

| Status | Comando | Evidência |
|---|---|---|
| PASS | ./smk-p4/tests/run_host.ps1 | host.log;15 testes Python + C++/PCM |
| PASS | ./smk-p4/tests/run_amy.ps1 | amy.log; AMY real, USB, contrato, UI antiga |
| PASS | ./smk-p4/tests/run_usb.ps1 | usb.log; lifecycle e falhas injetadas |
| PASS | ./tests/run_smk_safety.ps1 | safety.log |
| PASS | ./tests/run_smk_ui.ps1 | ui.log |
| PASS | ./tests/run_smk_link.ps1 | link.log |
| PASS | ./smk-p4/ui-preview/run_preview.ps1 | preview_final.log;20 capturas,7 grupos |
| PASS | ./smk-p4/tests/run_blueprint_adapter.ps1 | adapter_final.log; adaptador real/LVGL/AMY |
| PASS | python smk-p4/tests/verify_03a_reference.py --image-dir <checkout-original>/build/p403a/removal_grace_image | reference_verification.log |
| PASS | pio run -d smk-p4 -e guition-standalone-blueprint (preliminar seguro) | blueprint_build_final.log;83,06s após correção |
| FAIL, corrigido | Primeiro build Blueprint | blueprint_build.log; snprintf Harmony com aviso de truncamento; buffer local ampliado160→224, sem remover Werror |
| BLOCKED, disco | Primeira tentativa de build legado e musical paralelo à falta de espaço | legacy_build.log / blueprint_musical_build.log; C: sem espaço, não erro musical |
| PASS | pio run -d smk-p4 -e guition-standalone-blueprint -j 2 com perfil0.3A + UI | blueprint_musical_build_retry.log;656,44s após limpeza/espaço liberado |
| PASS | Mesmo build musical incremental -j 8, com árvore de produção e2b583d3 | blueprint_musical_final.log;23,36s |
| PASS | python smk-p4/tests/check_blueprint_build.py --musical | blueprint_musical_check.log; target/font/pool/gates/AMY/único LVGL |
| PASS | python smk-p4/tests/verify_03a_reference.py --sdkconfig <candidato>/sdkconfig --config-header <candidato>/sdkconfig.h | musical_profile_verification.log; configurações solicitada e efetiva |
| PASS | Comparação de todos os valores Kconfig normalizados | musical_config_diff.json; única diferença: SMKP4_UI_BLUEPRINT n→y |
| PASS | python -m serial.tools.list_ports -v | Somente enumeração, sem abrir/resetar COM13 |
| PASS | pio run -d smk-p4 -e guition-display -j 8 (na execução multiambiente) | legacy_environments_build.log;377,13s; RAM12968/Flash576804 |
| PASS | pio run -d smk-p4 -e guition-link -j 8 (na execução multiambiente) | legacy_environments_build.log;308,92s; RAM18216/Flash670450 |
| PASS | pio run -d smk-p4 -e guition-standalone -j 8 (na execução multiambiente) | legacy_environments_build.log;392,19s |
| PASS | python smk-p4/tests/check_standalone_build.py | legacy_standalone_check.log; defaults seguros, variantes isoladas e ELF USB |
| PASS | python -m unittest discover -s smk-p4/tests -p test_standalone_capture_report.py | capture_report_tests.log;8 casos de validação fail-closed |
| PASS | python -m unittest discover -s smk-p4/tests -p test_*.py | python_final.log;23 testes |
| PASS | standalone_capture_report.py <log0.3A> --legacy --output <json> | baseline_03a_serial_analysis.json; amostras históricas, não novo GateE |
| PASS | git diff adf2e95a --check | Diff completo;6 hard-breaks Markdown importados convertidos para backslash sem espaços finais |
| NOT RUN | Build S3 nativo nesta rodada | Produção S3 intacta; testes compartilhados executados |
| PASS | pio run -d smk-p4 -e guition-standalone-blueprint -j 8 após restaurar sdkconfig seguro/mtime | blueprint_safe_restored_build.log;302,64s |
| PASS | python smk-p4/tests/check_blueprint_build.py (seguro) | blueprint_safe_check.log; ambos gates efetivos desabilitados |
| PASS | Hash do bundle musical após restaurar saída segura | Os6 arquivos permanecem iguais ao manifesto |
| PASS | Kconfig seguro antigo × novo | safe_config_diff.json; somente UI_BLUEPRINT n→y |
| MANUAL / NOT RUN | Áudio/MIDI/touch físicos do candidato, hot-plug e janela30min | Nenhuma gravação do candidato até este checkpoint |

Ferramentas: C:/.platformio/penv/Scripts/pio.exe (6.1.18), ESP-IDF5.5.2,
pioarduino55.03.36-1, GCC RISC-V14.2.0+20251107, esptool5.1.0, LVGL9.3.0.
Scripts host usam gcc/g++/Python no PATH como regressões existentes.
Falhas iniciais de teste: include initializer_list ausente corrigido antes de
executar; comparação de memória inicial sem aquecimento substituída por duas
janelas com estado comparável e contagem explícita32bytes/zero crescimento.
A falta de disco também impediu o primeiro git add e uma tentativa musical posterior; nada foi perdido. Limpeza
oficial pio -t clean do build legado incompleto e compressão NTFS somente dos
intermediários deste worktree liberaram espaço; fontes/backups preservados.
O operador liberou espaço adicional no C:; builds retomados com -j 2.

O novo analisador usa timestamps vinculados à tag do registro, não a um
fragmento de boot anterior na mesma linha. A primeira leitura do log0.3A
produziu FAIL de DISPLAY_gap por esse enquadramento; corrigido e coberto por
teste sem relaxar o limite10s. Rejeita campos ausentes/inválidos, resets,
lacunas, MIDI/hot-plugs apenas históricos e deadlines; requer novas ações na
janela. Contagem de transições não prova visita às14 telas nem ausência de
vazamento: confirmação do operador e comparação de estados equivalentes continuam
necessárias. Seu resultado serial_checks_pass nunca concede GateE sozinho.

## Bundle musical verificável

[Manifesto do candidato](integration_04_candidate.json), árvore de produção
`e2b583d3`. Bundle local deste worktree: `build/p404/musical_candidate` contém
firmware.bin, bootloader.bin, partitions.bin, sdkconfig, sdkconfig.h e lock.

- firmware.bin:1201408bytes, SHA256
  `414624d934fc83f920cebe7d5359825aa3ddaf5f29cfdeb19192c7ffbc8259c7`.
- Tamanho informado pelo linker: RAM30924 / Flash1200684bytes.
  Referência física: RAM30344 / Flash1157856bytes. Diferenças estáticas:
  +580bytes RAM / +42828bytes Flash. Não são medições de heap ou carga DSP.
- Kconfig normalizado difere do perfil0.3A somente em CONFIG_SMKP4_UI_BLUEPRINT=y.
  Pinos/gates qualificados e controlador0 vêm da configuração congelada; não
  foram promovidos a defaults nem verificados eletricamente nesta rodada.
- É uma nova imagem compilada, sem aprovação física. Nunca atribuir a ela o
  PASS físico do hash89fc42… de referência.

## Preparação física antes da gravação

Operador confirmou mesma montagem qualificada0.3A e disponibilidade para testes.
PASS enumeração COM13; captura sem reset de12s (`preflash_runtime.log`,3106bytes)
observou o firmware musical antigo ativo, Host Connected, MIDI3223 eventos,
render/compute deadlines0, erro/write/timeout0 e heap interno250283/PSRAM31176952.
Esses dados são da imagem antiga, não da integração.

PASS consulta ROM: P4ES1.3, MAC80:F1:B2:D3:CC:86, USB-Serial/JTAG e Flash16MiB.
PASS backup integral novo, sem sobrescrever backups anteriores:
`build/p404/flash_before_integration_04_20261103.bin`,16777216bytes, leitura em124,4s,
SHA256 `12cc9d4335606b04ff0dad79ebfc7eac245a35e3225b0bb8fb3b9c5bb00b8c99`.
Metadados em [integration_04_preflash.json](integration_04_preflash.json).
ROM/read-flash resetaram intencionalmente a unidade; não escreveram Flash/eFuses.

Comparação byte a byte por SHA256 da aplicação em0x10000 (1158576bytes)
confirmou exatamente a referência89fc42…; tabela de partições em0x8000 também
coincide. image-info confirmou checksum/digest válidos, projeto smk-p4-gate0,
versão0.1.0, compilado Nov2/23:07:45, ELF4864c7e3…60884. Configuração atual
não foi inferida de mock: identidade binária é a referência física0.3A.

Saída corrente de build foi restaurada para Blueprint seguro, gates PCM/USB=n,
SHA firmware `33ae585e28e1b4b3fadf0293ba271b17a738cc487feb8583099f411f0ccc9796`,
preservada em `build/p404/safe_final_image`. O bundle musical habilitado414624…
fica somente no destino explicitamente nomeado `musical_candidate`.

PASS GateA software: builds antigo/novo/display/link e checkers executados.
No checkpoint pré-gravação, GateB/C/D tinham cobertura host/native e GateE era NOT RUN/MANUAL.
No checkpoint pré-gravação, a escrita aguardava aprovação explícita exigida no
item12, após identificação e backup. A execução autorizada está registrada abaixo.

## Gravação autorizada e smoke0.4

Operador aprovou explicitamente: “Autorizar gravação e testes”, com mixer baixo.
PASS hashes do bundle414624… e backup12cc9d… conferidos novamente antes da escrita.
PASS esptool5.1.0 na COM13/P4ES1.3/MAC confirmado: offsets0x2000/0x8000/0x10000,
DIO80MHz, baud115200, três segmentos com hash verificado e reset de boot previsto.
`build/p404/upload_04.log` preservado. Sem eFuses ou escrita de NVS.

PASS smoke serial: `smoke_04_runtime.log` (41529bytes), captura180s sem reset
adicional. Registros completos cobrem170,73s; AMY ready, I2S_PRODUCING e Host
conectado. MIDI0→182 eventos, sem parse/drop/overflow; notes0 ao terminar.
Render máximo2074us; render/compute deadlines, write errors/timeouts/refill risks,
clipping e OOM0. Pilha livre mínima áudio7440 / USB4144 / UI3348bytes.
Heap interno248703bytes, PSRAM31176952bytes; LVGL peak35624 no smoke inicial.
O início do log contém fragmento antigo de boot seguido de registro válido;
nenhum boot completo foi artificialmente atribuído a essa captura.

MANUAL/PASS: operador confirmou imagem/touch, áudioL/R limpo, C4 USB e Touch,
release/arrastar para fora, patches128/135, Master0→retorno e Panic ativo:
“Smoke passou em todos esses itens”. Confirmou depois as ações da matriz:
“Roteiro executado sem falhas observadas”. Contadores físicos são correlacionados
separadamente, sem substituir confirmação por mock.

A idade máxima agregada de comando foi20ms, versus6ms no baseline. Auditoria:
o adapter usa now_ms_ do último service; eventos LVGL ocorrem antes do próximo
service. Portanto essa idade inclui defasagem do ciclo UI e não mede somente
residência em fila, nem latência física tecla→saída. Não foi chamado de regressão
USB ou de prova de baixa latência; owner/prioridade/afinidade/DMA não foram
alterados especulativamente. O valor e seu limite de interpretação ficam registrados.

Janela prolongada1860s aberta em `soak_04_runtime.log`, mesmo boot, sem reset.
Roteiro: três voltas14 telas, Focus, controles sob música, Touch>10s/rearm,
oitava/navegação, fontes mesma nota/release nas duas ordens, Panic/sustain e
≥4 hot-plugs HS. Confirmação auditiva/manual recebida; ainda é necessário
concluir a duração e analisar todos os contadores/memória. Metadados:
[integration_04_hardware.json](integration_04_hardware.json).

## Cliques em retrigger — diagnóstico aberto

Após a confirmação inicial, o operador relatou pequenos cliques em repetição da
mesma nota e sequências rápidas pelo SMK25 USB, mais claros no135 e também no128.
Isso não foi ignorado por haver contadores zerados. Não foi declarado “áudio
limpo” para esses novos casos. A/B físico posterior reproduziu o mesmo sintoma
na referência; causa específica de fase permanece não confirmada.

O código USB/runtime/PCM e AMY core permanece idêntico ao0.3B. Runtime::release
libera o slot lógico ao enviar Off; o DSP pode continuar a cauda de release.
Reutilização do slot/retrigger é hipótese a medir, não conclusão nem mudança DSP.

Probe offline pelo caminho público Runtime/UsbInput, Master64/velocity100,
48kHz/256frames, quatro vozes, sem sustain/FX: notas isoladas, repetidas rápidas,
arpejos e acordes de4. O runner compila contra o runtime0.3B e o integrado,
usando os mesmos objetos AMY (core sem diff). WAVs11,10s/532736frames por patch
foram byte-idênticos nas duas variantes. Sem clipping ou rejeição por polifonia.
No135, maior salto amostra-a-amostra1087 unidadesPCM ocorreu na fronteira On;
no128, saltoOn274/maxglobal457. Isso sustenta investigar um transitório de
retrigger, mas não prova especificamente fase nem exclui artefato I²S físico.
Métricas/hashes: [integration_04_retrigger_probe.json](integration_04_retrigger_probe.json).

PASS execução reproduzível:
`./smk-p4/tests/run_retrigger_probe.ps1 -BaselineRoot <worktree-0.3B>`
(pré-requisito: run_amy.ps1 para objetos compilados). Fonte host-only:
render_retrigger_probe.cpp. Não compila no firmware e não modifica DSP/presets.

Operador autorizou “Autorizar comparação A/B”. Após preservar a janela, o bundle
0.3A89fc42… foi regravado com hash verificado; captura180s retida em
ab_baseline_runtime.log (44980bytes), com entrada USB real e contadores de erro0.
O roteiro usou Master64, patches128/135, sustainOff/bend centro/mod mínimo e
notas rápidas com intensidade semelhante. Resposta literal:
“acontece da mesma forma. O problema não foi introduzido pela integração da UI ao
firmware. Podemos seguir e verificar isso em uma próxima etapa. Conclua os trabalhos”.
Assim, os cliques são classificados como limitação preexistente observada no A/B,
não regressão nova demonstrada. Nenhuma correção de oscilador/envelope, polifonia,
prioridade ou DMA foi introduzida. A causa exata fica para a próxima etapa.

## Janela física concluída e comparação

PASS `standalone_capture_report.py build/p404/soak_04_runtime.log --output build/p404/soak_04_analysis.json`.
Captura solicitada1860s, sem resets durante a janela;371 amostras completas por
subsistema principal, span1851,70s (30min51,7s). Arquivo428968bytes, SHA256
`3d8ad874cdafd77e625419ce0824474456deb251cb9621aa603b5fe1170eea43`.
Incluiu carga musical inicial e adicional, navegação/controles e repouso conectado;
não foi música ininterrupta por todos os31min. Os dados são deste candidato no P4.

| Medida | Resultado0.4 |
|---|---|
| USB/MIDI | Connected ao final;6 conexões/5 remoções no boot;5 ciclos novos na janela |
| Eventos |1021 selecionados totais,755 novos;1021 auxiliares ignorados |
| Parse / drops / overflow / erros USB |0 /0 /0 /0 |
| Vozes |Máximo amostrado4;0 ao terminar;7 rejeições por capacidade, não drops MIDI |
| Render |Média acumulada final159us / máximo2113us; orçamento5333,33us |
| Render / compute deadlines |0 /0 |
| I²S write errors / timeouts / refill risks |0 /0 /0 |
| Clipping / OOM / fila overflow |0 /0 /0 |
| Fila |High-water4; idade máxima agregada20ms com limite de interpretação descrito acima |
| Comandos / patch máximos |1221 /325us |
| DMA write / serviço completo máximos |5208 /5707us; serviço inclui espera DMA |
| Frames |101438464 escritos no boot; progressão da janela coerente com47999,76frames/s |
| Heap interno |Mínimo/final248703bytes; maior bloco mínimo172032bytes |
| PSRAM |Mínimo/final31176952bytes; maior bloco30932992bytes |
| Pilha livre mínima |Áudio7440 / USB4144 / UI3332bytes |
| LVGL |Livre amostrado78992..102708bytes, final91116; maior bloco mínimo74992, final91056 |
| Pico/fragmentação LVGL |Pico50176bytes; fragmentação máxima amostrada6%, final1% |
| UI |47 transições totais,43 novas na janela;3 voltas14 telas confirmadas manualmente |

Sem queda progressiva observada após a navegação; campos livres variam com a
tela/foco, portanto não foram comparados como layouts idênticos sem evidência.
O período final na tela mantida ficou estável. Isso não é teste de integridade
universal do heap. CPU total, latência física e DMA underruns exatos continuam
NOT RUN/UNMEASURED. Render menor que2601us de referência não é otimização
comprovada: a carga não foi um replay idêntico.

## Estado final após A/B

PASS retorno autorizado ao bundle0.4, três segmentos com hash verificado em
return_04_upload.log. Imagem instalada termina414624…8259c7; não ficou no0.3A.
Captura final60s, return_04_runtime.log (14849bytes): AMY ready, I2S_PRODUCING,
Host Connected, sem falhas de render/compute/I²S/USB, OOM ou notas ativas ao final.
Render máximo2128us; idade máxima6ms neste boot com entrada USB sem os comandos
UI do ensaio anterior. Os578 eventos MIDI já existiam no primeiro registro após
retorno; não houve incremento durante os60s retidos, e isso não foi anunciado
como nova carga nessa curta captura. A carga da janela31min está separada.

Dados completos/hashes e aprovação: [integration_04_hardware.json](integration_04_hardware.json).
Backups0.3A e backup integral pré0.4 permanecem preservados. Cache de build
corrente permanece seguro (PCM/USB desabilitados); imagens musicais qualificadas
estão nos bundles explícitos. Branches de origem não foram modificadas.

| Gate final | Resultado e escopo |
|---|---|
| A Build |PASS novo seguro/musical, UI antiga, display/link, checkers e dependências preservadas |
| B Backend |PASS host/native + MANUAL/PASS patch128/135, Master, notas, Panic e snapshots reais |
| C Segurança |PASS testes de ledger/lifecycle/prioridade; MANUAL/PASS casos Touch/USB/lease/oitava/navegação/Panic executados |
| D Interface |PASS host1400 transições live e previews; MANUAL/PASS14 telas/Focus/touch; memória física dentro do pool128KB |
| E Estabilidade inicial |PASS janela física30min51,7s no perfil/unidade qualificados, sem falhas instrumentadas; cliques preexistentes mantidos e adiados explicitamente |

Não há aprovação de ausência de cliques nem de palco. NOT RUN: novo build S3 e
preview ESP-IDF isolado nesta rodada; fontes S3 intactas e host/preview reais
executados. MANUAL: VCC/straps/VBUS quantitativos e cold boot sem computador não
foram certificados independentemente; qualificação da mesma montagem foi
declarada pelo operador antes da gravação.

## Referência histórica e limites


Referência0.3A: máximo DSP2601us contra5333us; render/compute deadlines0;
write errors/timeouts/refill risks0; latência fila6ms; USB5 conexões/4 remoções,
3215 eventos; interno250283/PSRAM31176952bytes ao final. Valores históricos,
não medições0.4. Na fase de software não houve medição física do candidato de render médio/máximo,
compute/render deadlines, I²S, fila, MIDI/drop/polifonia, hot-plug, touch, memória,
pilhas ou fragmentação P4. Não extrapolar desempenho host para ESP32-P4.

Os estados pré-gravação foram superados pelos resultados físicos acima. A UI
nova continua opt-in, mantendo a antiga como padrão de build.
Qualificação elétrica original é declaração do operador; VCC/straps/VBUS e
pinagem atual da montagem não foram medidos novamente. Não foi inventado pinout.
Perfil0.3A é específico da unidade registrada, não default para qualquer placa.

## Roteiro físico e rollback — MANUAL

1. Identificar unidade/porta/serial e firmware realmente instalado, confirmar
   PCM/VBUS/HS/alimentação com operador. Não presumir que a Flash ainda seja0.3A.
2. Preservar backup integral atual e bundle instalado em destino novo; verificar
   hashes/partições16MiB e bundle0.3A preservado. Nenhuma substituição automática.
3. Conferir candidato, hash e header efetivo com gates corretos da montagem;
   obter aprovação explícita do operador para substituir aquela imagem concreta.
4. Mixer baixo: boot com fonte qualificada, display/cantos/touch, áudio L/R, SMK25, patches128/135,
   Master, On/Off, PRESS_LOST, Panic. Falha crítica: guardar logs e interromper.
5. Ambos Focus, dois bancos de macros desabilitados, Browser, todas14 telas.
   Com nota ativa: navegar rápido, sliders, trocar oitava/patch, segurar Touch
   >10s (expira), soltar e rearmar, Panic e USB/Touch mesma altura nas duas ordens.
6. Repetir≥4 hot-plugs HS com notas/sustain; áudio continua e novos eventos
   retornam sem reboot. Touch não é cancelado por disconnect USB, como baseline.
7. Capturar≥1860s (≥30min entre amostras) no mesmo boot com carga musical,
   navegação/controles, ambos patches e USB ligado. Anotar ações/resultado auditivo.
   Esperado: nenhum deadline compute/render, erro/timeouts/refill persistentes,
   reset, stuck note, parse/drop/overflow; heap/maior bloco/pilhas sem queda
   progressiva após repetidas transições. Preservar contadores de polifonia e trace
   separados de perdas MIDI. Comparar com0.3A sob estímulo equivalente.
8. Se falhar, preservar imagem/logs/contadores e usar bundle validado nos offsets
   documentados em firmware_integration_readiness_03b.md (0x2000/0x8000/0x10000),
   após confirmar unidade/layout/modo. Backup integral0x0 é outro tipo de imagem.
   Verificar hashes antes/depois e repetir smoke. Sem force-push ou reset Git.

Prontidão: integração funcional e estável no ensaio assistido inicial executado,
com firmware0.4 instalado, quatro vozes e ROM128/135. Não é release de palco.
Riscos/limites: cliques rápidos herdados, ausência de ACK individual de nota,
idade UI não equivalente a latência física e DMA underruns exatos não medidos.
Próximo menor passo, conforme orientação explícita do operador: diagnóstico
focado do retrigger/release/fase com gravação de PCM e comparação controlada,
sem ampliar polifonia/patches/FX ou sequencer. Nenhuma dessas mudanças DSP foi
incluída nesta integração.
