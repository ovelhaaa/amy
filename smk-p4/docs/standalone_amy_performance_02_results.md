# SMK-P4 standalone AMY + Performance 0.2

Data: 2026-11-02. **Integração de software; aceitação analógica e musical pendente.**
Não aprovado para palco/ensaio prolongado. Compilação, host, mock LVGL, execução
P4 e som analógico são evidências distintas.

## Referência, escopo e commits

Branch de trabalho: `feature/smk-p4-standalone-amy`, criada da árvore limpa em
`b858a44c` (`feature/smk-p4-standalone`). Primeiro commit: `8f96e039`, runtime
AMY, regressão PCM e correções de finalização. `74141efe` integra
backend/UI, diagnóstico RAM e regressões. `f53f4ef2` corrige a
rotulagem DMA da UI e rejeita evidência PCM parcial/múltiplos boots; o commit
documental consolida os resultados; consultar `git log b858a44c..HEAD --oneline`.
Sem git reset/rebase/force-push, alteração de main, upgrade de SDK ou push remoto.

Lidos AGENTS, specs, READMEs, builds/dependências/CI, relatórios PCM/MVP/
estabilização, driver P4, adapter/presets S3 e contratos reais AMY. Conflito:
specs/AGENTS descrevem S3 e LCD284x76; o pedido atual exige P4/800x480/bloco256.
Prevaleceu o pedido no ambiente isolado `guition-standalone`. Os ambientes
`guition-display`, `guition-link`, `esp32-s3-link` e `esp32-s3-devkitc-1` permanecem.
README raiz contém marcadores de conflito preexistentes; não foi alterado.

## Implementado / arquitetura

- `components/amy_runtime/`: compila diretamente os C compartilhados em `src/`,
  sem cópia do engine nem transporte do adapter S3/drums/macros/FX/EventBus.
- `main/performance_backend.h`: contrato tipado de apresentação/comandos;
  `RemotePerformanceBackend` preserva P4Link; `LocalAmyPerformanceBackend`
  publica estado aplicado pelo owner, sem UART/HELLO/conexão S3 simulada.
  O DTO de apresentação reutiliza `link::State`; não serializa pacotes no local.
- Mesma `performance_ui.cpp`, widgets/LVGL/display/touch preservados. Backend
  selecionado pelo entry point/CMake do ambiente. Sem copiar a tela inteira.
- Um owner (prioridade5, stack12288 bytes, sem afinidade forçada) inicializa,
  modifica, renderiza e finaliza AMY; é também o único escritor do I²S.
  Main é o único produtor de comandos e owner LVGL. Snapshot de uma posição
  FreeRTOS passa cópias limitadas do áudio à UI, sem mutex compartilhado longo.
- Fila SPSC de32 comandos; sem heap por bloco. Note Off tem128 latches atômicos
  independentes. Panic incrementa geração fora da fila; comandos anteriores são
  descartados. Tokens de notas são monotônicos durante a vida do mailbox,
  inclusive ao reiniciar engine/UI. Off antigo não termina uma nota nova.
- On duplicado não cria referência adicional; velocity0 vira Off. Máximo4
  notas mantidas; novas alturas acima desse limite são rejeitadas e contadas.
  Release perdido expira no owner em10s, independentemente de LVGL. O driver
  touch existente também invalida contato antigo. Mudança de patch cancela
  notas; não há seamless switching, sustain virtual ou layers nesta entrega.
- Master com rampa por frame e headroom máximo~0,5, padrão64/127 (~0,25).
  Os oito slots de macros existentes mostram indisponível e ficam desabilitados:
  nenhum cutoff/resonance/envelope fictício é enviado a patches FM.
- Browser oferece somente os IDs reais128/135; demais linhas indisponíveis.
  Teclado virtual monofônico de toque, oitava, saída da tecla, release,
  mudança de tela/patch e Panic usam o mesmo backend. Boot visual não toca notas.
- Sem confirmação elétrica, AMY renderiza em RAM e mostra `AUDIO RAM`;
  isso não representa áudio transmitido. Erro do engine desabilita controles.
  Falha de display não para o owner. Falha I²S para o canal e finaliza o engine.

## Configuração AMY e portabilidade RISC-V

48kHz,256 frames, BLOCK_SIZE_BITS8, PCM16 signed estéreo, fixed-point interno.
Features default_synths/startup_bleep/reverb/chorus/echo/partials/custom off;
plataforma multithread/multicore off, audio/MIDI próprios AMY desabilitados.
Limites64 osciladores,8 vozes alocadas,3 slots de synth (0 reservado),1 patch RAM,
1 tag sequencer, KS off. Apenas um banco melódico toca por vez, com4 vozes.

