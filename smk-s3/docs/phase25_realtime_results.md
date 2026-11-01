# Fase 2.5 — consolidação temporal

Data: 2026-11-01. Auditoria iniciada em `9dcc4a8a07b99cc1071ed3cf9012a86c5a5f3336`, árvore limpa. Comparado a `3f4054ed1fd55517cbd58c56592b786d6915e674`, somente `library.properties` difere. Branch `hardening/smk-s3-realtime`.

## Auditoria e classificação

| Classe | Evidência | Decisão |
| --- | --- | --- |
| Defeito comprovado | Teste de 500.000 publicações concorrentes falhou associando timestamp de outra publicação ao tick. | Snapshot protegido por seção crítica curta no ESP; mutex no host, sem seqlock informal. |
| Risco de concorrência | Callback verificava running/source antes de Stop/reset e publicava depois; contador era reiniciado fora da mesma exclusão. | Transições e callback compartilham exclusão temporal; APIs de timer serializadas entre tarefas, nunca chamadas sob a seção crítica. |
| Defeito comprovado por inspeção | Callback atrasada avançava apenas um tick; timestamp novo escondia o atraso musical. | Projeta deadlines vencidos em O(1), publica somente o último com timestamp lógico. |
| Limitação arquitetural | Controle executa SPIFFS/NVS/UI; console também acessa objetos de controle. | Isolar operações comprovadas com ownership explícito; medir em placa. |
| Ausência de medição | Nenhuma captura MIDI/analógica ou duração física de Flash nesta sessão. | MANUAL, sem inferir latência física por contadores internos. |
| Comportamento correto preservado | EventBus reserva releases e usa geração/Panic; AMY owner único, PCM geracional, notas repetidas por referências. | Manter regressões existentes. |
| Conflito com premissa do pedido/relatório anterior | `src/api.c:amy_sysclock64/amy_add_event` usa **milissegundos**, não amostras; `src/amy.c:amy_execute_deltas` avalia antes do bloco. | Corrigir documentação; não declarar precisão por amostra nem mudar core AMY. |

## Entrega A — clock/mailbox

Todos os campos do mailbox, inclusive posição consumida/reset, são protegidos juntos. ClockManager usa uma seção crítica temporal para contador, deadlines, callback e transições; getters públicos continuam atômicos. API de controle usa mutex recursivo entre tarefas para serializar start/stop/reconfiguração; callback nunca adquire esse mutex. Ordem fixa: API → temporal → mailbox. Nenhum log/API de timer dentro da seção crítica da callback. Reset invalida a publicação anterior sob a mesma exclusão; não há escritor antigo que sobreviva ao reset. Source swap mantém posição musical, descarta publicação pendente e reinicia fase/estimativa de período. Start reinicia posição; Stop remove pendência. Callback residual antes do primeiro deadline novo é ignorada. BPM mantém posição e inicia novo período a partir da mudança. Swing permanece nos geradores.

Validação e demais entregas serão registradas abaixo.

## Entrega B — política musical

Note On físico sem deadline é aplicado na primeira janela disponível (não há conhecimento do gate futuro). Geradores aceitam atraso menor que meia pulsação PPQN; a razão é configurável em `MusicalTiming`. O mínimo gate é uma pulsação. Onsets vencidos não são recuperados. Arp mantém a posição da lista quando descarta onset; sequenciador avança posições e chains por aritmética limitada, sem executar os passos perdidos. Ratchets de um onset descartado ficam desarmados. Gates vencidos liberam notas antes de novos triggers, com comparação wrap-safe.

Eventos gerados guardam origem lógica e deadline de gate até o owner. Caso expirem na FIFO/owner, Panic invalida a geração inteira e solicita limpeza dos geradores: sem ownership por origem, descartar somente o On e posteriormente executar seu Off poderia liberar uma referência de outra origem. Essa recuperação conservadora pode silenciar notas válidas sob sobrecarga e é intencional. Stop/All Notes Off agora são barreiras de geração e cancelam cena pendente; Panic também para clock e limpa a cena. Cena pendente em tick atrasado aguarda o próximo beat válido; não é recuperada em rajada.

