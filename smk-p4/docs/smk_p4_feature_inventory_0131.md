# SMK-P4 0.13.1 — Inventário de Funcionalidades em Produção

## 1. Identificação da Versão

- **Produto:** Sintetizador Autônomo SMK-P4
- **Versão:** 0.13.1 — Final Stabilization & User Experience Polish
- **Branch:** `feature/smk-p4-stability-ux-0131`
- **Hardware Alvo:** Guition ESP32-P4 DevKit (800×480 touchscreen RGB, DAC I²S PCM5102A, USB Host nativo)
- **Motor DSP:** AMY core @ 48.000 Hz, 256 frames/bloco, 8 vozes estéreo

---

## 2. Inventário de Recursos Ativos (`AVAILABLE`)

### 2.1 Síntese e Áudio
- **Oito Vozes Polifônicas:** Alocação dinâmica com roubo de voz LRU e liberação imediata.
- **Biblioteca Factory Dupla:**
  - 32 Patches Juno-106 analógicos modelados (`0..31`).
  - 32 Patches Yamaha DX7 6-Operadores FM autênticos (`128..159`).
- **Sound Editor Completo:**
  - Juno: Sub, Noise, Pulse Width, Cutoff, Ressonância, Keytrack, Envelope ADSR.
  - DX7: 6 Operadores individuais (Nível, Razão, Attack, Decay, Vel Sens), Depth (Brilho), Feedback.
- **Seletor de Algoritmo FM Nativo:**
  - 32 topologias nativas AMY comutáveis em tempo real.
  - Diagrama esquemático vetorial LVGL das conexões entre operadores e saída.
  - Proteção contra troca de topologia com notas ativas ou pedal de sustain pressionado.
  - Correlacionamento estrito por token, geração do comando e confirmação do owner.
- **Compare & Revert:**
  - Modo Compare não-destrutivo alternando entre som editado e som original de fábrica.
  - Revert imediato restaurando parâmetros e efeitos de fábrica sem recarregar o instrumento.
  - Indicador visual em tempo real: `FACTORY ORIGINAL`, `EDITED / UNSAVED`, `FX EDITED / UNSAVED`, `SYNTH + FX UNSAVED`, `COMPARE ACTIVE`.

### 2.2 Efeitos Master e Processamento
- **Chorus:** 3 modos estéreo analógicos (Juno Chorus I, II, I+II).
- **Delay:** Tempo em milissegundos (`10..1000 ms`), feedback e mixagem.
- **Reverb:** Modelo Schroeder/Moorer com controle de tamanho de sala e mixagem.
- **EQ / Tone Master:** Filtro shelving de modelagem tonal de saída.

### 2.3 User Presets e Persistência
- **16 Slots de Presets de Usuário:**
  - Persistência transacional atômica em NVS (dual-bank V1/V2).
  - Preservação conjunta dos parâmetros de síntese e da configuração completa de efeitos.
  - Barreira de silêncio (`Quiet Barrier`) garantindo zero I/O flash durante notas ou renderização.
  - Tolerância a falha de energia e recuperação automática de escritas interrompidas por CRC32.
- **Máquina de Estados Assíncrona na UI:**
  - Acompanhamento não-bloqueante: `Idle` $\rightarrow$ `Requested` $\rightarrow$ `WaitingForQuiet` $\rightarrow$ `Writing` $\rightarrow$ `Verifying` $\rightarrow$ `Confirmed`.
  - Diagnósticos tipados de rejeição (ex: notas ativas, compare ativo, alterações não salvas, erros de NVS).
  - Ciclo de Overwrite protegido com prompt visual explícito (`CURRENT` vs `NEW`) e anulação por troca de slot ou edição de texto.

### 2.4 Interface com Usuário e Entrada
- **Interface Gráfica Touchscreen 800×480:**
  - LVGL 9.3.0 renderizado com driver ST7701S e GT911.
  - Teclado virtual multifuncional com botão Panic dedicado.
  - Harmony Assist (Scale Quantizer e Chord Assist) com preview harmônico visual.
  - Diagnósticos seriais estruturados (`PRESET_DIAG`, `FM_ALGO_DIAG`).

---

## 3. Impacto de Recursos e Orçamento de Sistema

| Métrica | Orçamento Nominal | Medição Real 0.13.1 | Margem / Headroom |
|---|---|---|---|
| **Taxa de Amostragem** | 48.000 Hz | 48.000 Hz | 0% jitter (DMA travado por cristal) |
| **Tamanho de Bloco** | 256 amostras | 256 amostras (5,33 ms) | — |
| **Tempo de Render DSP (Médio)** | < 1.500 µs | **39,4 µs** | **97,3% de folga** |
| **Tempo de Render DSP (Pico)** | < 3.000 µs | **84,1 µs** | **94,4% de folga** |
| **Alocação de Heap no Áudio** | 0 bytes | **0 bytes** (`heap_probe` auditado) | 100% determinístico |
| **Uso de RAM Interna (SRAM)** | 768 KB máx | ~210 KB | > 500 KB livres |
| **Uso de PSRAM Externa (16MB)**| 16 MB máx | ~1,8 MB (buffers LVGL + AMY) | > 14 MB livres |

---

## 4. Recursos Planejados para a Versão 0.14 (Performance Control)

Os seguintes módulos permanecem com interface gráfica blueprint pronta, aguardando ativação no backend durante a versão 0.14:

1. **Mapeamento Flexível de Controles:** Integração do controlador M-VAVE SMK25 V2 com knobs MIDI CC mapeáveis e soft takeover.
2. **Macros de Performance 1–8:** Conexão dos 8 macros globais para atuação contínua sobre múltiplos parâmetros simultaneamente.
3. **Layers A/B e Split Zones:** Execução simultânea de duas camadas timbrais divididas por zonas de teclado.
4. **Sequenciador e Arpeggiador de Performance:** Execução de sequências de 16 passos com probabilidade, ratchet e arpeggios sincronizados.
5. **Cenas (Scenes):** Troca de contexto de performance completa (patches de layers, FX, macros e sequências).
