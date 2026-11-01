# Fase 3B — performance e layers (entrega parcial segura)

## A — auditoria antes das alterações

Base: `6fd57c5a3673a6d77c65d373c29f1c1004738970`, merge da Fase 3A,
branch inicial main, árvore limpa. Branch de trabalho:
`feature/smk-s3-performance-layers`.

Leitura: AGENTS.md, specs.md, README, manifests/builds/CI, relatórios 2.5/3A,
amy_boot_allocation, amy_ownership, FX M3, smoke 20261101, adapter/mailbox,
NoteOwnership, app_main, PatchManager, StorageWorker, formatos v6 e UIManager,
alocadores src/instrument.c e src/patches.c e suites host.

O README conserva conflitos antigos AMY/P4; o projeto isolado smk-s3 continua
sendo a referência de build. Specs pede 284×76; o perfil local usa 240×240.
Nenhuma decisão de hardware será inferida. O pedido explícito desta fase
autoriza a infraestrutura de layers, mas permite entrega parcial segura.

### Pré-requisitos e gate

- Comprovado: Fase 3A integrada; identidade origem/instância/canal/entrada/token;
  64 identidades, fan-out 4, sustain por canal, releases capturados no onset,
  Stop seletivo e Panic geracional. Destinos aceitos hoje: synth 1 e drums 10.
- PASS antes de editar: run_smk_safety.ps1 (inclui timing) e
  run_smk_amy_boot.ps1, engine e owner reais, inclusive 256 trocas de patch.
  Logs locais ignorados: build/phase3b_baseline_safety.log e
  build/phase3b_baseline_amy.log. As demais regressões de baseline também
  passaram (patches, expansão/storage e UI), sem mudanças nos caminhos testados.
- Não atendido: bancada/soak musical da Fase 3A. O smoke 20261101 registrou
  38.479 bytes internos livres e maior bloco de 21.504 bytes em repouso, mas
  também 63 lacunas PCM, três deadlines perdidos e render máximo 241.039 us,
  de origem não determinada. Não é orçamento de performance aprovado.
- Bloqueio de integração: executePreset faz AllNotesOff global; fila de preset
  avança geração global. Mono stack/modo e snapshots FM são únicos. Rotas
  convertem todo destino não-drums para synth 1. Habilitar synth 2 sem separar
  esses estados corromperia isolamento entre layers.
- Bloqueio de recursos: pool 120; bateria patch 258 reserva 32; Juno patch 0
  usa seis oscs/voz, DX7 oito. A8 Juno+B8 DX7+drums =144; dois DX7 de oito
  vozes =160. Mesmo 4+4 DX7+drums =96 é apenas limite aritmético, não prova
  contiguidade, heap, PSRAM ou deadline. Alocador exige bloco contíguo por voz.
- instrument_init/voice_fifo_init alocam heap; troca de patch pode materializar
  recursos na fase de comandos do owner. Não há transação/ACK de capacidade
  que garanta habilitação B sem alocação tardia. Não duplicar esse caminho.

### Orçamento e fronteiras atuais

Synth 1: principal, bus 0, oito vozes no boot (patch pode mudar orçamento),
delay de stealing 4 ms, última nota em mono/legato. Synth 10: container de
uma voz/32 osciladores PCM, bus 1, IGNORE_NOTE_OFFS e mapa GM. Limite upstream
por instrumento: 32 vozes; não equivale a 32 vozes disponíveis neste produto.
AMY faz stealing e agrega pitches iguais por instrumento. Referências lógicas
do adapter não medem vozes físicas nem caudas de release.

FX/EQ/drive são compartilhados por bus; bus 1 é seco. A e B no bus 0
compartilhariam FX. Macros/patch v6, scenes, perfis MIDI e UI hoje operam sobre
um principal. StorageWorker usa snapshots copiados e único worker de I/O;
UI/console ainda têm acessos paralelos a estado de controle (risco prévio).