ROM128 = `DX7 BRASS   1`, ROM135 = `DX7 PIANO   1`, confirmados em `src/patches.h`.
Cada voz usa8 osciladores. Ambos os bancos4x8 são materializados no boot,
com verificação de alocação completa e aquecimento de deltas. Trocar patch
seleciona o banco já preparado; não aloca nem parseia patch no serviço de áudio.
Não se aumentou o pool S3/AMY global nem se habilitaram FX pesados.

Configuração ESP: eventos/estado/buffers quentes em RAM interna8bit; caps de
sample/delay/sysex PSRAM, sem carregar samples/delays externos nesta configuração.
Reserva conservadora do maior bloco interno196608 bytes antes de entrar em
`amy_start`; UI aguarda a inicialização. `amy_start` é void e contém alocações
iniciais sem checagem: a reserva não é prova de recuperação de todo OOM possível.
Essa limitação upstream permanece explícita; falhas de patch/alocação detectadas
pelo runtime não são ignoradas. Não ampliar limites sem medir.

`__XTENSA__ && CONFIG_IDF_TARGET_ESP32S3` protege assembly de `algorithms.c`;
P4 compila C. Alocação alinhada especial S3 não foi copiada. SAMPLE continua
fixed-point; sequência real usada: `amy_execute_deltas` -> `amy_render(0,...,0)`
-> `amy_fill_buffer`, que já retorna L/R intercalados int16. Não converte SAMPLE
como float nem duplica manualmente canais. AMY mantém buffers do segundo core
zerados; não há segunda tarefa de render. Hooks de plataforma locais não
inicializam hardware. Não se chama `amy_update`/I²S AMY em paralelo.

## Modificações mínimas no core AMY

O teste de heap expôs recursos perdidos que impediam afirmar reset limpo:

1. `src/filters.c`: `filters_init` alocava novamente os13 blocos EQ por bus
   durante o segundo reset do boot. Reutiliza armazenamento e limpa coeficientes/
   delays, preservando o resultado DSP. Foram52 alocações retidas no teste.
2. `src/patches.c`: `patches_deinit` libera a alocação única das tabelas. O pool
   de deltas já foi liberado antes; não libera deltas novamente.
3. `src/amy.c`: destrói o lock Win32/POSIX/FreeRTOS correspondente no shutdown,
   depois de terminar workers. APIs públicas e contratos de render preservados.

São correções de ciclo de vida candidatas a upstream; adapter externo não
consegue liberar ownership interno de forma segura. Sem refactor amplo,
renomeação ou dependência nova. Licença/atribuições AMY preservadas.

## PCM5102A e modos de operação

Mesmo `components/audio_pcm5102/`: Philips master,48kHz,16bit estéreo,
DMA4x256, silêncio pré-carregado, MCLK não usado, timeout20ms, erro interrompe
produção. Fonte principal AMY; fonte teste separada por
`CONFIG_SMKP4_PCM_DIAGNOSTIC`. Nesse modo o display diagnóstico anterior volta
a ser usado e `PCM_TEST_SEQUENCE` escolhe silêncio ou sequência L/R/stereo.

`CONFIG_SMKP4_PCM_WIRING_CONFIRMED` continua off por padrão. Não foram inventados
pinos finais: candidatos50/51/52, conferência de JP1/alimentação/XSMT/straps/GND,
UART S3 desconectada e L/R analógico continuam no [roteiro](pcm5102_wiring.md).
Nenhum ES8311, NS4150, MCLK, USB Host ou fallback UART foi inicializado.

Self-test RAM é opt-in, incompatível em Kconfig com confirmação I²S e modo
PCM diagnóstico. Renderiza o mesmo caso do host, mede pico/RMS/clipping, verifica
release/Panic, finaliza, checa integridade de heap e reinicializa. Não é boot
musical normal. Durante esse teste o owner cede ao watchdog entre lotes.

## Instrumentação e limites de medição

Logs limitados a5s pela main; sem logs/Flash/arquivos/LVGL no render. Separados:
DSP (deltas/render/conversão/ganho/telemetria), aplicação de comandos, serviço
computacional (inclui publicação/housekeeping, exclui escrita DMA) e espera DMA. Publica média/máximo, patch máximo,
latência máxima de aplicação, deadlines DSP/compute, filas/high-water/overflow,
notas/expirações/peak/clipped/OOM, stack e frames/blocos. RAM/PSRAM/maior bloco
são publicados pelas rotinas de diagnóstico existentes.

