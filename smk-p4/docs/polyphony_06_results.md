# SMK-P4 0.6 — resultados finais de software e bancada

2026-11-05. Branch `feature/smk-p4-polyphony-06`, base0.5 `dd4753af`,
worktree separado. **PASS software 4/6/8; bancada 6→8 após baseline0.5/quatro já qualificado.**
Oito vozes estão instaladas na Guition, com autorização própria e hash verificado.
Qualificação8: pelo menos 1874,48 s contínuos,
oito notas nos patches128/135, quatro hot-plugs,39 transições, audição aprovada,
zero deadlines/clipping/OOM/drops. Render máximo de 4234 us (79,4% do período);
heap interno mínimo de 169075 B. Seis também foi qualificado e permanece como rollback.
Recomendação musical nesta montagem/configuração congelada: oito; seis oferece
maior margem observada. Default genérico seguro4 preservado.
[Relatório físico8 e limites](polyphony_06_eight_hardware_qualification.md),
[relatório físico6](polyphony_06_hardware_qualification.md),
[candidato/hash/autorizações8](polyphony_06_eight_bench_candidate.md).
Os checkpoints datados ao final registram estados anteriores e não substituem
esta conclusão atual. Rollback6/0.5 e backup integral foram preservados.

O pedido explícito P4/Guition/800×480 prevalece sobre specs.md/AGENTS gerais
S3/284×76. Checkout principal, branches anteriores, backups, configurações físicas
e bundles validados não foram alterados. Sem reset Git, rebase ou force-push.

## Implementado e impacto de arquitetura

- Kconfig/ambientes estáticos4/6/8; host usa SMKP4_VOICES. Sem mudança dinâmica.
- Dimensionamento único de instrumentos, vozes AMY, osciladores e reserva delta;
  limites de compilação e verificação real dos mapas/instrumentos. Sem stealing.
- Dois bancos ROM128/135 mantidos pré-carregados; somente um musicalmente ativo.
  Os slots livres ainda podem ter cauda DSP, reutilizada com fase contínua.
- Pré-alocação do pior caso das duas filas no boot, com falha checked e teardown.
  Sem alocação/locks/logs novos no serviço. I²S/DMA/USB não foram modificados.
- Admissão verifica total livre e maior bloco separadamente, preserva margem
  interna pós-boot em6/8 e falha sem fallback silencioso nem engine ready.
- Contrato mínimo PerformanceBackend::voiceCapacity atualiza VX:n/N desde o
  primeiro frame. Navegação/controles Blueprint1.1 preservados; sem recursos novos.
- Core AMY: duas condições de fase ampliadas sob AMY_EXCLUSIVE_FM_PHASE_RESTART,
  habilitado somente no P4 e harnesses correspondentes. Fontes privadas dos dois
  patches foram auditadas; outras plataformas mantêm a política0.5. Nenhuma
  mudança de API AMY, envelopes, ganho, normalização, patches ou filtros.

|Vozes musicais|Instrumentos/vozes AMY (2 bancos)|Osciladores|max_synths (ID0 reservado)|Deltas reservados/blocos|
|---|---|---|---|---|
|4|8|64|9|1944/1|
|6|12|96|13|2536/2|
|8|16|128|17|3128/2|

## Gates e comportamento musical

|Gate|Resultado|
|---|---|
|0 — Audio0.5A|PASS software A/B independente/matriz A–H; audição atual MANUAL/PASS (não A/B físico controlado)|
|A — quatro|PASS host e build; PCM comum idêntico à0.5A|
|B — seis|PASS software/build e bancada>=30min, seis notas e audição|
|C — oito|PASS software/build e bancada>=30min, oito notas e audição; escopo congelado|
|D — interface|PASS host/adaptadores e Guition; VX8/8,39 transições e memória estável observada|
|E — hardware|PASS seis e oito na Guition; audição MANUAL/PASS, limites no relatório físico|

Seis/oito notas reais foram admitidas em instrumentos DSP distintos, com alturas
verificadas no engine. A sétima/nona foi rejeitada sem alterar gates existentes.
PASS: release em ordem inversa, caudas/novos acordes, velocity, sustain por canal,
repetição sob sustain, bend/CC1, USB/Touch de mesma altura nas duas ordens,
disconnect/reconnect lógico com Touch preservado, Panic ativo/release, patch
ativo, oitava/pressões/lease10s e filas saturadas. Drops/cancelamentos esperados
nos testes negativos são distintos de perdas MIDI inesperadas. Não é USB físico.

