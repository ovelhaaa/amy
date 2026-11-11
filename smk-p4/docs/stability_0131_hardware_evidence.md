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

Em estrita conformidade com as regras inegociáveis de `AGENTS.md` e deste Codex:

> **REGRA:** Não gravar firmware na Guition sem autorização explícita do usuário para o binário e SHA-256 específicos.

### Declaração de Estado

1. **Firmwares Candidatos 0.13.1:**
   - Os binários `guition-editor-8` e `guition-editor-8-bench` foram compilados e qualificados em ambiente de build (PlatformIO / ESP-IDF).
   - Os digests criptográficos SHA-256 foram calculados e registrados formalmente.
2. **Gravação Físcia em 0.13.1:**
   - **NÃO GRAVADO (NOT RUN / PENDING OPERATOR AUTHORIZATION)**.
   - Nenhuma tentativa de gravação via esptool ou porta serial foi realizada na versão 0.13.1.
3. **Instruções para Teste Manual em Bancada (Pós-Autorização):**
   - Conectar cabo USB-C na porta UART/Prog da Guition ESP32-P4.
   - Conectar fone de ouvido ou amplificador na saída do PCM5102A.
   - Conectar controlador M-VAVE SMK25 V2 na porta USB Host.
   - Gravar via comando:
     ```bash
     pio run -d smk-p4 -e guition-editor-8 -t upload
     ```
   - Monitorar serial (`pio device monitor -d smk-p4 -b 115200`).
   - Testar o fluxo de User Presets (Save As, Overwrite com prompt de confirmação, Recall) e verificar que as novas mensagens tipadas e os logs `PRESET_DIAG` e `FM_ALGO_DIAG` aparecem no terminal.
