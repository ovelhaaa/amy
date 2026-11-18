# Bancada de qualificação 0.17B

## QUICK START — FIRST HARDWARE SESSION

1. Confirme a porta COM pelo cabo do console. Na raiz do repositório, abra PowerShell 7 e configure a sessão. Substitua o exemplo de porta e placa pelos identificadores observados.

   ```powershell
   $kit = 'smk-p4/tests/hardware_017b'
   $pio = 'pio' # ou caminho do executável PlatformIO
   $port = 'COM13'
   $board = 'Guition-unidade-revisao'
   $bundle = 'build/release-017b/guition-editor-8-bench'
   $id = 'guition-017b-01'
   $session = "build/hardware-017b/$id"
   $raw = "$session/serial.log"
   [IO.Ports.SerialPort]::GetPortNames()
   ```

2. Verifique a integridade do bundle bench local.
3. Confira o SHA-256 do binário e o ELF do manifesto contra o [candidato Guition](performance_scenes_017b_hardware_candidate.md).

   ```powershell
   python "$kit/firmware.py" check --profile guition-editor-8-bench --bundle $bundle
   Get-FileHash -LiteralPath "$bundle/firmware.bin" -Algorithm SHA256
   ```

4. Confirme backup verificável de flash/NVS, tabela de partições e autorização para a unidade e o hash exatos. Veja a preparação abaixo.
5. Quando autorizado, instale manualmente somente a aplicação bench no offset confirmado. Se a imagem já estiver instalada, confirme sua identidade pela serial durante a coleta.
6. Execute o preflight.
7. Inicie a coleta temporizada.

   ```powershell
   & "$kit/preflight.ps1" -Port $port -Bundle $bundle -Pio $pio
   & "$kit/run_bench.ps1" -Port $port -BoardId $board -Bundle $bundle -SessionId $id -Seconds 1200 -Pio $pio
   ```

8. Execute o roteiro A–H durante a coleta. Faça B em uma captura complementar independente. Em outro PowerShell, acompanhe `Get-Content -LiteralPath $raw -Wait` com o caminho definido também nessa janela. Registre os timestamps de F conforme a seção de cobertura.
9. Aguarde o timer para encerrar a coleta e gravar os metadados. Use uma duração maior se o roteiro exigir. A captura e o runner operam passivamente, com RTS/DTR desativados.
10. Confira `session_report.md`, `warnings.txt` e `release_gate.md` na pasta da sessão.
11. Copie o modelo, preencha os marcadores e anexe as evidências conforme abaixo.
12. Reavalie a sessão com o log original, os metadados e o bundle.

   ```powershell
   Copy-Item "$kit/evidence.example.json" "$session/evidence.json"
   # Preencha evidence.json antes de reavaliar.
   python "$kit/evaluate.py" "$session/metrics.json" --raw $raw --metadata "$raw.metadata.json" --bundle $bundle --evidence "$session/evidence.json" --out $session
   ```

O perfil `guition-editor-8-bench` habilita a montagem USB/I²S documentada e a telemetria desta bancada. Use PowerShell 7, Python 3.10 ou superior com pyserial, PlatformIO Core e um console serial que preserve o primeiro canal da Guition. Separe um analisador lógico para BCLK/WS/DATA e uma interface de captura para as duas saídas do PCM5102A. Sem os instrumentos, complete as etapas auditivas e deixe as medições físicas em aberto.

## Preparação

