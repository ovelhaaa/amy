# SMK-P4 0.11.1 — Relatório Final de Correções, Integridade de Presets & Prontidão de Hardware

- **Repositório:** `ovelhaaa/amy`
- **Branch Ativa:** `feature/smk-p4-sound-editor-completion-0111`
- **Baseline Independente:** `feature/smk-p4-live-sound-editor-011` (commit `dd491f03`, binário SHA-256 `0BD1FD6CE2F6C72679C17CECF9E96CA6E61CEFC8993349463E70701F5C3F2911`)
- **Hardware Alvo:** Guition ESP32-P4 (800×480), PCM5102A (I²S), USB MIDI Host (M-VAVE SMK25 V2)
- **Engine:** AMY unificado — 8 vozes polifônicas, 8 slots de instrumentos reutilizáveis, 64 osciladores

---

## 1. Resumo Executivo & Missão

A versão `0.11.1` realizou a etapa final de auditoria, saneamento e qualificação sem reimplementar os Gates A–H, sem criar branches adicionais e sem ampliar o escopo de síntese. Todos os 10 objetivos foram cumpridos:

1. **Origem `OFF` na Matriz de Modulação:** Corrigida a divergência entre UI e Runtime. `ModSource = 0` (`OFF`) desabilita efetivamente a contribuição modulatória preservando o estado do CC1.
2. **Polaridade de Modulação (`UNI +`, `BIP +/-`, `INV -`):** Semântica musical implementada com distinção clara entre unipolar positiva, bipolar centrada em zero (100% compatível com 0.11) e unipolar invertida. Estado neutro em CC1=0 é idêntico e sem artefatos.
3. **Precisão Numérica do Codec V2:** Verificada a tolerância e ausência de overflow em valores não quantizados, limites numéricos (NaN, Inf, IDs fora de domínio) e capacidade máxima (25 params Juno, 42 params FM em 160 bytes).
4. **Compatibilidade V1/V2 e Segurança NVS:** Preservação estrita do namespace V1 (`sound011`, chaves `s00a..s15b`, 128B) e namespace V2 (`sound011v2`, chaves `v00a..v15b`, 160B) com limite de admissão de 390 entradas em 24 KiB.
5. **Divergências Documentação vs Código:** Corrigidas as chaves V2 na documentação (`vXXa`/`vXXb`) e reconciliado o inventário de parâmetros (137 descritores no total, sendo 44 no Sound Editor e 57 IDs no motor).
6. **Regressão PCM Independente contra Baseline 0.11:** Comparação byte a byte (`compare_editor_factory.ps1`) com `SMKP4_SOUND_EDITOR=1` e zero overrides resultou em hash SHA-256 de áudio idêntico ao 0.11 em todos os 7 patches avaliados.
7. **Suítes de Testes Executadas:** `run_sound_editor.ps1` (`PASS`), `run_sound_editor_ui.ps1` (`PASS`), `run_harmony.ps1` (`PASS`) e `run_preview.ps1` (`PASS`).
8. **Compilação dos Perfis de Firmware:** Ambos os perfis `guition-editor-8` e `guition-editor-8-bench` foram compilados com sucesso via PlatformIO (IDF 5.5.2).
9. **Manifesto SHA-256 e Rollback Verificável:** Hashes criptográficos registrados e procedimento explícito de rollback na partição de app (`0x10000`) preservando bootloader e partição NVS.
10. **Procedimento de Smoke Test Musical:** Sequência determinística de teste físico preparada para execução no Guition ESP32-P4.

---

## 2. Detalhamento Técnico das Correções

### 2.1 Gate A: Correção da Origem `OFF` na Matriz de Modulação
- **Causa Raiz:** `UiBlueprintPerformanceAdapter::isParameterValueSupported()` aceitava `mod.slot1.source = 0` (`OFF`), mas `Runtime::applyEdit()` rejeitava qualquer valor diferente de `MOD WHEEL = 1`.
- **Implementação:**
  - Em `smk-p4/components/amy_runtime/sound_edit_runtime.inc`:
    ```cpp
    const auto ms = std::lround(request.values[edit::ModSource]);
    if (ms < 0 || ms > 1) { // 0 = OFF, 1 = MOD WHEEL
        ++snapshot_.edit.errors;
        return;
    }
    ```
  - Em `smk-p4/components/amy_runtime/amy_runtime.cpp`: quando `mod_source == 0` (`OFF`), o cálculo modulatória zera a contribuição para Pitch e Cutoff:
    - Pitch: `applyVibrato(v, vibrato_hz, 0.0f)`
    - Cutoff: não soma deslocamento ao logfreq do filtro.
