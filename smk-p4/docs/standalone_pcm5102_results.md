# SMK-P4 Standalone 0.1 — progresso, sem aprovação musical

Data: 2026-11-02. **Standalone 0.1 ainda não concluído.** Esta entrega prepara
o Gate A (PCM5102A P4); ligação e áudio físico estão MANUAL. Não há AMY local,
Performance local ou USB MIDI habilitados neste incremento.

## Baseline, arquitetura e preservação

Referência inspecionada: `feature/smk-p4-rehearsal-stabilization`, HEAD
`90787646`. Árvore inicial limpa, branch alinhada ao remoto, sem commits locais
não publicados. Nova branch `feature/smk-p4-standalone` criada desse HEAD;
sem reset, rebase, force-push, alteração de main ou gravação de firmware.
Commits desta entrega podem ser consultados com
`git log feature/smk-p4-rehearsal-stabilization..feature/smk-p4-standalone --oneline`.
Implementação: `17fb5221` — `feat(smk-p4): add isolated PCM5102A bring-up target`.
Este relatório e o guia de ligação ficam em commit documental separado.
O remoto da referência foi consultado com `git ls-remote origin
refs/heads/feature/smk-p4-rehearsal-stabilization` e confirmou o mesmo HEAD.

Conflito explícito: specs/AGENTS priorizam S3 e LCD 284×76; pedido atual exige
P4 autônomo/800×480 e bloco256. Seguido o pedido no novo ambiente, preservando
os firmwares híbridos. README raiz já contém marcadores de conflito, sem mudança.
Não houve alteração no AMY `src/` nem nas aplicações S3. Performance remota
continua ligada exclusivamente ao backend anterior em `guition-link`.

Perfil preservado: Guition JC4880P443C_I_W / JC-ESP32P4-M3, ST7701S/GT911,
P4 ES1.3, 16 MB Flash; identificação física vem dos relatórios anteriores,
não de nova consulta ROM. SDK instalado: `5.5.2.260116`, plataforma pioarduino
55.03.36-1, LVGL9.3.0; sem atualização/dependência nova.

## Implementado

- Ambiente `guition-standalone`, sdkconfig separado e build CI incluído.
- Driver isolado `audio_pcm5102`: master, Philips, 48 kHz, signed16, estéreo,
  256 frames, DMA4×256, MCLK não usado. API retorna erros, rejeita buffers
  inválidos e GPIOs duplicados, pré-carrega todos os descritores com zeros.
- Clear automático de buffers DMA sem refill; falha de escrita interrompe o
  canal. Não há mecanismo externo XSMT nem promessa de mute analógico.
- Teste de 440 Hz com frequência configurável, 0,05 FS, esquerda/direita/ambos,
  silêncio e rampas. Saída desabilitada por padrão até conferência elétrica.
- Um owner de áudio não fixado a core, prioridade inicial5/stack4096 bytes;
  configuração central, sem copiar afinidade S3. Main continua owner LVGL.
  Mailbox estático de uma posição publica diagnósticos por cópia limitada;
  sem logs/heap por bloco. Escrita I²S é a única espera, limitada a20 ms.
- Instrumentação distingue geração PCM de serviço com espera DMA; não apresenta
  `refill_risks` ou timeouts como medição exata de underrun. Logs a cada5 s na
  tarefa principal. Memória interna/PSRAM/maior bloco e stack reportados.
- Falha de display permite continuar áudio; falha de áudio é publicada e para
  produção. Teste não envia comandos musicais pela criação de widgets.

## Portabilidade AMY e plano B–D (auditoria, não implementação)

O componente S3 lista os fontes C de `src/`, sem necessidade de outra cópia.
`amy.h` usa AMY_BLOCK_SIZE e BLOCK_SIZE_BITS; manter256/8 e AMY_SAMPLE_RATE48000.
`SAMPLE` interno é fixed-point; saída de `amy_fill_buffer()` após
`amy_render()` é PCM16 intercalado.
Não aplicar conversão float ao SAMPLE interno sem respeitar sua escala.
`algorithms.c` protege assembly com `__XTENSA__ && CONFIG_IDF_TARGET_ESP32S3`;
P4 usa fallback C. `amy_fixedpoint.h` também tem variante Xtensa desativada;
`amy.c` restringe alocação alinhada especial ao S3. Isso é inspeção, não PASS
de compilação ou performance RISC-V do engine.

