# SMK-P4 0.15.2a — Candidato de Hardware e Roteiro de Ensaio Físico

## 1. Binários Candidatos de Firmware

Os binários a seguir foram compilados no ambiente de desenvolvimento Windows utilizando PlatformIO e o toolchain ESP32-P4 (GCC 14.2.0, ESP-IDF 5.5.2):

### Perfil 1: `guition-editor-8` (Candidato de Produção / Release)
- **Caminho do Binário:** `smk-p4/.pio/build/guition-editor-8/firmware.bin`
- **Tamanho do Arquivo:** 1.334.976 bytes
- **Ocupação de Flash:** 1.334.252 B / 16.777.216 B (8,0%)
- **Ocupação de RAM Interna:** 42.124 B / 327.680 B (12,9%)
- **SHA-256:**
  ```text
  021B8452DEA3B3E33D3D941736C4DC9B38247FFE6AB62717F3CE54E55BE64D76
  ```

### Perfil 2: `guition-editor-8-bench` (Candidato para Bancada com Telemetria e Diagnósticos)
- **Caminho do Binário:** `smk-p4/.pio/build/guition-editor-8-bench/firmware.bin`
- **Tamanho do Arquivo:** 1.436.912 bytes
- **Ocupação de Flash:** 1.436.188 B / 16.777.216 B (8,6%)
- **Ocupação de RAM Interna:** 41.968 B / 327.680 B (12,8%)
- **SHA-256:**
  ```text
  96A66A26D5F7A2DAE9502D3F8B32B175D3882D3D5A2238E554D092EEC0FDAF9F
  ```

---

## 2. Política Estrita de Gravação de Firmware

> [!IMPORTANT]
> **Nenhuma gravação de firmware será executada automaticamente sem autorização explícita do usuário.**
> As partições existentes e a memória NVS contendo presets do usuário não serão apagadas ou reformatadas.

### Instruções para Gravação Manual (quando autorizado)
Após confirmação explícita do usuário e conferência do SHA-256 desejado, a gravação na porta COM da Guition (ex: `COM13`) deve ser realizada via:

```powershell
python -m esptool --chip esp32p4 -p COM13 -b 921600 write-flash 0x00010000 smk-p4/.pio/build/guition-editor-8-bench/firmware.bin
```

---

## 3. Auditoria de Stack e Memória Livre no Hardware

### 3.1 Divergência Identificada na 0.15.2
- **Medição Física 0.15.2:** `stack_free_bytes = 3584 B`.
- **Requisito de Hardware:** Mínimo de 4.000 B de stack livre.
- **Análise Técnica:** O overhead de salvamento de registros callee-saved da arquitetura RISC-V e as chamadas de interrupção DMA e context switches FreeRTOS utilizam aproximadamente 1.000 B a mais do que a pilha simplificada da emulação host.

### 3.2 Solução Implementada em 0.15.2a
- Incremento controlado de 512 B no stack do Audio Owner (`kOwnerStackBytes = 12800`).
- **Margem Física Projetada:**
  $$\text{Stack Livre} \approx 3.584 \text{ B} + 512 \text{ B} = 4.096 \text{ B} \quad (\ge 4.000 \text{ B})$$
- **Margem de Admissão de RAM Interna:**
  $$\text{RAM Livre na Admissão} \approx 357.763 \text{ B} - 512 \text{ B} = 357.251 \text{ B} \quad (\gg 352.256 \text{ B})$$
  Preserva-se uma folga confortável de $\approx 5 \text{ KiB}$ acima do piso estrito de admissão.

---

## 4. Roteiro de Ensaio Físico na Bancada (Guition JC4880P433 + M-VAVE SMK25 V2)

Para validação em bancada, seguir a sequência de testes abaixo:

### Passo 1: Troca Normal de Timbres (Clean Load)
1. Tocar teclas no SMK25 no Patch 0 (Juno Lead).
2. Tocar no Patch Selector na tela touchscreen e selecionar Patch 135 (DX7 Piano).
3. Selecionar **LOAD PATCH** (sem locks).
4. **Verificação:** O novo timbre soa limpo e seco (dry); sem sobras de som ou efeitos do patch anterior.

### Passo 2: Troca com Keep FX
1. No Patch 135, ajustar Delay (Mix 40%, Time 350 ms, Feedback 50%) e Reverb.
2. Abrir o Patch Selector, marcar **Keep FX** e selecionar Patch 0.
3. Tocar em **LOAD WITH LOCKS**.
4. **Verificação:** O Patch 0 carrega imediatamente com os efeitos de delay e reverb preservados sem interrupção abrupta.
5. Observar a indicação na tela: deve exibir **UNSAVED** (dirty state correto).

### Passo 3: Troca com Keep Harmony e Arpeggiator
1. Ativar Arpeggiator (Up, 1/16) e segurar um acorde no SMK25.
2. Trocar de patch com **Keep Arpeggiator** e **Keep Harmony** ligados.
3. **Verificação:** As notas antigas são cortadas imediatamente (sem stuck notes); ao tocar uma nova nota, o arpejo e a harmonia continuam configurados exatamente como antes.

### Passo 4: Troca entre Famílias de Síntese com Keep Macros
1. Posicionar Macro M1 e M2 em valores diferentes de 64.
2. Alternar entre Juno (Subtractive) e DX7 (FM) com **Keep Macros** ligado.
3. **Verificação:** Parâmetros universais permanecem ajustados; parâmetros incompatíveis não causam estalos, falhas de áudio ou travamento.

### Passo 5: Dirty State, Save As e Revert
1. Em um patch com Keep FX ativo (estado UNSAVED), acessar o menu de Presets.
2. Executar **Save As** em um slot de usuário livre (ex: Slot 1).
3. **Verificação:** A indicação muda imediatamente para **CLEAN**.
4. Modificar o mix de delay e pressionar **Revert**.
5. **Verificação:** O efeito retorna exatamente ao valor salvo no preset.

### Passo 6: PREV / NEXT Rápido em Performance
1. Pressionar os botões físicos ou touch de PREV e NEXT repetidamente em rápida sucessão.
2. **Verificação:** A interface não trava; requisições intermediárias são ignoradas de forma determinística enquanto o áudio estiver carregando o novo timbre; a troca é suave e o sistema não entra em estado inconsistente.

### Passo 7: Teste sob Carga Musical Máxima
1. Tocar polifonia de 8 vozes com acordes densos, delay sincronizado e reverb ativos.
2. Alternar timbres durante a sustentação.
3. **Verificação:** Ausência de underruns de áudio (I²S errors = 0), sem distorções no buffer circular e latência estável.

### Passo 8: Panic e Recuperação de Emergência
1. Com notas sustentadas e delay soando, pressionar o botão **PANIC** na UI ou o atalho no SMK25.
2. **Verificação:** Todas as vozes são imediatamente silenciadas; o sistema permanece pronto e responsivo para novas notas.