`SynthEvent` passa de 16 para 24 bytes, ainda limitado e sem heap no roteamento; FIFO padrão cresce 2 KiB, além do alinhamento dos metadados de Entry. Não houve alteração de formato persistente. Nova espera do controle é 1 ms; a referência temporal continua sendo esp_timer/deadlines, não a contagem de delays da tarefa. Custo/prioridades reais precisam de bancada.

PASS: `./tests/run_smk_safety.ps1` (inclui testes de clock real com tempo simulado e geradores em 30/60/120/180/300 BPM, swing 50/67/75, coalescência, expiração e Panic). PASS: `./tests/run_smk_synth_expansion.ps1` após mudança de posições perdidas. Regressão AMY owner inclui expiração dentro da fila antes de executar On.

## Entrega C — ponte AMY experimental

`AmyTimeBridge` é infraestrutura isolada, sem chamada no firmware e desabilitada por padrão. Requer habilitação explícita no argumento experimental de `anchor`. Converte µs para frames desde uma âncora fixa com aritmética inteira de 64 bits, taxa efetiva, ordem FIFO monotônica, clamp de evento vencido, geração, limite de lookahead e rejeição de overflow. Converte então frames para `amy_event.time` em ms de 32 bits, inclusive rollover e sentinela UINT32_MAX. Não cria scheduler.

Ativação no owner foi adiada: AMY avalia deltas antes do render, `amy_sysclock64()` deriva dos blocos produzidos, o ring mantém até dois blocos adiantados e I2S tem quatro buffers DMA. Amostrar esp_timer no render não ancora automaticamente a reprodução. AMY soma `latency_ms`; eventos vencidos são atendidos na próxima avaliação e a ordenação usa comparação wrap-relative de meia faixa. Patches materializam eventos/deltas no owner e podem alocar; Panic chama `amy_deltas_reset()` para cancelar deltas futuros. O único produtor AMY após boot passa pela fila limitada; isso não garante prazo de materialização de patch.

Não se alterou a timeline global nem o core AMY. Antes de ativar: medir lead PCM/DMA e drift, definir âncora de reprodução, validar patches/Note Off/CC/sustain no mesmo domínio e limites do pool de deltas. O engine atual tem resolução de ms e execução por bloco: não é sample-accurate.

PASS: teste isolado de conversão (44.1/48 kHz, bloco atual/próximo, atraso, lookahead, geração, overflow, rollover/sentinela e ausência de truncamento acumulado). A suíte AMY real conserva a regressão de deltas futuros cancelados por Panic e passou com expiração no owner.

## Entrega D — operações lentas e telemetria

Worker de storage no core 0, prioridade 2, stack 8 KiB, um Job pré-alocado (2.064 bytes no host). Protocolo Idle → Filling → Pending → Working → Done → Idle com release/acquire. O controle reserva sem bloquear e copia valores estáveis; o worker só toca o Job e StorageManager. Não recebe referência a patches/cenas/perfis vivos. Resultado fica estável até o controle liberá-lo. Capacidade um, rejeição visível/busy contabilizada e sem backlog ilimitado. Falha de criação deixa runtime storage indisponível com erro explícito.

| Operação | Antes / frequência | Depois | Pior duração física |
| --- | --- | --- | --- |
| MIDI Learn saveProfile | Controle, fim do wizard | Snapshot ao worker; QUEUED e conclusão separadas | MANUAL |
| Long hold: patch + perfil + 8 cenas | Controle, ação deliberada | Um snapshot de bundle ao worker; sucesso só se todas as escritas concluírem | MANUAL |
| Captura de cena | Controle, gesto de pad | Captura em RAM e save de snapshot no worker | MANUAL |
| Load de perfil USB | Controle, conexão/reconexão | Worker carrega específico/fallback; controle aceita token vigente e aplica com Panic | MANUAL |
| NVS boot / cenas boot | Antes do loop musical | Permanece síncrono na inicialização | MANUAL |
| NVS e load/save de console | Tarefa console prioridade 5 | Permanece síncrono nessa tarefa; I/O StorageManager serializado com worker | MANUAL |
| Troca de patch / macros | Controle → fila AMY | Preservada; materialização no owner pode consumir deadline | MANUAL |
| Switch de tela | Controle podia chamar brightness/invalidate do driver | Flag atômica; hardware executado na tarefa UI | MANUAL |
| Logs | Controle/console, vários caminhos | Telemetria agregada disponível em `status`, sem novos logs por evento | MANUAL |