1. Registre unidade e serigrafia da Guition, revisão de chip, console COM, fonte e VBUS do USB Host, versão/serial do SMK25, módulo PCM5102A, straps, terra comum, pinagem efetiva e instrumentos. Confira a montagem do [candidato Guition](performance_scenes_017b_hardware_candidate.md). O identificador de hardware no log Gate 0 vem da configuração histórica do P4SDM. A revisão e a serigrafia devem ser comprovadas na unidade.
2. Prefira os bundles preservados em `build/release-017b/<perfil>/`. Para reproduzir os builds do candidato, use o commit de código indicado no manifesto em um checkout limpo separado. Execute os perfis em sequência na raiz desse checkout. Em instalações fora do PATH, defina `$pio` com o executável local.

   ```powershell
   $pio = 'pio'
   $kit = 'smk-p4/tests/hardware_017b'
   foreach ($profile in 'guition-editor-8', 'guition-editor-8-bench') {
       $log = "build/$profile-017b.log"
       & $pio run -d smk-p4 -e $profile -j 8 *> $log
       if ($LASTEXITCODE) { throw "Falha de build: $profile" }
       python "$kit/firmware.py" package --profile $profile --bundle "build/release-017b/$profile" --log $log
       if ($LASTEXITCODE) { throw "Falha de manifesto: $profile" }
   }
   ```

3. Confira a porta por desconexão/reconexão do cabo de console. Compare o SHA-256, o perfil, a versão, o commit, a partição do manifesto e o offset da aplicação. Preflight verifica arquivos locais e enumera portas, sem abrir a porta ou consultar a flash da placa.

   ```powershell
   $port = 'COM13' # exemplo; substitua pela porta identificada
   $bundle = 'build/release-017b/guition-editor-8-bench'
   & "$kit/preflight.ps1" -Port $port -Bundle $bundle -Pio $pio
   ```

4. Antes de qualquer gravação autorizada, faça backup verificável da flash ou da tabela, aplicação atual e NVS com a ferramenta e o modo de leitura aprovados para a placa. Registre SHA-256, offsets e tamanho de cada backup e confirme o plano de restauração. Compare a tabela lida da unidade com a tabela compilada. O perfil atual usa aplicação `factory` em `0x10000` e NVS em `0x9000` com 0x6000 bytes. Após autorização para o hash concreto, use a ferramenta de gravação da bancada em modo somente aplicação, no offset confirmado, preservando configurações de flash e dados. Interrompa o procedimento se a tabela da placa divergir. O kit opera com o firmware já instalado.
5. Feche outros monitores seriais. Inicie a captura antes do primeiro teste, com a placa já ligada se a porta desaparecer no boot. Reserve relatórios periódicos antes e depois de F e após a última ação. A maioria das métricas sai a cada 5 s. Se a porta cair no reboot, anexe captura separada do boot ou vídeo para A/B. Use um único boot no ensaio principal e somente o SMK25 como fonte MIDI externa.

   ```powershell
   & "$kit/run_bench.ps1" -Port $port -BoardId 'Guition-unidade-revisao' -Bundle $bundle -SessionId 'guition-017b-01' -Seconds 1200 -Pio $pio
   ```

   Captura complementar de persistência (etapa B) e reanálise da sessão principal.

   ```powershell
   $session = 'build/hardware-017b/guition-017b-01'
   & "$kit/capture.ps1" -Port $port -BoardId $board -Bundle $bundle -SessionId 'guition-017b-persistencia' -Seconds 180 -Output "$session/persistencia.log" -Persistence
   python "$kit/analyze.py" "$session/serial.log" --metadata "$session/serial.log.metadata.json" --bundle $bundle --out $session
   ```

## Cobertura temporal

| Critério em `criteria.json` 0.17B.2 | Limiar |
| --- | --- |
| `MINIMUM_SESSION_DURATION` | Pelo menos **120 s** entre o primeiro e o último `AUDIO` válidos. |
| `MINIMUM_PERIODIC_SNAPSHOTS` | Pelo menos **24 relatórios completos**. Cada relatório agrupa as famílias obrigatórias de `AUDIO` até `USB`. |
| `MAXIMUM_TELEMETRY_GAP` | No máximo **12 s** entre relatórios completos ou amostras da mesma família, incluindo as bordas da janela `AUDIO` para cada família. |
| `PRE_STRESS_BASELINE` e `POST_STRESS_RECOVERY` | Pelo menos **dois relatórios completos** em cada janela. |

