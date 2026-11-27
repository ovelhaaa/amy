# SMK-P4 0.19-A.1 — Harmonic Pads UX Hardening

## Branch, referência e revisão

Branch isolada: `feature/smk-p4-harmonic-pads-019a1`. A origem remota foi verificada com `git ls-remote` e atualizada com `git fetch`: `53da881192d24e815cf3b5eb68c32bb2a322cc3d`, sem commits posteriores naquele momento. A worktree `codex/p4my-boot-splash` foi preservada.

Revisão de código congelada: `b3c70b92bbd9b6a86d5dede50c236251b09cf9d0`. O SHA final de entrega, após o commit destas evidências, é informado na entrega e obtido com `git rev-parse HEAD` na branch. Commits documentais seguem a revisão validada; um documento não pode conter o hash do próprio commit.

O pedido explícito P4/LVGL 9.3/800×480 prevalece sobre `specs.md`, que ainda descreve S3/284×76. Não houve alteração no DSP AMY, aplicação S3, GPIO, drivers, Harmony Engine, alocador, Note Ownership, USB, SMK25, Touch, Arp, Sequencer, clocks, Scenes ou formatos NVS. Não foram adicionadas dependências.

## Problemas confirmados e correções

1. `Pad::rejected` permanecia verdadeiro após RELEASE ALL. Foi removido: rejeição é resultado histórico, e o pad fica READY para tentar novamente.
2. Capacity e Invalid compartilhavam uma mensagem ambígua. Agora são `VOZES INSUFICIENTES` e `CONFIG INVALIDA`.
3. Expired não era apresentado. A UI agora mostra `HOLD EXPIRED - LIBERADO POR SEGURANCA`, associado ao token/pad, e retira ACTIVE somente conforme a confirmação do owner.
4. `release()` apagava o token antes da confirmação. Agora solicita o latch, preserva a identidade e aguarda o owner. Snapshots anteriores não reabrem uma operação terminal.
5. Controles de 32 px e seleção cíclica exigiam muitos toques. ROOT/SCALE/OCT têm seletores diretos e controles superiores de 44 px.
6. O folding herdado podia apresentar como válida uma oitava que soaria mais grave em alguns pads. A UI desabilita escolhas/previews que exigem folding, sem alterar a política do motor. C8/C#8 são os testes de borda.
7. O refresh reaplicava textos/cores continuamente. A UI compara textos, cores e disabled antes dos setters; os contadores de instrumentação só existem em host (`!ESP_PLATFORM`).
8. Capturas herdavam um Recall Failed de outro teste. A fixture principal agora confirma uma Scene válida antes dos 19 cenários dos pads. Os casos negativos anteriores continuam como regressões, sem gerar capturas principais.

## Modelo de feedback

O owner continua sendo a única autoridade musical. ACTIVE exige `active_token`, token da operação e geração correspondentes. PENDING conserva a identidade capturada, inclusive durante mudanças locais. RELEASED exige o resultado Released do owner e identidade exibida compatível; uma nova configuração mostra o próximo acorde READY, preservando LAST. Rejeições não produzem vozes nem deixam um pad permanentemente vermelho. Nova admissão confirmada substitui a rejeição anterior.

LAST conserva o acorde capturado e distingue Capacity, Invalid, Released, Expired e Admitted. Uma expiração recém-observada recebe prioridade nessa linha. Um snapshot antigo com o mesmo token não faz uma operação terminal voltar a ACTIVE; tokens anteriores e gerações anteriores não substituem a apresentação atual.

Não foi inventado Cancelled: o protocolo não oferece esse resultado. Uma intenção sem confirmação por dois segundos solicita o latch e deixa de aguardar; a UI não fabrica Released. Submissão não aceita é apresentada como ausência de confirmação, não como Capacity.

## HOLD e auditoria do heartbeat

O lease permanece **2000 ms**, independente dos dez segundos de Touch. `UiBlueprint::service()` continua atendendo PADS sem depender de revisão do modelo. `refresh()` renova o heartbeat antes de decidir qualquer redraw. A tarefa de controle tem espera nominal de 10 ms; isso é a cadência prevista no código, não uma medição física de jitter.

| Situação | Contrato auditado |
|---|---|
| View estável / overlays abertos | Renovação continua, mesmo sem setters LVGL |
| Refreshes de outros elementos | A view ativa continua atendida |
| Saída de PADS / destruição | Latches liberam exclusivamente os tokens dos pads |
| Patch / PANIC | Owner limpa grupos e invalida geração |
| Armazenamento com quiet barrier | Owner já silencia e muda geração antes da gravação |
| Operação síncrona longa / pausa UI | Se ultrapassar o lease, o owner libera por segurança |
| Retomada após Expired | Heartbeat não recria grupos; novo toque usa novo token |
| PadOn anterior à expiração | Rejeitado pela idade/fence de sessão |
| Off antigo | Não libera admissão posterior |
| Wrap de milissegundos | Aritmética modular existente preservada e testada |

