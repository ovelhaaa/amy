# Especificação de Arquitetura UX, Navegação e Interações — SMK-P4 0.18-A

## 1. Mapa de Reorganização: Telas Atuais (15) → Nova Arquitetura (5 Áreas Fixas)

| Área Fixa (Rodapé) | Módulo (Sidebar 160 px / Modos em `Tocar`) | Tela / Origem 0.17B | Vista / Subguia / Overlay |
| --- | --- | --- | --- |
| **TOCAR** *(sem sidebar, 800 px)* | **Palco Principal (`MACROS 1-4` / `5-8`)** | `Performance` (`performance_screen.cpp`) | Patch/Cena + `ARP/SEQ/FX/HARM` pills + Macros `1..4` (ou `5..8`) + Cenas rápidas `1..8` |
| **TOCAR** *(sem sidebar)* | **Cenas (`CENAS 1-8`)** | `Scenes` (`scenes_screen.cpp`) + Quick Scenes | 8 slots (`ACTIVE` / `PENDING` / `PARTIAL` / `FAILED` / `SAVED` / `EMPTY`), Recall/Save/Clear, Perf Locks + **Overlay Seletivo** |
| **TOCAR** *(sem sidebar)* | **Teclado (`TECLADO`)** | `Performance` (tab `KEYS`) + `VirtualKeyboard` | Teclado touch 2 oitavas (`OCT -`/`+`) com destaque harmônico opcional; libera só notas touch ao sair do modo |
| **TOCAR** *(sem sidebar)* | **Encadear (`ENCADEAR [PLAN]`)** | *Novo (somente proposta)* | Wireframe estático **`NÃO IMPLEMENTADO`** (`A -> B -> C`, fronteira Next Bar, desarme manual) |
| **TIMBRES** | **Biblioteca** | `PatchBrowser` (`patch_browser_screen.cpp`) | Lista paginada (`UiModel::catalog`), filtro/disponibilidade, `< ANT` / `PROX >`, confirmação de descarte |
| **TIMBRES** | **Síntese** | `SoundEditor` (`sound_editor_screen.cpp`) | 4 subguias contextuais à família do patch (`GERAL`, `FM ALGO/OP` ou `OSC SUB`, `FILTRO/ENV`, `ROUTING`) |
| **TIMBRES** | **Camadas** | `Layers` + `Mixer` (Layer A/B) | Sinalização explícita **`PLANNED — NÃO OPERACIONAL`** no engine unificado 8 vozes |
| **TIMBRES** | **Modulação** | `Modulation` + `routing_editor.h` | Visão de rotas Macro 1–4 + `routingSnapshot()` + Editor contextual com ajuste fino `-`/`+` |
| **RITMO** | **Sequenciador** | `Sequencer` (`sequencer_screen.cpp`) | Separação explícita **`PLAY MODE`** (`PLAY/STOP SEQ`, `DIV`, `SWING`, `TR -/+`, `SEQ QUANT != Q`, `LIVE TR`) vs **`EDIT MODE`** (`16 passos`, nota/vel/gate, `STEP REC`) |
| **RITMO** | **Arpejador** | `Arpeggiator` (`arpeggiator_screen.cpp`) | Transporte independente (`ARP: ON/OFF`, `PLAY/STOP ARP`, `CLR LATCH`, direcao, taxa, oitavas, gate, swing, latch) |
| **RITMO** | **Padrões** | `Sequencer` (Pattern Browser) | 8 slots (`patternSlot 0..7`), `LOAD`, `SAVE`, `DELETE`, `patternNotice()` |
| **RITMO** | **Bateria** | `Drums` (`drums_screen.cpp`) | Bloco **`PLANNED — NÃO OPERACIONAL`** sem handlers falsos |
| **FX** | **Delay** | `Effects` (`effects_screen.cpp`) | `mode` (`FREE/SYNC`), `division`, `time_ms` (travado em `SYNC`), `feedback`, `mix`, `delayStatus()` + aviso `BYPASS INDIV: N/A` |
| **FX** | **Reverb** | `Effects` (`effects_screen.cpp`) | `size`, `mix` + Editor contextual + aviso `BYPASS INDIV: N/A (MIX 0% NÃO CORTA M6/M7)` |
| **FX** | **Chorus** | `Effects` (`effects_screen.cpp`) | `mode` (`OFF..VIBRATO`, `OFF` real em `applyBusFx`), `depth` (desabilitado se `OFF`) |
| **FX** | **Master** | `Effects` + `Mixer` (Master) | `perf.master_volume`, `fx.master_tone`, `fx.eq.low_db`; `fx.drive`/`fx.routing` `PLANNED`; gaps `eq_mid_db`/`eq_high_db`/`bypass_all` documentados p/ `0.18-B` |
| **MAIS** | **MIDI / SMK25** | `MidiSettings` (`midi_screen.cpp`) | Status USB, perfil/banco/Learn SMK25, `harmony.usb_channel`, curva de velocidade, lembrete de escopo sem Omni |
| **MAIS** | **Preferências** | `GlobalSettings` (`settings_screen.cpp`) | Performance Locks globais (`Fx`, `Harmony`, `Arp`, `Macros`), seletor Variante Cabeçalho `A`/`B`, Tempo NVS |
| **MAIS** | **Diagnósticos** | `Diagnostics` (`diagnostics_screen.cpp`) | Heap LVGL live, contadores de shell (`shell_builds`, `module_mounts`), vozes, aviso `HOST != GUITION` |
| **GLOBAL (Topo)** | **Overlays Globais** | `StatusStrip` (BPM) + `Harmony` | `TempoBpm` (BPM `40..240`, `-5/-1/+1/+5`, `TAP`, `SAVE`), `HarmonyContext` (`Q`, `CH`, Root, Scale, Quant, Chord, Voicing, escopo), `PANIC` sempre acessível acima do backdrop |

