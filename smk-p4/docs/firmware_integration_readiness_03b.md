# SMK-P4 Firmware 0.3B — preparação para integração

2026-11-03. Branch `feature/smk-p4-firmware-integration-ready`, worktree independente
`build/worktrees/smk-p4-firmware-03b` no checkout original. Base exata
`3dd0cf288dbf5fba69afed4efe4288f7d2dac4cc`; correção de lifecycle USB incluída
`a31a78bc6c3b9dd3e4b5018aca85dd8b99fdc559`. A branch musical e seus backups não
foram sobrescritos; sem reset/rebase/force-push/merge de UI Blueprint.

**PASS: preparação de software0.3B concluída, com contrato e regressões.** Nenhuma nova imagem
foi gravada e nenhuma COM foi aberta nesta rodada0.3B. A identidade física atual
da Flash não foi consultada; a referência validada é a que consta do relatório
0.3A, não uma afirmação sobre o que outra tarefa possa ter gravado depois.

## Escopo e baseline preservado

Lidos AGENTS/specs, README, builds/dependências/CI, consolidado0.3A, integração
AMY S3/P4, backend, owner, filas, USB, TouchNote, port LVGL e testes. O pedido
explícito P4/Guition800×480 prevalece sobre os documentos gerais S3/284×76;
essa divergência não motiva alteração de arquitetura nem expansão do MVP.

O bundle original `build/p403a/removal_grace_image` existe no worktree musical
original e seu firmware foi confirmado por SHA256:
`89fc42a19b0d588970bf6bd18ded139af1bc3843ae398d0cc2404fa1cfa2e9a7`.
Os quatro arquivos foram copiados, sem sobrescrever o original, para
`build/p403b/reference_03a` DESTE worktree. Não presumir essa cópia em outro clone.

Referência versionada em [baselines/standalone_03a.json](baselines/standalone_03a.json),
incluindo hashes/tamanhos de firmware, bootloader, partições e sdkconfig; sdkconfig
completo e lock de componentes preservados ao lado. Os atributos locais Git
preservam bytes/CRLF das cópias congeladas. São evidências, não defaults ativos
de build. O checker considera símbolos Kconfig ausentes como desabilitados (`n`),
inclusive opções ocultas por dependências; não permite substituir bundle ausente
por uma recompilação.

| Item | Baseline musical validado |
|---|---|
| Código / relatório consolidado | a31a78bc /3dd0cf28 |
| Ambiente | guition-standalone; ESP-IDF5.5.2; pioarduino55.03.36-1; PlatformIO6.1.18 |
| Compilador / esptool | RISC-V GCC14.2.0+20251107 /5.1.0 |
| Bibliotecas | AMY do próprio commit (MIT), LVGL9.3.0 (MIT), IDF5.5.2; nenhuma nova dependência0.3B |
| PCM | 48000Hz, PCM16 estéreo,256frames/bloco, DMA4×256; GPIO50/51/52 na unidade qualificada |
| USB | HS controlador0; cabo virtual0 selecionado, auxiliares contados/ignorados |
| Engine |4 vozes totais,64osciladores, bancos ROM128/135 pré-alocados, FX/multicore/multithread AMY desativados |
| Filas |32 comandos por origem; UI/USB com produtores distintos |
| Touch | lease10s e helper UI mono; heldNote sem renovação |
| Janela física |1825,04s,366 amostras/subsistema,3215 eventos,4 desconexões/5 conexões |
| Falhas finais0.3A |render/compute deadlines0; write errors/timeouts/refill risks0; USB/parse/drop/overflow0 |
| DSP / fila |máximo2601us frente a prazo5333,33us; idade máxima da fila6ms |
| Heap final |interno250283bytes; PSRAM31176952bytes |
| Limite musical |227 rejeições por polifonia; não são drops MIDI |
| Não medido |DMA underruns exatos, CPU total, latência física, integridade heap em execução |

As métricas são herdadas da execução0.3A e não foram rebatizadas como novas
medições0.3B. O operador havia confirmado áudio limpo, controles, independência
Touch/USB e sustain. Qualificação elétrica foi declarada por ele; valores VCC,
straps e detalhes VBUS quantitativos continuam sem certificação independente.

## Contrato musical e segurança