`service_max_us` inclui espera I²S; não equivale a CPU DSP. Intervalo de refill
>21,33ms é risco de scheduling, não sensor de underrun. DMA underruns continuam
`UNMEASURED`; não se inventou contagem de zero. Orçamento compute/DSP5333,33us.
Em modo RAM o loop cede ticks e não promete cadência de48kHz nem latência física.
Master não oculta clipping interno: o contador analisa PCM antes do ganho final.
Panic não retira frames que já chegaram aos descritores DMA; essa cauda deve ser
medida fisicamente. UI representa notas mantidas, não certifica a cauda de release.

## Testes host e regressões

Comandos da raiz; logs locais ignorados em `build/p4_amy02_*`.
`PYTHONIOENCODING=utf-8`, `PYTHONUTF8=1`, builds com
`IDF_COMPONENT_CHECK_NEW_VERSION=0`, dependências fixadas mantidas.

| Status | Comando | Evidência/escopo |
|---|---|---|
| PASS | `./smk-p4/tests/run_host.ps1` | geometria, dirty buffers, PCM, driver I²S mock e9 testes Python |
| PASS | `./tests/run_smk_link.ps1` | protocolo, TouchNote real, releases e UART congestionada em host |
| PASS | `./tests/run_smk_safety.ps1` | MIDI/USB/áudio, falhas e ownership S3 |
| PASS | `./tests/run_smk_ui.ps1` | UI/modelo S3 |
| PASS | `./tests/run_smk_amy_boot.ps1 -Legacy` | adapter legado/AMY real; warnings preexistentes de ponteiros Windows |
| PASS | `./tests/run_smk_amy_boot.ps1 -Mutations` | recursos,256 patches, falha de alocação, mutations, owner e gerações |
| PASS | `./smk-p4/tests/run_amy.ps1` | AMY P4 real em host, backend local e callbacks reais com LVGL mock |
| NOT RUN | CI remota | workflow atualizado para novo runner e alterações `src/**` |

O probe intercepta malloc/calloc/realloc/free dos objetos vinculados: rejeita
heap no serviço/patch/Panic/render e verifica o mesmo número de alocações após
três ciclos de init/stop, além do diagnóstico comum. Baseline do processo2,
final2 em cada ciclo. Isso não inclui todas as alocações internas do sistema
operacional/CRT nem substitui heap checker do target. Testes de backend cobrem
controles inválidos/duplicados, snapshots aplicados/antigos, fila cheia, Note Off,
Panic, cancelamento de patch, vel0, lease, reset e produtor/owner concorrentes.
Teste UI executa `performance_ui.cpp` com mock LVGL; não é teste de pixels/touch real.

Caso comum host, quatro notas60/64/68/72 velocity100 por40 blocos, release
até15000 blocos, silêncio final/Panic. Valores iniciais host:

| ROM | Pico PCM16 | RMS | Clipping | Release/Panic |
|---|---:|---:|---:|---|
|128|2148|572,0148|0|PASS|
|135|1810|558,2924|0|PASS|

Comparador `tests/compare_amy_pcm.py` exige dois patches, sinal não zero,
clipping0, release/Panic verdadeiros, conclusão/restart RAM bem-sucedidos e
tolerância relativa10% para pico/RMS. Rejeita captura incompleta, FAIL e
patches duplicados de mais de um boot; cinco testes novos cobrem essa recusa.
Não exige igualdade bit a bit entre arquiteturas. Esses valores são digitais,
não tensão/ruído medidos nas saídas RCA.

## Builds e execução P4

Baseline anterior às mudanças: PASS nos5 ambientes. P4: display29,25s,
link25,49s, standalone89,61s. S3: original86,26s/link105,98s.
Matriz final e execução: resultados acrescentados abaixo após os comandos.
Primeiro build runtime falhou por dependência UART ausente na descoberta IDF;
resolvido mantendo REQUIRES incondicional e apenas os fontes condicionais.
Não se removeu o gerenciador de dependências.

