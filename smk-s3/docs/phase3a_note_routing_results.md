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

## B/C — arquitetura resultante

USB/parser → EventBus com gerações global e por origem → controle musical →
fila AMY → NoteOwnership no amy_owner → AmyAdapter → AMY. Pads usam a mesma
representação de rota, copiada diretamente para a fila existente. A tabela tem
um único writer, o synthesis owner. UI/USB não acessam estruturas AMY.

NoteIdentity contém origem, instância, canal físico, entrada e token uint64.
Fontes: Keyboard, Pad, Arpeggiator, Sequencer, Internal e ExternalMidi (console
normalizado). Há 16 instâncias por origem: tracks 0–3, pads 0–7; UI reserva
instância interna 15. Um transporte USB é suportado. Múltiplos dispositivos ou
cabos USB ainda precisam de instância explícita no parser.

NoteRoute guarda identidade, velocity e até quatro pares {instrumento, pitch}.
Off consulta as saídas aceitas no On. Instrumentos atuais: 1 e 10; destino 2 é
representável, mas rejeitado até existir layer alocada/validada. Pitch zero é
rejeitado, pois AMY usa essa sentinela e não suporta seu onset. Nenhuma layer,
split, harmonização ou transformação nova foi adicionada. Acordes existentes
de pads são preservados; teste fictício de fan-out não cria recurso de produção.

Capacidade: 64 entradas. Orçamento musical: 25 teclas +8 pads +16 saídas de arp
+4 tracks =53, deixando 11 entradas de margem. Sustain/repetições podem ocupar
essa margem. Tabela cheia rejeita On, conta note_identities_rejected e preserva
releases já aceitos. Não faz Panic por falta de identidade nem emite Off sem On.
A polifonia AMY padrão permanece oito vozes e o container de drums existente.

Política física (token zero): On repetido enquanto held é idempotente, inclusive
em pads, sem acumular referências ou retrigger. Após Off físico, uma nova pressão
pode coexistir com a anterior retida pelo pedal. Off duplicado/sem On é ignorado.
Isso altera deliberadamente o retrigger de mensagens On repetidas sem Off:
fontes que precisam desse comportamento devem produzir pares ou tokens próprios.
MIDI físico exige ordem FIFO; sem token de protocolo, um Off arbitrariamente
reordenado após uma nova pressão não é distinguível de seu Off legítimo.

Arp/seq usam tokens monotônicos, preservados em Stop/Panic/patch/pattern. Cada Off
carrega o token do onset. Rollover uint32 não faz alias; em UINT64_MAX o gerador
rejeita onsets, conta falha e não reutiliza zero. Watermark por origem/instância
rejeita replay após release/reset. Novos geradores devem publicar tokens em ordem
por instância; não devem recapturar uma geração nova para publicar trabalho antigo.
A produção de ticks atual permanece na tarefa de controle.

Referências AMY são contadas por destino/pitch, com uint16 e duas tabelas de 128
pitches. Isso corrige liberação prematura entre canais convergentes no synth 1 e
suporta 256 referências (64×4). Notas iguais no mesmo instrumento continuam
compartilhando voz/retrigger upstream. Ownership isola liberações; não promete
envelopes ou vozes independentes por origem para o mesmo pitch.

Pads/preview deixam de consultar banco/tela/track atual no release. Tracks também
guardam o canal original. Arp distingue canal físico de entrada e limita seus
vectors held/latch às 16 entradas já reservadas, sem crescer no caminho de notas.

## D — sustain, Stop, Panic e transições

A tabela distingue held, liberação física e retained por pedal. Pedal é por canal
físico. Off sob sustain conserva identidade, saídas e referências até pedal-up.
Isso também funciona em mono, cujo container upstream de uma voz não administra
sustain. Envelope em release e oscilador alocado continuam pertencendo ao AMY.
note_refs inclui referências retidas por pedal, não mede somente teclas nem vozes
DSP. A apresentação existente da UI satura em 255; o adapter conta 256.

API legada note/CC continua disponível para compatibilidade e regressões; CC64
legado usa sustain AMY. Novas fontes devem usar routeNoteOn/Off/routeSustain.
O fluxo normal do console usa eventos normalizados e está roteado. Misturar APIs
legadas ou wire arbitrário com ownership não é um contrato suportado: wire
continua diagnóstico amplo capaz de alterar instrumentos fora do roteador.