`amy_adapter.cpp` S3 inclui bateria/bus1, FX, hooks e dependências synth/system,
além de APIs internas de ownership. Não reutilizar esse adaptador inteiro no
caminho mínimo P4. Criar runtime separado, single owner, default_synths/startup
bleep desativados, um patch ROM/vozes conservadoras e FX desativados. Definir
caps de RAM/PSRAM no boot; medir deltas/osciladores/stack antes de aumentar limites.
Preservar configuração global AMY e firmware S3. Os patches experimentais 3B
não devem se tornar requisito do runtime inicial.

Próximo Gate B só depois do caminho I²S compilável, testes PCM e procedimento
físico disponíveis e sem erro grave aberto. Para Gate C, extrair contrato local
de performance mantendo a mesma UI; callbacks produzem comandos em fila
limitada com reserva/latch para Note Off/Panic. Publicar estado aplicado pelo
owner; não fabricar snapshots S3 nem promover RECEIVED a APPLIED. Somente
master/macros realmente suportados habilitados. Gate D exige medição física
de LVGL+AMY, notas/acordes/releases/Panic, não pode ser aprovado em host.

## USB Host: viabilidade investigada, implementação adiada

[IDF5.5.2 para P4](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32p4/api-reference/peripherals/usb_host.html)
oferece Host FS/HS e bulk, compatíveis em princípio com USB MIDI. Um único
controlador Host por vez nessa versão. USB-JTAG/debug não é a conexão MIDI.
[Folha03](https://github.com/ultramcu/guition-jc4880p443c-i-w/blob/master/schematic/03-expand-io_usb-fs-hs_lcd-backlight.webp)
mostra conectores FS/HS distintos, CC com resistores5k1 a GND e VBUS
`USB5V_IN`. Não identifica uma chave de VBUS Host controlada/qualificada.
Conclusão de engenharia: não assumir que basta cabo USB-C/OTG para alimentar
o teclado. Porta física real, alimentação/proteção/backfeed e solução com hub
alimentado precisam de confirmação antes de qualquer teste Host.

Parser MIDI e parser de descriptors S3 são candidatos portáveis; usam eventos
internos e APIs públicas ESP-IDF. O serviço Host inclui prioridades/afinidade,
política Panic e alocação de transferências que precisam ser revalidadas no
P4; não copiar as escolhas do S3 sem medir. Nenhum stack USB foi inicializado,
teclado enumerado ou reconexão testada neste P4. Gate E permanece **BLOCKED**
pelos gates de áudio/UI e caminho físico VBUS/porta.

## Hardware, riscos e próximo passo

O operador localizou os pinos e vai transferir o DAC S3; ainda não o conectou.
Pinagem documental candidata50/51/52, JP1-11/9/7; orientação física, VCC,
regulador, straps, continuidade e GND ficam MANUAL. Sem trilhas modificadas.
Folha03 consultada em2026-11-02, SHA-256 do arquivo de referência:
`3ae2b9d1664d4f42d9c290cade14ffec2e3a9f05b86232d48e3f182308c595b1`.
Seguir [ligação e procedimento exato](pcm5102_wiring.md), primeiro silêncio,
depois canais. Não habilitar saída apenas por sucesso de build.

Máximos, média, memória runtime, CPU, latência e áudio analógico P4: **MANUAL**.
Polifonia/patches/voice stealing/ownership locais: **NOT RUN**, sem engine local.
Não há base para afirmar áudio limpo, estabilidade com LVGL ou prontidão para
ensaio. Após validar a saída, próximo menor passo é runtime AMY mínimo e
teste programático de patch/Note On/Off/Panic, antes de conectar a UI musical.

## Validação executada

Comandos a partir da raiz, com `PYTHONIOENCODING=utf-8` e `PYTHONUTF8=1`.
Nos rebuilds também foi usado `IDF_COMPONENT_CHECK_NEW_VERSION=0`, opção do
gerenciador instalado para dispensar consulta de novas versões; dependências
continuam fixadas e o gerenciador permanece ativo.

| Status | Comando / verificação | Escopo |
|---|---|---|
| PASS | `./smk-p4/tests/run_host.ps1` | Geometry, dirty buffers, 4 testes Python; novos testes PCM e driver mock |
| PASS | `./tests/run_smk_link.ps1` | Protocolo, gestos Touch, saturação/release e UART reais em host |
| PASS | `./tests/run_smk_amy_boot.ps1 -Legacy` | AMY real legado host; warnings preexistentes de casts de ponteiro Windows |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-display -e guition-link` | Baseline antes de alterações: 19,921s / 25,592s |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-link -e esp32-s3-devkitc-1` | S3 preservado: link170,394s / original101,715s |
| FAIL, corrigido | Primeiro build `guition-standalone` | Faltava declarar `esp_driver_gpio`; corrigida dependência |
| PASS, intermediário | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone` | 279,704s, imagem PCM desabilitado; RAM13108 / Flash578034 bytes |
| MANUAL | Procedimento do guia de ligação | Sem gravação, clocks/RCAs/som/touch simultâneo ainda não observados |
| NOT RUN | AMY/RISC-V e Performance locais | Não implementados neste gate |
| BLOCKED | USB MIDI P4 | Gates A–D físicos e alimentação/porta Host pendentes |

A verificação extra do comando nativo CMake detectou includes ausentes por
`REQUIRES` condicionado a Kconfig, indisponível na expansão inicial ESP-IDF.
Registro de dependências tornado incondicional; seleção de fontes continua
por ambiente. Isso não inicializa UART nem DAC nos perfis que não os usam.
O teste `check_standalone_build.py` usa a base de compilação real para compilar
o ramo habilitado sem alterar sdkconfig nem criar firmware ativo. Sua primeira
tentativa ad hoc também encontrou quoting Windows incorreto; o runner usa a
linha de comando original diretamente, sem reconstruí-la por split em espaços.

| Status | Verificação final | Resultado |
|---|---|---|
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-display -e guition-link` | display294,901s / link289,890s; sem warnings novos no log |
| PASS | `C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-standalone` | 94,013s após CMake/telemetria finais; RAM13108 / Flash578050 bytes |
| PASS | `C:/.platformio/penv/Scripts/python.exe smk-p4/tests/check_standalone_build.py` | P4 ES correto, UART/saída desativadas e ramo habilitado compilado com includes nativos |
| PASS | `git diff --cached --check` | Incluindo fontes/testes novos; sem erros de whitespace |
| PASS | Inspeção dos sdkconfigs e diff `src/` / `smk-s3/` | Link antigo permanece habilitado somente no seu perfil; AMY/S3 sem alteração |
| NOT RUN | CI remota | Workflow atualizado, não disparado nesta sessão |

Imagens locais de desenvolvimento, **não gravadas**. Foram geradas antes dos
commits, com metadata baseada em `90787646`/árvore dirty; commit não regrava nem
recompila esses artefatos. RAM/Flash abaixo são resumos estáticos do linker,
não heap runtime, carga de CPU ou demonstração de estabilidade musical.

| Perfil | `.bin` bytes | RAM / Flash reportadas | SHA-256 `.bin` |
|---|---:|---:|---|
| guition-standalone (saída desabilitada) | 578768 | 13108 / 578050 | `87765bbc63a0a846dc61984925261e3213b131ce161c23602e1410beee60b058` |
| guition-display | 577552 | 12424 / 576830 | `d5dc5a421c9d1d28464f0a0848e51eb34bdbf19ba39659d058256334c8ed5130` |
| guition-link | 669504 | 17624 / 668788 | `22c59f72523d1aef0b889f91b7b6e64db86afd109ac7fee121c8074a6a31ad97` |

O objeto habilitado de teste não foi incorporado ao firmware e não prova
execução física. `frames_written` conta frames aceitos pelo driver;
`blocks_rendered` inclui geração cujo write subsequente falhe, preservando
o denominador correto da média. Nenhum contador pode certificar áudio RCA.
Gate1 permanece aberto pela ligação/conferência elétrica ainda não realizada;
a aceitação física A não foi promovida a PASS. B–D continuam planejados e E
bloqueado; o conjunto Standalone0.1 não satisfaz ainda os critérios do pedido.

Logs locais ignorados: `build/standalone_baseline_*`, `standalone_build_p4*`,
`standalone_regression_p4.log`, `standalone_final_p4.log`,
`standalone_enabled_check.log`. Listagem serial somente leitura reencontrou
COM13/303A:1001 e COM8/1A86:55D3; nenhuma porta foi aberta/resetada para áudio.
