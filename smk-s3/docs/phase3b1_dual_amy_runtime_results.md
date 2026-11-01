# Fase 3B.1 â€” gate de runtime dual e isolamento FM (entrega parcial)

**Estado: fase NÃƒO concluÃ­da. Layer B permanece indisponÃ­vel no produto.**
O experimento com dois instrumentos no engine passou; isso nÃ£o equivale a uma
integraÃ§Ã£o dual no EventBus/mailbox/ownership, nem a Ã¡udio fÃ­sico qualificado.

## 1. Auditoria inicial e escopo

Base real: `13571860` (merge da Fase 3B), Ã¡rvore inicialmente limpa, branch
`feature/smk-s3-dual-amy-runtime`. Difere da referÃªncia `836de360` do pedido.
Nenhuma alteraÃ§Ã£o do usuÃ¡rio foi descartada. Leitura: AGENTS.md, specs.md,
README, relatÃ³rios 3A/3B, ownership/boot allocation, manifests/CMake/CI,
app_main, AmyAdapter/amy_commands, NoteOwnership, PerformanceConfig/Route/State,
PatchManager, StorageWorker e alocadores/render reais do AMY.

As seis suÃ­tes existentes passaram ANTES das alteraÃ§Ãµes. Logs ignorados:
`build/phase3b1_baseline_{safety,amy_boot,performance,patch_integrity,synth_expansion,ui}.log`.
README jÃ¡ contÃ©m conflitos AMY/P4; builds usados sÃ£o os do projeto isolado
smk-s3. Specs define 284Ã—76, perfil local mantÃ©m 240Ã—240. NÃ£o alterar hardware
para resolver esse conflito. O pedido explÃ­cito desta fase autoriza investigar
layers alÃ©m do milestone inicial, mas exige gate e permite entrega parcial segura.

Problemas comprovados antes da ediÃ§Ã£o:

- `executePreset()` chama `executeAllNotesOff()` e altera modo/pilha mono Ãºnicos.
- Preset avanÃ§a a geraÃ§Ã£o GLOBAL em `amy_commands.cpp`; PatchManager tambÃ©m
  solicita Panic global. `amy_deltas_reset()` e descarte PCM sÃ£o globais.
- Eventos de controle/macros antigos nÃ£o carregam uma geraÃ§Ã£o por instrumento.
  Remover somente o AllNotesOff nÃ£o torna patch switching seletivo seguro.
- Snapshots FM eram indexados por ordinal local de voz, compartilhados entre
  destinos. Capturar destino diferente de 1 invalidava a baseline principal;
  controles relativos de outro synth podiam usar os coeficientes de A.
- `routePerformance()` podia retornar A quando A+B eram solicitadas e B nÃ£o
  estava pronta. O teste anterior explicitamente aceitava essa degradaÃ§Ã£o.
- `patches_voices_for_load_synth()` libera osciladores antes da nova alocaÃ§Ã£o.
  `patches_load_patch()` registra o instrumento mesmo apÃ³s falhas de contiguidade;
  retorna void. `instrument_add_new()` libera o container antigo antes de
  `instrument_init()` alocar seu substituto e FIFOs. OOM Ã© contado, nÃ£o Ã© ACK.
- A evidÃªncia fÃ­sica anterior registrada na Fase 3B incluÃ­a render mÃ¡ximo
  241.039 us, trÃªs deadlines perdidos e 63 lacunas PCM. NÃ£o foi repetida nesta
  sessÃ£o e nÃ£o constitui orÃ§amento aprovado para duas layers.

Arquivos previstos/alterados: adapter/header, performance.h, testes AMY/performance,
script AMY e este relatÃ³rio. Sem dependÃªncias, GPIOs, DSP novo ou tarefas novas.
AceitaÃ§Ã£o deste incremento: isolamento FM real, recusa de onset parcial,
experimento de alocaÃ§Ã£o/PCM/ganho, falha de contiguidade reproduzÃ­vel,
regressÃµes Single e builds. NÃ£o publicar Ready B para contornar esses bloqueios.