| Ação | Política |
| --- | --- |
| Stop físico, ButtonPress ou MIDI transport | Para clock, cancela cena pendente, limpa arp/seq e libera somente suas identidades, inclusive retidas. Teclado/pads permanecem. |
| Arp disable/reset | Fence de origem + SourceStop, sem esquecer nota ativa. |
| Seq Stop/Play | Invalida onsets pendentes da execução anterior; tokens não reiniciam. |
| Mute/solo | Impede novos triggers; notas já aceitas terminam no gate original. Não corta outros módulos. |
| Pattern manual | Cancela eventos/playback pendentes da origem sequenciador e conserva transporte/posição. Chains automáticas mantêm gates e política anterior. |
| Panic, USB disconnect/reset | Barreira global existente: limpa held/latch, transport, tabela, pedal, mono, referências, deltas e PCM antigo. Disconnect permanece deliberadamente global. |
| All Notes Off, CC120/123 | Mantém reset global conservador anterior. |
| Patch/scene | Callback invalida FIFO anterior, limpa arp e para seq. PatchManager solicita Panic fora da capacidade da fila antes do preset; assim cancela deltas/tabela/pedal mesmo se o preset for rejeitado. Preset aceito mantém sua barreira AMY. Cena pode reativar seq conforme estado salvo. |
| Deadline vencido / release impossível na fila | Mantém recuperação global e limpeza dos geradores da Fase 2.5. |

EventBus/mailbox mantêm gerações independentes por origem. Entradas são estampadas
sob exclusão existente. SourceStop invalida comandos antigos e libera entradas
ainda vivas. Não antecipa Off ao On; reserva de releases e fallback Panic ficam
preservados. SourceStop atravessa MIDI Learn. O release físico também consulta a
rota direta quando arp mudou de modo, evitando nota esquecida nessa transição.

Drums usam IGNORE_NOTE_OFFS upstream: Stop seletivo encerra ownership mas conserva
caudas naturais de one-shots. Panic continua silenciando todas as vozes. Não há
crossfade nem separação de caudas do mesmo pitch compartilhado.

Mono preserva prioridade da última nota, retorno à anterior, legato sem novo
attack e portamento. Pilha passa a 128 pitches únicos para não esquecer um pitch
aceito acima do antigo limite de 16. Trocar modo limpa notas antes da mudança.
Voice stealing, envelopes e DSP continuam upstream; nenhum arquivo src/ mudou.

## Memória e custo limitado

PASS por sizeof no host: NoteIdentity=16, NoteRoute=32, Entry=40,
NoteOwnership=3.344 bytes (2.560 entradas +768 watermarks +16 pedais),
SynthEvent=32 bytes. A tabela é membro do adapter alocado no boot; não é heap
por nota nem aumento equivalente de BSS.

Estimativa de armazenamento adicional, sujeita ao alinhamento do target:

- Adapter: +3.344 de tabela +112 de mono −1.536 de referências =+1.920 bytes.
- Mailbox: aproximadamente +512 bytes em 64 comandos e +24 de gerações.
- EventBus: Entry 32→40 bytes, +2.048 em 256 entradas, +24 de gerações, além de
  metadados/Panic event. O evento usa o padding para source_instance.
- Tokens/playback/ponteiros/callbacks: pequenas dezenas/centenas de bytes; vectors
  de arp reservam 32 bytes extras no host pelo canal. Contador novo: quatro bytes.

Orçamento principal adicional: cerca de 4,5 KiB mais metadados/geradores, reservado
na inicialização. Memória livre real, PSRAM, cache e stacks permanecem MANUAL.
Não houve dependência nova nem mudança de patch/profile/scene persistido.

On: até 64 comparações de duplicata +64 buscas de slot +4 emissões. Off: até
64 buscas +4 emissões. Pedal-up/Stop: até 64 entradas e 256 saídas. Panic limpa
64 entradas +16 pedais, além do custo existente de AMY/deltas. Mono tem teto de
128 pitches. Sem heap, I/O, log por nota, containers crescentes ou locks novos
no roteamento. O tempo físico de CPU não foi medido.

Fan-out usa um comando em vez de até quatro comandos de pad. Budget permanece
16 comandos/bloco: até 64 emissões AMY por bloco no pior caso de On fan-out, além
das buscas. Limitar operações não prova deadline físico. Deadline, clock,
LatestTickMailbox, PCM/DMA e ponte experimental da Fase 2.5 foram preservados.