- **Validação:** `final_corrections_test.cpp::testModSourceOff()` e `gate_e_test.cpp` confirmam que com CC1=127 e `ModSource = OFF`, o áudio é estritamente idêntico ao áudio com CC1=0 (sem vibrato e sem deslocamento de filtro). Ao retornar para `ModSource = 1`, o vibrato e o filtro reagem imediatamente ao CC1.

### 2.2 Gate B: Semântica Musical da Polaridade (`UNI +`, `BIP +/-`, `INV -`)
- **Semântica Implementada:**
  - `UNI +` (0): Modulação unipolar positiva. Varia de 0 (em CC1=0) até `+Amount` (em CC1=127). No filtro Juno: abre a frequência de corte proporcionalmente ao valor absoluto do amount (`+std::fabs(raw_amt)`).
  - `BIP +/-` (1): Modulação bipolar centrada em zero. Varia de `-Amount` a `+Amount`. Preserva 100% de compatibilidade binária e acústica com os patches do release 0.11.
  - `INV -` (2): Modulação unipolar invertida. Varia de 0 (em CC1=0) até `-Amount` (em CC1=127), modulando o pitch para baixo e fechando a frequência de corte do filtro (`-std::fabs(raw_amt)`).
  - Estado Neutro: Em CC1 = 0, todas as três polaridades resultam em modulação zero, sem DC offset de pitch, sem estalos e sem alteração na frequência base.
- **Validação:** Matriz determinística testada em `final_corrections_test.cpp::testModPolarity()` cobrindo polaridades (0, 1, 2) × amounts (-100, -50, 0, 50, 100) × CC1 (0, 64, 127) em Juno (Patch 4) e DX7 (Patch 135).

### 2.3 Gate C: Precisão Numérica e Limites do Codec V2
- **Tolerância Numérica em Valores Não Alinhados:**
  - Valores testados: Attack = 123.456 ms, Decay = 987.321 ms, Feedback = 37.777%, OpLevel = 63.333%, Detune = -17.427 cents, FilterEnv = -2.345 oct, LfoRate = 7.319 Hz, ModAmount = -42.718%.
  - Modo 0 e Modo 1 (Juno): Codificação IEEE-754 float32 — erro de quantização = 0.000000 (bit-exact).
  - Modo 2 (FM Compacto): Ponto fixo com erro máximo estritamente delimitado:
    - Percentuais: <= 0.0051% (resolução de 0.01%)
    - Tempos de envelope: <= 0.0313 ms (resolução de 1/16 ms)
    - Frequência LFO, detune e ratios: float32, erro = 0.
- **Limites e Proteção:**
  - NaN e Infinito: Rejeitados na conversão em `user_presets_v2.h`.
  - Wraparound: Valores clamped estritamente antes do cast para uint16_t/uint8_t.
  - IDs fora de domínio (`>= Count = 57`) ou parâmetros de famílias trocadas: rejeitados sem corromper memória.
- **Capacidade Máxima:**
  - Juno + LFO + MOD (25 parâmetros): codificado em Modo 1 ocupando 152 bytes (<= 160 B).
  - FM 6-OP + LFO + MOD (42 parâmetros): codificado em Modo 2 ocupando 146 bytes (<= 160 B).
- **Validação:** `final_corrections_test.cpp::testCodecV2PrecisionAndLimits()` e persistência real em AMY.

### 2.4 Gate D & F: Compatibilidade NVS e Documentação
- **Nomenclatura de Chaves:**
  - V1 (`sound011`): chaves `s00a`..`s15b` (128 bytes).
  - V2 (`sound011v2`): chaves `v00a`..`v15b` (160 bytes).
  - Documentação em `editor_0111_gate_f.md` e `editor_0111_gate_h.md` corrigida.
- **Política de Admissão:**
  - Orçamento NVS: 24 KiB (756 entradas brutas).
  - Ocupação máxima: 32 bancos V1 (160 entradas) + 32 bancos V2 (192 entradas) + 2 namespaces + buffer = 358 entradas, dentro do threshold seguro `<= 390`.
  - Barreira de áudio silencioso (`storage_quiet`) atestada antes de qualquer operação de gravação Flash/NVS.

### 2.5 Gate E: Auditoria do ParameterRegistry e Telas
- **Inventário Auditado:**
  - Total de descritores no `ParameterRegistry::all()`: **137**
  - Parâmetros das 7 abas do Sound Editor: **44**
  - Parâmetros gerais da UI / Performance / Sistema: **93**
  - IDs persistentes do motor (`edit::Param`): **57** (`0..56`)
- **UI Behavior:** Verificado que a troca de operador FM (OP1–OP6) atualiza os sliders imediatamente sem manter valores visuais residuais do operador anterior. Controles não suportados no contexto exibem badge `N/A`.