StorageManager serializa I/O entre worker e console com mutex de tarefa; getters de capacidade/uso são atômicos. Controle não adquire esse mutex. São preservados formatos/CRC/migrações e snapshot sem dupla aplicação de macros. SaveProfile antes ignorava retorno de fwrite e todos os saves ignoravam fclose; agora falhas são propagadas. Não há nova garantia de tolerância a queda de energia: perfil/cena ainda usam escrita direta preexistente, patch usa temporário seguido de remove/rename (não é transação multi-arquivo). Um bundle pode salvar parcialmente antes de reportar falha.

Enquanto o perfil carrega, usa-se o default já existente; um perfil concluído válido substitui-o com Panic, que pode interromper notas tocadas nesse intervalo. Token cancela resultado após desconexão/nova conexão/wizard concluído. Falha mantém default. Nenhuma alteração de mapeamento persistido foi feita.

PASS: regressão host com I/O deliberadamente retido no mutex enquanto FIFO aceita release/Panic, capacidade um, snapshots, CRC de patch, fallback de perfil, erro real de caminho e falha de criação da tarefa. Esse teste demonstra independência do roteamento; não mede stalls de cache/Flash do ESP32.

Métricas novas: tentativas storage busy/rejeitadas, falhas, máximo de duração de Job, triggers late, lotes de releases late, recuperações por deadline. Valores atômicos expostos por `status`, resetados com `audio_reset`. Max render continua incluindo fase de comandos + síntese + cópia. PCM starvation e erros I2S permanecem distintos de underrun DMA não medido. Timestamp futuro é limitado a idade zero no owner, sem underflow de diagnóstico.

Persistem riscos preexistentes de UI/console lendo/mutando objetos de controle em paralelo e logs síncronos no controle. O worker não recebe esses objetos nem amplia esse compartilhamento. Eliminar todas essas condições exige ownership de UI/console em outra entrega; não se declara deadline garantido ou prontidão de palco.

## Entrega E — build, regressões e revisão

Correções encontradas durante validação:

- O Xtensa define uint32_t como unsigned long; `std::max(1u, uint32_t)` falhava mesmo passando no MinGW. Agora o template usa tipo explícito.
- Snapshot inclui tick, timestamp, período, fonte e geração sob a mesma exclusão. O controle não reconstrói o período a partir de um BPM que pode ter mudado depois de consumir o tick.
- Clock externo com timestamp anterior à seleção da fonte/Start é ignorado. O timestamp de Start recebido é preservado.
- Preset aceito na fila AMY solicita barreira de geração e limpeza dos deltas futuros antes da substituição. Um teste de mutação sem essa barreira produziu áudio antigo após troca de patch e falhou como esperado. Coalescência de CC/parâmetros não atravessa nota/preset nem reutiliza slot de geração invalidada.
- Teste AMY real fixa evento +1 ms dentro do bloco atual: não executa no meio do bloco, executa no bloco seguinte. Evento já vencido executa na próxima avaliação.
- Releases geradas carregam deadline lógico até o owner; são sempre executadas. Contadores distinguem lotes atrasados no gerador de releases individuais atendidas depois do deadline no owner. EventBus tem high-water próprio.
- Contador de triggers late conta onsets/ratchets elegíveis suprimidos e eventos expirados em fila; ticks coalescidos são contados separadamente. Não estima quantas notas teriam passado pela probabilidade em passos que nem foram avaliados.

### Ambiente e reprodução