## 2. Arquitetura antes e depois

ProduÃ§Ã£o permanece USB/Clock/Sequencer â†’ EventBus â†’ controle â†’ mailbox AMY â†’
amy_owner/NoteOwnership â†’ AMY â†’ fila PCM â†’ AudioTask/IÂ²S. HÃ¡ um Ãºnico engine,
um owner e um mix. Destinos de notas aceitos continuam 1 e 10. Nenhuma nova
rota 2 foi habilitada; PerformanceConfig ainda nÃ£o estÃ¡ ligado ao app_main.

MudanÃ§a implementada: estado de validade e baseline FM por instrumento 1/2;
coeficientes originais indexados pelo oscilador fÃ­sico, cuja propriedade AMY
jÃ¡ Ã© exclusiva. IDs 0/1 continuam aliases de A. Drums e outros IDs nÃ£o tomam
emprestada uma baseline melÃ³dica. APIs sem ID preservam o comportamento A.
Acesso ao DSP continua exclusivo do owner; o harness host dirige o mesmo
owner sincronamente quando usa a API C diretamente para preparar experimentos.

## 3. SeparaÃ§Ã£o de estados

| Estado | SituaÃ§Ã£o apÃ³s este incremento |
| --- | --- |
| FM: nÃ­veis, ratios e frequÃªncias originais | PASS: pool fixo por oscilador; controles relativos de A/B isolados |
| FM: baseline, validade e invalidaÃ§Ã£o para UI | PASS por destino; API legada continua A; nÃ£o Ã© ACK de patch |
| Patch/voices/portamento/bend/nÃ­vel no AMY | JÃ¡ pertencem ao instrumento no engine; nÃ£o duplicados |
| Mono/legato e pilha no adapter | BLOCKED: continuam Ãºnicos e orientados ao synth 1 |
| Sustain lÃ³gico no NoteOwnership | Por canal fÃ­sico; falta polÃ­tica/estado seletivo por saÃ­da de layer |
| ReferÃªncias no adapter | Continuam A/drums; B nÃ£o aceito |
| Macros/PatchManager | Continuam uma parte principal |
| GeraÃ§Ãµes de comandos/deltas/PCM | Globais + fences por fonte; falta fence por instrumento |

Recapturar B nÃ£o invalida A. Invalidar B por API nÃ£o invalida A. Alterar
Ã­ndice FM, ratio ou detune de uma parte preserva os coeficientes da outra.
A captura Ã© feita quando o patch foi materializado no owner, como antes.
Comandos wire arbitrÃ¡rios ainda podem invalidar o contrato de configuraÃ§Ã£o;
esta etapa nÃ£o os transforma em API transacional nem sandbox.

## 4. AlocaÃ§Ã£o e recursos: evidÃªncia AMY real

48 kHz, 256 frames, pool de 120, mesmos fontes C do componente de firmware,
com interfaces de plataforma substituÃ­das no host. Instrumento 1: patch 128,
quatro vozes completas/32 oscs. Instrumento 2: patch 135, quatro vozes
completas/32 oscs. Bateria 10: configuraÃ§Ã£o existente, 32 oscs completos.
**PASS** para esse arranjo de 96 osciladores no host. Quatro vozes nÃ£o foram
adotadas como limite garantido de performance. Boot Single continua oito
vozes e B ausente, sem reservar osciladores ou vozes para B.

Novo modo de teste `--dual-allocation-failure`, em processo separado:
A4/patch128 + B8/patch135 + bateria solicita 128 oscs. B Ã© registrado com
oito vozes, porÃ©m possui somente **48/64 oscs**; duas vozes nÃ£o sÃ£o completas.
A e drums permanecem com 32 oscs completos cada. O teste exige espaÃ§o livre
somado >=8 e maior trecho contÃ­guo <8, comprovando fragmentaÃ§Ã£o alÃ©m da soma.
O script aceita apenas o diagnÃ³stico esperado `cannot find 8 oscs...patch 135`;
qualquer outro stderr ou ausÃªncia do diagnÃ³stico falha a suÃ­te.
**Nunca se renderiza esse arranjo incompleto.** O teste remove B explicitamente
e a recria com quatro vozes, verificando A/B/drums completos novamente.
Isso Ã© recuperaÃ§Ã£o explÃ­cita em bancada host, nÃ£o rollback transacional do produto.

