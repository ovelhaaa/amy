# SMK-P4 0.15.2 — Auditoria e Recuperação de Stack do Audio Owner

## 1. Contexto e Diagnóstico

O ensaio físico da versão 0.15.1 na placa Guition ESP32-P4 registrou:
```text
Internal RAM admission free: 357779 B
Required:                    352256 B
Admission margin:              5523 B

Render average:                 232 us
Render maximum:                 392 us

Owner stack minimum free:       932 B

Render deadlines: 0
OOM: 0
I2S write errors: 0
```

Com uma stack total configurada de `kOwnerStackBytes = 12288` B (12 KiB), uma folga mínima de apenas 932 B sob repouso representa risco crítico de estouro (`Stack Overflow`) sob carga musical intensa (acordes de 8 vozes com harmonia quádrupla, arpejador, delays sincronizados, chorus e trocas de patch simultâneas).

---

## 2. Auditoria Detalhada de Consumo

### 2.1 Tamanhos e Alocações Efetivas

| Componente | Localização Original | Tamanho | Propósito |
|---|---|---:|---|
| `Snapshot state` | Stack de `runAudio()` | 2.608 B | Telemetria e cópia do mailbox de status para UI |
| `int16_t pcm[512]` | Stack de `runAudio()` | 1.024 B | Buffer temporário usado apenas em `PCM_DIAGNOSTIC` |
| `UnifiedOutput output` | Stack de `runAudio()` | ~3.150 B | Saída unificada contendo `PcmHandoff<3>` (3 × 1.028 B) |
| **Subtotal variáveis locais `runAudio()`** | **Stack do owner** | **~6.782 B** | **59,7% da stack total de 12 KiB consumida antes da 1ª chamada** |
| DSP Render (`amy_simple_fill_buffer`, bus, FX) | Chamadas aninhadas | ~2.500 B | Pipeline de síntese e efeitos de áudio AMY |
| Patch Loader (`patches_load_patch_checked`, `captureEdit`) | Chamadas aninhadas | ~1.500 B | Reconstrução de instrumentos e topologia |
| Sound Editor (`applyEdit`, `edit::Request`, rotas) | Chamadas aninhadas | ~700 B | Validação e aplicação de parâmetros e macros |
| FreeRTOS context switch, ISRs e `xQueueOverwrite` | Stack frame | ~400 B | Troca de contexto e envio de telemetria |
| **Total de pico observado na 0.15.1** | **Stack do owner** | **11.356 B** | **Folga restante: 932 B** |

---

## 3. Estratégia de Recuperação (Gate B.2)

1. **Deslocamento seguro de telemetria não crítica para PSRAM:**
   - A estrutura `Snapshot state` (2.608 B) em `standalone_main.cpp` contém apenas contadores de diagnóstico, métricas de tempo de execução e a cópia de estado do engine.
   - Ela é atualizada apenas uma vez por bloco de 256 frames (a cada 5,33 ms) e enviada via `xQueueOverwrite(snapshots, &state)`.
   - Alocar `state` uma única vez na inicialização da tarefa via `heap_caps_malloc(sizeof(Snapshot), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` remove 2.608 B da stack do owner sem consumir 1 byte sequer de RAM interna e sem violar as regras de real-time audio.
2. **Eliminação do buffer temporário inativo:**
   - O buffer `int16_t pcm[kBlockSamples]` (1.024 B) só é utilizado quando `CONFIG_SMKP4_PCM_DIAGNOSTIC` está ativo. Em builds padrão (`guition-editor-8` e `guition-editor-8-bench`), `samples = runtime.render()` entrega diretamente o ponteiro gerado pelo AMY.
   - Condicionar a declaração de `pcm` a `#if defined(CONFIG_SMKP4_PCM_DIAGNOSTIC)` remove 1.024 B da stack.
3. **Ganho Imediato:**
   - $2.608\text{ B} + 1.024\text{ B} = 3.632\text{ B}$ recuperados da stack do worker.
   - A folga mínima estimada sobe de 932 B para aproximadamente **4.564 B livres** (> 37% de margem de segurança).
   - A guarda de admissão de RAM interna (352.256 B) e a margem de admissão física (5.523 B) permanecem rigorosamente preservadas.
