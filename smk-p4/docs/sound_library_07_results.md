# SMK-P4 0.7 — biblioteca FM inicial e candidato de qualificação

2026-11-05. Branch `feature/smk-p4-sound-library-07`, worktree independente,
base `725ffb085e0bd7d99049eb049cd322486526e45c` da polyphony0.6.
**Entrega incremental: quatro timbres FM reais em perfil opt-in de oito vozes.
Expansão Juno e Gate E não concluídos. Gravação autorizada posteriormente; startup físico da revisão PASS,
primeira janela com overflow foi rejeitada e a revisão USB64 aguarda novo smoke.**

## Escopo, baseline e conflito de especificação

O pedido explícito Guition JC-ESP32P4-M3/800×480 prevalece sobre specs.md e
AGENTS.md gerais S3/284×76. Nenhuma alteração do projeto S3, core AMY compartilhado,
drivers I²S/USB/display/touch, efeitos, ganho, normalização ou dependências.
IDF5.5.2/pioarduino55.03.36-1/LVGL9.3.0 mantidos. Não houve reset Git, merge,
force-push ou sobrescrita dos bundles físicos4/6/8/backups.

Baseline host8 executado antes das mudanças: PASS. A evidência física0.6 de
4234us/render,169075B interno,94208B maior bloco e zero deadlines aplica-se
somente ao binário0.6 congelado. A imagem0.6 de oito vozes permanece preservada para rollback;
a revisão0.7 já tem medidas de startup, sem qualificação musical completa. Os ambientes antigos
permanecem com dois bancos e configuração elétrica anterior; default genérico4
preservado. Os candidatos novos têm nomes/SDKs/diretórios próprios.

## Inventário e seleção

[Inventário gerado](patch_library_inventory_07.md):391 posições,266 comandos,
125 lacunas259–383; não são391 timbres carregáveis. Classificação por comandos
reais:129 subtrativos,128 FM,8 PCM/drum mappings,1 partials. O piano256 requer
partials desligados no perfil atual; drums incluem configuração/MIDI mappings
fora do contrato local. Patch17 é subtrativo, não PCM; apresentação corrigida.

Metadados de candidatos centralizam ID/nome/família/categoria/custo; runtime
mantém a única lista de bancos suportados/residentes. UI conserva16 entradas
curadas e seus favoritos, mas disponibilidade live vem do backend. O inventário
de fábrica completo é documentação gerada, não uma lista de391 seleções da UI.

|ID|Som|Situação0.7|
|---|---|---|
|128|DX7 Brass1|Residente no candidato; baseline físico0.6 preservado|
|135|DX7 Piano1|Residente no candidato; baseline físico0.6 preservado|
|138|DX7 E.Piano1|Novo residente; PASS host, físico MANUAL/NOT RUN|
|142|DX7 Bass1|Novo residente; PASS host, físico MANUAL/NOT RUN|
|4,8|Moving Strings / OrganI|Render seco testado; PLANNED, não selecionáveis|
|31|Synth BassI|Render seco testado; PLANNED, não selecionável|
|32|Juno LeadI|FAIL clipping seco8 notas (144 amostras); não selecionável|
|0,1,12,14|Alternativas Juno|Render seco testado; PLANNED, não selecionáveis|

A primeira seleção acrescenta piano elétrico e baixo ao piano/brass reais.
Strings/lead/órgão/pads não foram promovidos por mero ID válido ou nota isolada.
Não se afirma cumprir a diversidade musical completa solicitada nesta entrega.

## Decisão de bancos e memória

Escolhida A, pré-carregamento fixo, por medição e inspeção. Dois bancos FM8:
16 instrumentos/128 osciladores; quatro bancos:32/256, max_synths33 incluindo ID0.
Só oito gates musicais simultâneos; caudas dos demais bancos são silenciadas
na troca. Não há cache, crossfade, redução dinâmica de vozes ou carregamento
no serviço. Vozes/FM usam8 osciladores:ALGO+6 operadores+LFO privados.

|Medição host x86-64|2 bancos FM8|4 bancos FM8|
|---|---:|---:|
|Heap AMY vivo com reserva do serviço|206010B|286490B|
|Delta de heap|referência|+80480B|
|Alocações vivas|250|426|
|Runtime estático (USB64 na revisão atual)|3276B|4044B|
|Deltas reservados/blocos|3128/2|3384/2|