### Correção localizada no owner

Um teste reproduziu que configuração inválida era descartada sem publicar Invalid para seu token. Após filtros de geração, idade e cancelamento, o owner agora publica Invalid para uma tentativa nova sem grupo ativo. Configuração inválida nunca substitui um grupo admitido. Um filtro monotônico impede replay de token anterior, inclusive quando a tentativa mais recente foi rejeitada e não há grupo ativo. Os testes de ownership cobrem ambos os casos.

A alteração fica no adapter P4 `amy_runtime.cpp`, sem mudança no upstream AMY, transformações, reserva atômica, timeout, capacidade ou interfaces. Não adiciona loops, alocação, mutex, logs, storage, timers bloqueantes ou trabalho de display ao áudio.

## Ergonomia, contextos e lifecycle

Os oito pads continuam **190×80 px**. A redução de margens permite controles superiores de 44 px, opções de 48 px e RELEASE ALL de 42 px. Os testes verificam dimensões e ausência de sobreposição entre pads/controles/PANIC. Seletores ocupam apenas o corpo PADS, preservando header/PANIC e rodapé.

ROOT oferece 12 notas; SCALE mostra as 11 escalas e marca incompatíveis com `*`; OCT apresenta 0–9 e desabilita escolhas que não geram oito acordes dentro da oitava pretendida e faixa MIDI. Se root/scale posteriormente invalidarem a oitava corrente, o contexto marca INDISP. e os pads afetados são desabilitados. Grupos admitidos continuam liberáveis e preservam as notas congeladas.

O header identifica **GLOBAL**; o corpo identifica **PADS LOCAL: root scale | OCT n**. Não existe sincronização automática nem transformação duplicada. Mudanças locais não escrevem Harmony global, e mudanças globais não reconfiguram os pads.

PRESSED cria intenção; RELEASED/PRESS_LOST liberam Momentary; HOLD alterna pelo token. Trocar modo solicita liberação seletiva. DELETE/destrutor solicitam os latches enquanto os dados dos callbacks ainda existem. Fechar seletor apenas o oculta; outra abertura ou teardown destrói os objetos com ownership definido. Callbacks de opções não navegam nem destroem a própria árvore. HOME/TOCAR/FX/Macros liberam pads sem Panic global. Confirmações destrutivas continuam bloqueando navegação direta, diferida e MIDI.

## Validação host

**PASS**: runner limpo 0.19-A.1 com música pura, owner AMY real, LVGL real, estresse e 19 PNGs; também passaram `ui_018c3a_test`, `ui_018c3_test`, `ui_018c2_test`, `ui_018c1_integration_test`, `ui_018c1a_hardening_test` e `sound_editor_ui_test` (incluindo SMK25/FX).

**PASS**: `run_harmony.ps1`, com Harmony/AMY, USB musical, sustain, bend/modulation, capacidade, controles por canal, desconexão, Touch completo e stress de dois produtores sem alocação no serviço. **PASS**: Arp engine/runtime e comparação PCM inativo contra 0.19-A, SHA256 `11FAB98798FB56EC284F0723D115C829643ED0DC41E46FAC926C023761FA6BF5`.

O manifesto local é `build/harmonic_pads_019a1/test_manifest.json`. O [resumo de proveniência](harmonic_pads_019a1_validation.json) registra SHA, compiladores/flags, digest do manifesto, quantidade/digest dos inputs, binários, logs e as 19 capturas. Todos os hashes de fonte foram conferidos novamente após a execução. O dirty registrado inclui README/documentação e evidências geradas; a implementação estava commitada. Nenhum input mudou durante a validação.

Cobertura: tríades/sétimas, 4+4 vozes, rejeição atômica, notas compartilhadas com gates distintos, USB/Touch simultâneos, sustain, bend/modulation, desconexão USB sem liberar pads, Harmony global sem segunda transformação, Off antigo, fila cheia, teardown pendente, press lost, mudança de modo com dois acordes, patch/Panic, wrap, HOLD acima de dez segundos, interrupção/retomada do heartbeat, PadOn anterior à expiração, replay de rejeição, Invalid preservando grupo ativo, seletores, notas congeladas, navegação e snapshots antigos.

Estresse: 1000 press/release; 500 idas/voltas PADS/MACROS; 1000 mudanças de root (D/C em 500 iterações); 1000 alternâncias de modo nessas iterações; mais de 200 aberturas/fechamentos de seletores; 200 ciclos HOLD/RELEASE ALL; 100 expirações; 100 saturações; 100 Panics com grupo ativo; 1000 refreshes estáveis. Nenhuma asserção foi enfraquecida.

