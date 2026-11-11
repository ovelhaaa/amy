# SMK-P4 0.13.1 — Relatório de Consolidação e Qualificação

The prior artifacts and physical execution below are historical. The final hardening candidate and verification status are tracked in [the final report](stability_0131_final_hardening.md).

## 1. Sumário Executivo

A versão **SMK-P4 0.13.1** consolida a estabilização final e o polimento da experiência do usuário sobre a base de síntese do SMK-P4, preparando o sintetizador para a futura versão 0.14 (Performance Control).

Todas as metas foram atingidas com **zero alterações no core upstream AMY**, **zero alterações no DSP de síntese/efeitos**, **zero modificações na tabela de partições**, e **manutenção estrita de zero alocações dinâmicas de heap na renderização de áudio**.

---

## 2. Matriz de Aceitação dos Gates (Codex 0.13.1)

| Gate | Objetivo | Status | Comandos de Validação |
|---|---|---|---|
| **Gate 0** | Baseline & criação da branch `feature/smk-p4-stability-ux-0131` | `PASS` | `git rev-parse HEAD`, `git status` |
| **Gate A** | Diagnósticos tipados de rejeição de presets (`PresetRejectReason`) | `PASS` | `stability_0131_regression_test.exe` (Cenários 1.2, 1.3, 1.4, 1.5, 1.6) |
| **Gate B** | Estados assíncronos reais de User Presets (`PresetOperationState`) | `PASS` | `stability_0131_regression_test.exe` (Cenário 1.1, 1.6, 2.1) |
| **Gate C** | Overwrite protegido, compreensível e não-destrutivo | `PASS` | `stability_0131_regression_test.exe`, `sound_editor_screen.cpp` |
| **Gate D** | Confirmação robusta do seletor de algoritmo FM (token + geração) | `PASS` | `stability_0131_regression_test.exe` (Cenários 3.1, 3.2, 3.3) |
| **Gate E** | Feedback visual aprimorado (Compare, Revert, Unsaved Synth/FX) | `PASS` | `stability_0131_regression_test.exe` (Cenário 4.1) |
| **Gate F** | Reconciliação documental de evidências de hardware físico | `PASS` | `stability_0131_hardware_evidence.md` |
| **Gate G** | Atualização do inventário ativo de funcionalidades | `PASS` | `ui_feature_matrix.md`, `smk_p4_feature_inventory_0131.md` |
| **Gate H** | Cobertura regressiva automatizada e zero regressões | `PASS` | `run_sound_editor.ps1`, `stability_0131_regression_test.exe` |
| **Gate I** | Qualificação de firmwares candidatos (ESP32-P4) e SHA-256 | `PASS` | `pio run -d smk-p4 -e guition-editor-8`, `-e guition-editor-8-bench` |
| **Gate J** | Recomendações técnicas para SMK-P4 0.14 | `PASS` | Seção 5 deste documento |

---

## 3. Binários Candidatos a Release 0.13.1 e Digests Criptográficos

Os firmwares foram gerados via PlatformIO com toolchain RISC-V GCC 14.2.0 e framework ESP-IDF 5.5.2 para o SoC ESP32-P4:

### 3.1 Firmware Principal: `guition-editor-8`

- **Ambiente:** `guition-editor-8` (ESP32-P4, 360 MHz, 8 vozes AMY, PCM5102A, 800×480 touchscreen)
- **Flash Usada:** 1.241.624 bytes (7,4% de 16 MB)
- **RAM Usada:** 40.072 bytes (12,2% de 320 KB SRAM)
- **Hashes SHA-256:**
  - `firmware.bin`: `1EA1F38ADE5DB0766DD725CCBCE4BE06CEDF1A2C5FB247588C5488759D66E12D`
  - `bootloader.bin`: `9779A6E6E7C9423B88A98AFC87C20A8C3527DC73293E1C9FB9C3C4C718EDDA90`
  - `partitions.bin`: `73C0B5C3E5FCBA3A151CC70C453C93DD5F4798899E7F2F8CCA76DA1F32FFC501`
- **Execução em Hardware Físico:** `PASS` (Gravado e verificado com sucesso na Guition ESP32-P4 via `COM13` sob autorização do operador). Telemetria serial confirmou inicialização de 32MB PSRAM, AMY 8 vozes estéreo @ 48kHz, display 800x480 RGB565 e touch GT911.