## 5. ACK, rollback e patch switching

**BLOCKED / nÃ£o implementados no runtime dual.** NÃ£o existe protocolo novo
Requested/Preparing/Ready/Failed nem confirmaÃ§Ã£o por request ID nesta entrega.
NÃ£o apresentar o sucesso do teste de engine como ACK da configuraÃ§Ã£o do produto.
A alocaÃ§Ã£o existente Ã© destrutiva; validar somente contagem de vozes ou soma
nÃ£o basta. A reproduÃ§Ã£o negativa acima demonstra por que Ready B continua falso.

Uma soluÃ§Ã£o ainda precisa: validar patch/custo/contiguidade antes de mutaÃ§Ã£o;
preparar containers/FIFOs e recursos fora do deadline; manter configuraÃ§Ã£o
aplicada atÃ© commit; reverter uma preparaÃ§Ã£o falha sem liberar A/drums;
invalidar comandos/deltas do destino sem descartar o Ã¡udio da outra parte;
rejeitar ACKs antigos. NÃ£o hÃ¡ timeout arbitrÃ¡rio implementado.
Trocas legadas continuam usando sua barreira global conservadora. Nenhuma
promessa de troca seletiva, mute/ramp seletivo ou crossfade foi adicionada.

## 6. NoteOwnership e roteamento

**PASS:** o plano agora Ã© recusado inteiro se alguma saÃ­da que corresponde Ã 
nota/velocity/canal/zona/transpose estiver indisponÃ­vel. Isso vale nas duas
direÃ§Ãµes e no overlap de split. B muted/disabled ou fora da zona nÃ£o impede
uma saÃ­da A vÃ¡lida. Single nÃ£o exige readiness B. NÃ£o recalcula o destino
de Note Off nem altera watermark, capacidade, prioridade ou sustain existente.

**BLOCKED:** integraÃ§Ã£o de ownership dual, incluindo disable B com acorde,
pedal seletivo, troca B com A soando, stealing por destino e disconnect dual.
Os testes existentes continuam cobrindo esses mecanismos para A/drums e
rejeiÃ§Ã£o atÃ´mica de uma rota contendo B sem backend. NÃ£o ampliar a whitelist
para instrumentos arbitrÃ¡rios ou passar B como se fosse A.

## 7. Ganho e efeitos

Descoberta importante: este HEAD jÃ¡ tem `amy_event.synth_level`,
`instrument_set_level()` e lookup de nÃ­vel por voz no mix. NÃ£o Ã© necessÃ¡rio
multiplicar velocity ou introduzir DSP novo. O ganho escala saÃ­da audÃ­vel;
nÃ£o escala os operadores de controle/modulaÃ§Ã£o de novo.

**PASS no engine:** ambos os synths em nÃ­vel zero produzem silÃªncio, A isolada
produz PCM, B isolada produz PCM, ambas habilitadas produzem PCM; os valores
de velocity dos dois instrumentos nÃ£o mudam. Bateria continua audÃ­vel com
ambos os ganhos melÃ³dicos em zero. Effects sÃ£o desabilitados no ensaio de
silÃªncio para nÃ£o confundir caudas com ganho de instrumento.
**NOT RUN no produto:** PerformanceRoute.volume ainda nÃ£o chega a esse ganho.

| Escopo | ParÃ¢metros |
| --- | --- |
| Instrumento | synth_level, patch, vozes, sustain upstream, portamento e bend |
| Bus 0 compartilhado A/B | chorus, delay, reverb/freeze, EQ e hook de drive |
| Bus 1 bateria | EQ e configuraÃ§Ã£o seca existentes |
| Global | master gain/proteÃ§Ã£o, engine/delta queue e mix PCM |

