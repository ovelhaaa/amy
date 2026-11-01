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