Os pisos correspondem à cadência nominal de 5 s, aos dois relatórios por grupo de F e às janelas de referência e recuperação. Planeje tempo adicional para A e C–H. Uma lacuna de até 12 s acomoda um relatório perdido e 2 s de variação. A janela observada usa o tempo da placa, em milissegundos, e a janela do host serve como verificação de interrupção. O gate sinaliza encerramento mais de 12 s antes da duração solicitada ou mais de 24 s da janela do host sem cobertura observada, margem que inclui entrada e saída da coleta. Reboot no ensaio principal e timestamps omitidos ou regressivos impedem a qualificação.

Para marcar F, use o número entre parênteses da linha `I (…) P4_AMY: AUDIO …`. Antes da primeira troca, aguarde pelo menos dois ciclos completos até `USB` e anote o timestamp `AUDIO` do último ciclo em `stress.start_ms`. Ao terminar F, registre em `stress.end_ms` o timestamp `AUDIO` do primeiro ciclo completo visto após o fim. Deixe passar mais dois ciclos completos antes de encerrar a coleta. Os dois números pertencem ao log da sessão principal. O avaliador confere a ordem, os anchors e o vínculo entre o arquivo de evidência e o SHA-256 do log. Marcadores ausentes ficam `NOT MEASURED`.

Exemplo de registro de campo, com valores a preencher na bancada:

```text
Sessão guition-017b-01 | serial.log e serial.log.metadata.json
F início | último AUDIO anterior à primeira troca | start_ms=<copiar do log>
F fim    | primeiro AUDIO após a última troca    | end_ms=<copiar do log>
B        | persistencia.log e persistencia.log.metadata.json | reboot manual
H        | dac-estereo.wav e i2s.vcd | instrumentos, resolução e intervalos no diário
```

`INCONCLUSIVE` indica que o kit precisa de evidência adicional ou consistente para decidir. Procure em `gate.json` as famílias sem amostras, a duração observada, as linhas da evidência e a diferença para a janela do host. Preserve a sessão incompleta. Para cobrir uma interrupção da coleta principal, repita A e C–H sob um novo ID em um único boot. Para documentar B, use `-Persistence` e anexe os dois arquivos complementares e o registro de releitura. O reboot intencional dessa captura fica fora dos contadores do ensaio principal. A avaliação usa um único log principal, com as capturas complementares anexadas como evidência.

## Roteiro musical e físico

Registre hora aproximada, ação, resultado e referência à captura. Mantenha a sessão principal em execução durante C–H. Faça o reboot intencional de B antes ou depois do boot principal. Registre falhas imediatamente, com token, geração, patch, página e aproximadamente o instante observado.