Contrato completo: [performance_backend_contract.md](performance_backend_contract.md).
Matriz executável/manual: [ui_integration_regression_matrix.md](ui_integration_regression_matrix.md).
Não há implementação de Blueprint1.1 nesta branch.

| Operação | Disponibilidade / confirmação |
|---|---|
| Catálogo ROM128/135 |Consulta estática síncrona, indexada; IDs não são índices |
| setPatch/setMaster |Assíncronos via fila; confirmar pelo snapshot aplicado |
| noteOn |Aceitação ou duplicata idempotente; owner ainda pode rejeitar por capacidade/idade |
| noteOff |Latch por pitch/token, fora da fila comum; prioridade preservada |
| panic |Geração e cancelamento global no apply; não depende de espaço na fila |
| heldNote |No-op; lease fixo10s para Touch; USB ativo não usa esse lease |
| setMacro |false, sem efeito; todos os oito slots macro indisponíveis |
| Oitava |Transformação UI com release da nota enviada, não novo método no backend |

`connected()` indica engine ready; conexão/erro MIDI têm fontes próprias.
`busy()` sempre false. `stateRevision()` cresce em publish, não é ACK nem
notificação isolada de publishMidi. `active_notes` é agregado lógico, não ID de
gesto nem medição de caudas DSP. Campos heap/BPM/underrun do DTO não preenchidos
devem ser tratados como indisponíveis. Código de erro do owner está em logs,
não no DTO público. Não inferir erro AMY de erro USB.

Preservadas duas origens, tokens monotônicos, gerações e Off prioritário. Backend
tem um token por pitch, sem refcount de dedos; a UI complexa precisa de ledger
por gesto e primeiro On/último Off por nota. Releases/cancelamentos/navegação/
focus loss devem servir Off antes de destruir o produtor. `TouchNote::disconnect`
sozinho não libera notas. Expiração10s não limpa automaticamente o token backend;
soltar/Panic é necessário antes de rearmar a mesma nota. Não renovar por On
periódico. Trocar patch chama Panic mesmo se a fila cheia fizer setPatch retornar
false. Esses comportamentos foram reproduzidos e documentados; não houve mudança
de semântica, API pública, locks ou render para acomodar a futura UI.

## Observabilidade

Os contadores existentes são suficientes para o gate inicial de comparação:
falhas de push/overflow UI e USB, rejeição por polifonia, deadlines de render e
compute, tempos de DSP/serviço/DMA, frames, write errors/timeouts/refill risks,
heap/maior bloco/PSRAM/pilhas, LVGL heap/fragmentação e lifecycle USB. Mantidos os
logs a5s e trace limitado, sem instrumentação nova no owner ou logging por bloco.

Limites: retorno false não tem enum público de motivo; estado aplicado não tem
ACK individual de nota; não existe medição de resposta UI ponta a ponta ou DMA
underrun exato. Refresh count e testes mock não preenchem essas lacunas. Se0.4
precisar de telemetria adicional, propor uma fonte explícita amostrada fora do
áudio e medir impacto antes de alterar o contrato. Não ampliar o DTO agora apenas
para preencher widgets demonstrativos.

Restrição de lifecycle herdada: o encerramento do Host retém recursos até drenar
callbacks; stop/shutdown pode aguardar indefinidamente se o SDK nunca devolver
um callback. Isso não está no render e não foi observado nos testes, mas a UI
não deve iniciar/parar Host ou engine ao navegar. Nenhum driver foi alterado
para tratar uma falha física não reproduzida nesta rodada.

## Polifonia — análise para fase posterior

Quatro slots lógicos são compartilhados por USB/Touch. Não há roubo de vozes:
primeiro slot livre é usado; sem espaço, a nova nota é rejeitada e contada.
Duplicate On USB enquanto held e Duplicate On Touch/token são idempotentes.
USB por canal com sustain mantém slots ocupados depois do Off até liberar pedal.
Repetir uma nota USB sustentada pode reutilizar seu próprio slot, sem roubar outra.
Isso limita acordes: quatro notas USB podem impedir uma nota Touch, e vice-versa.

Cada voz ROM usa oito osciladores, com dois bancos pré-alocados:4×8×2=64.
Uma meta futura6vozes precisaria estudar pelo menos96osciladores/12vozes AMY/
13synths;8vozes,128osciladores/16vozes AMY/17synths no mesmo desenho. São contas
de capacidade, não configurações implementadas ou garantia de desempenho.
Não basta mudar kVoices com pool64. O máximo DSP2601us e média final185us vêm de
uma carga que também teve repouso; não extrapolar esses números para prometer6/8.