COM13: VID:PID303A:1001, serial80:F1:B2:D3:CC:86; ROM confirmou P4rev1.3.
Backup da partição factory antes do upload:3.145.728 bytes, offset0x10000,
`build/p4_amy02_before_upload_factory.bin`, SHA256
`f283150df2cddd30eaf58ff38eca93237f95961401dbe47598de6834735527e8`.
Não é backup de toda Flash/NVS. A primeira leitura sem stub falhou com
`Packet content transfer stopped`; leitura com stub a460800 passou em16,4s.
Nenhuma escrita/eFuse nessa leitura; ela reiniciou o P4.

## Evidência executada no P4 (RAM, sem saída analógica)

**PASS funcional em RAM**: `build/p4_amy02_ram_selftest_final.log`, boot por
reset ROM/USB-JTAG, não partida fria. ROM128/135, acorde60/64/68/72 velocity100,
PCM não silencioso, Note Off, silêncio após release, Panic, finalização e novo
`begin` funcionaram. O comparador real passou para ambos os patches:

| ROM | Pico P4 | RMS P4 | Clip | DSP médio / máximo,4 vozes |
|---|---:|---:|---:|---:|
|128|2148|572,0152|0|1394 /1732us|
|135|1810|558,2924|0|1410 /1499us|

As40 renderizações de notas mantidas foram medidas antes de iniciar LVGL;
a varredura externa de RMS e os yields não entram nesses tempos. Incluem
render/deltas/ganho/telemetria do runtime e qualquer preempção ocorrida.
Máximo1732us <5333,33us nesse caso. Não é WCET, carga máxima com touch, latência
ponta a ponta, nem prova de áudio DMA/analógico. Não habilitar FX com base só nisso.

Memória interna antes do primeiro begin395795 bytes; após end395767; após
segundo begin/render/end395767; integridade do heap=1, novo begin ready=1.
Há um **resíduo inicial de28 bytes sem atribuição de callsite**, sem crescimento
no segundo ciclo. Não foi escondido nem compensado aumentando buffers/pool.
A primeira captura mediu248 bytes depois de logs float; a medição final moveu
logs para depois da amostragem e repetiu o ciclo. Não afirmar que todos esses
bytes são AMY, printf ou vazamento sem tracing. Host mantém alocações
interceptadas2->2; target ainda tem essa ressalva para desalocação absoluta.

Janela após self-test com LVGL ativo: render médio~106us/máximo280us em silêncio,
serviço máximo465us, compute/deadlines0, queue/high/overflow0, OOM0, notas0,
frames_written0, stack livre7360 bytes. Heap interno275707/maior196608;
PSRAM31178568 livres/maior30932992; valores estáveis nas amostras até~46s.
Touch/flush errors0, refresh avançando; pool LVGL90520 livres/81172 maior bloco,
pico45180/fragmentação11%. Isso é repouso com tela atualizando, sem aceitação
visual ou toque físico novo, não soak. DMA espera0/0 porque I²S não foi iniciado;
underruns UNMEASURED, nunca zero certificado. O footer local usa UNMEASURED.

Imagem diagnóstica efetivamente gravada:1.063.104 bytes, SHA256
`2e52e744500c9e16d1c8942a4dce62c53ad1977cf3ae2421b18c9fe1f7210c30`,
cópia em `build/p4_amy02_ram_diagnostic.bin`, config em
`build/p4_amy02_ram_sdkconfig.h`. Upload PIO passou, hash verificado,
`CONFIG_SMKP4_AMY_RAM_SELF_TEST=1`, confirmação de wiring/diagnóstico PCM/UART off.
Foi compilada do working tree após74141efe, incluindo a rotulagem DMA posterior;
a identificação verificável da imagem é o hash, não o nome do relatório.

A primeira captura `p4_amy02_ram_runtime.log` não contém o self-test completo;
RTS do capturador não forneceu novo boot completo nessa tentativa. Repetida com
consulta ROM `flash-id --after hard-reset`, seguida imediatamente de captura.
Usar `p4_amy02_ram_selftest_final.log` para a evidência completa. Não fundir
máximos/counters entre boots. Zero reset espontâneo foi observado nessa janela,
mas não é certificação prolongada.

## Matriz final de builds

