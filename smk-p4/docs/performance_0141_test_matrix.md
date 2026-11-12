# SMK-P4 0.14.1 — matriz de aceitação

| Gate | Critério | Evidência |
|---|---|---|
| A | Auditoria de arquitetura | PASS. Base remota conferida e fluxo auditado |
| B | Compatibilidade visual legada | PASS em host. Painel, reset confirmado e PCM legado |
| C | Modelo editável | PASS. Validação integral e limite de 48 rotas |
| D | Composição dinâmica | PASS em host. Oito vozes, DSP Synth/FX e remoção |
| E | Editor visual | PASS em LVGL host. Edição numérica, curvas e ACK |
| F | Manual/Applied | PASS em host. Snapshot do owner, contribuição M7 e FX nativo |
| G | Persistência transacional | PASS com NVS simulada. V4, 48 rotas, falhas, reboot, corrupção e orçamento |
| H | Compare/Revert/Reset | PASS em host. Compare ROM, Revert de preset V4, reset de posição, rotas e ambos |
| I | Bancos MIDI personalizáveis | PASS em host. Bank D, CC real do parser, Pickup, perfil V1/V2 |
| J | Motor de macros | PASS em host. Cutoff 2.000 → 3.500 → 2.000 Hz, 48 rotas e remoção de reverb |
| K | Compatibilidade PCM | PASS em host. V1/V2/V3 iguais à 0.14 e V4 antes/depois do codec |
| L | Interface LVGL | PASS em host. Aplicar, rejeitar, cancelar, destruir, reabrir, Save/Recall e família |
| M | Regressões | PASS em host. Ver [resultados e métricas](performance_0141_results.md) |
| N | Builds | PASS para ambos os perfis. Ver [imagens e hashes](performance_0141_hardware_candidate.md) |
| Hardware | Guition e PCM5102A | MANUAL / NOT RUN |

## Ensaio físico pendente

Após autorização vinculada ao SHA-256 e backup da unidade, iniciar pelo perfil de diagnóstico. Confirmar oito vozes, 48 kHz e 256 frames no log. Conectar o SMK25 V2, tocar e soltar acordes de oito notas com sustain, desconectar e reconectar. Esperar silêncio após Panic, áudio nas duas saídas e novo Note On/Off sem reboot. Registrar underruns, deadlines, fila, clipping, RAM interna, PSRAM e stack livre.

Abrir um preset Juno histórico com offset diferente de 64. Conferir família, destino e posição. Entrar e sair de Compare. Confirmar reset e fazer Revert para restaurar a referência salva. No Routing Editor, selecionar M7, editar Reverb Mix e Delay Mix, aplicar, mover M7 e ouvir ambos os efeitos. Verificar Manual/Applied, desativar a rota e conferir a recuperação do valor manual. Salvar em slot de teste, reiniciar e verificar Recall com rotas, posições, Synth e FX. Em Bank D, selecionar Delay Feedback para um knob aprendido e confirmar que o parâmetro só muda após Pickup. Registrar toque, duração de edição, espaço livre NVS e pior tempo por bloco durante o ensaio.