NÃ£o se duplicaram efeitos nem se expuseram controles B para efeitos compartilhados.
PersistÃªncia SMPF/CRC permanece intacta, sem jobs novos de StorageWorker ou UI
que anuncie dual operacional.

## 8. MemÃ³ria, tempo real e polifonia

Snapshots antigos: 32 vozes Ã—(6 operadores +1 mod_source) Ã—16 bytes =3.584 bytes.
Agora: 120 osciladores Ã—16 =1.920 bytes, com metadados separados (12 bytes por
instrumento no host). Pool estÃ¡ no prÃ³prio adapter, alocado no boot. Nenhum
heap, I/O ou log novo por nota/bloco; a indexaÃ§Ã£o FM tem limites explÃ­citos.

| Medida sizeof | Antes (HEAD base) | Depois | DiferenÃ§a |
| --- | ---: | ---: | ---: |
| AmyAdapter, host MinGW | 8.656 | 7.008 | âˆ’1.648 bytes |
| AmyAdapter, Xtensa S3 | 8.648 | 7.000 | âˆ’1.648 bytes |

Valores Xtensa lidos da constante gerada em assembly, com os headers antes/depois
compilados no mesmo toolchain. NÃ£o sÃ£o mediÃ§Ãµes de heap livre. Logs/probes em
`build/phase3b1_size/`. RAM/PSRAM livres, maior bloco fÃ­sico, stack, carga e
latÃªncia dual continuam MANUAL. Nenhum nÃºmero de voz ativa foi inventado.

Polifonia fÃ­sica/stealing continuam upstream por instrumento; referÃªncias
lÃ³gicas nÃ£o medem vozes ou releases. O teste 4+4 nÃ£o autoriza todo par de
patches. AlocaÃ§Ãµes de instrumentos/FIFOs e materializaÃ§Ã£o tardia de osciladores
ou deltas continuam podendo ocorrer no owner; hÃ¡ apenas dois blocos PCM de
amortecimento. NÃ£o alegar deadline garantido com base no ensaio host.

## 9. ValidaÃ§Ã£o e comandos executados

Da raiz, PowerShell, toolchains existentes; sem novos pacotes:

```powershell
./tests/run_smk_safety.ps1
./tests/run_smk_amy_boot.ps1
./tests/run_smk_performance.ps1
./tests/run_smk_patch_integrity.ps1
./tests/run_smk_synth_expansion.ps1
./tests/run_smk_ui.ps1
& C:/.platformio/penv/Scripts/python.exe -m platformio run -d smk-s3 -e esp32-s3-devkitc-1 -e esp32-s3-devkitc-1-128
git diff --check
```

| Status | Teste |
| --- | --- |
| PASS | Todas as seis suÃ­tes, baseline e versÃ£o final |
| PASS | AMY real: alocaÃ§Ã£o 4+4/drums, PCM/ganho, velocity intacta e FM por destino |
| PASS | AMY real: falha/fragmentaÃ§Ã£o e recuperaÃ§Ã£o explÃ­cita em processo separado |
| PASS | AMY legado: 256 patches, ownership, mono/legato, sustain, Panic, stealing, owner concorrente e filas/PCM |
| PASS | Performance: recusa dual parcial, split/overlap/mute, comandos/CRC e snapshots |
| PASS | Patch integrity, synth expansion/StorageWorker, UI e safety/timing existentes |
| PASS | Probe Xtensa de performance_format/routePerformance, C++17, sem RTTI/exceÃ§Ãµes, -Wall -Wextra -Werror |
| PASS | MutaÃ§Ã£o: colisÃ£o de snapshots por Ã­ndice de operador causa assert em run_dual_fm_isolation; versÃ£o correta passa |
| NOT RUN | Runtime dual integrado, ACK/rollback, sustain/mono seletivo, persistÃªncia/UI de performance |
| NOT RUN | Engine real em blocos de 128 no host, CI remota IDF 5.3/5.5.2, outras plataformas e TSan |
| MANUAL | USB/PCM5102A, latÃªncia, deadlines reais, heap/PSRAM, Flash stalls e soak |