---

## 3. Matriz de Validação & Resultados dos Testes

| Teste / Suíte | Escopo | Resultado |
|---|---|---|
| `smk-p4/tests/compare_editor_factory.ps1` | Regressão PCM independente contra baseline 0.11 com zero overrides em 7 patches de fábrica | `PASS` |
| `smk-p4/tests/final_corrections_test.cpp` | Teste específico dos Gates A, B e C (Mod OFF, Mod Polarity, Codec V2 Precision & Limits) | `PASS` |
| `smk-p4/tests/run_sound_editor.ps1` | Suíte completa do Sound Editor: NVS v2, 8 vozes, Juno/FM, Compare/Revert, Gates A–E, Factory Runtime | `PASS` |
| `smk-p4/tests/run_sound_editor_ui.ps1` | Interface LVGL 9.3 (800×480), auditoria de inventário, simulação de boot e dirty guard | `PASS` |
| `smk-p4/tests/run_harmony.ps1` | Harmony engine, escalas, acordes, inversões, 8 vozes AMY, zero clipping, matriz de reprodução | `PASS` |
| `smk-p4/ui-preview/run_preview.ps1` | Suite de preview da UI: 7 checagens, teste de stress de 700 telas com 0 leaks, 20 frames capturados | `PASS` |
| `pio run -e guition-editor-8` | Build release ESP32-P4 (perfil padrão seguro) | `PASS` |
| `pio run -e guition-editor-8-bench` | Build release ESP32-P4 (perfil de bancada com pinos confirmados) | `PASS` |
| Validação Física na Guition ESP32-P4 | Smoke musical e ciclo de energia com hardware físico conectado | `MANUAL / PENDING HARDWARE` |

---

## 4. Manifestos de Firmware Candidato

### 4.1 Binários Compilados (`0.11.1`)

| Perfil | Caminho do Binário | Tamanho (Bytes) | Flash Utilizada | RAM Utilizada | SHA-256 |
|---|---|---:|---:|---:|---|
| `guition-editor-8` | `smk-p4/.pio/build/guition-editor-8/firmware.bin` | `1,206,640` | `1,205,916` / `16,777,216` B (7.2%) | `38,308` / `327,680` B (11.7%) | `6A4A2FBF40363CE5BEC0E422F9B8A7EF30B1BAC87F5D3EEA58DE1431E09DD2F4` |
| `guition-editor-8-bench` | `smk-p4/.pio/build/guition-editor-8-bench/firmware.bin` | `1,307,600` | `1,306,888` / `16,777,216` B (7.8%) | `39,248` / `327,680` B (12.0%) | `9CF78C4AFE032F640632CD65ECDACD2D6C0F23B3A0EE387D81F6B56B8AB0D864` |

### 4.2 Baseline de Referência para Rollback (`0.11`)

| Perfil | Caminho do Binário | SHA-256 |
|---|---|---|
| `guition-editor-8-bench (0.11)` | `build/worktrees/smk-p4-live-sound-editor-011/smk-p4/.pio/build/guition-editor-8-bench/firmware.bin` | `0BD1FD6CE2F6C72679C17CECF9E96CA6E61CEFC8993349463E70701F5C3F2911` |

---

## 5. Plano de Rollback Verificável

**Atenção:** Nenhuma gravação de firmware deve ser feita sem autorização explícita do operador.

Se for necessário retornar à versão `0.11`:

1. **(Opcional) Backup da Partição NVS Atual:**
   ```powershell
   esptool.py --chip esp32p4 -p COMx read_flash 0x9000 0x6000 nvs_backup_current.bin
   ```
2. **Restauração da Partição de Aplicação (`0x10000`):**
   Gravar o binário original de baseline 0.11 verificado diretamente no endereço `0x10000`. O bootloader (`0x0000`) e a tabela de partições (`0x8000`) permanecem intactos:
   ```powershell
   esptool.py --chip esp32p4 -p COMx write_flash 0x10000 build/worktrees/smk-p4-live-sound-editor-011/smk-p4/.pio/build/guition-editor-8-bench/firmware.bin
   ```
3. **Coexistência NVS:** O firmware `0.11` acessa unicamente o namespace `sound011`. A partição `sound011v2` criada pela versão 0.11.1 é ignorada pelo 0.11, não havendo risco de incompatibilidade ou corrupção de presets legados. Caso o operador deseje uma partição NVS totalmente limpa:
   ```powershell
   esptool.py --chip esp32p4 -p COMx write_flash 0x9000 nvs_backup_pre_0111.bin
   ```

---

