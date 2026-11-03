# Matriz de regressão — musical 0.3A × Integration 0.4

Referência: `3dd0cf28`/código `a31a78bc`, imagem SHA256
`89fc42a19b0d588970bf6bd18ded139af1bc3843ae398d0cc2404fa1cfa2e9a7`.
Esta branch0.3B não integra Blueprint1.1. Coluna0.4 é um plano a executar, não
aprovação por analogia com mocks. Contrato em [performance_backend_contract.md](performance_backend_contract.md).

## Preparação e condições de comparação

1. Preservar baseline binário/config/partições, identificar worktree, commit,
   toolchain, porta e imagem que será substituída. Usar checker de hashes do
   [baseline](baselines/README.md). Não habilitar gates de outra montagem.
2. Usar a mesma unidade P4 qualificada, PCM, HS/SMK25, alimentação, cabo, mixer e
   níveis. Manter48kHz/PCM16/estéreo/256frames/DMA4×256, quatro vozes e ROM128/135.
3. Um único produtor UI. Áudio owner/USB e a separação das filas permanecem.
   Não rodar UI antiga e nova simultaneamente para comparar. Criar logs separados
   por imagem/boot; anotar início/fim dos cenários e confirmação auditiva.
4. Capturar sem reset com `smk-p4/tests/capture_serial.py`; confirmar AMY ready,
   perfil físico e Host conectado antes da carga. Se SMK25 estiver desligado,
   isso não qualifica MIDI. Uptime, contadores e frames não podem reiniciar no log.
5. Repetir os mesmos estímulos no baseline e candidato. Para ensaio inicial,
   capturar pelo menos1860s para obter30min entre amostras de5s, incluindo carga,
   navegação e hot-plug. Ensaios prolongados posteriores exigem nova avaliação.

Classificação: PASS executado; FAIL executado e falhou; NOT RUN não executado;
MANUAL procedimento físico; BLOCKED dependência identificada. PASS host não
substitui MANUAL. As observações0.3A abaixo vêm do relatório físico anterior.

## Cenários musicais e segurança

| ID | Procedimento/resultado esperado | Evidência reproduzível sem hardware | Baseline0.3A | UI0.4 |
|---|---|---|---|---|
| M01 | C4 USB On/Off, velocity0 e suave/forte; release sem nota presa | `run_amy.ps1`: usb_musical/runtime | PASS/MANUAL operador | NOT RUN/MANUAL |
| M02 | C4 Touch; release/arrastar para fora/cancelar; não repetir gate após soltura | local_backend/performance_ui/contract; `run_smk_link.ps1` gesture | PASS/MANUAL | NOT RUN/MANUAL |
| M03 | Quatro notas simultâneas; quinta rejeitada, sem roubo ou nota irrecuperável | runtime/usb_musical/contract; verificar máximo4 e contador de rejeição | PASS;227 rejeições na carga livre | NOT RUN/MANUAL |
| M04 | Acorde com sustain: soltar teclas, Touch independente, liberar pedal | usb_musical, CC64 por canal; liberação USB não libera Touch | PASS/MANUAL | NOT RUN/MANUAL |
| M05 | Bend mínimo/centro/máximo; CC1 em nota e acorde; Touch independente | usb_musical: sinais AMY reais, bend/mod por canal | PASS/MANUAL | NOT RUN/MANUAL |
| M06 | USB+Touch mesma C4; releases nas duas ordens | usb_musical: duas identidades distintas | PASS/MANUAL | NOT RUN/MANUAL |
| M07 | Panic durante acorde/sustain/controles, seguido de novas notas | local_backend/usb_musical/contract/UI: geração invalida ambas as filas | PASS/MANUAL | NOT RUN/MANUAL |
| M08 | Selecionar128/135 com notas; confirmar snapshot aplicado e silêncio dos gates antigos | runtime/local_backend/UI/contract: aceitação ≠ confirmação | PASS/MANUAL | NOT RUN/MANUAL |
| M09 | Master0..127, movimentos rápidos; snapshot aplicado, sem clipping ou falha | runtime/local_backend/contract/UI | PASS/MANUAL | NOT RUN/MANUAL |
| M10 | Fila cheia: On não aceito cancelado não toca depois; Off/Panic funcionam; patch false ainda cancela | contract/local_backend; usb_musical cobre fallback de release USB | PASS host; não injetado fisicamente | NOT RUN/MANUAL |
| M11 | Touch mantido: antes10s ativo, em10s expira; heldNote/On repetido não renovam; soltar e rearmar | contract/local_backend; USB pode ficar ativo além10s | PASS host; duração específica não revalidada fisicamente | NOT RUN/MANUAL |
| M12 | Dois dedos no mesmo pitch: primeiro On/último Off pelo ledger da UI; soltar callback antigo não deve matar gesto novo | Backend demonstra ausência de refcount; testes do novo ledger serão obrigatórios em0.4 | Sem teclado multitouch no baseline | NOT RUN; bloqueia teclado complexo até implementação/testes |
| M13 | Timestamp just-future, fila velha, wrap e geração anterior | runtime/usb_musical/local_backend | PASS host | NOT RUN host |