Ambiente local encontrado e usado: Python 3.11.7 em `C:/.platformio/penv/Scripts/python.exe`, PlatformIO Core 6.1.18; plataforma já instalada pioarduino 55.3.36 (release 55.03.36-1), ESP-IDF 5.5.2 (`framework-espidf 3.50502.0`), Xtensa GCC 14.2.0+20251107, CMake 4.0.3, Ninja 1.13.1. O Python 3.14 do PATH não foi usado para PlatformIO. Não houve upgrade de engine/plataforma: a URL da versão instalada foi fixada para evitar resolução variável de `platform = espressif32`.

Builds locais usam cópia gerada de sdkconfig dentro de cada diretório `.pio/build/<env>` e os arquivos versionados `sdkconfig.defaults.128`/`.256`. A definição AMY_BLOCK_SIZE=256 duplicada no platformio.ini foi removida: Kconfig/CMake determina ambos engine e saída. O sdkconfig versionado não é regravado pelo build. `pio run` sem `-e` conserva somente o ambiente padrão 256.

**Conflito de hardware existente, preservado:** o snapshot local `sdkconfig.esp32-s3-devkitc-1` seleciona painel square 240×240/inversão; specs.md e defaults genéricos/CI selecionam 284×76. Os builds PlatformIO usam o snapshot local como base para não trocar a interface do protótipo. CI conserva defaults genéricos. Esta fase não decide qual painel físico é final e não muda pinagem, offsets ou cores. Não confundir compilação de um perfil com validação física do outro.

CI agora cobre IDF 5.3 **e** 5.5.2, cada um em 128/256; configura sdkconfig isolado e executa também a suíte UI. O manifest declara IDF >=5.3,<5.6 em acordo com os componentes esp_driver_* usados. O ambiente IDF 5.3 não está instalado localmente e o workflow remoto não foi disparado nesta sessão: sua execução continua NOT RUN. A opção USB gerenciada mantém a declaração preexistente ~1.4.0 condicionada a IDF >=5.4.

Comandos executados da raiz (PowerShell):

```powershell
& C:/.platformio/penv/Scripts/python.exe --version
& C:/.platformio/penv/Scripts/python.exe -m platformio --version
./tests/run_smk_safety.ps1
./tests/run_smk_synth_expansion.ps1
./tests/run_smk_amy_boot.ps1
./tests/run_smk_patch_integrity.ps1
./tests/run_smk_ui.ps1
& C:/.platformio/penv/Scripts/python.exe -m platformio run -d smk-s3 -e esp32-s3-devkitc-1
& C:/.platformio/penv/Scripts/python.exe -m platformio run -d smk-s3 -e esp32-s3-devkitc-1-128
git diff --check
```

Para outra máquina, usar o mesmo PlatformIO Core em um ambiente Python 3.11 compatível e executar `python -m platformio` desse ambiente. Os caminhos C:/.platformio acima identificam este ambiente, não um requisito universal. O script `build_firmware.ps1` de ESP-IDF nativo foi preservado; a matriz CI mostra os comandos idf.py com os mesmos defaults de blocos.

### Separação das medições

A. Idade de recepção→controle e origem/enqueue→owner, atrasos de clock/release e duração de Job/render: contadores internos de firmware, coletáveis via `status` e `audio_status`.

B. Capacidade de filas/buffers: EventBus 256 entradas + reserva, mailbox AMY 64 + reserva 8, consumo máximo de 16 comandos/bloco, PCM dois blocos e DMA quatro blocos. A 48 kHz: bloco 128 = 2,667 ms; bloco 256 = 5,333 ms. Capacidade PCM é 5,333/10,667 ms e DMA 10,667/21,333 ms. Isso não é atraso observado nem deve ser somado como uma medição de latência.

C. MIDI físico→saída analógica, jitter audível, stalls de Flash/cache, underrun DMA real e margem de tarefas: MANUAL. Não foram medidos nesta sessão. Counters `frames_rendered` e agregados de 32 bits são de janela e podem dar rollover em execução prolongada; usar reset/coleta por janelas. `audio_reset` concorrente é aproximação da fronteira da janela, não snapshot transacional do sistema inteiro.