## 6. Procedimento de Smoke Test Musical na Guition ESP32-P4 (`MANUAL`)

Este roteiro objetivo valida musicalmente o sistema na bancada com Guition ESP32-P4 + PCM5102A + M-VAVE SMK25 V2:

### Passo 1: Inicialização e Monitoramento Serial
1. Ligar o Guition conectado via USB ao SMK25 V2 e I²S ao PCM5102A.
2. Abrir monitor serial a 115200 bps.
3. Observar log de inicialização:
   - AMY inicializado com 8 vozes e 64 osciladores.
   - Enchimento de buffers I²S sem estalos.
   - Detecção do USB MIDI Host e enumeração do SMK25 V2.

### Passo 2: Patch 4 (`Moving Strings`) — Teste Juno, LFO e Mod Matrix
1. Selecionar Patch `4` (`Moving Strings`).
2. Tocar acordes polifônicos no teclado: verificar som característico de cordas e sustentação natural.
3. Acessar a aba **ENV**: alterar `Decay` e `Sustain`. Verificar audivelmente a alteração do envelope.
4. Acessar a aba **FILTER**: ajustar `FilterEnv` e `FilterKeytrack`. Verificar brilho e resposta ao teclado.
5. Acessar a aba **MOD**:
   - Ajustar `Destination` para `Filter Cutoff`, `Polarity` para `UNI +`, `Amount` para `+80%`.
   - Mover a Mod Wheel (CC1): confirmar abertura gradual do filtro para agudos mais brilhantes.
   - Mudar `Polarity` para `INV -`: mover a roda e confirmar escurecimento gradual do som.
   - Mudar `Source` para `OFF`: mover a roda e confirmar que o som não sofre qualquer alteração.
   - Retornar `Source` para `MOD WHEEL`: confirmar restauração imediata do controle.
6. Pressionar **COMPARE**: confirmar audição do som original de fábrica. Pressionar novamente: confirma som editado.
7. Pressionar **SAVE**: salvar no Slot `01` (`"STRINGS MOD"`).

### Passo 3: Patch 135 (`DX7 PIANO 1`) — Teste FM e Modulação de Pitch
1. Selecionar Patch `135` (`DX7 PIANO 1`).
2. Tocar dinâmico: verificar resposta de velocidade.
3. Acessar aba **FM**:
   - Ajustar `Feedback` e `OpVel` do Operador 1.
   - Verificar variação de harmônicos e ataque.
4. Acessar aba **MOD**:
   - Ajustar `Destination` para `Pitch`, `Polarity` para `BIP +/-`, `Amount` para `+40%`.
   - Mover a Mod Wheel: confirmar vibrato tradicional DX7 centrado na afinação correta.
5. Pressionar **SAVE**: salvar no Slot `02` (`"DX7 VIB"`).

### Passo 4: Polifonia, Harmony Assist e USB Reconnect
1. Ativar **Harmony Assist** na UI (modo Dórico ou Menor Harmônica).
2. Tocar progressão rápida de acordes com 8 vozes. Confirmar que não há notas presas (`stuck notes`) nem engasgos no áudio (`underruns == 0`).
3. Desconectar fisicamente o cabo USB do SMK25 V2 enquanto notas estiverem soando:
   - Confirmar acionamento imediato do **Panic Automático**: silêncio instantâneo e limpeza de todas as vozes.
4. Reconectar o cabo USB:
   - Confirmar reenumeração automática sem reinicialização do ESP32 e tocar novamente.

### Passo 5: Persistência Musical e Ciclo de Energia Físico (Power-Cycle)
1. Desconectar completamente a alimentação (cabo 5V) da Guition por 10 segundos.
2. Reconectar a energia e aguardar o boot.
3. Abrir a aba **PRESETS** e selecionar **RECALL** do Slot `01`.
   - Tocar e confirmar que todos os parâmetros editados de filtro, envelope e modulação do Juno estão presentes com o mesmo timbre exato anterior.
4. Selecionar **RECALL** do Slot `02`.
   - Tocar e confirmar restauração completa das edições FM e vibrato do DX7.

---

## 7. Status Final e Próximo Passo

- **Status de Software:** `PASS` (qualificação rigorosa completa em host e targets).
- **Status de Hardware:** `MANUAL / PENDING HARDWARE` (pronto para gravação de bancada mediante aprovação).
- **Branch:** Mantida em `feature/smk-p4-sound-editor-completion-0111`.
- **Próximo Passo:** Solicitar autorização do operador para gravação de bancada na Guition ESP32-P4 com o binário `guition-editor-8-bench` (SHA-256 `9CF78C4AFE032F640632CD65ECDACD2D6C0F23B3A0EE387D81F6B56B8AB0D864`).