## Consolidação0.5A e falhas encontradas

Ver [verificação0.5A](audio_quality_05a_verification.md). O runner antigo
vinculava ambos runtimes ao mesmo core e exigia WAV idêntico. Agora compila
cd9ed0ba e581570c6 em árvores/objetos independentes, mesmo harness/configuração,
registra SHA completo, flags e hashes do harness/WAV. A/B mais recente:
`build/retrigger_ab/8862ec3a00d04a0daa9ee8685b00bc63`.

PASS: onset274→16 no128 e1087→225 no135; primeiro ataque/release byte-idêntico,
A/B/H idênticos na matriz isolada, C/E/F com redução, clipping0. A/B aguardam
15000 blocos de release e verificam bloco totalmente silencioso; B também prova
operadores com last_amp residual. A0.5 alterava ataques após silêncio porque o
pai parava de renderizar operadores/fontes; a extensão exclusiva restaura esse
ataque sem reiniciar fase durante caudas audíveis. Relatos contraditórios de
imagem/hash/autorização0.5 foram anotados sem apagar histórico.

FAIL preservado:8 vozes com32 CC1 de um canal cresciam o pool durante o serviço.
A reserva calculada eliminou isso em4/6/8, inclusive32 Touch retriggers+32 CC1
simultâneos, velocity/master127,16 acordes rápidos e todos os testes anteriores.
PASS: OOM injetado no bloco adicional6/8, sem ready/render parcial, sem leak e
com retry normal. Quatro manteve o pool e os bytes de heap anteriores.

## Memória

|Vozes|Runtime estático host|Heap AMY vivo/pico host|Alocações vivas|
|---|---|---|---|
|4|3212 B|116618/116749 B|161|
|6|3244 B|185890/186023 B|206|
|8|3276 B|206010/206143 B|250|

ABI P4 medida com comando real do compilador: delta20 B, synthinfo356 B,
mod_synthinfo52 B. Host delta24 B; não converter seus totais em heap físico.
Bloco extra é40960 B no P4,49152 B no host. Ver [orçamento](polyphony_06_memory_budget.md).

|Vozes|DRAM data+BSS ELF (B)|IRAM ELF (B)|firmware.bin (B)|SHA256 firmware.bin seguro|
|---|---|---|---|---|
|4|169484|96768|1108336|dfb7a3bf88cafdd9e64c8d8c80aa5bbdf0ad0fe566a6df5f63a695325aacbaff|
|6|169504|96768|1108528|b9d8cacc61914e14166178bda19e825803ef8e8032871059719a02158dcf8a55|
|8|169548|96768|1108512|9ee48f7c7320e949509ab0103b6d12e68d86c7a1614441ab628b350ba6ff738a|

DRAM soma os bancos0/1. O resumo PlatformIO omite DRAM1 neste alvo e não deve
ser apresentado como RAM estática total. DRAM1 inclui work_mem_int de131072 B
(LVGL). Variações de alinhamento/link não são otimização. IRAM permanece96768 B;
RTC/TCM/padding constam nas seções do manifesto. Esses números não medem heap.
Heap interno/maior bloco/PSRAM livre/stacks físicos atuais: MANUAL/NOT RUN.
Nenhuma nova alocação PSRAM por voz foi solicitada pelo adapter.

## DSP e áudio

48kHz/PCM16estéreo/256frames; orçamento5333,33 us, DMA inalterado. Medições
host finais, sem builds Codex concorrentes:

|Vozes|Render cheio128 média/máximo(us)|Render cheio135 média/máximo(us)|Pior render(us)|Pior serviço apply+render(us)|
|---|---|---|---|---|
|4|31/50|29/45|99|1703|
|6|43/87|44/52|172|2849|
|8|57/78|59/72|258|4223|

Não são medidas P4. Serviço8 atingiu4223 us no host com ambas filas cheias:
não aprovar8 por render médio baixo. Amostras sob compilação concorrente chegaram
a10558 us de render e estão preservadas; não atribuir redução à otimização.
Render/compute deadlines, serviço/DMA/refill, write/timeouts, latência física,
CPUload, MIDI físico e margem operacional P4: MANUAL. O máximo histórico0.4
2113 us/zero deadlines em>30min não aprova estes perfis. Ver [matriz](polyphony_06_performance_matrix.md).

