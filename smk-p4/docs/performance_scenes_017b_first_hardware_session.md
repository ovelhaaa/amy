# SMK-P4 0.17B — primeira sessão na Guition

**SMOKE FAIL — admissão de memória do Audio Owner. Release NO-GO.** A análise usa a captura física já existente de 17 de novembro de 2026. A sequência física fica interrompida em A1. Qualificação completa **PENDING**.

O [candidato de recuperação de RAM](performance_scenes_017b_memory_recovery.md) tem identidade própria e roteiro para repetir A1. Os resultados abaixo pertencem ao firmware do primeiro ensaio.

## Ambiente e identidade

| Item | Evidência |
| --- | --- |
| Repositório | Branch `feature/smk-p4-performance-scenes-017b`, HEAD local/remoto `1afa1cc1b78fd0fb30569d33707d1a37395eb443` e árvore limpa antes do relatório. [PR #47](https://github.com/ovelhaaa/amy/pull/47) aberta como rascunho. |
| Ferramentas | PowerShell 7.6.10, Python 3.14.0, pyserial 3.5, PlatformIO Core 6.1.18 em `C:/.platformio/penv/Scripts/pio.exe`. Bundles editor e bench presentes em `build/release-017b/`. |
| Preflight | **PASS** em COM13. A enumeração USB do Windows mostrou `303A:1001`, serial `80:F1:B2:D3:CC:86`. O log anterior do esptool associa a mesma porta e MAC ao ESP32-P4 rev. 1.3. COM15 é outro dispositivo com o mesmo VID/PID e serial diferente. |
| Placa | Guition JC4880P433 de referência. A serigrafia/revisão da PCB da unidade ainda precisa de confirmação visual. O log de instalação identifica a revisão do chip, v1.3. |
| Candidato | `guition-editor-8-bench`, commit `453add7b3a21168a0dc0b3a24fa513e414952588`. `firmware.py check`, SHA-256 independente do arquivo e comparação com a leitura anterior **PASS**. |
| SHA-256 de firmware.bin | `45ef5cfe0f9b3b82daa71e5cc2edb91ef7d3e356274fc8e813cd59e9bd8ff07e` |
| ELF no BENCH_ID | `6b353619f4cdcbb036ed104b66395b1f0315f6b54c40afc18f19f9098586b5eb` |
| Configuração efetiva | Oito vozes, 48 kHz, 256 frames/bloco, estéreo. I²S BCLK/WS/DATA em 50/51/52 e USB HS0 conforme o sdkconfig e a montagem de referência. |
| A confirmar na bancada | Fonte e VBUS, cabo e revisão do SMK25 V2, módulo e straps do PCM5102A, terra comum, saída estéreo, monitor de áudio e touchscreen 800×480. [Candidato e referências elétricas](performance_scenes_017b_hardware_candidate.md) |

## Evidência anterior de instalação

Em `build/hardware-017b/guition-017b-flash/`, `write.log` e `readback.log` registram a instalação anterior em `0x10000`. A comparação local confirmou que a aplicação lida de volta corresponde ao bundle, que a tabela lida em `0x8000` coincide com a do bundle e que o prefixo `0x0000–0xFFFF` ficou byte a byte igual, incluindo a NVS. A identificação do componente e da capacidade da flash instalada permanece pendente.

| Região ou arquivo preservado | SHA-256 |
| --- | --- |
| `before-boot-nvs-factory.bin` | `702eeedcbd7c29d21ed971133f978e53646f38054b9650a953ad12fe7ff2aeb0` |
| Aplicação anterior em `0x10000`, partição de `0x300000` bytes | `60c3d03c8f46aa8d1de28b0c2805fa452b018bc41e11e4c5861ac584859a5b9a` |
| NVS anterior em `0x9000`, `0x6000` bytes | `2e9e858b22c13f60d3c8c9a8bafc2a23f85925b69a1be43bf176e0429b3bd04c` |
| `after-prefix-and-app.bin` | `8ddfb541193040b214f64666ecf131728e23ef3430c600a93c410eb2a5ba664e` |

Se uma restauração vier a ser necessária, verificar novamente a unidade, a flash, a tabela e os hashes. Preparar a imagem anterior a partir da região factory do backup, autorizar separadamente a gravação somente dessa região e verificar o readback. Preservar bootloader, tabela e NVS. A análise atual consistiu em operações locais e preflight.

## Etapa A

| Etapa | Resultado observado e critério para repetir |
| --- | --- |
| A1 · Boot | **FAIL** na inicialização de áudio. `ADMISSION free=337619 largest=245760 required=352256 admitted=0` seguido de `AUDIO FAIL ... ESP_ERR_NO_MEM` e `ENGINE ready=0`. O log inicia com linha truncada e não documenta o boot completo. Nenhum marcador de crash, watchdog ou reboot foi detectado nos 20,4 s observados. |
| A2 · Touchscreen | **PENDING / MANUAL**. Há refresh e heap LVGL no serial, sem confirmação de interação. Verificar Performance, Scenes, browser, seleção, fechamento de modal e acesso ao Panic, sem salvar ou excluir Scenes durante A2. |
| A3 · USB MIDI | **PENDING / MANUAL**. USB ficou em `state=0` e sem eventos. A inicialização USB depende do estado inicial do áudio. Após A1 funcionar, confirmar enumeração do SMK25 V2, Note On/Off, pitch bend, modulação e perfil existente. |
| A4 · Áudio | **FAIL** em produção digital, com zero frames no `AUDIO` e no `OUTPUT08`. Audição **PENDING / MANUAL**. Após A1 funcionar, ouvir ambos os canais com ganho baixo, verificar ruído, clipping, Note Off, Panic e ausência de notas presas. Registrar a audição no diário. |
| A5 · Scenes | **PENDING / MANUAL**. Abrir a biblioteca e usar somente slots vazios autorizados ou dados de teste. Capturar, fazer recall imediato e seletivo simples, observar indicador ativo e Dirty State, e cancelar uma operação pendente. Evitar trocas agressivas de patch e preservar Scenes existentes. |

A causa imediata está no teste de heap interno executado antes de `Runtime::begin()`. Faltaram **14.637 bytes** de heap livre para o piso de admissão. O maior bloco de 245.760 bytes atendia ao piso de 196.608. O log localiza o bloqueio antes do início de AMY e I²S. A causa do déficit de heap e a margem necessária após a inicialização ainda precisam de medição. O último estado de Scene observado foi `token=0 generation=0 phase=0`.

## Log bruto, análise e reprodução mínima

- Bruto e sidecar preservados em `build/hardware-017b/guition-017b-flash/serial.log` e `serial.log.metadata.json`. SHA-256 do bruto `4a87a4667cb7beb0feff213535b06629f9c866690d53f421a456ccc2974ab4af`.
- Reanálise em `build/hardware-017b/smoke-01-review/`, com `metrics.json`, `summary.csv`, `warnings.txt`, `session_report.md`, `gate.json` e `release_gate.md`. Foram observados cinco relatórios completos, 20,4 s e zero boots completos. O gate atual registrou **UNVERIFIED / FAIL / NO-GO**. O sidecar antigo carece de `role` e `manifest_sha256`; os artefatos originais permanecem disponíveis.

Para uma reprodução passiva curta, confirme a mesma unidade, a montagem e a disponibilidade da COM13, feche outros monitores seriais e use um ID novo. Execute da raiz do repositório após combinar a operação com o responsável da bancada.

```powershell
$kit = 'smk-p4/tests/hardware_017b'
$bundle = 'build/release-017b/guition-editor-8-bench'
$out = 'build/hardware-017b/smoke-01'
$raw = "$out/serial.log"
& "$kit/capture.ps1" -Port COM13 -BoardId 'Guition-P4-rev1.3-80F1B2D3CC86' -Bundle $bundle -Output $raw -SessionId 'smoke-01' -Seconds 30
python "$kit/analyze.py" $raw --metadata "$raw.metadata.json" --bundle $bundle --out $out
python "$kit/evaluate.py" "$out/metrics.json" --raw $raw --metadata "$raw.metadata.json" --bundle $bundle --out $out
```

Espere novos relatórios `BENCH_ID`, `ADMISSION`, `AUDIO`, `OUTPUT08` e `USB`. A captura passiva observa o estado atual. Para registrar o boot completo, combine uma captura durante uma inicialização manual com o operador. Se a porta desaparecer, preserve o bruto parcial e use outro ID. A janela curta serve ao diagnóstico do smoke. O gate de release conserva os critérios normais.

**Próxima ação.** Diagnosticar o orçamento de RAM interna no boot do perfil bench e medir o consumo antes da criação do Audio Owner, confrontando-o com a reserva exigida pelo runtime. Repetir A1 antes de avançar para MIDI, Scenes ou stress. A aprovação de A exige ainda toque, enumeração, Note On/Off, áudio, Panic e recall básico observados, sem crash, corrupção ou notas presas.