Banco FM8 materializado acrescentou38976B/88 alocações; banco Juno8,
29952B/72 alocações, nesta ABI. São custos incrementais medidos, distintos do
heap total de um engine isolado. Raw boot quatro FM+Juno4/8:299194B antes da
reserva extra do serviço, contra237338B com quatro FM. Ver
[matriz e tempos medidos](sound_library_07_benchmark.md) e JSON do bundle.

Arquitetura B substituiu bancos FM de custo igual e fez24 alocações por troca
(oito instrumentos ×3), mesmo reutilizando capacidade. instrument_prepare aloca
instrumento/FIFOs; materialização faz parsing e não é transação completa após
admissão. Não foi implementada no owner de áudio. A carga host de centenas de
microssegundos não prova latência segura na placa nem rollback completo.

No P4, o ABI anterior mediu synthinfo356B/mod52B/delta20B. A primeira imagem
somava uma estimativa adicional de512B/oscilador ao piso histórico e exigia
401408B. A placa a recusou antes de iniciar AMY (FAIL preservado). A referência
histórica de411199B não representava o heap já com a stack do owner alocada.

A revisão usa orçamento do engine completo de300KiB: pico rastreado host286635B,
ABI maior e>20KiB de margem para bookkeeping. O guard é o máximo entre o piso
histórico e esse orçamento +65536B de reserva, total372736B neste perfil.
Piso original335872B, maior bloco inicial196608B, stack12288B e verificações
após engine (livre>=65536B/maior bloco>=32768B) permanecem. Não há redução de vozes,
retirada de guardas ou fallback silencioso. A nova telemetria captura heap antes/
depois de begin no owner, publicando o log somente na tarefa principal.

PASS boot físico da revisão: antes388259B/maior245760B; após begin111027B/maior
38912B. Delta observado do engine277232B, abaixo do orçamento307200B. Depois
I²S/UI e USB ainda ausente:95479B livres, maior31744B. Esse maior bloco já está
abaixo de32KiB após a UI, embora a guarda original nesse ponto seja aplicada
antes de I²S/USB/UI. Essa margem e hot-plug exigem qualificação explícita; não
rotular o resultado como memória ampla. Detalhes e hashes no relatório físico.

Seis bancos (4FM+2Juno) teriam352 osciladores/48 instrumentos. O host raw já
mediu299194B antes da reserva extra do serviço (348346B se somado o segundo
pool de49152B); ultrapassa o orçamento inteiro300KiB aprovado para quatro FM.
Não se conclui que Juno seja impossível no P4; falta demonstrar margem com USB,
LVGL e serviço, além de isolar comandos globais de EQ/chorus dos patches Juno.
A guarda não foi reduzida para encaixá-los. Lead32 também clipou no AMY bruto,
antes do master local; reduzir apenas o master não corrige esse clipping interno.

## Segurança musical e integração

begin valida patch real, número de vozes, custo e mapa exclusivo dos osciladores,
aquece DSP e pré-aloca a pressão das filas antes de ready. Qualquer falha
verificada encerra o engine; nenhum instrumento parcial fica disponível.
O teste de OOM cobre expansão checked de deltas e teardown/retry, não todas
as alocações legadas de amy_start. Sem novas alocações/locks/logs no serviço. Áudio mantém48kHz/PCM16estéreo,
256frames,4 descritores DMA e timeout de escrita20ms do driver existente.

setPatch valida residência antes de cancelar notas. A seleção enfileirada
continua REQUESTED; só snapshot novo do owner altera nome/número/categoria.
PatchInfo.available é falso até owner ready. Falha de enqueue mantém patch
confirmado e relata rejeição (notas canceladas por segurança). Sem confirmação
por2s, informa PATCH NOT CONFIRMED e conserva o estado confirmado. Rejeições de
recursos desconhecidos não alteram a nota ativa nem a geração do backend.

Troca residente preserva política0.6: silencia gates/releases/sustain/bend/CC1,
invalida USB pendente e conserva segurança de tokens Touch. Não há reprogramação
de instrumentos. Panic/overflow/leases/fontes/canais continuam nos caminhos
existentes. O teste cobre queued USB anterior à troca, gerações antigas,
mesma altura Touch/USB e falha de seleção com fila saturada.