WAVs determinísticos comuns4 notas são byte-idênticos entre4/6/8, inclusive
uma voz, sem normalização por polifonia. Picos comuns128/135:2850/1518 PCM;
RMS176,535/153,018. WAVs de capacidade total também foram gerados (notas48+3*i):
128 pico/RMS:4=2102/187,484;6=2733/209,487;8=3123/225,560.
135:4=1633/157,938;6=2296/189,353;8=2672/210,870.
Clipping bruto do runtime0 nos testes positivos; OOM de serviço0; heap do
serviço0. SHA256 por WAV e dados por cena em build/polyphony_06_summary.json.

## Validação executada

Toolchain: PlatformIO6.1.18, pioarduino55.03.36-1, IDF5.5.2, LVGL9.3.0,
GCC14.2.0; PowerShell7.6.9 para testes S3 com stderr esperado. Sem upgrades.
USB S3 preservado em1.4.1 via lock existente. Comandos a partir deste worktree:

|Status|Comando/teste|
|---|---|
|PASS|pwsh -NoProfile -File smk-p4/tests/run_retrigger_probe.ps1|
|PASS|powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_amy.ps1 -Voices 4 (também6/8)|
|PASS|polyphony.exe4/6/8 recompilado após hasBootMemory; OOM, admissão e carga máxima|
|PASS|polyphony.exe4/6/8 sem builds concorrentes; logs profileN_capacity_quiet.log|
|PASS|python smk-p4/tests/summarize_polyphony.py --log-suffix tests_final --probe-suffix capacity_quiet|
|PASS|powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_host.ps1 (23 Python + C++)|
|PASS|powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_usb.ps1|
|PASS|powershell -ExecutionPolicy Bypass -File tests/run_smk_safety.ps1|
|PASS|powershell -ExecutionPolicy Bypass -File tests/run_smk_ui.ps1 e tests/run_smk_link.ps1|
|PASS|powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_preview.ps1 (20 capturas,700 telas)|
|PASS|pwsh -NoProfile -File smk-p4/tests/run_blueprint_adapter.ps1 -Voices 4 (também6/8;1400 telas/execução,sem crescimento na segunda janela)|
|PASS|pwsh -NoProfile -File tests/run_smk_amy_boot.ps1 -Mutations|
|PASS|pwsh -NoProfile -File tests/run_smk_amy_boot.ps1 -Legacy (diagnóstico negativo esperado)|
|PASS|powershell -ExecutionPolicy Bypass -File tests/run_smk_amy_boot.ps1 -ResourcesOnly -BlockSize 128|
|PASS|C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone-blueprint -e guition-standalone-blueprint-6 -e guition-standalone-blueprint-8 -j 8|
|PASS|python smk-p4/tests/check_blueprint_build.py --voices N (4/6/8), header/flags reais/gates desligados|
|PASS|pio run -d smk-p4 -e guition-standalone -j 8; python smk-p4/tests/check_standalone_build.py (paths enabled compilados/linkados separadamente)|
|PASS|pio run -d smk-s3 -e esp32-s3-link -j 8|
|PASS|riscv32-esp-elf-size -A e nm -S: ABI/seções/símbolos dos ELFs reais|
|NOT RUN|Demais ambientes S3/P4 híbridos e suíte Python AMY upstream completa|
|PASS + MANUAL/PASS|Guition6/8, PCM5102A/SMK25 reais, audição e janela>=30min em cada perfil|

Logs finais: profileN_tests_final.log, profileN_capacity_admission.log,
profileN_capacity_quiet.log, polyphony_final_uptodate.log,
standalone_legacy_firmware.log, s3_link_firmware_final.log e adapters finais.

Falhas de infraestrutura preservadas: build6/8 simultâneos disputaram
managed_components/LVGL (WinError32/arquivo ausente); retry sequencial PASS.
Adapter6 falhou enquanto lvgl.h era reparado, depois PASS. Um cache PlatformIO
ignorou alteração de CMake do componente: auditoria recusou flag ausente;
reconfiguração forçada via timestamp do CMakeLists raiz e auditoria efetiva
resolveram. PowerShell5 tratou stderr negativo esperado dos testes S3 como
NativeCommandError; execução em pwsh, como no CI, PASS. Não foram falhas DSP
ocultadas nem justificativa para promover testes físicos.

## Checkpoint do pacote seguro (anterior à bancada)