## E — validação executada

Comandos da raiz, PowerShell, GCC/G++ do ambiente existente:

```powershell
./tests/run_smk_safety.ps1
./tests/run_smk_amy_boot.ps1
./tests/run_smk_patch_integrity.ps1
./tests/run_smk_synth_expansion.ps1
./tests/run_smk_ui.ps1
& C:/.platformio/penv/Scripts/python.exe -m platformio run -d smk-s3 -e esp32-s3-devkitc-1 -e esp32-s3-devkitc-1-128
git diff --check
```

| Status | Evidência |
| --- | --- |
| PASS | Safety: identidade/origem/canal, duplicatas, Off sem On, capacidade/reuso, fan-out fictício C/E/G, sustain, replay, rollover uint32/exaustão uint64, tokens de tracks e fences; regressões MIDI/USB/audio/clock/timing. |
| PASS | AMY real: notas concorrentes por origem/canal, pedal antes de On, re-press sustentado, pedal por canal, Stop seletivo sob pedal, patch/Panic, mono/legato (note_on_clock estável), retorno ao pitch anterior, portamento e voice stealing. |
| PASS | AMY real: 64 identidades ×4 saídas, 256 referências, rejeição da 65ª entrada, liberações sem consumir outra origem; velocity zero e expiração de rota. |
| PASS | Owner real: produtores concorrentes com rotas +presets/Panic, FIFO/PCM, reserva de releases e SourceStop quando até a reserva está cheia. |
| PASS | PadManager real: acorde existente, banco alterado enquanto held, press duplicado, colisão com teclado e preview liberado em outro banco. |
| PASS | PatchManager real: callback limpa FIFO/seq, sustain e delta AMY futuro cancelados mesmo quando o preset não cabe na fila. |
| PASS | Mutação somente em cópia ignorada de PatchManager (Panic→AllNotesOff): teste falha em silent[i]==0; versão corrigida passa. Executável mutante retorna -1073740791, falha esperada. |
| PASS | Macros/presets/CRC/migrações, StorageWorker, FM relativo e integridade de operadores, FX/freeze/persistência e UI nos três perfis. |
| PASS | Matriz local ESP-IDF 5.5.2 / PlatformIO 128 e 256, tabela abaixo. |
| NOT RUN | CI remota IDF 5.3/5.5.2, builds de outras plataformas AMY e sanitizadores de threads. |
| MANUAL | USB/áudio/latência analógica, stalls Flash/cache, carga integrada, heap/stacks e soak físico. |

Os primeiros comandos de compilação encontraram asserts antigos de 24 bytes e
um overload ambíguo no teste novo; ambos corrigidos. Avisos host de casts de
ponteiros em diagnósticos AMY são preexistentes. Suítes finais retornaram zero.
Logs locais ignorados: build/phase3a_safety.log, phase3a_amy.log,
phase3a_patch_final.log, phase3a_expansion_final.log, phase3a_ui_final.log,
phase3a_firmware_final.log e phase3a_mutation.log. A mutação foi compilada com
G++ e as mesmas fontes/flags da suíte patch_integrity, substituindo somente
patch_manager.cpp por build/smk_patch_integrity/patch_manager_without_barrier.cpp.

| Status | Ambiente | Bloco | RAM estática | Flash |
| --- | --- | ---: | ---: | ---: |
| PASS | PlatformIO / IDF 5.5.2 | 256 | 30.116 /327.680 bytes | 1.140.603 /16.777.216 bytes |
| PASS | PlatformIO / IDF 5.5.2 | 128 | 30.116 /327.680 bytes | 1.140.623 /16.777.216 bytes |

Invocação final: exit code zero, 103,54 s +96,73 s. Depois destes builds não houve
alteração de firmware, somente documentação. Comparação com valores registrados
na Fase 2.5 (baseline não recompilada nesta sessão): RAM estática +8 bytes;
Flash +3.860 bytes (256) e +3.804 bytes (128). Isso não mede heap nem CPU e não
inclui a memória de objetos/filas alocados no boot. Firmware/bootloader/partições
ficam em smk-s3/.pio/build/<env>/; não houve upload nesta sessão.

