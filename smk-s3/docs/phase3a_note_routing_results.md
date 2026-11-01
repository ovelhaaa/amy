# Fase 3A — ownership e roteamento de notas

## A — auditoria (antes da integração)

Base real: `2818a9c6d410ffde9026f21f75ae0e1e94d94123`, branch inicial main,
árvore limpa. O HEAD difere da referência e0489906 do pedido; nenhuma alteração
local foi descartada. Trabalho em feature/smk-s3-note-routing.

Leitura: AGENTS.md, specs.md, README, builds/CI, documentos de ownership,
perfil SMK25 e relatório 2.5; app_main, parser/EventBus, adapter/mailbox,
pads, arp, sequenciador, patches/cenas e src/instrument.c.
README contém marcadores de conflito antigos entre AMY e outro produto P4;
o projeto isolado smk-s3 e sua matriz documentada são a referência de build.
Specs pede 284×76; o perfil local mantém 240×240. Não decidir hardware nesta fase.
A autorização explícita desta fase permite integrar geradores existentes sem
implementar layers/splits/harmonização; a qualificação física anterior continua MANUAL.

| Fonte | Estado anterior / destino | Lacuna comprovada |
| --- | --- | --- |
| USB teclado | parser → EventBus → controle → adapter, synth 1 (canal 9 → drums 10) | Off identifica somente canal/pitch; canais distintos convergem em synth 1 |
| Pads | PadManager, bancos existentes (inclusive acordes de quatro notas) | Release consulta banco atual; mudança de banco/tela pode redirecionar Off |
| Prévia sequenciador | app_main → drums | Release consulta track selecionada no momento do Off |
| Arp | held/latch, último pitch/acorde, EventBus | Sem token do onset; held vector podia crescer além da reserva |
| Sequenciador | playback por quatro tracks, EventBus | Track não acompanha evento; release consulta canal editável |
| Console/UI/interno | comandos tipados e wire, fila AMY | API legada não exprime origem; wire é diagnóstico amplo |
| Patch/cena | PatchManager/SceneManager, preset cria barreira AMY | FIFO anterior ao adapter e estado dos geradores exigem invalidação coordenada |

Comportamento correto preservado: reserva de releases, Panic geracional,
PCM geracional, owner AMY único, deadline por bloco/ms, supressão de onsets
vencidos e voice stealing upstream. AMY poly mantém uma voz por pitch de
instrumento; re-onset reutiliza voz e registra forgotten note. Mono/legato é
pilha do adapter; instrumento de uma voz ignora gestão upstream de sustain.
Sustain poly é estado de instrumento no AMY; estado físico e retenção de
identidade precisam ser distintos no roteamento. Não prometer vozes separadas
para o mesmo pitch no mesmo instrumento.

Invariantes: identidade inclui origem/instância/canal/entrada/token; Off usa
saídas capturadas no On; fan-out limitado e validado antes de emitir; sem heap
no roteamento; tabela cheia rejeita On, nunca libera outra identidade; repetição
física enquanto held é idempotente; tokens gerados crescem sem reset; Stop é
seletivo, Panic global; patch invalida estado e filas anteriores. Um writer da
tabela, na tarefa synthesis owner, usa o transporte existente. Nenhum core AMY
precisa ser alterado.

Arquivos previstos: roteamento/adapter, metadados de eventos, geradores,
integração app/pads/patch, testes host e este relatório. Nenhuma pinagem,
polifonia padrão, formato persistido ou dependência nova. Aceitação: regressões
host, integração AMY real, builds ESP32-S3 128/256 e procedimento físico separado.

Resultados B–E serão acrescentados após validação.