| Status | Comando executado | Resultado |
|---|---|---|
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone -e guition-display -e guition-link` | ordem efetiva PIO: display56,57s/link39,12s/standalone496,50s; `p4_amy02_normal_matrix.log` |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-devkitc-1 -e esp32-s3-link` | original205,27s/link79,27s; `p4_amy02_final_s3.log` |
| PASS | `C:/.platformio/penv/Scripts/python.exe smk-p4/tests/check_standalone_build.py --ram-self-test` | config RAM explícita, GPIOs seguros, sem P4Link, ramos I²S musical/diagnóstico compilados separadamente, sem gerar firmware com I²S habilitado |
| PASS | `C:/.platformio/penv/Scripts/python.exe smk-p4/tests/check_standalone_build.py` | configuração normal sem self-test, saída I²S e UART desabilitadas; `p4_amy02_normal_config.log` |
| PASS | `C:/.platformio/penv/Scripts/python.exe smk-p4/tests/compare_amy_pcm.py --host build/p4_amy02_runtime_final.log --p4 build/p4_amy02_ram_selftest_final.log` | comparação pico/RMS e release/Panic; `p4_amy02_pcm_comparison.log` |
| MANUAL | I²S habilitado e som analógico | operador confirmou nesta conversa que montagem ainda não foi conferida |
| NOT RUN | outros targets upstream/Python/Arduino/Daisy e execução física S3 nova | não confundir matriz local com aprovação de todas as plataformas |

Resumos PlatformIO normais: display RAM12968/Flash576804; link RAM18212/
Flash670362; standalone RAM28936/Flash1058854 bytes. Baseline standalone
RAM13104/Flash578056: deltas estáticos+15832/+480798. Não são pico de heap,
nem toda a ocupação do mapa IDF. O runtime aloca no boot; usar medições de
heap/stack acima para avaliação, não percentuais de RAM do resumo PIO.
Bin normal gerado antes do upload:1.059.584 bytes (<partição factory3MiB),
SHA256 `cfe2b765905ac80cfda709ffec57c6548f81c5b242dda575deeaade307fd324f`.
Confirmar o hash efetivamente gravado no registro final abaixo se upload recompilar.

Comandos físicos usados, todos com I²S off:

```powershell
C:/.platformio/penv/Scripts/python.exe -m esptool --chip esp32p4 --port COM13 --baud 460800 --after hard-reset read-flash --no-progress 0x10000 0x300000 build/p4_amy02_before_upload_factory.bin
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone -t upload --upload-port COM13
C:/.platformio/penv/Scripts/python.exe -m esptool --chip esp32p4 --port COM13 --baud 115200 --no-stub --after hard-reset flash-id
C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port COM13 --output build/p4_amy02_ram_selftest_final.log --seconds 50
```

## Imagem normal deixada na placa

**PASS build/upload/execução RAM normal**, sem self-test automático. Configuração
gerada conferida pelo checker: STANDALONE=1, wiring/diagnóstico PCM/self-test/UART
desabilitados. Upload COM13 com hash verificado,119,77s;
`build/p4_amy02_normal_upload.log`. Como o upload recompilou, a imagem realmente
gravada é1.059.568 bytes, SHA256
`c84340251021d77552a2d7366527351d2b23e3a84a5dbcec4bd1c9c0839a6612`,
cópia em `build/p4_amy02_normal.bin`. Não usar o hash anterior à recompilação
para identificar a imagem instalada. Commits posteriores não a regravam.

`build/p4_amy02_normal_runtime.log`: reset por consulta ROM, captura30s,
Performance inicializou em~1,8s; zero linhas de self-test; owner pronto,
patch128, notas0, queue/high/overflow0. Blocos543->2543, frames_written0;
render médio112->103us, máximo272us; serviço459us; DSP/compute deadlines0;
write_errors/timeouts0 **com I²S desabilitado**; não são teste de escrita DMA.
Stack livre7380 bytes. Heap interno275959/maior196608 e PSRAM31178568/
maior30932992 estáveis nas amostras. Touch/flush errors0 e refresh avançando.
Isso confirma boot visual sem comandos musicais registrados; não confirma
imagem/toque/som pelo operador. O firmware fica em `AUDIO RAM` até conferência
elétrica e reconfiguração explícita. Não foi gravado firmware no S3 nesta rodada.

Último `./smk-p4/tests/run_host.ps1`: PASS, inclusive9 testes Python,
`build/p4_amy02_host_final.log`; `run_amy.ps1` final também PASS, com velocity30/100,
acordes/release/Panic, heap, filas e callbacks, `build/p4_amy02_runtime_final.log`.

## Gates e aceitação física

A compilação e testes host não aprovam os gates físicos. Usar o roteiro de
ligação para silêncio/L/R, seguida de Performance, arraste/release/perda de
contato, hold10s, oitavas, browser/patch, master rápido, Panic e novo toque.
Registrar áudio+touch simultâneos, erros/deadlines/filas, memória e stack em
janelas do mesmo boot. Fazer partida fria e teste com alimentação independente do computador,
30min inicial e ensaio prolongado posteriormente.
Ainda não há medição ponta a ponta nem qualificação de banda.