---

## 2. Wireframes Estruturais (`800×480` Horizontal)

### 2.1 Shell Comum (Área `Tocar` vs Demais 4 Áreas)

```text
+----------------------------------------------------------------------------------+ y=0
| HEADER PERSISTENTE (Variante A: 44 px  |  Variante B: 68 px) + [PANIC] topo-dir. |
+--------------------+-------------------------------------------------------------+ y=44/68
|                    |  [Area TOCAR]: Ocupa largura total 800 px (sem sidebar).   |
|  SIDEBAR ESQ.      |                                                             |
|  160 px            |  [TIMBRES / RITMO / FX / MAIS]:                             |
|  3-4 modulos       |  Painel contextual = 640 x (388 ou 364 px).                 |
|  148 x 58 px       |  Apenas 1 modulo montado por vez; overlay cobre APENAS       |
|  (oculta em TOCAR) |  conteudo y=44/68..431, deixando Header+PANIC e Footer      |
|                    |  100% descobertos e clicaveis.                              |
+--------------------+-------------------------------------------------------------+ y=432
| [ TOCAR ]     [ TIMBRES ]     [ RITMO ]     [ FX ]     [ MAIS ]    (48 px fixo) |
+----------------------------------------------------------------------------------+ y=479
```

### 2.2 Comparativo das Variantes de Cabeçalho (`800×480`)

| Critério | **Variante A — Compacto + Overlays (`800×44`)** | **Variante B — Expandido 2 Linhas (`800×68`)** |
| --- | --- | --- |
| **Altura útil de conteúdo** | **388 px** (`y=44..431`) | **364 px** (`y=68..431`) |
| **Tamanho de alvos de toque** | Botões principais `36 px` de altura (`w=54..126`, gap `4 px`), **`PANIC 74×36`** | Row 1/2 `30 px` de altura (`w=76..206`, gap `>=3 px`), **`PANIC 82×60`** |
| **Legibilidade em execução** | Máxima área para macros, grade 16 passos e teclado touch; harmonia detalhada abre por toque em `[C MAJ]` | Mantém `Root/Scale + Quant Mode + Q + CH + Escopo USB` sempre visíveis simultaneamente na linha 2 |
| **Sobreposição verificada** | **0 overlaps** (`headerTargetsDisjoint` min gap = `4 px`) | **0 overlaps** (`headerTargetsDisjoint` min gap = `3 px`) |
| **Recomendação UX 0.18-B** | **Padrão recomendado (`DEFAULT`)** — oferece melhor proporção touch vertical nos sliders/teclado mantendo `BPM`, `Root/Scale`, `Q`, `CH` e `PANIC` em 1 toque | Manter como modo opcional acessível em `Mais > Preferências` caso o músico prefira telemetria harmônica fixa |