1. **A · Boot e áudio.** Inicialize, navegue pelos controles da touchscreen, confirme o SMK25 e toque oito notas com velocidades variadas. Teste pitch bend e modulação. Escute L/R do PCM5102A. Confira `BENCH_ID`, `AUDIO I2S_PRODUCING`, avanço de `OUTPUT08 frames`, USB Connected e NVS. Registre a evidência de boot e áudio.
2. **B · Persistência.** Fotografe os presets V4 de referência sem sobrescrevê-los. Use dois slots de Pattern disponíveis e oito slots de Scene autorizados para o ensaio. Prepare nomes e conteúdos distinguíveis, salve, reinicie manualmente e releia os dois Patterns e as oito Scenes. Confira conteúdo, referências e presets V4. Se os oito slots estiverem ocupados por dados importantes, use uma unidade de bancada ou dataset previamente autorizado.
3. **C · Pattern-only.** Segure um acorde no SMK25, inicie o sequenciador, aplique SELECTIVE apenas para PATTERN, escute a mudança de Pattern com o acorde preservado. Solte as teclas e confirme silêncio quando o transporte parar.
4. **D · FULL RECALL.** Prepare cenas Juno e DX7 com diferenças audíveis e visíveis nos sete domínios PATCH, FX, HARMONY, ARP, MACROS, PATTERN e TEMPO, com Apply Scene Tempo ativo e sem locks para esta etapa. Alterne nos dois sentidos e confira cada domínio. Aguarde um snapshot terminal em cada direção antes da próxima troca.
5. **E · NEXT BAR.** Em 40, 120 e 240 BPM, configure o BPM de origem, deixe transporte e clock ativos, solicite NEXT BAR no meio de um compasso e observe a troca. Em cada BPM deixe a cena terminal aparecer na serial antes do próximo pedido. Registre token, `request_ms`, fase esperada, fase aplicada e instante audível. Compare o desvio com 256 frames. Repita com patch e com Pattern quando possível. O relógio do motor mede a fronteira musical, e a captura analógica confirma a consequência audível.
6. **F · Stress.** Aqueça alternando os dois patches, anote memória em cada topologia e tela de referência. Faça 100 trocas, em grupos de 10, intercalando notas sustentadas, sustain, Arp, sequenciador e FX. Ao fim de cada grupo, observe dois relatórios no mesmo patch e tela, conte notas presas e travamentos, e compare mínimos e maior bloco interno/PSRAM. Revise margem da tarefa PCM e heap LVGL.
7. **G · Recuperação.** Acione Panic durante notas sustentadas e Stop com transporte em execução. Faça dez desconexões/reconexões com a mesma sessão de console e confirme Note On/Off após cada reconexão. Comece e termine conectado. Teste cancelamento de NEXT BAR antes da fronteira e retorno por seleção de patch válido. Registre separadamente qualquer ensaio de falha controlada com firmware específico; a rotina primária espera zero falhas de carga e zero Partial Apply. Evite induzir pressão global de memória.
8. **H · Áudio físico.** Capture DAC estéreo e BCLK/WS/DATA sob oito vozes e durante as trocas. Registre a configuração e resolução temporal dos instrumentos. Com carga contínua, compare clock/frames, fluxo de dados e áudio fora da barreira. Marque pops, clicks, lacunas e o maior intervalo entre o último áudio anterior e o primeiro áudio válido após confirmação com nota de prova. Compare o maior silêncio com a troca direta da 0.17A sob a mesma carga, quando disponível. Registre tolerância musical e valor em ms. Se clocks ou áudio estiverem ausentes, deixe os critérios correspondentes sem evidência.

## Inventário auditado