### Escopo seguro escolhido antes de editar

Entregar A/B portáteis e testes: configuração validada, comandos por valor,
snapshot copiado, roteador limitado e codec explícito versionado/CRC. Arquivos
previstos: novos headers em audio/include (junto ao NoteRoute), teste/script,
CI e este relatório. Sem mudança no core AMY, hardware, filas ou presets
existentes. O firmware permanece Single; não expor controles B que não possam
ser aplicados. C e integração runtime de D ficam bloqueadas pelas condições
acima. Não declarar a fase completa.

Tempo real: roteador tem duas iterações, sem heap/I/O/locks; codec não pertence
ao render. Compatibilidade: preservar APIs/formatos atuais, nenhuma dependência.
Aceitação deste incremento: modelo/codec/routing host, regressões existentes,
AMY real preservado, builds 128/256 e diff limitado. Não alegar áudio dual.

## B — modelo e roteamento entregues

`components/audio/include/performance.h` é header-only, C++17 e independente
de AMY C API, ESP-IDF, FreeRTOS, display e GPIO. Reutiliza NoteIdentity/NoteRoute
sem alterar o ownership de produção. A arquitetura anterior de produção permanece
USB/EventBus → controle → mailbox AMY → owner/NoteOwnership → AMY → PCM/I²S.
O módulo novo é infraestrutura disponível para integração futura, exercitada nos
testes; **app_main não chama o novo roteador ou comandos**.

- PerformanceConfig: Single padrão, A habilitada com patch inicial, B
  desabilitada. IDs de destino fixos A=1, B=2; drums=10 permanece fora dele.
- Modos Layer e Split consultam zonas independentes. Split permite overlap e
  gaps; SetSplitPoint define A=[0,ponto−1], B=[ponto,127] e não habilita B.
- Filtros inclusivos de nota e velocity, máscara de canais (bit 0 = canal MIDI
  1), transpose −24..+24. Máscara vazia e limites invertidos são rejeitados.
- Pitch calculado em inteiro; saída inválida é descartada individualmente.
  Nota 0 permanece sem onset por restrição AMY já existente na Fase 3A, mesmo
  sendo entrada MIDI válida. Uma entrada 0 transposta para pitch válido é aceita.
- Enable/mute/filtros/transpose afetam somente novos onsets. Off usa identidade
  original no ownership, sem recalcular rota. Volume é ganho linear solicitado
  0..127, transportado como metadado, **não implementado como escala de velocity**.
- PerformanceRoute guarda no máximo duas saídas, volumes e patch IDs por saída.
  Sua NoteRoute mantém a identidade completa. Ready A/B exige confirmação do
  backend; configuração enabled não comprova instrumento alocado.
- PerformanceCommand valida operandos int32 antes de estreitar. Seleção, modo,
  split, zona, volume, transpose, enable, mute e patch operam sobre uma cópia
  transacional. IDs de patch são chaves uint16 opacas; disponibilidade deve ser
  validada pelo backend. Comando de patch não constitui ACK do AMY.
- PerformanceState é cópia por valor: configuração/modo, seleção, transporte,
  readiness, referências lógicas, slots físicos e recusas de alocação.
- PerformanceMailbox: capacidade um, exatamente um produtor e um consumidor,
  publicação release/acquire, sem referência mutável, heap, espera ou spin.
  Full retorna false e exige nova tentativa posterior com estado atualizado.
  Não suporta múltiplas UIs leitoras; cada frontend deve ter seu consumidor
  definido. Teste concorrente verifica 10.000 publicações coerentes.

Roteamento musical tem duas iterações; todas as operações são limitadas e sem
heap/I/O. Não altera filas, deadline, Panic, sustain ou reserva de releases.
Arp/seq/pads continuam seus caminhos anteriores; bateria não ganha fan-out
automático. O novo módulo não determina rotas de produção dessas fontes.

## C — integração dual AMY bloqueada