Estado dos gates: A tem PASS funcional/RAM para init/patch/PCM/notas/Panic e
restart, com a ressalva explícita do resíduo inicial28 bytes; não é fechamento
incondicional de toda auditoria de memória. B=MANUAL (ligação e áudio analógico),
C=PASS em host/mock para callbacks, MANUAL para Performance/touch/som reais,
D=MANUAL para estabilidade musical sob I²S+touch e ensaio prolongado. Não há
aprovação de instrumento pronto para banda.

## USB MIDI Host: preparado documentalmente, sem implementação parcial

Reutilizáveis: parser de MIDI/descritores S3, eventos normalizados e política de
Panic. Revalidar serviço Host/alocações/tarefas no P4 e fornecer novo produtor
serializado ao mailbox: ele é SPSC, não deve receber USB e UI diretamente de
tarefas distintas. Usar task de controle ou outro transporte SPSC dedicado.
Não reutilizar os tokens Touch para outro source sem ownership explícito.

Porta física FS/HS, alimentação `USB5V_IN`, proteção/backfeed e hub alimentado
precisam de conferência na unidade/esquema. USB-JTAG/COM13 é debug, não porta
MIDI. O suporte IDF5.5.2 e o esquema foram investigados no relatório0.1; isso
não prova VBUS/Host operacional nesta placa. Não inicializar Host antes de
validar AMY+PCM5102A+touch, nem usar S3/UART como fallback standalone.

## Riscos / próximo menor passo

Confirmar e testar a montagem PCM5102A; validar Performance física no firmware
musical, notas/release/Panic e coexistência sob carga. Macros FM e FX continuam
indisponíveis; USB MIDI, layers e novo sequencer não foram implementados.
A reserva de boot não resolve todos os OOM upstream; não aumentar banco/pool
sem novo teste de memória. O resíduo inicial28 bytes no target está sem
atribuição, embora o segundo ciclo não mostre crescimento. Contadores de scheduling não certificam ausência
de underrun analógico. A próxima etapa é aceitação do caminho vertical físico,
seguida de USB Host em incremento isolado, e não recursos extras.

## Arquivos desta entrega

- `.github/workflows/smk-p4-build.yml`
- `smk-p4/README.md`
- `smk-p4/components/amy_runtime/CMakeLists.txt`
- `smk-p4/components/amy_runtime/amy_runtime.cpp`
- `smk-p4/components/amy_runtime/amy_runtime.h`
- `smk-p4/components/amy_runtime/commands.h`
- `smk-p4/components/amy_runtime/runtime_config.h`
- `smk-p4/components/amy_runtime/self_test.cpp`
- `smk-p4/components/amy_runtime/self_test.h`
- `smk-p4/components/audio_pcm5102/Kconfig`
- `smk-p4/docs/pcm5102_wiring.md`
- `smk-p4/docs/standalone_amy_performance_02_results.md`
- `smk-p4/main/CMakeLists.txt`
- `smk-p4/main/app_main.cpp`
- `smk-p4/main/gate0_diagnostics.cpp`
- `smk-p4/main/local_performance_backend.h`
- `smk-p4/main/p4_link.h`
- `smk-p4/main/performance_backend.h`
- `smk-p4/main/performance_ui.cpp`
- `smk-p4/main/performance_ui.h`
- `smk-p4/main/remote_performance_backend.h`
- `smk-p4/main/standalone_main.cpp`
- `smk-p4/main/touch_note.h`
- `smk-p4/sdkconfig.defaults.standalone`
- `smk-p4/tests/amy_runtime_test.cpp`
- `smk-p4/tests/check_standalone_build.py`
- `smk-p4/tests/compare_amy_pcm.py`
- `smk-p4/tests/heap_probe.cpp`
- `smk-p4/tests/local_backend_test.cpp`
- `smk-p4/tests/mocks/esp_log.h`
- `smk-p4/tests/mocks/esp_timer.h`
- `smk-p4/tests/mocks/lvgl.h`
- `smk-p4/tests/performance_ui_test.cpp`
- `smk-p4/tests/run_amy.ps1`
- `smk-p4/tests/run_host.ps1`
- `smk-p4/tests/test_amy_pcm_comparison.py`
- `src/amy.c`
- `src/filters.c`
- `src/patches.c`