MutaÃ§Ã£o somente em cÃ³pia ignorada `build/phase3b1_mutation/amy_adapter_mutant.cpp`:
substituir `fm_base_oscs_[op_osc]` por `fm_base_oscs_[op]`. Compilada com G++,
mesmas flags/fontes/objetos de run_smk_amy_boot e amy_commands real; executÃ¡vel
retornou âˆ’1073740791 no assert de ratio do teste dual (falha esperada).
A primeira compilaÃ§Ã£o da cÃ³pia encontrou um header antigo do probe de sizeof;
foi movida para diretÃ³rio separado e entÃ£o compilou e falhou como esperado.
O repositÃ³rio nÃ£o recebeu a mutaÃ§Ã£o. Avisos preexistentes upstream de casts de
ponteiros continuam no host; nÃ£o houve stderr inesperado na suÃ­te positiva.

## 10. Builds ESP32-S3

| Status | Bloco | RAM estÃ¡tica | Flash | DuraÃ§Ã£o |
| --- | ---: | ---: | ---: | ---: |
| PASS | 256 | 30.116 /327.680 | 1.140.715 /16.777.216 | 51,681 s |
| PASS | 128 | 30.116 /327.680 | 1.140.735 /16.777.216 | 62,954 s |

IDF 5.5.2, plataforma pioarduino 55.03.36-1, Xtensa GCC 14.2.0, 48 kHz.
Comparado aos nÃºmeros registrados na Fase 3B (nÃ£o a outro build de baseline
nesta sessÃ£o): RAM estÃ¡tica igual, Flash +112 bytes em cada ambiente. ReduÃ§Ã£o
do tamanho do adapter acontece no objeto de boot e nÃ£o aparece como BSS livre.
PlatformIO avisou sobre codepage de mÃ©tricas, mas ambos terminaram SUCCESS.
Sem upload. Logs: `build/phase3b1_firmware.log`, `phase3b1_final_*.log`,
`phase3b1_mutation.log` e `build/smk_amy_boot/dual_allocation_failure.log`.

## 11. Bancada fÃ­sica â€” MANUAL

Nenhuma pinagem, revisÃ£o, display, strap PCM5102A, VBUS ou estÃ¡gio de saÃ­da foi
inferido. Manter o wiring documentado existente e confirmar alimentaÃ§Ã£o USB
segura antes de conectar o controlador. Porta USB nativa para host, USB-UART
para console. NÃ£o existe comando novo para habilitar dual nesta entrega.

1. Em cada build, resetar, executar `audio_reset`, `status`, `audio_status` e
   `memory`; registrar deadlines, render mÃ¡ximo/mÃ©dio, PCM gaps, erros IÂ²S,
   heap interno/mÃ­nimo/maior bloco e PSRAM antes/depois. NÃ£o exigir zero de
   contadores cumulativos sem abrir uma janela de mediÃ§Ã£o nova.
2. Em Single, teclado/pedal reais: C4, acorde, re-press sob sustain, pedal-up,
   arp/seq + teclado e Stop. Esperado: nenhum stuck note, Stop preserva teclado.
   Repetir troca de patch, Panic, USB disconnect e 100 reconnects, verificando
   retorno de notas sem reboot quando suportado.
3. ApÃ³s existir backend dual validado, usar A4 patch128 e B4 patch135; tocar C4
   nas duas, soltar, repetir com sequenciador e transposiÃ§Ãµes opostas. Alterar
   split com nota mantida e desabilitar B com acorde. Esperado: Off na saÃ­da
   capturada, nenhuma referÃªncia Ã³rfÃ£. Hoje: BLOCKED, nÃ£o executÃ¡vel pelo produto.
4. SÃ³ com o mesmo backend, segurar A e trocar patch de B (e vice-versa), pedal
   pressionado, caudas e pads/drums ativos. Esperado: parte nÃ£o afetada continua;
   ACK sÃ³ apÃ³s aplicaÃ§Ã£o; recusa preserva configuraÃ§Ã£o aplicada; nenhuma nota
   antiga reaparece. Saturar ownership e provocar pedido inviÃ¡vel de recursos.