Capturas de comparação:
- Variante A (`800×44`): [`01_tocar_hdr_a_live.png`](ui_018a_shots/01_tocar_hdr_a_live.png)
- Variante B (`800×68`): [`02_tocar_hdr_b_live.png`](ui_018a_shots/02_tocar_hdr_b_live.png)
- Overlay Harmônico sobre Variante A com `PANIC` livre: [`07_overlay_harmony_q_ch.png`](ui_018a_shots/07_overlay_harmony_q_ch.png)
- Proposta visual `Tocar > Encadear` (**`NÃO IMPLEMENTADO`**): [`05_tocar_encadear_plan.png`](ui_018a_shots/05_tocar_encadear_plan.png)

---

## 3. Política de Parâmetros, Ciclo de Vida e Segurança Musical

1. **Ciclo de Vida em Memória (LVGL `128 KiB` TLSF)**:
   - O shell global (`root_`, `footer_`, `header_`, containers `sidebar_` e `panel_`) é criado **uma única vez** em `UiShell018A::begin()` (`shell_builds == 1`).
   - Apenas o módulo ativo dentro de `panel_` é montado por vez (evita manter 15 telas montadas em RAM).
   - **Disciplina zero-reallocation no shell persistente**: labels e estados do `header_` e `footer_` utilizam exclusivamente `lv_label_set_text_static()` com buffers fixos e estilos pré-associados via `LV_STATE_CHECKED` / `LV_STATE_USER_1`, evitando `lv_realloc` mid-heap enquanto painéis transitórios ocupam o topo do pool.
2. **Política do Editor Contextual de Parâmetros**:
   - **Durante `LV_EVENT_VALUE_CHANGED` (drag)**: atualiza leitura local imediatamente (`LOCAL DRAG`) e envia comando throttled ao backend (`>= 35 ms` entre disparos).
   - **Ao soltar (`LV_EVENT_RELEASED`)**: envia sempre o valor final exato a `IUiBackend::requestParameterValue()`.
   - **Estados visíveis**: `CONFIRMED`, `Requested` (pendente), `DEMO RAM` (quando `UiDemoBackend`), `REJECTED`/`Unsupported` (ex.: acorde `ADD9` ou voicing `DROP2` — reverte leitura para o estado do owner na próxima atualização), `SYNC LOCKED` (delay time sob `SYNC`), `CHORUS OFF` (`depth` desabilitado quando Chorus `OFF`), `PLANNED`.
   - **Ajuste fino**: botões `-` e `+` (`>= 42×34 px`) presentes nos cards slider e no Inspetor Contextual incrementam exatamente `Descriptor::step`.
3. **Preservação de Notas e Transporte durante Navegação**:
   - Trocar de área no rodapé, mudar módulo na sidebar, trocar `PLAY MODE ↔ EDIT MODE`, ou abrir/fechar overlays **NUNCA** envia `NoteOff`, `AllNotesOff`, `Stop` global nem reinicia o AMY.
   - `Arp` e `Sequencer` têm comandos independentes (`STOP ARP` vs `STOP SEQ`).
   - Somente sair de `Tocar > TECLADO` (ou destruir o widget de teclado) dispara `LV_EVENT_DELETE` liberando exclusivamente as teclas touch pressionadas na tela, preservando integralmente notas USB MIDI recebidas do SMK25.

---

## 4. Matriz de Navegação e Interações (`Origem → Evento → Confirmação → Falha → UX`)