| Medida host da execução final | Resultado |
|---|---:|
| Refresh estável, 1000 vezes, HOLD ativo | 0 alterações de labels; 0 de estilos; 1000 heartbeats |
| Objetos LVGL, antes/depois do stress de pads | 115 / 115 |
| Memória LVGL livre, antes/depois | 65184 / 65448 B |
| Maior bloco livre final | 65424 B |
| Fragmentação final | 1% |
| Pico no estágio dos pads | 65168 B |
| Gates ativos / comandos UI descartados no fim | 0 / 0 |
| Alterações efetivas durante cenários/stress da view | 28818 labels / 7282 estilos |
| Ciclo press/release host, mediana / P95 / máximo | 515.6 / 571.5 / 1009.4 us |
| Owner: transform / admission máximo | 6 / 65 us |
| Alocações no trecho musical instrumentado | delta 0 |
| Stress de navegação: objetos / memória livre | 94 / 94; 73744 / 73744 B |
| Stress de navegação: pico / fragmentação | 69144 B / 2% |
| Navegação host: mediana / P95 / máximo | 3886.0 / 4587.0 / 7217.9 us |

O seletor fechado fica alocado, portanto a topologia de 115 objetos inclui o overlay e não equivale aos 86 objetos sem seletor da 0.19-A. A medição confirma ausência de crescimento líquido no pool LVGL e nos objetos. A view C++ é medida separadamente; não se afirma uma medição de RSS completo do processo.

Comparação isolada de 1000 refreshes READY, sem render de framebuffer, com wrappers dos mesmos setters LVGL e flags O2 do manifesto:

| Revisão | Chamadas label / cor | Mediana / P95 / máximo |
|---|---:|---:|
| 0.19-A, código e objetos de origem preservados | 9000 / 8000 | 8.10 / 8.20 / 11.20 us |
| 0.19-A.1 | 0 / 0 | 2.90 / 3.00 / 6.20 us |

São chamadas de API, não contagem de pixels invalidados. A instrumentação integrada também confirma zero alterações efetivas e heartbeat contínuo com HOLD ativo. A sonda suplementar e seus executáveis ficam em `build/refresh_probe.cpp`, `build/run_refresh_probe.ps1` e `build/refresh_probe_{baseline,current}.exe`; foi executada com `powershell -ExecutionPolicy Bypass -File build/run_refresh_probe.ps1` e `-Current`. Os digests dos logs estão no resumo de proveniência.

Tempos host, inclusive o service máximo de Arp de 15 us na regressão específica, não predizem carga, deadlines ou latência física do P4. O contador do owner mede gates ocupados; caudas de release AMY seguem a política existente.

## Builds ESP32-P4 — PASS

| Perfil | RAM estática 0.19-A / A.1 | Delta | Flash 0.19-A / A.1 | Delta | Imagem A.1 |
|---|---:|---:|---:|---:|---:|
| guition-editor-8 | 71084 / 71096 B | +12 B | 1533036 / 1535010 B | +1974 B | 1535744 B |
| guition-editor-8-bench | 71636 / 71644 B | +8 B | 1638428 / 1640406 B | +1978 B | 1641136 B |

Referências: medidas publicadas da 0.19-A. Finais: size checks executados. Ambos os auditores passaram: oito vozes, LVGL único, pool de 128 KiB, 48000/256 e gates elétricos existentes. Nenhum warning ou erro de linkagem foi emitido pelos builds P4 executados. Host C mantém warnings herdados de AMY (parâmetros sem uso e casts de ponteiros para unsigned long no ABI Windows); C++ usa Werror.

Runtime no ELF: 15480 B editor, 15520 B bench, iguais à origem. Command/Commands/Snapshot host: 28/5448/2752 B, sem aumento. View C++: 664 B host (antes 584, +80) e 496 B ABI P4 (antes 424, +72), aferidos pelos compiladores em retornos constantes de sizeof. Essa alocação é externa ao pool LVGL. Filas e stacks de tarefas não mudaram; stack high-water é MANUAL.

SHA256 dos binários:

- editor: `8F8A4FA07E3F8DCB06AF9AC31B5A7D74828E55BF97450989A08BE6DEA88972B6`
- bench: `074DE32EF51C693565266D25BEE19C31AA67F0B090129440C266840B3DE4C86C`

## Comandos executados