Após integrar a UI, medir separadamente4/6/8 com ambos os patches, sustain e
caudas, bend/CC1, mudanças de patch, controles e navegação, saturação e hot-plug.
Comparar compute/render máximos e deadlines, heap interno/maior bloco/pilhas,
filas/idade/drop e áudio analógico. Aumentos em pools/bancos podem comprometer a
reserva interna de boot e o orçamento de5,333ms. Nenhum kVoices, pool, instrumento
ou FX foi alterado nesta fase.

## Reprodução segura e rollback

Os comandos abaixo são procedimentos para a equipe, não execução automática de
Flash. Escolher explicitamente o worktree e preservar configurações/cache locais
antes de substituí-los. Os defaults de PCM/USB do repositório continuam seguros.

### Verificar e compilar

```powershell
python smk-p4/tests/verify_03a_reference.py --image-dir <bundle-explicito>
# Em checkout/worktree novo, build seguro conforme README/CI:
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone
python smk-p4/tests/check_standalone_build.py
```

O checker existente verifica defaults desabilitados e compila/linka combinações
PCM/USB isoladas. Não executar seu teste de defaults sobre sdkconfig de bancada
habilitado esperando o mesmo resultado. Para build equivalente ao musical,
guardar o sdkconfig/artefatos seguros em um diretório explicitamente escolhido,
copiar `docs/baselines/standalone_03a.sdkconfig` para
`smk-p4/.pio/build/guition-standalone/sdkconfig` DESTE worktree e recompilar.
Esse arquivo de referência não deve ser carregado por defaults de ambiente.

```powershell
# Depois de guardar configuração e artefatos locais que precisam ser preservados:
Copy-Item smk-p4/docs/baselines/standalone_03a.sdkconfig smk-p4/.pio/build/guition-standalone/sdkconfig
(Get-Item smk-p4/.pio/build/guition-standalone/sdkconfig).LastWriteTime=Get-Date
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone
python smk-p4/tests/verify_03a_reference.py --sdkconfig smk-p4/.pio/build/guition-standalone/sdkconfig --config-header smk-p4/.pio/build/guition-standalone/config/sdkconfig.h
```

Registrar SHA do novo binário e configuração gerada, flags de AMY no compile
database, lock/toolchain e tamanhos. Comparar o perfil crítico e executar a matriz;
hash diferente por metadados Git/caminhos não reprova equivalência lógica, mas
jamais atribuir ao novo binário a aprovação física do hash original. Restaurar
o sdkconfig seguro salvo e recompilar ao terminar, evitando deixar um cache de
bancada habilitado como saída corrente para a equipe de integração.
Atualizar também seu mtime após a cópia de retorno, antes de recompilar.

Esse passo é necessário: nesta auditoria, Copy-Item preservou data anterior ao
CMakeCache. Um comando de build terminou em39,32s com o sdkconfig solicitado
musical, mas header C e binário ainda seguros (RAM30000/Flash1064166). Resultado
FAIL de equivalência, não aprovado como build musical. A tentativa foi preservada
em `build/p403b/stale_cache_attempt`. O verificador foi ampliado com comparação
do header, reproduções negativas e caso positivo; o arquivo de entrada isolado
agora é explicitamente insuficiente. A fonte desse comportamento está no teste
de mtime de SDKCONFIG_PATH em espidf.py do PlatformIO instalado. Nenhum arquivo
da plataforma foi modificado. Não substituir esse diagnóstico por habilitar
gates nos defaults ou declarar nova validação física.

### Restaurar em hardware — MANUAL, não executado0.3B

1. Obter autorização de bancada para a imagem concreta a substituir, identificar
   unidade/porta/conectores e confirmar os gates da montagem. A autorização da
   execução0.3A não identifica o conteúdo que pode estar na Flash hoje.
2. Preservar firmware/partições/dados atuais antes da escrita, com nome novo.
   `serial.tools.list_ports -v` não reseta; flash-id/read-flash e gravação resetam
   a placa e só pertencem a esta etapa autorizada. Não alterar eFuses/VBUS.
3. Conferir os quatro hashes do bundle0.3A com o verificador. Sem arquivo ou hash
   correto: BLOCKED. Não usar `firmware.bin` recém-compilado como substituto da
   referência validada. Confirmar16MiB e offset/layout da unidade original.
