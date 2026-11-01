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
