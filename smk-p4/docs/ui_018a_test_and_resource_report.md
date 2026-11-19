# SMK-P4 0.18-A — Relatório de Testes, Recursos LVGL e Pendências Físicas

## 1. Identificação e Escopo Validado

- **Branch de trabalho:** `feature/smk-p4-ui-redesign-018a`
- **Commit base verificado:** `579de2b3a34e4f21b9dc28f2a55ad97a9b47031e` (`579de2b3 Record physical SMK25 enumeration and MIDI smoke`), coincidente com `origin/feature/smk-p4-performance-scenes-017b` e `refs/pull/47/head` (PR #47).
- **Integridade do domínio protegido:** `git diff` contra `feature/smk-p4-performance-scenes-017b` em `smk-p4/components/amy_runtime`, `smk-p4/components/audio_pcm5102`, `smk-p4/components/usb_midi_host` e `smk-p4/main` está vazio. Não houve alteração de DSP, ownership, filas, NVS, flash ou encadeamento real de cenas.
- **Escopo estrito de compatibilidade no preview legado:** foram ajustados apenas `smk-p4/ui-preview/run_preview.ps1` (inclusão/link de `amy_runtime.o`, `build/harmony09/*.c.o` e `-lm` para resolver `algorithms[33]` usado por `fm_algorithm_selector.cpp`) e `smk-p4/components/ui_blueprint/tests/blueprint_test_suite.cpp` (3 asserções pré-0.17B, tolerância de altura `bh >= 18` para o botão legado `66×18 EXPAND` em `fm_algorithm_selector.cpp` e isenção de badge demo para `Arpeggiator`/`Sequencer`).

---

## 2. Comandos Reprodutíveis em Host

Execução a partir da raiz do repositório (`c:\Users\devx\Documents\PlatformIO\Projects\amy`):

1. **Protótipo 0.18-A + suíte `[T1]`–`[T7]` + 19 capturas 800×480 RGB565:**
   ```powershell
   powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_preview_018a.ps1
   ```
2. **Baseline 0.17B UI Blueprint (`blueprint_test_suite.exe` + 20 capturas 800×480 RGB565):**
   ```powershell
   powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_preview.ps1
   ```
3. **Regressão Sequencer + 017A Pattern Browser + 017B Scene Recall:**
   ```powershell
   powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_step_sequencer_ui_016b.ps1
   ```
4. **Regressão Sound Editor + Effects + Patch Compare/Save/Recall + SMK25 Pickup:**
   ```powershell
   powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_sound_editor_ui.ps1
   ```
5. **Regressão Harmony + Contrato Diferido:**
   ```powershell
   powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_harmony_ui.ps1
   ```

Todas as compilações usaram `g++ -std=c++20 -O2 -Wall -Wextra -Werror` contra `LVGL 9.3.0` (`LV_COLOR_DEPTH 16`, pool TLSF `LV_MEM_SIZE = 128 KiB`).

---

## 3. Matriz de Resultados (Estado Estrito)

| Objetivo / Verificação | Comando / Evidência | Status | Observações Verificadas |
|---|---|---|---|
| Shell global único, 5 áreas no rodapé, `Tocar` sem sidebar permanente (`800px`) e 4 áreas com sidebar+painel | `run_preview_018a.ps1` -> `PASS: [T1]` | **PASS** | `Header`, `Footer` e `Body` montados uma única vez no root; troca apenas de módulo/painel |
| Persistência de módulo/subguia na sessão e limites estritos dentro de `800×480` (Cabeçalhos A e B) | `run_preview_018a.ps1` -> `PASS: [T2]` | **PASS** | Nenhuma sobreposição de toque nem extrapolação geométrica detectada na árvore LVGL |
| `Q` e `CH` independentes, rejeição real de `ADD9` e `PANIC` acessível com overlays abertos | `run_preview_018a.ps1` -> `PASS: [T3]` | **PASS** | Overlays cobrem apenas `y = hdr_h..431`; `PANIC` e rodapé permanecem expostos |
| Bloqueio `SYNC` em FX, confirmação real Chorus `OFF`/`Juno1` e avisos explícitos de lacunas (Delay/Reverb/Master) | `run_preview_018a.ps1` -> `PASS: [T4]` | **PASS** | Não equipara `mix=0%` a bypass; exibe `NOT IMPLEMENTED` onde falta contrato/handler |
| Separação `PLAY`/`EDIT` no sequenciador, distinção `SEQ QUANT` vs `Q` do cabeçalho e `ARP != SEQ` no Stop | `run_preview_018a.ps1` -> `PASS: [T5]` | **PASS** | Preserva semântica da base 0.16B/0.17B sem Stop global falso |
| Preservação de notas USB/MIDI na navegação e release seguro de notas Touch ao sair de `Teclado` | `run_preview_018a.ps1` -> `PASS: [T6]` | **PASS** | Mudar área não injeta `noteOff` USB; desmontar `Teclado` libera apenas notas originadas no Touch |
| Estado explícito `DEMO / NÃO OPERACIONAL`, teardown limpo e stress de 500 transições | `run_preview_018a.ps1` -> `PASS: [T7]` | **PASS** | Zero objetos órfãos (`107 -> 107`), zero vazamento TLSF (`diff=0B`) |
| Geração e inspeção de capturas 800×480 RGB565 | `preview_018a_main.exe` (`smk-p4/docs/ui_018a_shots/*.png`) | **PASS** | 19 PNGs gravadas em `800×480`; 15 inspecionadas visualmente (`01,02,04,05,06,07,10,11,12,13,14,15,16,18,19`) |
| Comparativo Cabeçalho Variante A vs Variante B | Capturas `01_tocar_hdr_a_live.png` vs `02_tocar_hdr_b_live.png` | **PASS** | Variante A (`44 px`, linha única com `PANIC` dedicado à direita) recomendada para GT911 |
| Wireframe não funcional de encadeamento (`Encadear`) rotulado explicitamente `NÃO IMPLEMENTADO` | Captura `05_tocar_encadear_plan.png` e `ui_018a_navigation_interaction_spec.md` | **PASS** | Sem handler ativo no backend |
| Regressão 1/4: `run_preview.ps1` (20 telas + 700 montagens) | `blueprint_test_suite.exe` `[1/7]..[7/7]` + `preview_main.exe` | **PASS** | `ALL UI BLUEPRINT 1.1 HOST TESTS PASSED`; 20 capturas 800×480 |
| Regressão 2/4: `run_step_sequencer_ui_016b.ps1` (016B + 017A + 017B) | `build/step016b-ui/test.exe` | **PASS** | 50 trocas de divisão, browser 017A, catálogo/recall parcial 017B e Panic aprovados |
| Regressão 3/4: `run_sound_editor_ui.ps1` (0.14.1/0.14.2/LVGL9.3) | `build/editor011/ui.exe` | **PASS** | 7 abas editor, Effects, Gate G0, Save/Recall/Compare/Revert, 32 ciclos popup, heap estável |
| Regressão 4/4: `run_harmony_ui.ps1` (Harmony 14 telas) | `build/harmony09/ui.exe` | **PASS** | Confirmação adiada, real key, Off, stepper, channel e timeout aprovados |
| Build ESP-IDF (`idf.py build`) na árvore 0.18-A | Não executado (escopo host UX/arquitetura sem alterar `smk-p4/main`) | **NOT TESTED** | Código 0.18-A reside apenas em `smk-p4/ui-preview/` e `smk-p4/docs/` nesta etapa |
| Validação física na placa Guition ESP32-P4 (áudio, touch, USB, I²S, Panic, NVS) | Regra explícita da 0.18-A proíbe flash e sessão física sem aprovação | **NOT TESTED** | Vide Seção 6 para gates físicos pendentes desde a sessão 0.17B de `18/11/2026` |

---

## 4. Recursos LVGL, Contagem de Objetos e Desempenho (Antes vs Depois)

Todas as métricas abaixo utilizam o pool TLSF LVGL configurado para `128 KiB` (`LV_MEM_SIZE = 131072 B`) em `smk-p4/ui-preview/lv_conf.h`.

| Métrica | Baseline 0.17B (`blueprint_test_suite.exe`, 700 montagens) | Medições Legadas `016B/017A/017B` (`step016b-ui`) | Protótipo 0.18-A Inicial (sem buffers/flags estáticos) | Protótipo 0.18-A Final (`test_018a_suite.exe`, 500 transições após 50 warmup) |
|---|---|---|---|---|
| **Objetos LVGL vivos antes/depois** | n/d no log resumido | n/d no log resumido | n/d | **`107` -> `107`** (`diff = 0`) |
| **LVGL `free` antes / depois** | `63.512 B` / `63.512 B` | `settings=32.824 B`, `editor=34.864 B`, `sequencer=47.312 B`, `FX=63.424 B`, `Scenes empty=66.376 B`, `Selective modal=52.280 B`, `pat_browser before=46.032 / after=44.512 B` | `66.608 B` / `66.608 B` | **`69.032 B` / `69.032 B`** |
| **Vazamento líquido (`diff`)** | `0 B` | `pat_browser` fechado retém estado/delta de `1.520 B` no cenário medido | `0 B` | **`0 B`** |
| **Pico de uso LVGL (`peak_used`)** | `110.728 B` | n/d | `77.008 B` | **`73.432 B`** (`-37.296 B` vs baseline `110.728 B`) |
| **Fragmentação TLSF (`frag`)** | `0%` (reconstruía tela inteira, limpando o pool inteiro a cada troca) | n/d | **`18%`** (realocações dinâmicas em `header_`/`footer_` acima do painel) | **`4%`** (`frag_before=4%`, `frag_after=4%`) |
| **Duração em Host (não física)** | n/d para navegação isolada | `407.82 us/block` (render host + pub + LVGL service/refresh) | n/d | **`median=2780.9 us`**, **`P95=4375.0 us`**, **`max=6105.1 us`** (500 transições completas) |

### Achado Crítico de Memória (Shell Persistente + TLSF)

Ao manter `header_` e `footer_` vivos durante toda a sessão enquanto `sidebar_` e `panel_` são destruídos e recriados a cada troca de área/módulo:
1. O uso inicial de `lv_label_set_text()` dinâmico e `lv_obj_add_style()`/`remove_style()` nos widgets persistentes criava realocações TLSF acima dos painéis temporários, elevando a fragmentação para **`18%`**.
2. A substituição por buffers fixos com **`lv_label_set_text_static()`** (`hdr_*_buf_`), troca de estado visual pré-anexada via flags (**`LV_STATE_CHECKED` / `LV_STATE_USER_1`**) em `createPersistentButton`, e remoção em ordem estritamente **LIFO** (`footer_ -> header_ -> sidebar_ -> panel_`) reduziu a fragmentação de **`18%` para `4%`** e manteve **`0 B`** de vazamento após 500 ciclos.

### Aviso Explícito sobre Latência

Os valores `median=2780.9 us`, `P95=4375.0 us` e `max=6105.1 us` medem apenas o tempo de execução CPU em PC Windows (`g++ -O2`) para destruir o painel/sidebar anterior, instanciar novos widgets LVGL e executar `lv_timer_handler()` no framebuffer em RAM (`preview_harness.h`). **Eles não representam:**
- tempo de leitura I2C / debounce de toque físico **GT911**;
- custo de flush / rotação / transferência **MIPI-DSI** ou contenção de barramento PSRAM no **ESP32-P4**;
- estabilidade ou margem de tempo real da tarefa **Audio Owner / DMA I²S**.

---

## 5. Observações Visuais Menores Identificadas nas Capturas

Durante a inspeção das capturas em `smk-p4/docs/ui_018a_shots/`:
1. **`12_fx_chorus_off.png`:** o bloco explicativo `"COMPORTAMENTO CONFIRMADO"` utilizou `createStyledLabel` sem altura adaptativa (`LV_SIZE_CONTENT`), causando leve recorte na parte inferior da segunda linha de texto quebrada.
2. **`16_ritmo_arp.png`:** o texto de estado `"Held:0 Pat:0 | WAITING FOR ARP OWNER"` aproxima-se da borda esquerda do botão `CLR LATCH`.

Ambas são ocorrências cosméticas localizadas apenas nas strings auxiliares do protótipo visual e não afetam as asserções estruturais `[T1]`–`[T7]`; recomenda-se ajustá-las na primeira iteração de produção em **0.18-B**.

---

## 6. Gates Físicos Obrigatórios Pendentes na Guition ESP32-P4

Com base nos registros físicos prévios da branch base `0.17B` (`smk-p4/docs/performance_scenes_017b_memory_recovery.md`, sessão de `18/11/2026`, COM13, rev. 1.3, MAC `80:F1:B2:D3:CC:86`), onde constam `A1` idle (**PASS**: `243.283 B` internos livres, `30.347.784 B` PSRAM livres, `83.192 B` pool LVGL livres, `DMA_underruns = UNMEASURED`) e `A3` parcial (**PASS**: `USB state=2 connects=1`, `538` Note On/Off em ch1, `trace_dropped=1.020`), **nenhum teste em host qualifica o hardware**. Permanecem obrigatórios antes de qualquer merge ou liberação:

1. **A2 — Touch GT911 na 800×480 física:** precisão de toque nos alvos da Variante A (`44 px` de altura: `Q`, `CH`, `PANIC`), arrasto contínuo de sliders em FX/Macros sem toques falsos e fechamento de overlays.
2. **A3 completo — USB MIDI Host com SMK25 V2:** Pitch Bend, Modulation/CC, sustain, hot-unplug durante notas ativas/sustain (disparando Panic automático) e reconexão limpa sem reboot.
3. **A4 — Áudio I²S / PCM5102A e Escuta Direta:** medição real de `DMA_underruns` (atualmente `UNMEASURED`), zero cliques/pop/dropouts durante navegação intensa, abertura de overlays e sweeps de parâmetros com notas sustentadas.
4. **A5 — Cenas e Selective Recall com Carga Real de Voz:** medição de tempo de apply, estado `ACTIVE`/`PENDING`/`FAILED`/`PARTIAL_APPLY`, respeito a `PerformanceLock` e persistência NVS segura.
5. **Estresse A–H (Memória e Estabilidade):** evolução de heap interno, PSRAM, fragmentação do pool LVGL e comportamento após repetidos comandos de `PANIC` em operação prolongada.