| Origem real | Métrica e unidade | Disponibilidade e confiabilidade |
| --- | --- | --- |
| `SMK_P4_GATE0 IDF=`, `PCM=`, `BENCH_ID` | reset, IDF, sample rate, frames/bloco, configuração DMA e hash ELF | Observação da configuração e identidade. O hash ELF conecta o console ao manifesto. |
| `AUDIO` | frames e blocos, render médio/máximo em µs, deadlines, erros, timeouts, stack livre em bytes | Contadores e máximos desde boot. High-water medido em bytes. Serviço e refill do owner caracterizam o produtor PCM. |
| `ENGINE`, `ADMISSION` | vozes ocupadas, OOM, fila, latência em ms, compute deadlines, heap de admissão em bytes | Contadores e amostras. `cmd_avg/max` inclui loader. `dma_avg/max` no perfil unificado mede pacing do handoff. `BENCH_AUDIO apply_*` mede só apply. |
| `PATCH08`, `PATCH_LOAD`, `SWAP08`, `BENCH_AUDIO` | estado 0 Ready / 1 Requested / 2 Loading / 3 Verifying / 4 Failed, mudanças, falhas, tentativas, último/máximo em µs, gerações e heap antes/depois em bytes | Loading e Verifying podem passar entre relatórios. Última amostra mantém a última troca observável. Contagem de alocações por carga e pico transitório seguem sem instrumentação física. |
| `BENCH_AUDIO global_panics / usb_invalidations` | invalidações de geração atendidas pelo owner | Contadores por tratamento. Várias solicitações podem ser coalescidas. |
| `OUTPUT08`, `GAP09`, `BENCH_AUDIO` | blocos escritos, erros/timeouts, rampa, gaps por motivo, descartes, intervalo máximo entre inícios de writes em µs, stack em bytes | Escrita confirmada pelo driver e contadores de software. Rampa conta fallback com amostra anterior não zero. `patch_gap_max_blocks` é maior sequência de fallbacks classificados PATCH escritos com sucesso. Contagem exclui PCM silencioso já produzido e blocos que estavam no DMA. |
| Refill e underruns | `refill_risks`, `refill_gap_max_us`, capacidade nominal 21,333 ms | Proxy da cadência de alimentação. Contador físico de underruns indisponível. Só a captura física qualifica continuidade do clock e da saída. |
| `SCENE`, `BENCH_SCENE` | token, geração, slots, fase, máscaras hex, erros, fase esperada/aplicada em unidades musicais, espera em ms e duração em µs | Snapshot periódico. `request_ms` é o primeiro atendimento no owner. `elapsed_us` inclui barreira do loader quando o patch muda. `start_ms - request_ms` estima espera. O desvio de NEXT BAR divide a diferença de fase por `units_per_frame` do BPM de origem. |
| `USB`, `MIDI_TRACE` e logs do driver | conexão, desconexão, eventos, erros e rastreio de pacotes | Contadores e snapshots. Estados 0 Disabled / 1 Disconnected / 2 Connected / 3 Error. O teste de tocabilidade requer acompanhamento manual. |
| `NVS`, `MEMORY`, `UI_BLUEPRINT` | estado, entries usadas/livres, heap interno e PSRAM livre/mínimo/maior bloco em bytes, pool LVGL | Amostras e mínimos acumulados. O estado do NVS indica acesso às estatísticas. A releitura dos registros após reboot verifica persistência. |

## Relatório e decisão

1. Leia `session_report.md`, `warnings.txt` e `release_gate.md`. `metrics.json` e `summary.csv` preservam número da linha, timestamp e campos observados. O log bruto preserva mensagens desconhecidas. `PASS` do parser identifica cobertura da leitura. O avaliador decide os critérios de release.
2. Copie `evidence.example.json` para `$session/evidence.json`. Copie de `serial.log.metadata.json` os identificadores de sessão e placa, o perfil e os hashes do log, firmware, ELF e manifesto. Identifique o revisor. Para cada observação, preencha o valor medido, a nota, o caminho relativo e o SHA-256 do anexo em minúsculas. Use `(Get-FileHash -LiteralPath "$session/<arquivo>" -Algorithm SHA256).Hash.ToLowerInvariant()`. Para o clock I²S, anexe a exportação lógica CSV, VCD, SR ou SAL. Para os critérios analógicos instrumentais, anexe a captura DAC WAV ou FLAC. Indique instrumento, configuração, intervalo e correspondência com o log. O revisor interpreta a captura física. Para a exceção, anexe a aprovação explícita do responsável técnico. Critérios sem instrumentos ou revisão permanecem `NOT MEASURED` ou `INCONCLUSIVE`.
3. Reavalie e envie o diretório da sessão, manifesto, evidências e a versão de `criteria.json`. Inclua negativos, lacunas e sessões complementares. Se o console desconectar ou o equipamento reiniciar fora da etapa B, preserve a captura com falha e repita em diretório novo após diagnosticar.

   ```powershell
   python "$kit/evaluate.py" "$session/metrics.json" --raw "$session/serial.log" --metadata "$session/serial.log.metadata.json" --bundle $bundle --evidence "$session/evidence.json" --out $session
   python -m unittest discover -s $kit -v
   ```

`GO` exige todos os critérios obrigatórios, identidade de firmware conferida e evidência real. `TEST FIXTURE` valida os scripts. `HOST TEST` valida a regressão de software. `REAL HARDWARE SESSION` vincula o log da Guition, o binário e os anexos revisados. O limite de 5.333 µs é do render. O tempo total do loader e a tolerância musical do silêncio são avaliados com a saída física e a aprovação técnica.
