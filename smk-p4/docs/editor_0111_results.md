# 0.11.1 investigation — incomplete

Baseline: dd491f039561771385b2672c86ca3a315adca8bf. This is not a 0.11.1 release report and no new synthesis parameter is qualified.

## Findings

* `UiBlueprintPerformanceAdapter::editIndex` exposes cutoff, resonance, attack, release, FM depth, operator level and ratio. The owner additionally checks the patch's materialized eight-voice map. The registry alone is not the runtime authority.
* Registry `amp.vel_sens`, `lfo.1.rate_hz` and `lfo.1.depth` claim Available but the adapter has no matching edit command. Modulation slot source and destination also claim Available in the registry although the matrix only changes `UiModel`.
* The live editor's MOD widget previously marked the first mod-wheel-to-pitch row Available based solely on model values and accepted model-only changes. The fix disables those buttons for the real editor and labels the routes Planned; the independent CC1 vibrato is unchanged. The demo backend retains its prior behavior.
* User preset v1 uses fixed 128-byte records, 17 float positions, dual keys, CRC and ROM discriminator. Existing 24 KiB NVS admission assumes seven entries per blob and a 126-entry GC reserve; actual free capacity has not been measured. Extending `Count` without changing this encoding overlaps its ROM and CRC fields, so no new persistent parameters should be enabled before a separately qualified codec and non-destructive storage strategy exist.
* The current Juno capture recognizes exactly three breakpoints but checks envelope topology on the first voice only. New ADSR controls require validating all voice breakpoints and native restoration, not merely adding a slider.
* Prior hardware receipt records a 0.11 image written to the identified board and successful readback, not the current device state. Existing rollback artifacts are in the 0.11 worktree's `build/editor011/flash`; their continued presence and hashes have not been verified in this investigation.

## Validation and remaining work

Baseline `run_harmony.ps1` and `run_sound_editor.ps1` passed in the preceding session. These cover existing controls, not the 0.11.1 objectives. A full inventory, P0/P1 integration, versioned persistence, functional tests, target builds, current backup and physical candidate are outstanding. No NVS, firmware, AMY core or hardware driver was modified. P4 hardware and 800x480 request takes precedence for this task over the older S3-specific specification; no S3 behavior is changed.

Current change validation: PASS direct compilation of `modulation_matrix.cpp` using g++ C++20, warnings as errors and the pinned LVGL headers. This is not a widget interaction test. Full preview and on-device interaction have not been qualified.
PASS `./smk-p4/ui-preview/run_preview.ps1` and `./smk-p4/tests/run_sound_editor_ui.ps1`: existing preview and live-editor host suites. They do not specifically assert the disabled MOD buttons or constitute physical validation.

## Incremento em RAM — Decay e Sustain Juno (0.11.1 incompleta)

Dois IDs novos comandam breakpoints nativos do primeiro oscilador de amplitude
compatível, após verificar a topologia nas oito vozes. Os demais envelopes são
preservados; em Moving Strings o segundo oscilador possui envelope diferente.
A disponibilidade depende da topologia e faixa capturada. Compare e Revert
restauram os valores nativos; a restauração do tempo de decay usa amostras,
sem arredondamento por milissegundos. A UI apresenta o snapshot confirmado.

O codec v1 foi limitado explicitamente aos 17 campos originais; rejeita Save
quando o mask contém os novos IDs. Não há persistência v2, migração nem Recall
desses parâmetros. Portanto o Gate A e a versão 0.11.1 permanecem incompletos.
Não houve alteração das partições ou da NVS. O pedido explícito para P4 tem
precedência para este incremento sobre o alvo S3 mais antigo em specs.md;
nenhum caminho S3 ou núcleo AMY foi alterado.

Testes host com AMY real: patches 4, 0, 2, 3 e 31 tiveram PCM alterado para
ambos os parâmetros em uma e oito vozes, incluindo sustain, retrigger e release;
contagem de clipping foi zero nos cenários executados. IDs 1 e 8 foram
indisponíveis por topologia/faixa. Compare de Decay e Revert de Sustain em ID 4
restauraram PCM de nota nova byte a byte; o teste de estado verifica oito vozes,
preservação de outro envelope e ausência de alocações no caminho observado.
Isso não comprova todas as topologias, deadlines no P4 ou operação física.
Os gates B–H e o inventário completo continuam pendentes.

Validação deste incremento:

| Verificação | Resultado |
|---|---|
| `./smk-p4/tests/run_sound_editor.ps1` | PASS — AMY/PCM, codec v1, Harmony, USB e catálogo host; testes não qualificam hardware |
| `./smk-p4/tests/run_sound_editor_ui.ps1` | PASS — suíte LVGL existente; não cobre exaustivamente os novos sliders |
| Novos parâmetros com persistência v2 | 0; NOT RUN — v2 não implementado |
| Migração v1 → v2 | NOT RUN |
| Smoke físico | MANUAL / NOT RUN; firmware não gravado |

## Codec v2 isolado — persistência ainda desabilitada

O codec experimental usa 160 bytes: cabeçalho e nome preservam os campos v1;
pares ordenados ID/float codificam somente bits presentes (até 19 pares), ROM
em 152 e CRC32 em 156. O decoder rejeita IDs fora de ordem, versão, ROM, CRC,
valores e máscaras inválidos. Sem bit de override, o valor novo é neutro no
sentido de deixar o patch de fábrica intacto. A conversão host v1 → v2 preserva
nome, máscara e valores antigos. A codificação tem tamanho fixo, não depende de
padding e não substitui as chaves v1.

Este codec NÃO está conectado ao PresetStore nem à NVS. Para 16 slots com duas
cópias v2 seriam 5120 bytes de payload adicionais, além das cópias v1 e overhead
NVS; não há comprovação de capacidade/GC na partição de 24 KiB. Permanecem
necessários chaves separadas, seleção de geração, recuperação após interrupção,
admissão de espaço, testes de reinício e rollback. Save dos novos parâmetros
continua rejeitado; nenhuma migração ou escrita real ocorreu.

## Seleção de cópias v2 em host

`VersionedPresetStore` adiciona dois bancos v2 independentes dos bancos v1.
O load escolhe a maior sequência válida, preferindo v1 em empate; save escreve
somente v2, alterna bancos e verifica todos os bytes após a gravação. Em teste
host, importação v1 preserva a cópia original; interrupção, falta de espaço
simulada e corrupção em uma ou ambas as cópias recuperam a geração válida.
A classe ainda não é usada por NVS nem pela interface: a API booleana atual não
separa ausência de falha de leitura, e a admissão dos bancos adicionais e do GC
não foi comprovada. Logo não se admite escrita real com ela. Teste host não
comprova recuperação após perda de energia física.

## Leitura segura dos bancos v2

A interface experimental agora diferencia `Present`, `Missing` e `Error`.
A seleção examina os dois bancos de cada versão em uma única passagem; qualquer
erro de leitura aborta load e save antes de escrever. Um registro presente mas
inválido pelo decoder é ignorado e pode ser substituído em um banco v2. O teste
host injeta erro de leitura e verifica que nenhuma cópia é alterada. Readback
continua comparando todos os bytes. A API ainda não está ligada à NVS: além da
admissão de capacidade, falta mapear seus erros e tratar versões futuras sem
sobrescrever dados desconhecidos. Save de Decay/Sustain permanece desabilitado.

## Adaptador de leitura NVS v2

Foi adicionado `NvsVersionedPresets`, isolado do firmware de produção. Ele usa
as chaves v1 existentes `sNN[a|b]` e chaves v2 separadas `vNN[a|b]`, mapeia
`ESP_ERR_NVS_NOT_FOUND` para ausência e demais falhas para erro, e não apaga
ou grava dados. `write` e `writeV2` retornam falha deliberadamente. O teste host
usa uma API NVS simulada para conferir nomes, tamanho, ausência, erro, handle
inválido e bloqueio de escrita; não é teste de Flash nem de queda de energia.
A conexão do adaptador ao backend e a habilitação de Save dependem de política
de capacidade/GC e testes de recuperação. O Gate F continua pendente.


## Filtro Juno em RAM

`filter.env_amount` e `filter.keytrack` alteram respectivamente os coeficientes
nativos EG0 e NOTE de filtro do AMY, nas oito vozes. Os coeficientes restantes
s�o preservados, e Compare/Revert restauram os valores nativos capturados. A
disponibilidade depende de filtro e faixa compat�vel. Os IDs novos n�o s�o
persistentes: o encoder v1 os rejeita, e o codec v2 ainda n�o os admite.

O teste host com AMY real no patch 4 verificou altera��o PCM nos dois controles
com uma e oito vozes, zero clipping nos cen�rios testados e valores nativos nas
oito vozes. Ainda faltam C2/C4/C6, Harmony para esses controles, cobertura de
outras topologias e Save/Recall. N�o h� qualifica��o de hardware ou deadlines.
Gate B permanece incompleto. O n�cleo AMY n�o foi alterado.