O bundle local de revisão contém3 imagens seguras, SDKs/flags/layout nativos,
WAVs A/B e4/6/8, logs, patch e manifesto SHA256. É criado por
python smk-p4/tests/package_polyphony_review.py, sem sobrescrever bundles;
confere cópias/hashes e integridade ZIP. **I²S e USB permanecem desligados.**
Não é a imagem audível aprovada para bancada nem autorização de gravação.

Recomendação: manter4 como default até qualificação; preparar imagem audível6
após reconfirmação da mesma montagem, então identificar unidade/porta/imagem
instalada, preservar backup, verificar hash/configuração concreta e obter
autorização explícita para gravar. Qualificar6 antes de8; repetir piores cargas,
audição e>=30min. Não reduzir reservas nem promover8 apenas por build/host.
Montagem reconfirmada pelo operador: Guition JC-ESP32P4-M3, BCLK50/WS51/DATA52,
USB HS controlador0, VBUS/proteções sem alterações. Preparação do candidato
audível6 em ambiente separado; nenhuma autorização de gravação inferida.

Riscos: margem DSP/filas e heap físico ainda desconhecidos; amy_start conserva
alocações iniciais sem NULL check. A guarda é conservadora, não prova formal
contra fragmentação/OOM. OOM injetado cobre a nova expansão checked, não todo o
core legado. MIDI/display/USB físicos com estas imagens não foram exercitados.

Rollback: perfil4 recompilável; branch/worktree0.5 intactos. Rollback físico
somente bundle/backup anteriormente validado, hash e layout confirmados e
autorização para aquela imagem. Firmware.bin não é backup integral. Não foi
substituída a imagem instalada; no checkpoint posterior ela foi identificada
por leitura como328d510e…4c5e997 (0.5).

## Commits de implementação

Código funcional consolidado em94cfcae0; mudanças posteriores de relatório,
harness/empacotamento não alteram o caminho musical. SHA HEAD completo consta
no manifesto do bundle. Commits desde a base:

- `d8cd25b3` fix(p4): independently verify retriggers and restore silent FM attacks
- `3fa65331` fix(amy): restart frozen modulation phase with a silent owner
- `25355c55` feat(p4): size static voice profiles and verify real engine capacity
- `aad319ab` test(p4): qualify six-voice software admission and record memory evidence
- `9bd8579c` feat(p4): add experimental eight-voice profile after six-voice host qualification
- `99499aef` fix(p4): preallocate bounded event pressure before expanded audio service
- `3067137e` fix(p4): show compiled capacity before the first UI snapshot
- `581570c6` fix(amy): scope silent-owner phase restart to exclusive FM platforms
- `7d7f5fa2` test(p4): pin final exclusive-FM A/B provenance and harness hashes
- `94cfcae0` fix(p4): admit aggregate engine memory separately from contiguous reserve

## Candidato audível6 — checkpoint posterior à confirmação

Operador confirmou mesma montagem e autorizou build/testes. PASS build
`guition-standalone-blueprint-6-bench`, auditoria --voices6/--musical e integridade
esptool. SHA25641c12553a95136e7f9dbf45bbc1dbf199d119c23489294cd2aad435c62a5b3b8.
Ainda sem autorização para gravar esta imagem concreta. Preparação e backup
estão no [registro de bancada](polyphony_06_bench_candidate.md).
Os bundles seguros anteriores permanecem congelados; os diretórios .pio
anteriores foram invalidados pelo PlatformIO ao mudar platformio.ini. A tentativa
de repetir o checker nesses caches ausentes foi BLOCKED por sdkconfig.h ausente;
não altera os PASS nem hashes dos artefatos seguros preservados.


## Qualificação física6 concluída — 2026-11-05

PASS: janela contínua >30min, seis notas nos dois patches, quatro hot-plugs,
Touch/USB, 30 mudanças de tela e audição aprovada pelo operador. Render máximo
2736us, deadlines/clipping/OOM/drops0, heap interno mínimo188415B.
Detalhes e limites: [qualificação física](polyphony_06_hardware_qualification.md).
O checkpoint pré-autorização acima é histórico. Oito ainda não foi gravado.


## Oito audível preparado — 2026-11-05

PASS build/auditoria/integridade do candidato8, SHA887cee5d…ae5b7af7.
Ainda não gravado; seis continua instalado e validado.
[Imagem, rollback e proposta concreta](polyphony_06_eight_bench_candidate.md).