```powershell
powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_harmonic_pads_019a1.ps1 -Clean -Regressions
powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_harmony.ps1
powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_arp_015.ps1 -Runtime -BaselineRoot C:/Users/devx/Documents/PlatformIO/Projects/amy-harmonic-pads-019a
powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_amy.ps1 -Voices 8 -SoundLibrary
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8 -j 8
python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --environment guition-editor-8
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8-bench -j 8
python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --musical --environment guition-editor-8-bench
```

A suíte antiga 0.19-A descreve a UX cíclica anterior; usar o novo runner para esta versão. Um teste adicional de owner inicialmente reproduziu a falha de Invalid; passou após a correção. Uma execução exploratória falhou por LVGL temporariamente ausente durante preparação de dependências bench; outra foi encerrada ao congelar o limite MIDI. Essas execuções não contam como PASS de proveniência. A execução final separa preparação de dependências e valida hashes antes/depois.

## Capturas

[19 PNGs LVGL reais, 800×480, HOST SIM](harmonic_pads_019a1_shots/README.md). ACTIVE e vozes vêm do runtime AMY real. USB e NVS usam as fixtures host existentes. Apenas a marca HOST SIM identifica o ambiente; nenhum estado musical foi forçado visualmente. As capturas principais usam Scene/patch válidos, e a confirmação destrutiva foi recapturada após restaurar essa fixture limpa.

## Riscos e qualificação física

FAIL herdado e separado: `run_amy.ps1 -Voices 8 -SoundLibrary` reproduziu `polyphony_test.cpp:82`, `probe_peak_bytes()-bytes<=kLibraryEngineHeapBudgetBytes`, já documentado na 0.19-A. Nenhum orçamento, limite ou asserção foi alterado para obter PASS. O pool LVGL continua 128 KiB e a polifonia continua oito vozes. Outros builds upstream/S3: NOT RUN, sem alterações nesses arquivos.

MANUAL/adiado: jitter de serviço, multitouch, DMA/USB sob carga, stack high-water, RAM interna/PSRAM física, deadlines, latência e saída analógica. Não houve flash nem monitor serial. Nenhum GPIO, VBUS, controlador de display, strap de DAC ou revisão de placa foi presumido ou alterado. Uma pausa UI de dois segundos intencionalmente libera HOLD.

Em sessão futura autorizada, com o candidato já instalado pelo operador:

1. Tocar todos os graus em Momentary e arrastar o dedo para fora. Esperado: acordes corretos e release sem nota presa.
2. Em HOLD, admitir dois acordes, rejeitar um terceiro e usar RELEASE ALL. Esperado: rejeição atômica, histórico separado e zero gates dos pads ao liberar.
3. Sustentar e mudar root/scale/octave. Esperado: notas congeladas; próximos acordes usam PADS LOCAL. Conferir limites de oitava.
4. Manter USB/Touch junto aos pads; mudar modo, sair para FX/Timbres e voltar via TOCAR, inclusive com Arp/Sequencer. Esperado: liberação seletiva. Repetir desconexão/reconexão USB.
5. Pausar só UI por mais de 2000 ms em diagnóstico controlado. Esperado: liberação pelo owner, HOLD EXPIRED na retomada e nenhuma ressurreição; novo toque funciona.
6. Abrir seletores e confirmação destrutiva. Conferir leitura, hitboxes, PANIC acessível e navegação bloqueada durante confirmação. PANIC silencia todos os origins.
7. Registrar underruns reais, máximo render, deadlines, stack high-water, memória e latência sob carga. Compilação não substitui essas medições.

## Veredito e próxima fase

**CONDITIONAL GO para software host e candidatos P4 compilados.** Os critérios funcionais e de pool LVGL desta etapa passaram. A falha herdada SoundLibrary segue separada, e a qualificação física continua adiada. Não é um firmware declarado pronto para palco.

Plano 0.19-B, sem implementação:

1. Auditar `main/controller_profile.h`, `main/controller_profile_nvs.h`, `docs/smk25_0142_physical_banks.md` e `docs/smk25_0142_pad_shortcuts.md`; confirmar no monitor mensagens reais de Note On/Off, velocidade zero, canal e banco. Não assumir números pelo nome SMK25.
2. Especificar Performance Pads explícito, ativável e reversível, preservando o mapeamento atual quando desativado.
3. Reutilizar intenção de pad com identidade independente de notas USB comuns; definir tokens, HOLD, PANIC, desconexão/reconexão, mudança de modo e admissão atômica em oito vozes.
4. Testar fontes simultâneas e gerações antigas no owner antes da UI indicar acionamento físico. Qualificação elétrica/acústica continua sendo uma sessão separada.

Não implementados: acionamento SMK25, Pad-to-Arp, progressões, bancos de acordes, editor de acordes, persistência em Scenes ou novo NVS. O próximo menor passo é revisar o perfil MIDI confirmado para especificar a primeira fatia da 0.19-B.