### Procedimento físico pendente

1. Confirmar placa/revisão, GPIOs existentes, alimentação/VBUS, straps PCM5102A e perfil/controlador de display. Manter o painel configurado para o hardware confirmado. Usar a USB-UART para console e USB nativa em Host.
2. Para cada bloco 128/256, iniciar em silêncio e executar `audio_reset`, `status`, `audio_status` e `memory`. Registrar sample rate, memória, filas, PCM gaps, I2S errors e max/avg render. Não esperar valores físicos específicos sem captura.
3. A 30/60/120/180/300 BPM, testar swing 50/67/75, arp e sequenciador, acordes rápidos, sustain e oito knobs. Capturar MIDI/clock e saída analógica simultaneamente. Esperar ausência de rajadas de passos perdidos, releases preservadas e zero notas presas após Panic.
4. Durante reprodução, executar long hold save, captura de cena e wizard. Confirmar QUEUED seguido de SAVED ou SAVE FAILED; repetir com storage cheio/falha. Observar `storage_max_job_us`, HWM, late triggers/releases, recuperação e render deadline. Flash pode suspender caches mesmo em worker: procurar gaps na captura.
5. Saturar tráfego, acionar Stop/Panic, desconectar e reconectar pelo menos 100 vezes, inclusive durante save. Esperar silêncio sem ressurreição de notas antigas e recuperação sem reboot onde o lifecycle já suporta. Perfil antigo concluído após disconnect não deve substituir o da conexão seguinte.
6. Trocar patches/FX durante acordes e durante eventos pendentes; verificar nenhuma nota antiga reaparece, nenhuma mudança não solicitada de timbre/velocity/volume/mapa e feedback de falha consistente. Um reset de geração pode silenciar notas em sobrecarga.
7. Executar soak de pelo menos duas horas por configuração, anotando resets/watchdog/memória mínima e falhas. Salvar capturas e máximos por janela; comparar MIDI→analógico medido separadamente de A/B antes de aprovar uso em palco.

### Limitações e entrada para Fase 3

Implementação de A–D e infraestrutura de build são entregues incrementalmente; a qualificação de performance não é encerrada por compilar. Entrada para ampliação musical: executar matriz IDF 5.3 remota, confirmar perfis de hardware, passar procedimento físico acima, medir stalls/logs/UI e resolver ownership restante de UI/console se afetar a previsibilidade. Ativação da ponte AMY requer âncora de reprodução validada e testes de integração; continua desabilitada. Não foi alterado core upstream AMY nem adicionada dependência de runtime.

### Resultados host finais

| Status | Comando / evidência |
| --- | --- |
| PASS | `./tests/run_smk_safety.ps1`: MIDI/EventBus, USB, áudio/I2S, CLI, mailbox concorrente/reset, ClockManager simulado, política musical e ponte temporal. |
| PASS | `./tests/run_smk_synth_expansion.ps1`: regressões de sequenciador, macros/presets/CRC/migrações e novo worker com I/O bloqueado. |
| PASS | `./tests/run_smk_amy_boot.ps1`: boot/256 patches/referências/sustain/mono e AMY owner real; tempo de ms/bloco, deadline de release, expiração e barreiras Panic/preset. |
| PASS | `./tests/run_smk_patch_integrity.ps1`: FM, FX, freeze e persistência existentes. |
| PASS | `./tests/run_smk_ui.ps1`: perfis 160×128/240×240/284×76, limites geométricos, cabeçalho e troca de tela sem brightness no chamador. |
| PASS | Mutação de teste retirando somente barreira de preset: versão inválida falha na asserção de silêncio; versão corrigida passa. Teste original de mailbox concorrente também falhou antes da correção. |
| PASS | `git diff --check`. |
| NOT RUN | CI remota IDF 5.3; não há instalação IDF 5.3 nem Docker disponível no ambiente local. Nenhum build de outra plataforma AMY foi executado; fontes upstream permanecem intactos. |
| NOT RUN | Sanitizador de threads e comprovação exaustiva de todos os interleavings em ESP32. Testes de threads reais de host não substituem prioridades/memória do ESP32. |
| MANUAL | MIDI físico→analógico, cache/Flash, CPU/heap/stacks em placa, USB/reconnect sob carga, continuidade e qualidade de áudio. |