O teclado atual é monofônico e o port GT911 só publica um toque. Não declarar
multitouch suportado por usar uma cena demonstrativa com várias teclas. M12 é
um requisito da nova UI caso ela prometa esse comportamento; nenhuma extensão
AMY ou novo driver foi implementado nesta rodada.

## USB e recuperação

| ID | Procedimento e critério | Cobertura host atual | Baseline0.3A | UI0.4 |
|---|---|---|---|---|
| U01 | Ligar/conectar SMK25 no HS; descritores/interface/IN corretos e eventos reais | `run_usb.ps1`, `run_amy.ps1` | PASS físico | NOT RUN/MANUAL |
| U02 | Nota mantida, retirar HS por3s, reconectar e tocar; zero gate preso, sem reboot | lifecycle + cancelamento USB preservando Touch | PASS físico:4 ciclos,5 conexões,0 erros | NOT RUN/MANUAL; repetir≥4 ciclos |
| U03 | ERROR antes de DEV_GONE: cancelamento imediato, handle retido, uma desconexão, sem retry do endereço retirado | usb_host:3 ciclos atrasados e NO_DEVICE nas duas ordens | PASS físico após correção | NOT RUN/MANUAL |
| U04 | ERROR sem DEV_GONE: após≤1000ms conta falha e faz retry limitado; não ocultar erro genuíno | usb_host: relógio determinístico, cleanup/restart | PASS host; injeção física NOT RUN | NOT RUN host/MANUAL |
| U05 | Fila/transferência inválida: fonte USB cancelada, Touch independente | USB parser/safety/usb_musical | PASS host; injeção física NOT RUN | NOT RUN host |
| U06 | Cabos virtuais0/1 duplicados, sem dobrar eventos/notas nem reabrir dispositivo | Reprodução dos bytes reais SMK25 em usb_musical | PASS físico:3215 selecionados/3215 auxiliares | NOT RUN/MANUAL |

Host ERROR transitório durante a janela de remoção não é falha persistente por
si só: correlacionar confirmação DEV_GONE, counters e recuperação. ERROR sem
confirmação, parse_errors/drop/overflow não podem ser apagados/reclassificados
por reset para aprovar o candidato. `trace_dropped` é uma fila de diagnóstico;
registrar separado de MIDI dropped.

## Interação da UI

| ID | Sequência | Cobertura0.3B / critério para0.4 |
|---|---|---|
| I01 | Abrir/fechar browser e navegar rapidamente com áudio USB | UI antiga testada em host e operador0.3A; nova UI MANUAL/NOT RUN. Não liberar notas USB por navegação comum. |
| I02 | Mudar tela/oitava com Touch ativo; perda de foco e cancelamento | Touch deve liberar a nota enviada antes da mudança; soltar com o pitch salvo. Mock antigo cobre PRESS_LOST/oitava; contract cobre releaseAll e playable=false. |
| I03 | Recriar widgets/frontend sem tocar controles | Nenhum comando musical de startup. Mock antigo verifica fila vazia na recriação;0.4 deve repetir. |
| I04 | Sliders em movimento prolongado com acordes | Coalescer/retry não bloqueante, não encher fila; não apresentar como aplicado antes de snapshot. Macros continuam desabilitadas. |
| I05 | Panic durante edição e enquanto há patch/controles pendentes | Cancelar pending UI para não reaplicar gate/comando antigo; validar novas notas após Panic. |
| I06 | Atualização de telemetria com/somente MIDI, modo RAM e erro de engine | Verificar fontes do contrato; revisão de publishMidi não muda sozinha. Zero em campo indisponível não deve virar medição. |
| I07 | Responsividade durante carga máxima e reconexões | MANUAL/NOT RUN0.4: observar toque/navegação e medir no port se necessário; não há métrica atual de tempo de resposta UI ponta a ponta. Refresh count não é latência de input. |