4. Com mixer baixo, restaurar o bundle nos offsets0x2000/0x8000/0x10000, modo
   DIO80MHz, conforme gravação0.3A. O backup integral16MiB a0x0 é OUTRO tipo de
   imagem; não o confundir com aplicação a0x10000. Guardar log/hash da operação.
5. Reabrir captura, verificar boot gate, ready, PCM/I²S, Host, C4 Touch/USB, release
   e Panic antes de comparar o novo candidato. Se falhar, guardar a evidência e
   seguir o roteiro de recuperação0.3A sem reset automático para esconder causa.

Forma documentada de gravação, somente depois dos passos autorizados acima:

```powershell
python -m esptool --chip esp32p4 --port <PORTA_CONFIRMADA> --baud 115200 --before default-reset --after hard-reset write-flash -z --flash-mode dio --flash-freq 80m --flash-size detect 0x2000 <BUNDLE>/bootloader.bin 0x8000 <BUNDLE>/partitions.bin 0x10000 <BUNDLE>/firmware.bin
```

## Validação0.3B — resultado final

PASS baseline no worktree independente: P4 host, AMY, USB, safety, UI antiga,
protocolo/touch/UART. PASS novo teste de contrato em AMY real no host; a primeira
compilação desse teste falhou por namespace link não qualificado, corrigido para
smk::link antes de executar. Nenhuma falha musical nova foi reproduzida.
PASS seis testes Python do verificador (config/hash, ausência explícita de
bundle, controlador incorreto e header antigo rejeitados, inclusive com Python
otimizado; caso positivo com header correspondente). A suíte P4 host inteira
terminou com15 testes Python e seus executáveis C++ passando.

O primeiro gerador de manifesto detectou uma opção Kconfig ausente por dependência;
foi ajustado para a semântica disabled/n. As cópias CRLF congeladas precisaram de
atributo whitespace=cr-at-eol para diff check sem normalizar e mudar hashes.
Essas correções são do material de auditoria/teste, não do firmware.

| Status | Comando/validação executado neste worktree | Evidência local |
|---|---|---|
| PASS | `./smk-p4/tests/run_host.ps1` baseline e final | `build/p403b/baseline_host.log`, `final_host.log`;15 testes Python no final |
| PASS | `./smk-p4/tests/run_amy.ps1` baseline e ampliado | `baseline_amy.log`, `final_amy.log`; AMY real, duas fontes, heap, contrato e UI antiga mock |
| PASS | `./smk-p4/tests/run_usb.ps1` | `baseline_usb.log`; lifecycle/falhas/remoção/reconexão, sem hardware |
| PASS | `./tests/run_smk_safety.ps1` | `baseline_safety.log` |
| PASS | `./tests/run_smk_ui.ps1` | `baseline_ui.log` |
| PASS | `./tests/run_smk_link.ps1` | `baseline_link.log`; protocolo, gesto Touch e UART |
| PASS | `pio run -d smk-p4 -e guition-standalone` seguro, inicial | `standalone_safe_build.log`;753,71s |
| FAIL equivalência | Primeira cópia/build com timestamp antigo, embora o processo de build tenha retornado sucesso | `standalone_equivalent_build.log`;39,32s, cache seguro antigo; tentativa preservada em `stale_cache_attempt` |
| PASS | Mesmo build com mtime atualizado e perfil musical | `standalone_equivalent_reconfigured_build.log`;310,87s |
| PASS | Checker do bundle + sdkconfig + header da recompilação | `equivalent_final_verification.log`; ambos os gates no header realmente habilitados |
| PASS | Comparação de todos os valores Kconfig normalizados | `equivalent_full_config_diff.json` vazio:0 diferenças entre musical recompilado e referência |
| PASS | Build após restaurar sdkconfig seguro e atualizar mtime | `standalone_safe_restored_build.log`;286,27s |
| PASS | `python smk-p4/tests/check_standalone_build.py`, antes e depois | `safe_config_checker.log`, `final_safe_config_checker.log`: defaults seguros, variantes e ELF USB linkado à parte |
| PASS | `git diff 3dd0cf28 --check` e comparação dos diretórios de produção | Logs/diff sem alterações em main, componentes P4, AMY core, shared ou S3 |
| NOT RUN | Builds nativos guition-display, guition-link e S3 nesta0.3B | Nenhum arquivo de produção/API desses perfis mudou; testes host compartilhados executados. Não reutilizar PASS histórico como novo build. |
| NOT RUN / MANUAL | Gravação, reset, reprodução musical física e nova janela de duração0.3B | Nenhuma COM aberta; dados físicos permanecem os da0.3A |