Browser/inspector/badges/filtros usam disponibilidade real; planejados ficam
inativos na Guition. Prev/Next percorre só128/135/138/142. Metadados e título/
subtítulo atualizam após confirmação, preservando alvos, navegação, paginação,
favoritos e preview demonstrativo. Famílias/categorias são enums compartilhados,
sem incluir API AMY no componente UI. Capturas host800×480 conferidas visualmente:
quatro linhas disponíveis e E.Piano138 ativo, sem truncamento de nomes.
Macros e demais recursos continuam PLAN/sem DSP.

## Áudio, capacidade e testes por timbre

Os quatro residentes passaram os cenários reais do engine com oito vozes:
Note On/Off, velocity0, sustain por canal, velocity, bend/CC1, retrigger,
caudas sobrepostas, acordes rápidos, limite/nona nota, USB/Touch, desconexão
lógica/reconexão, fila dupla cheia, lease, seleção e Panic. Pico/RMS/clipping,
release completo e silêncio após Panic foram verificados; malloc/calloc/realloc/
free instrumentados durante apply/render. Zero crescimento de heap nesses casos.

Os12 candidatos executaram probe seco em1/4/6/8 notas, com custos/mapas reais,
PCM não nulo e render sem heap; isso não aprova musicalmente os Juno. Somente os
quatro residentes têm a regressão completa do Runtime. Nenhum timbre foi ouvido
pelo operador nesta fase. Um pico/ataque não deve ser classificado como click
apenas por diferença adjacente: E.Piano138 exige audição específica de reataque.

Os WAVs comuns e full8 de128/135 são byte-idênticos ao baseline desta worktree;
não houve nova normalização/limiter/alteração de envelopes. Novos WAVs138/142
foram gerados nas mesmas cenas. A matriz separa probe AMY bruto do runtime com
master64: são estímulos/ganhos diferentes, não comparar RMS entre essas tabelas
como se fosse ganho físico. Tempos host não equivalem a P4 nem aprovam5333,33us.

## Validação executada e comandos

Comandos a partir da worktree0.7; logs e resultados negativos preservados no bundle.

|Status|Comando / escopo|
|---|---|
|PASS|`pwsh -NoProfile -File smk-p4/tests/run_amy.ps1 -Voices 8` baseline pré-mudança|
|PASS|`pwsh -NoProfile -File smk-p4/tests/run_amy.ps1 -Voices 4` e `-Voices 6`, após correção|
|PASS|`pwsh -NoProfile -File smk-p4/tests/run_amy.ps1 -Voices 8 -SoundLibrary` completo|
|PASS|`pwsh -NoProfile -File smk-p4/tests/run_retrigger_probe.ps1` A/B independente e matrizA–H|
|PASS|`pwsh -NoProfile -File smk-p4/tests/run_host.ps1` C++ e29 Python após extensão|
|PASS|`python -m unittest discover -s smk-p4/tests -p test_patch_library_inventory.py -v` três novos testes|
|PASS|`python smk-p4/tests/audit_patch_library.py --check` inventário reprodutível|
|PASS|`pwsh -NoProfile -File smk-p4/tests/run_usb.ps1` lifecycle/descritores com mocks|
|PASS|`pwsh -NoProfile -File tests/run_smk_link.ps1`, `tests/run_smk_safety.ps1`, `tests/run_smk_ui.ps1`|
|PASS|`pwsh -NoProfile -File smk-p4/ui-preview/run_preview.ps1` suíte demo e20 capturas800×480|
|PASS|`pwsh -NoProfile -File smk-p4/tests/run_blueprint_adapter.ps1 -Voices N` N=4/6/8|
|PASS|Mesmo runner `-Voices 8 -SoundLibrary`: descoberta/indisponíveis/timeout, Prev/Next e estado aplicado|
|PASS|Comparação binária WAV128/135 comum/full8 contra baseline|
|PASS|`git diff --check`|
|PASS startup / MANUAL restante|Gravação app-only, AMY ready e I²S em repouso; MIDI/touch/áudio musical sob carga, CPU/latência/underruns e>=30min ainda pendentes|
|NOT RUN|Todos os demais ambientes S3/híbridos e suíte Python upstream completa; core/ drivers não modificados|