5. Repetir patches densos, oito knobs, gravaÃ§Ã£o Flash via StorageWorker e display;
   registrar crescimentos de deadlines/gaps/IÂ²S e mÃ­nimo de memÃ³ria. Capturar
   MIDI e saÃ­da analÃ³gica em interface externa para latÃªncia, cliques e clipping.
6. Soak de duas horas por bloco, com acordes/pedal, drums, knobs, saves e ciclos
   USB. Esperado: nenhuma nota presa/crash, nenhuma falha de IÂ²S e nenhuma perda
   de prazo em regime qualificado; registrar qualquer ocorrÃªncia, sem mascarar
   gaps como simples underruns de DMA. NÃ£o aprovar palco sem essa evidÃªncia.

## 12. CritÃ©rios de aceitaÃ§Ã£o / prÃ³ximos passos

| CritÃ©rio da fase | SituaÃ§Ã£o |
| --- | --- |
| A/B simultÃ¢neos | PASS no experimento do engine; BLOCKED no produto |
| Synth 10 preservado | PASS nas regressÃµes e no ensaio dual |
| Off/sustain corretos por destino | PASS legado; BLOCKED dual integrado |
| Patch switching seletivo | BLOCKED |
| Panic global | PASS legado; dual integrado NOT RUN |
| Falha B nÃ£o corrompe A | PASS propriedade de oscs no ensaio negativo; transaÃ§Ã£o completa BLOCKED |
| PublicaÃ§Ã£o reflete estado aplicado | BLOCKED; sem ACK novo |
| Sem alocaÃ§Ãµes inseguras em performance dual | BLOCKED; B permanece desabilitada |
| Volume independente sem alterar velocity | PASS engine; integraÃ§Ã£o NOT RUN |
| Single legado compatÃ­vel | PASS suÃ­tes e builds |
| PerformanceConfig integrado | NOT RUN / nÃ£o implementado |
| Testes/builds disponÃ­veis | PASS |
| Limites fÃ­sicos documentados | PASS documentaÃ§Ã£o; MANUAL qualificaÃ§Ã£o |

PrÃ³ximo menor passo: uma transaÃ§Ã£o de preparaÃ§Ã£o/commit por instrumento com
reserva real, falhas injetadas de heap/contiguidade, rollback e confirmaÃ§Ã£o
versionada; incluir invalidaÃ§Ã£o seletiva de comandos/deltas antes de retirar a
barreira global legada. Separar mono/pedal/referÃªncias e sÃ³ entÃ£o ligar B ao
ownership e PerformanceConfig. O backend nÃ£o pode usar apenas este ensaio
4+4 como certificado de SRAM/deadline. Fase 3C exige completar esses pontos,
StorageWorker de performance e bancada/soak dual; nÃ£o iniciar harmonia/UI nova.

## 13. Arquivos e commits

- `smk-s3/components/audio/amy_adapter.cpp` e `include/amy_adapter.h`: isolamento FM.
- `smk-s3/components/audio/include/performance.h`: onset sem degradaÃ§Ã£o parcial.
- `tests/test_smk_amy_boot.cpp`, `run_smk_amy_boot.ps1`: AMY real e falha controlada.
- `tests/test_smk_performance.cpp`: readiness atÃ´mica nas zonas correspondentes.
- Este relatÃ³rio: auditoria, evidÃªncias e limites.

Sem alteraÃ§Ãµes em `src/` upstream. NÃ£o foi necessÃ¡rio patch upstream para
isolamento FM nem para provar ganho existente. MudanÃ§as futuras no carregador
transacional precisam justificar uma extensÃ£o estreita; nÃ£o reescrever o engine.
Commits locais, sem push/upload:

- `1f789b41` — isolamento FM e probes AMY reais de alocação/PCM/ganho/falha.
- `01575aec` — recusa de rotas parciais por indisponibilidade de layer.
- `docs(smk): record phase3b1 safety gates and validation` — este relatório.