Os comandos `pio`/`python` acima foram executados pelo ambiente
`C:/.platformio/penv/Scripts/`. Os logs/artefatos ficam ignorados em
`build/p403b` deste worktree. O primeiro compile do teste novo e os casos negativos
do checker foram corrigidos/verificados antes dos PASS finais; não são falhas
físicas escondidas por reboot.

### Artefatos finais e comparação

| Saída | Identidade / tamanho informado pelo build |
|---|---|
| Binário físico de referência | `build/p403b/reference_03a/firmware.bin`; SHA89fc42a19b0d588970bf6bd18ded139af1bc3843ae398d0cc2404fa1cfa2e9a7; RAM30344 /Flash1157856bytes |
| Musical equivalente recompilado | `build/p403b/equivalent_image/firmware.bin`; SHA8abc54b0d5e859191d604a60ffa10101d48ea9abe5f462ef638419bef5a1478f; RAM30344 /Flash1158032bytes |
| Saída corrente segura restaurada | `build/p403b/safe_final_image/firmware.bin`; SHA811eedf6c7152013475e7c18dc58ddbbb3b1870c4bf5a965e00b3432bdbc5e04; RAM29456 /Flash1064140bytes |

O musical recompilado tem mesma configuração Kconfig normalizada e RAM estática
da referência, Flash176bytes maior. Diferença de hash/tamanho não recebe atribuição
de nova funcionalidade ou nova aprovação física: os caminhos/metadados do build
não são uma reprodução byte a byte. A configuração segura inicial reportou
RAM30000/Flash1064166 e a restaurada29456/1064140, embora o roundtrip Kconfig
normalizado tenha0 diferenças (`safe_config_roundtrip_diff.json`). Registrar essa
variação sem alegar otimização de memória; não foram medidos novos tempos DSP ou
heap físico. Só o binário89fc42... tem a validação de bancada0.3A.

A configuração/cache corrente termina com PCM e USB desabilitados, assim como
os defaults versionados. O musical habilitado está apenas no arquivo de evidência
explicitamente nomeado. Não iniciar upload a partir desses artefatos sem o roteiro
de bancada/backup acima. Nenhum backup original foi removido ou substituído.

### Commits e avaliação

* `1b044b42`: referência congelada, manifesto/checker e testes de integridade.
* `2057edfa`: regressão do contrato local e atributos para preservar CRLF exato.
* `2835f4c7`: conferir header C e rejeitar cache antigo, com testes positivos/negativos.
* Consolidação documental: commit que contém este relatório; obter o HEAD final
  com `git log -1 --oneline feature/smk-p4-firmware-integration-ready` e a sequência
  completa com `git log --oneline 3dd0cf28..feature/smk-p4-firmware-integration-ready`.

PASS critérios de preparação0.3B: baseline identificado/verificado, build seguro e
equivalente funcionais, regressões executadas, contrato correspondente ao código
e instruções para integração/rollback. Sem modificação do caminho de áudio,
PerformanceBackend público, drivers, polifonia, FX ou UI. A próxima equipe recebe
um contrato verificável; não uma UI integrada ou nova qualificação física.

## Requisitos de aprovação para Integration0.4

Usar contrato e matriz, preservar um único produtor UI e nenhuma chamada AMY
direta de widgets. Implementar releases/cancelamento por gesto/nota e respeitar
lease/ownership; campos indisponíveis e macros devem continuar desabilitados.
Confirmar snapshots aplicados, sem comandos de startup na criação das telas.
Comparar logs equivalentes, repetir host/builds e obter nova autorização antes
de substituir imagem física. Validar navegação/controles sob áudio, múltiplos
hot-plugs, memória após navegação repetida e≥30min integrados no candidato real.
Não misturar esse gate com ampliação de polifonia, patches ou FX.

Próximo menor passo após aprovação0.3B: integração controlada das ações já
suportadas, seguindo a matriz. Blueprint não foi integrado, e seu desempenho
físico permanece NOT RUN/MANUAL.