**Não implementada e não habilitada.** Nenhum instrumento B é alocado, nem no
boot nem durante performance. Volume/patch/mono/sustain independentes de B,
troca seletiva de patch, voice stealing dual e áudio simultâneo não estão
validados. O teste AMY real verifica que uma rota A+B é rejeitada inteira,
sem iniciar parcialmente A ou converter B para A. Não se ampliou a lista de
destinos aceitos no NoteOwnership para contornar o gate.

Auditoria executada com AMY real: 256 patches, custo máximo observado oito
oscs/voz, patch 0 seis, patch 128 oito e drums 32. Todos os instrumentos da
varredura tiveram propriedade completa de osciladores. Boot mantém 48+32=80,
40 livres. Orçamentos do teste variam conforme patch (10 ou oito vozes),
exatamente como a regressão anterior; não são promessa de polifonia universal.

`fitsPerformanceOscillators` usa custos fornecidos/observados, reserva 32 para
drums e rejeita valores desconhecidos ou mais de 32 vozes por parte. É apenas
condição aritmética necessária. A8 Juno+B8 DX7=144 e A8 DX7+B8 DX7=160 falham.
4+4 DX7+drums=96 passa a aritmética, mas ainda precisa ensaio de alocação
contígua, isolamento de patch/mono/FM/pedal e medição de margem real.
Não se conclui que todo par de patches seja impossível: o bloqueio é a ausência
de uma integração com orçamento menor e transação segura já demonstrados.

Não houve alteração em src/ nem necessidade de patch upstream. Efeitos
continuam compartilhados por bus. Não foi duplicado tanque de FX ou PCM.

## D — codec entregue; UI e armazenamento runtime pendentes

`performance_format.h`: envelope independente de 37 bytes, little-endian,
magic SMPF, versão 1, payload 25 bytes, CRC32 IEEE do payload. Serialização
campo a campo, bool estrito 0/1, transpose codificado como valor+24. Não depende
de padding ou layout nativo. Tamanho extra/truncado, versão/magic desconhecidos,
CRC incorreto e campos inválidos são rejeitados; saída anterior fica intacta.

`performanceForLegacyPatch` cria Single/A usando o engine_patch **depois** de
o loader existente validar/migrar seu patch. É helper disponível, não uma
migração já conectada ao StorageManager. Formatos patch v3/v4/v5/v6, scenes,
macros e perfis permanecem intactos; arquivos antigos não são regravados.

**Não implementado:** gravação/leitura de performances em Flash, jobs novos
do StorageWorker, troca de performance na aplicação e controles UI/console.
O teste de interrupção cobre payload truncado; não prova atomicidade de rename
ou recuperação de uma operação Flash real. Futuro I/O deverá usar StorageWorker,
save explícito/deferido, arquivo temporário e validação antes de publicação.
Não foi criado controle visual que anuncie sucesso para backend indisponível.
Perfis e resoluções existentes, MIDI Learn e mapeamentos estão preservados.

## E — validação verificável

Comandos executados da raiz, PowerShell, GCC/G++ existentes no PATH:

```powershell
./tests/run_smk_safety.ps1
./tests/run_smk_amy_boot.ps1
./tests/run_smk_patch_integrity.ps1
./tests/run_smk_synth_expansion.ps1
./tests/run_smk_ui.ps1
./tests/run_smk_performance.ps1
& C:/.platformio/penv/Scripts/python.exe -m platformio run -d smk-s3 -e esp32-s3-devkitc-1 -e esp32-s3-devkitc-1-128
git diff --check
```

As primeiras cinco suítes passaram no baseline. AMY real foi executada novamente
com os testes novos; performance passou na versão final. Safety inclui testes
de clock/timing musical/time bridge; não existe suíte timing separada para
esta invocação. Matriz CI IDF 5.3/5.5.2 preservada; novo script incluído no job host.

