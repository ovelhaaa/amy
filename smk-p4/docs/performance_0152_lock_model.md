# SMK-P4 0.15.2 — Modelo de Preservação e Performance Lock

## 1. Princípios e Regras Fundamentais

O Performance Lock permite que o usuário troque o timbre de síntese (ROM Patch Load) preservando seletivamente contextos musicais ativos.
Ele resolve o problema de performances ao vivo onde o músico configura Delay sincronizado, acordes assistidos e arpejador, e deseja mudar o som sem ter de reconfigurar todos os efeitos e assistentes.

### Regras Inegociáveis:
1. **Preservar configurações NÃO significa preservar notas soando.** Ao trocar de patch, todas as notas ativas, vozes, buffers e tokens anteriores são sumariamente silenciados e invalidados via Panic.
2. **Performance Lock aplica-se ao carregamento de timbres ROM.** O Recall de User Preset V4 permanece transacional e integral (restaura synth + FX salvos).
3. **Não há gravação automática em NVS.** O Performance Lock opera em memória na sessão. Não altera o perfil MIDI global nem o Global BPM.
4. **Semântica de Dirty State:** Ao carregar um patch com Keep FX ou outros locks ativos, o patch é marcado como dirty (`state_.fx.dirty = true`), sinalizando que seu contexto de performance diverge do padrão neutro da ROM de fábrica.
5. **Comportamento Limpo (Clean Load):** Quando nenhuma opção Keep estiver selecionada (`preserve_mask == 0`), o comportamento é 100% equivalente ao Patch Load da versão 0.15.1.

---

## 2. Grupos de Preservação Suportados

| Grupo | Flag | O que é preservado | O que é purgado/reinicializado |
|---|---|---|---|
| **KEEP FX** | `perf_lock::Fx` (`1 << 0`) | Chorus Mode/Depth, Delay Mode (FREE/SYNC), Division, Time, Feedback, Mix, Reverb Size, EQ Low/Mid/High, Master Tone, Bypass | Amostras transitórias da linha circular de delay |
| **KEEP HARMONY** | `perf_lock::Harmony` (`1 << 1`) | Scale Assist On/Off, Root, Scale Type, Quantization Mode, Chord Assist On/Off, Chord Type, Inversion, Voicing, Spread, Canal MIDI | Grupos de harmonia polifônica ativos, notas soando |
| **KEEP ARPEGGIATOR** | `perf_lock::Arp` (`1 << 2`) | Enabled, Direction/Mode, Rate, Octaves, Gate, Swing, Latch, Restart on New Chord | Held notes, padrão rítmico antigo, notas latched, gate ativo |
| **KEEP MACROS (Adv)** | `perf_lock::Macros` (`1 << 3`) | Posições M1..M8 e rotas validadas como compatíveis com a família do novo patch | Rotas exclusivas da família anterior (ex: FM vs Juno Filter) |

---

## 3. Política Padrão da Sessão

Conforme especificação, a seleção padrão inicial de checkboxes na UI é:
```text
Keep FX:          ON  [x]
Keep Harmony:     ON  [x]
Keep Arpeggiator: ON  [x]
Keep Macros:      OFF [ ]
```
Representado por `perf_lock::kDefaultLockMask = (Fx | Harmony | Arp)`.
Essa seleção é mantida em memória volátil durante a execução da sessão.
