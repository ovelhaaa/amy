# SMK-P4 Integration 0.4 — resultados

2026-11-03. Trabalho em `feature/smk-p4-integration-04`, worktree separado
`build/worktrees/smk-p4-integration-04`. O pedido explícito Guition P4/800×480
prevalece sobre specs.md/AGENTS gerais S3/284×76; nenhuma migração do core AMY.

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
GateB/C/D têm cobertura host/native; GateE permanece NOT RUN/MANUAL.
Gravação do candidato aguarda aprovação explícita exigida no item12 do pedido,
após identificação e backup. Não houve gravação neste checkpoint.

## Física, desempenho e gates

Referência0.3A: máximo DSP2601us contra5333us; render/compute deadlines0;
write errors/timeouts/refill risks0; latência fila6ms; USB5 conexões/4 remoções,
3215 eventos; interno250283/PSRAM31176952bytes ao final. Valores históricos,
não medições0.4. Na fase de software não houve medição física do candidato de render médio/máximo,
compute/render deadlines, I²S, fila, MIDI/drop/polifonia, hot-plug, touch, memória,
pilhas ou fragmentação P4. Não extrapolar desempenho host para ESP32-P4.

GatesB/C/D: cobertura software/host PASS nos cenários acima; aceitação física
continua MANUAL/NOT RUN. GateE NÃO aprovado. UI nova continua opt-in.
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

Prontidão: software candidato para revisão e futura bancada assistida; NÃO é
aprovação para palco ou GateE. Risco principal restante: flush/touch/memória/CPU
reais sob áudio; também não há ACK individual de nota nem DMA underrun exato.
Próximo menor passo é autorizar a imagem concreta (identificação e backup concluídos) e executar
smoke curto, matriz e janela30min, mantendo quatro vozes e patches128/135.