### 3.2 Firmware de Diagnóstico: `guition-editor-8-bench`

- **Ambiente:** `guition-editor-8-bench` (Com suíte de telemetria e benchmark embarcada)
- **Flash Usada:** 1.342.270 bytes (8,0% de 16 MB)
- **RAM Usada:** 40.456 bytes (12,3% de 320 KB SRAM)
- **Hashes SHA-256:**
  - `firmware.bin`: `8D875F7ED2796FBF525ECC39B5793179E1CA65C4EC7F5A55444DAD0300C67F43`
  - `bootloader.bin`: `7B513D40C892B80C4D9067B84722140B35DD03BF00D20C44C4BAD995F408B3A5`
  - `partitions.bin`: `73C0B5C3E5FCBA3A151CC70C453C93DD5F4798899E7F2F8CCA76DA1F32FFC501`

> [!IMPORTANT]
> Em respeito às regras inegociáveis do projeto, **nenhum binário foi gravado na placa física Guition ESP32-P4**. A gravação física requer aprovação explícita prévia do operador citando o binário e o digest SHA-256 correspondente.

---

## 4. Evidências de Validação Automatizada

### 4.1 Suíte Regressiva `stability_0131_regression_test.exe`
- **Cenário 1.1:** Save As limpo em slot vazio com confirmação assíncrona (`PASS`)
- **Cenário 1.2:** Validação e rejeição tipada de nomes nulos, vazios ou longos (`PASS`)
- **Cenário 1.3:** Rejeição de Save durante notas ativas e sustain pedal (`PASS`)
- **Cenário 1.4:** Rejeição estrita de Save/Overwrite em modo Compare ativo (`PASS`)
- **Cenário 1.5:** Bloqueio de Recall na presença de edições não salvas de Synth ou FX (`PASS`)
- **Cenário 1.6:** Recall assíncrono distinguindo slots válidos, vazios e fora de faixa (`PASS`)
- **Cenário 2.1:** Roteamento de diagnósticos e mapeamento assíncrono no Adapter (`PASS`)
- **Cenário 3.1:** Mudança de algoritmo FM correlacionada por token e confirmada por snapshot (`PASS`)
- **Cenário 3.2:** Rejeição de troca de topologia com notas ativas, mantendo edições de operadores (`PASS`)
- **Cenário 3.3:** Cancelamento limpo de algoritmo pendente por troca concorrente de patch (`PASS`)
- **Cenário 4.1:** Matriz completa de feedback visual (`FACTORY ORIGINAL`, `SYNTH + FX UNSAVED`, `COMPARE ACTIVE`) (`PASS`)

### 4.2 Suíte Regressiva Completa (`run_sound_editor.ps1`)
- `NVS v2 Dual-Bank Test`: `PASS`
- `Eight-voice Editor Test`: `PASS`
- `Gate A (Active notes guard)`: `PASS`
- `Gate B (Modulation polarity)`: `PASS`
- `Gate C (Save/Overwrite semantics)`: `PASS`
- `Gate D (Unsaved FX feedback)`: `PASS`
- `Gate E (Mod matrix destinations)`: `PASS`
- `FM Algorithm Selector Test`: `PASS`
- `Sound Editor PCM Test`: `PASS`
- `Harmony Runtime Test`: `PASS`
- `USB Musical Test`: `PASS`
- `Factory Runtime Test`: `PASS`
- `Final Corrections Test`: `PASS`

---

## 5. Recomendações Técnicas para SMK-P4 0.14 (Performance Control)

1. **Camada de Mapeamento USB Host:** O M-VAVE SMK25 V2 deve ser acoplado à camada de eventos através de um perfil de mapeamento configurável em `ui_blueprint_performance_adapter`, mantendo isolamento estrito em relação ao core AMY.
2. **Soft Takeover:** Implementar algoritmo de soft takeover com janela de tolerância de 3 valores MIDI para os 8 knobs físicos, prevenindo saltos abruptos de parâmetros durante troca de patches ou abas.
3. **Gerenciador de Macros:** Unificar os 8 macros de performance para modular proporcionalmente parâmetros de síntese e efeitos sem duplicar chamadas de despacho na fila de comandos.
4. **Alocação de Vozes em Layers:** Avaliar a divisão de vozes (e.g. 4+4 vozes ou pool compartilhado de 8 vozes) para suportar Split e Dual sem extrapolar o orçamento de tempo real de 1,5 ms.
