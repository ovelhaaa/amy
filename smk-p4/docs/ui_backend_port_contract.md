# UI Blueprint 1.1 → PerformanceBackend: contrato auditado 0.4

Fontes: [PerformanceBackend](../main/performance_backend.h),
[LocalAmyPerformanceBackend](../main/local_performance_backend.h),
[IUiBackend](../components/ui_blueprint/include/ui_backend.h).
A descrição antiga continha assinaturas e confirmações incorretas.
A API musical não foi modificada. Ownership/fontes completos:
[contrato0.3B](performance_backend_contract.md).

## API efetiva

```cpp
const PerformanceState& state() const;
uint32_t stateRevision() const;
uint16_t patchCount() const;
uint16_t patchAt(uint16_t index) const;
bool patchInfo(uint16_t index, PatchInfo& info, uint32_t now_ms);
bool setPatch(uint16_t id, uint32_t now_ms);
bool setMaster(uint8_t value, uint32_t now_ms);
bool noteOn(uint8_t note, uint8_t velocity, uint32_t now_ms);
void noteOff(uint8_t note);
void heldNote(uint8_t note); // no-op local; não renova lease
void panic();
```

PerformanceState é alias de smk::link::State, não bridge::StatePayload.
patchAt devolve ID inteiro, não PatchChoice. PatchInfo contém id/nome/available.
Não existem setters de oitava/FX/Layer B.

## Adaptador implementado

[UiBlueprintPerformanceAdapter](../main/ui_blueprint_performance_adapter.h)
implementa IUiBackend na tarefa UI existente. Widgets só conhecem IUiBackend;
o adaptador usa PerformanceBackend. Nenhuma chamada AMY/I²S/USB nos widgets.
Mailbox owner e estado MIDI continuam publicados pelo loop existente.

| Ação | Resultado |
|---|---|
| Patch128/135, Layer A | release Touch; setPatch; Requested se aceito |
| Master0..127 | setMaster; Requested se aceito |
| Snapshot posterior com valor correspondente | atualiza modelo aplicado; Confirmed para pedido pendente |
| Rejeição/fila cheia/offline | Rejected; mantém valor confirmado; patch false ainda pode disparar Panic |
| Oitava -2..2 | release Touch; transformação local Confirmed |
| Note On | primeiro gesto por pitch chama noteOn; Requested, sem alegar ACK audível |
| Note Off | último gesto chama noteOff; Requested, latch prioritário |
| Panic | release Touch + panic global; Requested, sem inventar ACK |
| Macros/demais parâmetros/Layer B | Unsupported; nenhum comando DSP |

Modelo visual não é barramento automático de comandos. Favoritos/grids/previews
permanecem RAM demonstrativa. Reset global demo é bloqueado no live para não
substituir valores confirmados por defaults.

Revisão não é ACK. Confirmação exige revisão diferente da observada no pedido e
valor correspondente. Snapshot com valor anterior mantém Requested. Não há
sequência/timestamp/ACK individual de publicação na API musical: republicação de
mailbox atrasado com revisão nova não é distinguível. Valor igual comprova estado
observado, não causalidade. Antes da primeira revisão válida, comandos musicais
são rejeitados. active_notes agregado não confirma uma nota ou Panic individual.

## Gestos e lease

Ledger fixo de32 gestos; tokens UI monotônicos não reutilizados, independentes
dos tokens backend. Zero é inválido. Primeiro On/último Off por pitch. Releases
antigos não afetam gesto novo após Panic/patch/releaseAll/oitava/navegação.
Saturação rejeita gesto sem bloquear. Lease10s começa no primeiro On do pitch;
gestos adicionais herdam o início e não renovam mesmo se o primeiro soltar.
Expiração libera todos os gestos daquele pitch e envia Off. Não há On periódico
nem renovação por heldNote. Soltar e pressionar para rearmar. GT911 permanece
single-touch: ledger não promete novo suporte físico multitouch.

USB/Touch da mesma altura têm owners distintos. Disconnect USB cancela somente
USB no runtime existente, preservando Touch conforme baseline; Panic explícito
e patch cancelam ambas as fontes. Navegação libera apenas Touch.

LVGL9.3 obj_delete_core envia DELETE ao pai ANTES dos filhos. No DELETE do
teclado: release sem tocar visuais, remoção dos callbacks dos filhos que usam os
slots e só depois liberação do estado. DELETE isolado de tecla libera seu gesto
e zera ponteiros. Testes LVGL cobrem ambos, navegação e shutdown com nota.
Harmony/Split preview não transmitem notas live.

## Telemetria e build

connected() significa engine pronta. MIDI usa kUsbConnected/midiError; Audio RUN
exige kAudioLive (running && physical). publishMidi não avança revisão: flags
são consultadas em cada tick. Patch/master vêm do snapshot aplicado. Vozes são
gates lógicos, não caudas DSP. Render máximo/deadlines vêm do owner. Heap LVGL,
interno e PSRAM são medidos na tarefa UI a cada5s. Demais contadores continuam
nos logs existentes. DMA underruns, amplitude e latência física são N/A.

Legacy é padrão: guition-standalone. Candidato: guition-standalone-blueprint /
CONFIG_SMKP4_UI_BLUEPRINT=y. Uma única UI/port/tarefa; sem nova inicialização de
display/touch/LVGL. Gates elétricos seguros por padrão; IDF5.5.2/LVGL9.3/pool128KB
preservados. Preview isolado usa UiDemoBackend, sem eventos ao motor.

```powershell
./smk-p4/ui-preview/run_preview.ps1
./smk-p4/tests/run_amy.ps1
./smk-p4/tests/run_blueprint_adapter.ps1
```

O último script usa objetos/libraries dos anteriores. Resultados e procedimento
físico em [integration_04_results.md](integration_04_results.md).