Não alterar o port/telas/navegação nesta branch. Nenhum mock de alocação/render
LVGL aprova visual, desempenho real de flush ou fragmentação física.

## Métricas e fontes a registrar em ambos os lados

| Métrica | Fonte | Referência física0.3A / critério |
|---|---|---|
| frames escritos, blocks | AUDIO do mailbox owner | +87601920frames/1825,04s; monotônico e progressão coerente48000/s |
| render médio/máximo | AUDIO/ENGINE owner, não Host | final185us/máximo2601us; comparar mesma carga; nenhuma ultrapassagem do prazo5333,33us |
| render/compute deadlines | AUDIO render_deadlines / ENGINE compute_deadlines | ambos0; aumento bloqueia integração até causa resolvida |
| I²S write errors/timeouts/refill risks | AUDIO owner | todos0; qualquer aumento ou falha audível reprova a janela |
| clipping/OOM | ENGINE owner | ambos0; não inferir pelo master ou por campo DTO não preenchido |
| queue/high/overflow/expired/latency | ENGINE + filas UI/USB | high4, overflow/expired0, latência máxima6ms; repetir baseline diante de aumento e investigar regressão persistente |
| polyphony_rejected | ENGINE snapshot no log USB |227 na carga livre; respeitar4 vozes. Valor depende da execução, não é alvo fixo para runs diferentes |
| USB state/connects/disconnects/errors | Host |5/4/0 no teste; contabilizar novas ações sem reboot |
| events/ignored/parse/drop/overflow | UsbInput e sua fila |3215/3215/0/0/0; nenhuma perda musical ou parsing inválido esperado |
| heap interno/maior bloco | SMK_P4_GATE0 | final250283/172032bytes; amostrar antes/depois de navegação repetida; sem queda progressiva |
| PSRAM livre/maior bloco | SMK_P4_GATE0 |31176952/30932992bytes ao final; descontar alocações fixas iniciais e investigar crescimento |
| stack livre mínima áudio/USB | AUDIO/USB diagnostics |7332/4144bytes; não reduzir margens sem medição e revisão |
| LVGL heap/fragmentação/refresh | DISPLAY_TEST + lv_mem_monitor |free90532, largest81172, peak_used45180, fragmentation11% no fim0.3A; comparar ciclos de navegação, não exigir igualdade com layout maior |
| underruns exatos de DMA | Sem contador preciso atual | UNMEASURED em ambos; zeros de refill/write não podem ser renomeados como underruns0 |
| CPU total / latência física | Sem medição atual | NOT RUN/MANUAL; não derivar do render médio nem da idade da fila |

O modelo de memória da UI nova pode ter custo fixo diferente. Congelar esse
novo custo e verificar ausência de crescimento depois de muitas idas/voltas;
preservar a reserva de boot para AMY e a separação da tarefa de áudio. Se
introduzir métrica de UI, preferir amostragem/rate-limit na tarefa UI, sem logs
por callback de áudio/bloco ou alocações no render.

## Execução host reproduzível e aprovação

Da raiz do worktree, com GCC/G++/Python/PowerShell e o toolchain documentado:

```powershell
./smk-p4/tests/run_host.ps1
./smk-p4/tests/run_amy.ps1
./smk-p4/tests/run_usb.ps1
./tests/run_smk_safety.ps1
./tests/run_smk_ui.ps1
./tests/run_smk_link.ps1
git diff --check
```

`run_amy.ps1` inclui o novo `performance_backend_contract_test`; `run_host.ps1`
descobre também os testes do verificador de referência. Comandos/builds e status
real0.3B estão em [firmware_integration_readiness_03b.md](firmware_integration_readiness_03b.md).

0.4 só pode declarar integração física aprovada com logs/counters sem reset,
carga e observações auditivas registradas, todos os cenários relevantes de
teclado/gestos demonstrados,≥30min integrados e nenhum erro persistente ou
degradação inexplicada. Captura apenas em repouso com SMK25 desligado não satisfaz
a matriz. Se falhar, preservar evidência e usar rollback documentado; não ampliar
vozes, FX ou catálogo para contornar um teste. O ensaio prolongado vem depois da
aprovação dessa etapa inicial.