| Status | Resultado |
| --- | --- |
| PASS | Safety: MIDI/USB, overflow, Panic, áudio/I²S mock, clocks, timing, ownership e geradores da Fase 3A. |
| PASS | AMY real/owner: boot, 256 patches completos, drums, mono/legato, sustain, Panic, fontes concorrentes, PCM e barreiras. |
| PASS | AMY real novo: plano Single transposto produz sinal e libera a saída antiga; plano dual rejeitado atomicamente; custos reais de osciladores auditados. |
| PASS | Patch integrity: macros, FM/operadores, FX/freeze, CRC e migrações existentes. |
| PASS | Synth expansion e StorageWorker: presets/macros, snapshot, falha/fallback e I/O bloqueado. |
| PASS | UI nos perfis existentes. |
| PASS | Performance: Single/Layer/Split lógicos, overlap/gaps, todos os 16 canais, limites de velocity/transpose, descarte de saídas inválidas, readiness/mute/disable. |
| PASS | Comandos inválidos não alteram estado; snapshot sem alias e publicação concorrente de 10.000 estados. |
| PASS | Codec: round-trip/defaults/helper legado, vetor CRC conhecido, todos os 296 flips individuais de bits, todos os truncamentos, campos inválidos com CRC válido e estado anterior preservado. |
| PASS | Compilação dos headers novos para Xtensa S3, C++17, sem RTTI/exceções, com warnings como erros (probe abaixo). |
| PASS | Builds locais ESP32-S3 256/128, IDF 5.5.2. |
| NOT RUN | CI remota IDF 5.3/5.5.2, outras plataformas upstream, TSan. |
| BLOCKED | Áudio dual e ownership/sustain/patch/stealing entre A/B reais; dependem da integração C descrita acima. |
| NOT RUN | Persistência Flash de performance e controles UI/console novos: não implementados. |
| MANUAL | Áudio analógico, USB físico, VBUS, render sob carga, heap/PSRAM/stacks e soak. |

O teste novo começou com um warning de variável de cópia não lida, tratado como
erro; foi corrigido e a execução final passou. Avisos host upstream de cast de
ponteiros em diagnósticos AMY continuam preexistentes. Runtime AMY sem stderr
inesperado. PlatformIO avisou sobre codepage das métricas, mas terminou com
dois SUCCESS e exit code zero; tamanhos foram reportados normalmente.

Logs locais ignorados: build/phase3b_baseline_{safety,amy,patch,expansion,ui}.log,
build/phase3b_final_amy.log, build/phase3b_performance.log e
build/phase3b_firmware.log. Scripts verificam compilação e exit code de cada teste.
Probe local ignorado: build/smk_performance/target_probe.cpp inclui
performance_format.h e instancia decodePerformance/PerformanceMailbox.

```powershell
& C:/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-g++.exe -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -Werror -Ismk-s3/components/audio/include -c build/smk_performance/target_probe.cpp -o build/smk_performance/target_probe.o
```

| Status | Bloco | RAM estática | Flash | Duração |
| --- | ---: | ---: | ---: | ---: |
| PASS | 256 | 30.116 /327.680 bytes | 1.140.603 /16.777.216 bytes | 50,985 s |
| PASS | 128 | 30.116 /327.680 bytes | 1.140.623 /16.777.216 bytes | 49,885 s |

Tamanhos iguais aos registrados no HEAD da Fase 3A: o modelo não é instanciado
no firmware de produção. Não houve upload ou certificação de áudio. Perfil
local 240×240, CI/defaults 284×76, sample rate 48 kHz, toolchain Xtensa 14.2.0,
plataforma pioarduino 55.03.36-1. Não houve alteração de sdkconfig ou dependências.

Host sizeof: PerformanceConfig=26, PerformanceState=40, PerformanceMailbox=44,
PerformanceRoute=40 bytes; envelope persistido=37. Esses números não medem
heap/PSRAM do target, CPU dual ou consumo DSP. Referências por layer são campos
de snapshot preparados para backend futuro, não telemetria já coletada no S3.
Contadores atuais de render, deadline, starvation, I²S e heap permanecem os
existentes. Não alegar melhoria de latência ou carga.

