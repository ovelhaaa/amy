# SMK-P4 UI Blueprint 1.1 — Contrato de Integração com `PerformanceBackend`

**Branch:** `feature/smk-p4-ui-integration-ready`  
**Alvo:** Guition JC-ESP32P4-M3 (`800 × 480` horizontal, LVGL 9.3.0)  
**Componente UI:** [`smk-p4/components/ui_blueprint/`](file:///C:/Users/devx/Documents/PlatformIO/Projects/amy/build/worktrees/smk-p4-ui-integration-ready/smk-p4/components/ui_blueprint)

---

## 1. Visão Geral do Desacoplamento

No **UI Blueprint 1.1**, todas as 14 telas (`ScreenView`), os 11 widgets reutilizáveis, o gerenciador de navegação ([`UiNavigationManager`](file:///C:/Users/devx/Documents/PlatformIO/Projects/amy/build/worktrees/smk-p4-ui-integration-ready/smk-p4/components/ui_blueprint/include/ui_navigation.h)) e o coordenador principal ([`UiBlueprint`](file:///C:/Users/devx/Documents/PlatformIO/Projects/amy/build/worktrees/smk-p4-ui-integration-ready/smk-p4/components/ui_blueprint/include/ui_blueprint.h)) operam exclusivamente contra a interface abstrata [`IUiBackend`](file:///C:/Users/devx/Documents/PlatformIO/Projects/amy/build/worktrees/smk-p4-ui-integration-ready/smk-p4/components/ui_blueprint/include/ui_backend.h).

O componente `ui_blueprint` **não importa**:
- `amy.h` ou `amy_runtime.h`;
- `audio_pcm5102.h`;
- `usb_midi_host.h` ou `usb_input.h`;
- `performance_backend.h` ou `local_performance_backend.h`.

```text
Telas LVGL (01..14) + Widgets
            │
            ▼
       IUiBackend  (contrato puro em ui_backend.h)
       ├── UiDemoBackend                 (simulação em RAM no host/preview)
       └── UiBlueprintPerformanceAdapter (futuro adaptador no firmware principal smk-p4/main/)
                    │
                    ▼
           PerformanceBackend            (API real em smk-p4/main/performance_backend.h)
                    │
                    ▼
              AmyRuntime / I2S / USB-MIDI Host
```

---

## 2. Separação entre Modelo Visual e Comando Musical

Para evitar que um movimento de slider ou toque de botão na UI seja tratado automaticamente como confirmação de que o motor AMY aplicou a alteração, [`IUiBackend`](file:///C:/Users/devx/Documents/PlatformIO/Projects/amy/build/worktrees/smk-p4-ui-integration-ready/smk-p4/components/ui_blueprint/include/ui_backend.h) separa explicitamente:

1. **Leitura do modelo visual (`model()`)**: reflete o último snapshot confirmado/aplicado.
2. **Solicitação de comando musical (`request*()`)**: retorna um [`UiCommandResult`](file:///C:/Users/devx/Documents/PlatformIO/Projects/amy/build/worktrees/smk-p4-ui-integration-ready/smk-p4/components/ui_blueprint/include/ui_backend.h#L20-L30) com [`UiCommandState`](file:///C:/Users/devx/Documents/PlatformIO/Projects/amy/build/worktrees/smk-p4-ui-integration-ready/smk-p4/components/ui_blueprint/include/ui_backend.h#L10-L16):
   - `UiCommandState::Confirmed`: comando aceito e confirmado pelo motor musical.
   - `UiCommandState::Requested`: comando enfileirado/enviado ao backend; aguardando confirmação no próximo snapshot (`stateRevision()`).
   - `UiCommandState::DemoApplied`: aplicado apenas no modelo demonstrativo em RAM (`UiDemoBackend`), sem escrita em DSP.
   - `UiCommandState::Unsupported`: parâmetro ou patch planejado (`PLANNED`) ou indisponível (`UNAVAILABLE`) no backend atual (ex.: `setMacro()` no P4 0.3). Os widgets (`parameter_slider`, `parameter_knob`) revertem automaticamente sua posição visual para o valor confirmado do modelo quando `!result.accepted()`.
   - `UiCommandState::Rejected`: falha de comando, backend ocupado (`busy()`), desconectado (`!connected()`) ou valor fora de faixa.

---

## 3. Assinaturas Reais de `PerformanceBackend` Auditadas

Consultadas diretamente em `smk-p4/main/performance_backend.h` e `smk-p4/main/local_performance_backend.h` (`feature/smk-p4-standalone-hardware-validation`):

```cpp
class PerformanceBackend {
public:
    virtual ~PerformanceBackend() = default;
    virtual void service(uint32_t now_ms) = 0;
    virtual bool connected() const = 0;
    virtual bool busy() const = 0;
    virtual const bridge::StatePayload& state() const = 0;
    virtual uint32_t stateRevision() const = 0;
    virtual const char* status() const = 0;
    virtual const char* takeNotice() = 0;
    virtual bool local() const { return false; }
    virtual bool midiError() const { return false; }
    virtual size_t patchCount() const = 0;
    virtual const PatchChoice& patchAt(size_t index) const = 0;
    virtual bool patchInfo(size_t index, bridge::PatchInfoPayload& info, uint32_t now_ms) = 0;
    virtual bool setPatch(uint8_t id, uint32_t now_ms) = 0;
    virtual bool setMacro(uint8_t index, uint8_t value, uint32_t now_ms) = 0;
    virtual bool setMaster(uint8_t value, uint32_t now_ms) = 0;
    virtual bool noteOn(uint8_t note, uint8_t velocity, uint32_t now_ms) = 0;
    virtual void noteOff(uint8_t note) = 0;
    virtual uint8_t heldNote(uint8_t note) const = 0;
    virtual void panic() = 0;
};
```

> **Observações importantes da implementação real (`LocalAmyPerformanceBackend`):**
> - Não existe `setOctaveShift()` nem `selectProgram()` na API `PerformanceBackend`.
> - `setMacro(index, value, now_ms)` retorna `false` hoje no P4 standalone (`"MACRO PLANNED; NOT APPLIED"`).
> - Os patches materializados em ROM hoje são `128` (`DX7 BRASS 1`) e `135` (`DX7 PIANO 1`) em `runtime::kPatches`.

---

## 4. Especificação do Futuro `UiBlueprintPerformanceAdapter`

O adaptador concreto deverá residir futuramente em `smk-p4/main/ui_blueprint_performance_adapter.h` (fora de `components/ui_blueprint/`), conectando `IUiBackend` a `PerformanceBackend`:

### 4.1 Mapeamento de Comandos e Estado

| Método `IUiBackend` | Operação em `PerformanceBackend` | Estado Retornado (`UiCommandState`) |
|---|---|---|
| `isDemoMode()` | `false` | Badge superior exibe `P4 LIVE` |
| `isConnected()` | `perf.connected()` | Atualiza status `MIDI: OK` / `MIDI: WAIT` |
| `isBusy()` | `perf.busy()` | Bloqueia/rejeita comandos se `true` |
| `hasMidiError()` | `perf.midiError()` | Exibe `MIDI: ERR` no `StatusStrip` |
| `isPatchSelectable(id)` | Verifica se `id` existe em `perf.patchAt(0..patchCount()-1)` (`128`, `135`) | Filtra `< PREV` / `NEXT >` para selecionar apenas patches suportados |
| `requestSelectPatch(id, layer)` | Se `layer == 0` e `isPatchSelectable(id)`: libera notas UI ativas (`releaseAllUiNotes()`) e chama `perf.setPatch(id, now_ms)` | `Confirmed` se `setPatch()` retornar `true`; `Unsupported` para patches planejados ou `layer != 0` |
| `requestParameterInt("perf.master_volume", v)` | `perf.setMaster(v, now_ms)` | `Confirmed` se `true`, `Rejected` se `false` |
| `requestParameterInt("perf.octave_shift", v)` | Armazenado na camada de apresentação/entrada do adaptador (`-2..+2`) sem chamar `PerformanceBackend` | `Confirmed` |
| `requestParameterInt("perf.macro.1..8", v)` | Chama `perf.setMacro(idx, v, now_ms)`; enquanto retornar `false`, conserva indicação de planejamento e não altera o valor confirmado | `Unsupported` (reverte o slider/knob para o valor confirmado) |
| Demais parâmetros (`osc.*`, `fm.*`, `filter.*`, `fx.*`, `seq.*`, etc.) | Não enviados ao AMY no estágio 0.3; preservam badge `PLAN` | `Unsupported` |
| `requestNoteOn(note, vel)` | `perf.noteOn(note, vel, now_ms)` + registra token ativo para `note` | `Confirmed` + `token > 0` |
| `requestNoteOff(note, token)` | Verifica token ativo para `note` e chama `perf.noteOff(note)` | `Confirmed` |
| `releaseAllUiNotes()` | Para cada nota UI ativa na tabela `held_tokens_`, chama `perf.noteOff(note)` | Garante zero notas presas ao trocar de tela ou patch |
| `requestPanic()` | `releaseAllUiNotes()` + `perf.panic()` | `Confirmed` |
| `service(now_ms)` | `perf.service(now_ms)`; se `perf.stateRevision() != synced_rev_`, sincroniza `perf.state()` (`patch_id`, `master_volume`, `active_voices`, `usb_connected`, `render_max_us`, `underruns`) para `UiModel` | Mantém UI sincronizada com mudanças vindas de MIDI externo |

### 4.2 Transposição de Oitava Segura (`perf.octave_shift`)

Como `PerformanceBackend` não possui `setOctaveShift()`:
1. A transposição de oitava (`-2..+2`) é aplicada no momento do `LV_EVENT_PRESSED` em [`virtual_keyboard.cpp`](file:///C:/Users/devx/Documents/PlatformIO/Projects/amy/build/worktrees/smk-p4-ui-integration-ready/smk-p4/components/ui_blueprint/widgets/virtual_keyboard.cpp), calculando a nota MIDI física `note = start_note + semitone_offset`.
2. O slot da tecla pressionada armazena `slot.active_note = note` e `slot.active_token = res.token`.
3. Os botões `OCT -` e `OCT +` na tela Performance atualizam `perf.octave_shift` e chamam `nav.refreshActive()` (atualização seletiva in-place, **sem destruir o teclado**).
4. Se o músico mudar a oitava mantendo uma tecla pressionada, quando soltar a tecla (`LV_EVENT_RELEASED` ou `LV_EVENT_PRESS_LOST`), o `VirtualKeyboard` envia `requestNoteOff(slot.active_note, slot.active_token)` usando a altura exata que foi disparada no `Note On`, impedindo notas presas.