| Interação | Origem do estado atual | Evento enviado UI → Backend | Confirmação esperada (Owner → Snapshot) | Comportamento em falha / indisponibilidade | Resposta UX no protótipo |
| --- | --- | --- | --- | --- | --- |
| **Rodapé (5 Áreas)** | `UiShell018A::active_area_` | Navegação local (`selectArea`); fecha overlay aberto; **zero** evento de áudio/MIDI | N/A (apenas UI) | N/A | Troca em 1 toque sem ciclos de banco; preserva módulo/subguia anterior de cada área |
| **Header `Q` / `CH`** | `live_harmony.configuration` (`scale_on`, `chord_on`) + `live_harmony.pending` | `requestParameterBool("harmony.scale_enable" / "harmony.chord_enable", !cur)` | `snap.harmony.scale_on` / `chord_on` atualiza independentemente; UI exibe `Q ON`/`CH ON` | `pending` expira (`Rejected`) → restaura estado do owner + notice | `Q` e `CH` separados em botões independentes; badge mostra `WAIT` durante confirmação |
| **Overlay Harmônico** | `live_harmony.configuration` + `ParameterRegistry` (`harmony.*`) | `requestParameterValue("harmony.*", ...)` + `harmony.preset` | `amy::Runtime` atualiza `state_.harmony` respeitando escopo (`USB Ch + Arp + Touch`) | `ADD9`/`9TH`, voicing `>=2` ou acorde diatônico em penta retorna `Rejected/Unsupported` | Texto explícito diferencia `USB Ch N + Arp + Touch` de `SEQ` (que força `CH=OFF` e usa `SEQ QUANT`) |
| **Header `PANIC`** | Disponível em todas as áreas e **acima de overlays** | `requestPanic()` | Owner incrementa geração e zera vozes (`active_notes == 0`) | Contadores / status indicam se demo/erro | 1 toque imediato sem precisar fechar overlay de BPM, Harmonia ou Recall Seletivo |
| **FX Delay (`FREE`/`SYNC`)** | `fx.*` em `UiModel` + `delayStatus()` | `requestParameterValue("fx.delay.*", ...)` | `snap.fx.applied` e `delay_dsp_ms` confirmados; `delay_out_of_range` sinalizado | Sob `SYNC`, `time_ms` retorna `Rejected` / `!isParameterEditable` | Card `fx.delay.time_ms` trava visualmente como `SYNC LOCKED`; aviso explícito que Mix 0% != Bypass |
| **FX Chorus** | `fx.chorus.mode` / `fx.chorus.depth` | `requestParameterValue("fx.chorus.*", ...)` | `ChorusMode::Off` aplica `chorus_level = 0.0f` real no AMY | Modos fora `0..5` rejeitados | Badge `CHORUS OFF` real; slider `depth` desabilitado quando `OFF` |
| **FX Master & EQ** | `perf.master_volume`, `fx.master_tone`, `fx.eq.low_db` | `requestParameterValue(...)` para os 3 IDs suportados | `snap.fx.applied` confirma | `fx.drive` e `fx.routing` são `PLANNED`; `eq_mid_db`/`eq_high_db`/`bypass_all` sem ID em `IUiBackend` | Cards `fx.drive`/`fx.routing` travados como `PLANNED`; aviso transparente de gaps `0.18-B` sem sliders falsos |
| **Ritmo > Sequenciador (`PLAY` vs `EDIT`)** | `sequencerStatus()` + `sequencerPattern()` (ou `DEMO RAM` no DemoBackend) | `requestSequencer(Action::Play / Stop / SetQuantize / SetStep* / SetLive / SetInputMode)` | `snap.sequencer` e `snap.sequencer_pattern` confirmam passo, modo e `quantize` | Se `sequencerStatus() == nullptr` (demo), sinaliza modo Demo | Alternar `PLAY MODE ↔ EDIT MODE` não interrompe sequenciador; `SEQ QUANT` explicitamente separado de `Q` |
| **Ritmo > Arpejador** | `arpStatus()` (`config.enabled`, `armed`, `held_count`) | `requestParameterBool("arp.enable")`, `requestArpTransport`, `requestClearLatch` | `snap.arp` confirma armado/parado | Sem owner (`nullptr`), sinaliza Demo RAM | `PLAY/STOP ARP` distinto de `PLAY/STOP SEQ`; `STOP ARP` mantém sequenciador tocando |
| **Tocar > Cenas 1..8** | `sceneSlot(0..7)` + `sceneView()` + `sessionLockMask()` | `requestScene(Load/Save/Delete, domains, locks, apply_tempo)` | `SceneView::recall.phase` reporta `ACTIVE`, `PENDING BAR`, `FAILED`, `PARTIAL` | Rejeições (`IncompatibleMacros`, `EmptySelection`, `CompareActive`) mostradas no notice | Acesso rápido em `Tocar` + tela detalhada com Locks, Modal Seletivo e proposta separada `Encadear` (`NÃO IMPLEMENTADO`) |
| **Tocar > Teclado Touch** | Widget local 2 oitavas + `octave_shift` | `requestNoteOn(note, 100)` / `requestNoteOff(note, token)` | Voz admitida por `Audio Owner` conforme polifonia e harmonia (`apply_touch`) | Rejeitado em saturação ou modo demo RAM | Ao sair de `TECLADO`, `LV_EVENT_DELETE` envia `NoteOff` só das teclas touch mantidas; notas USB ficam intactas |