## Critérios de aceitação e pendências

| Critério do pedido | Situação |
| --- | --- |
| 1 Single preservado | PASS regressões e builds; novo modelo ainda não conectado ao runtime. |
| 2 Áudio A/B simultâneo | BLOCKED — C pendente. |
| 3 Split/zonas | PASS no modelo; NOT RUN no runtime. |
| 4 Split/transpose sem notas presas | PASS no modelo/A com AMY real; BLOCKED para duas layers reais. |
| 5 Sustain/Panic/Stop | PASS regressão existente; BLOCKED para combinação dual. |
| 6 Bateria com rota própria | PASS regressão preservada. |
| 7 B sem alocação insegura | B permanece indisponível; habilitação não implementada. |
| 8 Limites documentados | PASS osciladores reais; MANUAL CPU/memória sob carga. |
| 9 Estado S3/P4 | PASS infraestrutura portátil; integração com frontends pendente. |
| 10 Comandos sem hardware/UI | PASS no módulo novo. |
| 11 Presets legados | PASS regressão existente; helper para futura migração disponível. |
| 12 Testes/builds | PASS local; NOT RUN matriz remota. |

A Fase 3B **não está concluída**. Não há touchscreen, novas engines, escalas,
acordes novos, tracks ou arpejadores adicionais. Não há funcionalidades dual
anunciadas como utilizáveis. O maior risco de integração futura é confundir
PerformanceConfig solicitado com estado confirmado do AMY, ou descartar os
metadados de ganho/patch de PerformanceRoute.

Hardware sem mudanças: revisão exata da DevKitC, VBUS/power switch, pinagem
final, straps/mute do PCM5102A e painel físico continuam conforme documentação
anterior, sem novas inferências. O smoke anterior identificou chip S3 rev v0.2,
Flash 16 MB/PSRAM 8 MB; isso não resolve a revisão da placa nem aprova seu wiring.

## Próximo menor passo e requisitos antes da Fase 3C

1. Qualificar Single na placa com o procedimento da Fase 3A: ambos os blocos,
   status/audio_status/memory após reset; teclado/pedal/pads, Stop, troca de
   patch, Panic/desconexão e reconexão, gravação analógica e soak de duas horas.
   Registrar render máximo/médio, deadlines, gaps/underruns, heap mínimo,
   maior bloco interno, PSRAM mínima e stacks. Não aceitar notas presas.
2. Implementar transação de configuração com ACK/rejeição sem reset global de
   outra parte; separar referências/mono/FM/pedal/bend por instrumento, e
   verificar alocação completa antes de publicar ready B. Não basta aceitar ID 2.
3. Ensaiar orçamento conservador, por exemplo quatro vozes por parte, sem
   tratá-lo como aprovado antecipadamente. Preservar drums/FX e medir fragmentação
   e prazo por combinação de patch. Pré-alocar fora da apresentação.
4. Só então ligar comandos/snapshots ao controle/UI e StorageWorker; testar
   duas layers reais, mudanças com teclas mantidas, patch A com B soando,
   sustain seletivo, stealing, Stop e desconexão. Rejeitar performance sem
   recursos mantendo estado efetivo coerente. Completar C/D/E antes da Fase 3C.

## Commits

- A: `0da65db5` — auditoria e gates de segurança.
- B: `457ce599` — modelo, planos de rota e mailbox portáteis.
- D parcial: `f8a373cd` — codec versionado independente (sem Flash/UI runtime).
- E: `fcdb501f` — testes de performance e gate com AMY real.
- Relatório final: commit `docs(smk): record partial phase3b validation and remaining gates`.

Nenhum commit C de integração dual foi criado; seu trabalho está explicitamente
pendente. Todos os commits permanecem locais nesta branch.
