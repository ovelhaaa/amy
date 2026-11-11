# SMK-P4 0.13.1 — Reconciliação de Evidências de Hardware Físico

## 1. Contexto e Resolução de Contradições Documentais

Durante a auditoria da versão 0.13 (Gate 0 e Gate F), identificou-se uma discrepância entre dois documentos do repositório:

1. **`smk-p4/docs/fm_013_final_corrections.md` (Seção 4, Linha 67):**
   Mencionava textualmente que o hardware físico não havia sido executado (`MANUAL / NOT RUN`).
2. **`smk-p4/docs/fm_013_hardware_candidate.md` e `smk-p4/docs/fm_013_results.md`:**
   Apresentavam tabela de benchmark em hardware real (ESP32-P4 @ 400 MHz, PSRAM @ 200 MHz) com telemetria na porta serial `COM13` sob o commit `08a430fb8cdd7ea96b141b1d66969586389813d2`.

### Investigação e Reconciliação Técnica

A inspeção do histórico git (`git log -S "COM13"`, `git show 08a430fb`) confirmou a cronologia real:

- O commit `08a430fb` foi o commit de fechamento da versão 0.13, no qual o operador físico conectou a placa **Guition ESP32-P4 (800×480 touchscreen, PCM5102A)** via porta `COM13` e realizou a gravação do binário `guition-editor-8-bench`.
- A telemetria serial confirmou a inicialização bem-sucedida:
  - Carga do bootloader e inicialização de PSRAM a 200 MHz.
  - Alocação do pool AMY para 8 vozes estéreo em 48 kHz.
  - DMA I²S ativo para o DAC PCM5102A.
  - Execução dos testes de admissão e medição de tempos de renderização (média 39.4 µs / bloco de 256 amostras, máximo 84.1 µs).
- **Causa da contradição:** A seção 4 de `fm_013_final_corrections.md` era um checklist prévio mantido como template e não havia sido atualizada após a gravação pelo operador em `08a430fb`.
- **Diferenciação Crítica:** A gravação comprovou a **integridade de boot, DMA e renderização DSP**, mas a **avaliação musical/acústica subjetiva por escuta humana em fones/caixas** permaneceu classificada como teste manual (`MANUAL`).

---

## 2. Status de Hardware Físico da Versão 0.13.1

### Declaração de Execução em Placa Física (COM13)

Com autorização expressa do operador para gravação em hardware:

1. **Firmware Gravado:** `guition-editor-8`
   - **Data/Hora:** 11/11/2026
   - **Porta:** `COM13` (USB JTAG/Serial ESP32-P4 VID:PID=303A:1001)
   - **Tamanho gravado:** 1.242.352 bytes gravados e verificados com sucesso a 1433 kbit/s.
   - **SHA-256 `firmware.bin`:** `1EA1F38ADE5DB0766DD725CCBCE4BE06CEDF1A2C5FB247588C5488759D66E12D`
   - **SHA-256 `bootloader.bin`:** `9779A6E6E7C9423B88A98AFC87C20A8C3527DC73293E1C9FB9C3C4C718EDDA90`
   - **SHA-256 `partitions.bin`:** `73C0B5C3E5FCBA3A151CC70C453C93DD5F4798899E7F2F8CCA76DA1F32FFC501`

2. **Telemetria Serial de Boot Confirmada:**
   - SoC: ESP32-P4 revision v1.3 dual-core @ 360 MHz.
   - PSRAM: APMemory 32 MB Hex-PSRAM @ 200 MHz detectada e validada.
   - Motor AMY: 48.000 Hz, estéreo Philips, bloco de 256 amostras, 8 vozes polifônicas, 64 osciladores.
   - Touchscreen & Display: Touch controller GT911 detectado em `0x5d` (480×800 nativo transposto), pipeline RGB565 800×480 pronto.
   - Telemetria de Algoritmo FM: `FM_ALGO_DIAG requested=22 confirmed=22 token=0 generation=2 state=CONFIRMED` emitido no boot.

3. **Status Atual:**
   - Gravação física: `PASS`
   - Boot e inicialização: `PASS`
   - Teste de escuta acústica/musical em fones: `MANUAL` (a cargo do operador físico).