Resultados iniciais FAIL preservados: teste legado da terceira linha ainda esperava
patch indisponível; fixture adaptada ao catálogo real. Novo teste de teardown
assumia heap inicial zero, mas há duas alocações globais de baseline; corrigido
para comparar antes/depois. Nova validação de patch no boot comparava um campo
consumido por amy_add_event; corrigida para IDs imutáveis, e regressões repetidas.
Essas falhas não foram gravadas em hardware. FAIL clipping Juno32 permanece uma
limitação do candidato exploratório, sem ser mascarado por testes verdes de FM.

## Artefatos nativos da revisão gravada

PASS: `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-sound-library-8 -e guition-sound-library-8-bench -j 8`.
PASS: auditor `check_blueprint_build.py --voices 8 --sound-library --environment ENV`
(com `--musical` somente no bench), `esptool image-info` e `riscv32-esp-elf-size -A`.

|Ambiente|Aplicação B|DRAM0+DRAM1 data/BSS B|IRAM B|SHA256 firmware|
|---|---:|---:|---:|---|
|guition-sound-library-8|1110672|170340|96768|`d9453eda524dae0e6c97ea9cbd16ec39645eb2ee582da63fe23f9c14ccfddfdf`|
|guition-sound-library-8-bench|1204704|170696|101888|`2038bb68159ecc88aa256cf286a434b32914fc5e461b320f28216691b17b26f1`|

O resumo PIO omite DRAM1; a tabela acima soma ambos os bancos. Memória dinâmica
é reportada separadamente. O binário audível foi gravado app-only com hash
verificado; [registro físico e limitações](sound_library_07_hardware_qualification.md).

## Gates, riscos, rollback e próximo passo

- Gate A: inventário/custos/arquitetura comparados em host; margem física de memória
  ainda MANUAL. Escolha A é conservadora, não promessa de suporte a todos os patches.
- Gate B: PASS host para quatro FM; novas famílias Juno ainda PLANNED.
- Gate C: PASS catálogo/backend/Browser host e captura visual; Guition MANUAL.
- Gate D: PCM antigo e oito gates preservados em software; deadlines/heap físicos
  com novo número de residentes ainda MANUAL, não herdam o PASS0.6.
- Gate E: MANUAL / NOT RUN. Não declarar esta fase totalmente qualificada.

Riscos: varredura de256 osciladores e Panic em32 instrumentos aumentam trabalho;
memória/maior bloco após USB/UI e render máximo de cada patch ainda precisam
medição P4. A checagem inicial de memória não garante contra toda falha do core.
Não há latência de troca nem ausência de clicks físicos demonstradas. Não foi
introduzido mecanismo de troca dinâmica para esconder essas limitações.

Rollback: imagens/bundles/backup0.6 de4/6/8 permanecem intocados. Para rollback
físico, reconfirmar unidade/layout/hashes e obter autorização para aquela imagem.
Não usar o build novo de um ambiente antigo como substituto de bundle qualificado.

Próximo menor passo: obter o resultado do smoke já solicitado e executar o
[roteiro físico](sound_library_07_hardware_test.md) por>=30min com controlador,
oito notas em cada patch, quatro hot-plugs e navegação. Identificação/backup/
autorização/gravação já foram executados; não repetir aprovação desnecessária.
Depois de medir margem real sob carga, reavaliar Juno com política explícita de
EQ/efeitos e novo orçamento, preservando oito vozes.


## Correção de rajada USB após reprovação física

A primeira janela foi interrompida por25 drops/overflows de eventos USB,
com áudio/deadlines/OOM/driver errors0 e audição aprovada pelo operador.
Evidência raw/JSON preservada; não houve30min aprovados. Nova revisão usa
armazenamento USB64 somente no perfil Library; aplicação DSP continua32 eventos
válidos/bloco. Varredura de gerações antigas limitada a64. UI/legados mantêm32.
Sem alterações do driver USB, AMY, vozes, deltas, ganho ou DMA.

PASS host:57 eventos sem perda, drenagem32+25 em dois blocos,64 comandos antigos
invalidados após disconnect e overflow real com Panic. Runtime+768B, sem heap
adicional. Regressões completas Library8/legado4/USB passadas. Nova gravação
app-only hash verificado; startup USB conectado/ready/I²S e drops0 em repouso.
Reprodução física sob a mesma carga e nova janela>=30min continuam obrigatórias;
não considerar o defeito fisicamente encerrado apenas pelo host/boot.