Os builds mantêm o perfil local square 240×240; defaults/CI continuam 284×76.
Sample rate: 48 kHz. Sem mudança de GPIO/VBUS/straps PCM5102A/display. Toolchain:
plataforma pioarduino 55.03.36-1, IDF 5.5.2, GCC Xtensa 14.2.0, mesmo ambiente
Python 3.11/PlatformIO da Fase 2.5. Não se ampliou compatibilidade para IDF 6.

## Bancada pendente — MANUAL

1. Confirmar revisão N16R8, wiring existente, VBUS seguro, straps PCM5102A e painel
   físico. Usar USB-UART para console e USB nativa em Host. Testar ambos os blocos.
2. Rodar audio_reset, status, audio_status e memory. Registrar filas, rejeições,
   referências, heap/PSRAM, render máximo/médio e erros I2S. Não confundir esses
   contadores com medição de MIDI físico→saída analógica.
3. Com patch 0, tocar C4; injetar note_on 60 100 0 pelo console e soltar a tecla.
   Esperar que a referência de console continue; note_off 60 0 deve encerrá-la.
   Repetir com canais 0/1, acordes rápidos e pads melódicos existentes.
4. Ativar pedal antes da nota, soltar várias teclas, re-pressionar a mesma nota
   e levantar pedal. Repetir com arp e pads compartilhando pitch. Não aceitar
   notas presas; registrar a política idempotente para On duplicado sem Off.
5. Durante arp/seq, manter nota direta do console e apertar Stop físico/MIDI.
   A nota direta deve continuar, geradores devem parar. Caudas de drums podem
   terminar naturalmente. Testar mute/solo (gate original), pattern e arp disable.
6. Trocar patch/cena com notas/pedal/ticks pendentes. Nenhuma nota antiga deve
   reaparecer; cena pode reiniciar seq com novo estado. Sob fila cheia, falha de
   configuração pode manter o timbre anterior, mas não deltas/notas órfãos.
7. Panic e desconexão USB durante sustain/arp/seq devem silenciar globalmente.
   Reconectar pelo menos 100 vezes e verificar operação sem reboot onde o host
   já suporta. Repetir MIDI Learn/perfil substituído e recuperação por overflow.
8. Repetir BPM 30/60/120/180/300, swing 50/67/75, knobs/save/display, captura
   analógica/MIDI e soak de duas horas por bloco conforme relatório 2.5. Medir
   latência, jitter, underruns reais e margem de render antes de aprovar palco.

## Riscos e entrada para Fase 3B

- Qualificação física anterior e atual permanece pendente; build não aprova palco.
- UI/console ainda acessam estado de controle em paralelo (risco anterior). Esta
  fase garante writer único da tabela/AMY, não resolve todo ownership de UI/console.
- Preset/configuração pode ser rejeitado pela fila; a barreira agora garante
  silêncio seguro, mas não transação/ACK de patch ou concordância imediata da UI.
- Instrumento/pitch compartilhado não fornece envelopes independentes por origem;
  sustain retido continua ocupando a capacidade fixa. Wire cru contorna o modelo.
- Logs síncronos, alocação/materialização de patch e stalls Flash/cache existentes
  podem perder prazo. A tabela limitada não constitui medição de tempo real.

Antes de Layer A/B: passar bancada, medir orçamento de vozes/osc/heap/CPU por
patch, alocar destino adicional explicitamente, parametrizar os destinos aceitos,
separar mono/pedal/bend/parâmetros por instrumento, definir patch transaction/ACK
e política de stealing. Filtros de split podem preceder a construção de rotas;
fan-out já guarda saídas exatas. Não duplicar instâncias AMY sem orçamento medido.
Próximo menor passo: executar bancada/soak desta fase, sem acrescentar layers.

## Commits e conclusão

| Entrega | Commit |
| --- | --- |
| A — auditoria | 88de21f6 |
| B — ownership | a21e4003 |
| C — integração musical | 22f0f195 |
| D — lifecycle/regressões | 7909be32 |
| E — validação e relatório | Commit que contém este relatório final: test(smk): validate patch barriers and record phase3a results |

Infraestrutura e regressões de software entregues. Aprovação física permanece
MANUAL; CI remota e outras plataformas permanecem NOT RUN. Não foram inventadas
medições físicas nem pinagem final. Sem alterações de core AMY ou dependências.