Avisos host de conversão de ponteiro para long em diagnósticos C do AMY são preexistentes e não foram suprimidos ou refatorados. O primeiro build firmware falhou no novo std::max com tipos distintos; corrigido antes da matriz final. Logs completos locais estão em `build/phase25_*.log` (artefatos ignorados pelo Git).

### Commits e arquivos

| Entrega | Commit / identificação | Propósito |
| --- | --- | --- |
| A | `b806584a` | Snapshot e transições de clock, regressões concorrentes. |
| B | `707c569c` | Deadlines musicais, releases e barreiras Stop/Panic. |
| C | `cf84eeeb` | Conversão AMY experimental isolada, desabilitada. |
| D | `de1848e8` | Worker limitado de storage, snapshots, UI sem I/O de transição no controle e metadados coerentes de clock. |
| E | Commit que contém a versão final deste relatório: `test(smk): pin build matrix and record realtime validation` | Toolchain/matriz, regressões finais, correções comprovadas durante validação e relatório. |

A lista de caminhos abaixo inclui somente alterações desta fase (em relação ao HEAD inicial); nenhum arquivo src/ upstream ou pinout foi alterado.


```text
.github/workflows/smk-s3-build.yml
README.md
smk-s3/components/audio/amy_commands.cpp
smk-s3/components/audio/include/amy_adapter.h
smk-s3/components/audio/include/amy_time_bridge.h
smk-s3/components/midi/event_bus.cpp
smk-s3/components/midi/include/synth_event.h
smk-s3/components/midi/synth_event.cpp
smk-s3/components/sequencer/include/arpeggiator.h
smk-s3/components/sequencer/include/clock_manager.h
smk-s3/components/sequencer/include/clock_timing.h
smk-s3/components/sequencer/include/step_sequencer.h
smk-s3/components/sequencer/src/arpeggiator.cpp
smk-s3/components/sequencer/src/clock_manager.cpp
smk-s3/components/sequencer/src/step_sequencer.cpp
smk-s3/components/storage/CMakeLists.txt
smk-s3/components/storage/include/storage_manager.h
smk-s3/components/storage/include/storage_worker.h
smk-s3/components/storage/src/storage_manager.cpp
smk-s3/components/storage/src/storage_worker.cpp
smk-s3/components/synth/include/scene_manager.h
smk-s3/components/synth/src/scene_manager.cpp
smk-s3/components/system/diagnostics.cpp
smk-s3/components/system/include/diagnostics.h
smk-s3/components/ui/include/ui_manager.h
smk-s3/components/ui/ui_manager.cpp
smk-s3/docs/phase25_realtime_results.md
smk-s3/main/app_config.h
smk-s3/main/app_main.cpp
smk-s3/main/idf_component.yml
smk-s3/platformio.ini
smk-s3/sdkconfig.defaults.128
smk-s3/sdkconfig.defaults.256
tests/mock/esp_timer.h
tests/run_smk_safety.ps1
tests/run_smk_synth_expansion.ps1
tests/run_smk_ui.ps1
tests/test_smk_amy_owner.cpp
tests/test_smk_amy_time_bridge.cpp
tests/test_smk_clock_manager.cpp
tests/test_smk_clock_timing.cpp
tests/test_smk_musical_timing.cpp
tests/test_smk_storage_worker.cpp
tests/test_smk_ui.cpp
```

### Build final (ambos após as últimas alterações de código)

A execução final usa a mesma invocação sequencial de matriz, evitando builds simultâneos que disputem dependências gerenciadas:

```powershell
& C:/.platformio/penv/Scripts/python.exe -m platformio run -d smk-s3 -e esp32-s3-devkitc-1 -e esp32-s3-devkitc-1-128
```

O codemodel CMake foi inspecionado: engine e componente audio recebem AMY_BLOCK_SIZE=256/BLOCK_SIZE_BITS=8 no ambiente 256 e 128/7 no ambiente 128. Ambos conservam o perfil local de display square. Isso verifica a seleção efetiva, não apenas o nome do ambiente.

| Status | Ambiente | Bloco | RAM estática reportada | Flash reportada |
| --- | --- | ---: | ---: | ---: |
| PASS | PlatformIO / ESP-IDF 5.5.2, `esp32-s3-devkitc-1` | 256 | 30.108 / 327.680 bytes | 1.136.743 / 16.777.216 bytes |
| PASS | PlatformIO / ESP-IDF 5.5.2, `esp32-s3-devkitc-1-128` | 128 | 30.108 / 327.680 bytes | 1.136.819 / 16.777.216 bytes |

A invocação final retornou exit code 0, dois ambientes SUCCESS (109,38 s e 108,94 s). Firmware, bootloader e partições foram gerados nos respectivos diretórios `.pio/build/<env>/`. Houve builds completos prévios nesta sessão; a tabela registra os rebuilds finais depois da última correção temporal. Nenhuma alteração de código ocorreu depois desses builds. RAM estática não inclui stacks/filas/heap alocados no boot; memória livre e utilização real em placa continuam MANUAL. Não há baseline comparável medido nesta sessão para afirmar ganho de performance.

A última revisão acrescentou o caso de Play genérico em fonte externa: Start limpa estimativa e cutoff de timestamps da sessão anterior, assim como o Start MIDI. O teste direcionado e a suíte safety foram reexecutados com PASS antes desta matriz final.

NOT RUN: CI IDF 5.3/5.5.2 remota, rollover prolongado do engine AMY físico e toda a carga integrada USB/knobs/arp/seq/save/display em placa. O build local e as suítes host não encerram a qualificação física.

**Estado final:** cinco entregas de código/documentação implementadas, testes host e matriz local 128/256 aprovados; ativação de scheduler experimental e aprovação para performance física permanecem pendentes. Os riscos de Flash/cache, logs síncronos, ownership UI/console, notas agregadas por canal/nota e persistência sem transação de energia foram explicitados. O próximo passo mínimo é executar a bancada documentada, sem adicionar funcionalidades musicais.

## Verificação adicional do ESP-IDF instalado — 2026-11-01

A pedido do responsável, foram inspecionados `C:/esp` e `C:/Espressif` após a entrega inicial.

- PASS: fontes ESP-IDF em `C:/esp/esp-idf`, identificadas como **6.0.0** por `tools/cmake/version.cmake`, `components/esp_common/include/esp_idf_version.h` e execução de `idf.py --version`.
- PASS: ambiente Python `C:/Espressif/python_env/idf6.0_py3.11_env/Scripts/python.exe`, versão **3.11.2**.
- A árvore ESP-IDF não contém metadados `.git`; `idf.py --version` avisa que não obtém a versão por Git e retorna `ESP-IDF v6.0.0` a partir dos fontes.
- Os registros `C:/Espressif/esp_idf.json` e `idf-env.json` indicam versão 6.0 e caminhos antigos de extensão VS Code de outro perfil. Esses registros não foram modificados.
- NOT RUN: build deste firmware com IDF 6.0.0. A instalação encontrada está fora da faixa declarada `>=5.3,<5.6`; esta verificação não amplia a compatibilidade nem substitui os builds PASS com IDF 5.5.2 pelo PlatformIO. IDF 5.3 continua sem instalação identificada nesses locais.

Comando de verificação executado:

```powershell
& C:/Espressif/python_env/idf6.0_py3.11_env/Scripts/python.exe C:/esp/esp-idf/tools/idf.py --version
```

Esta atualização altera apenas documentação; os resultados de build/testes acima continuam referentes ao código entregue, sem novas alterações de firmware.
