# SMK-P4 0.15.2a — Candidato para ensaio na Guition

## Imagens

| Perfil | Build | Bytes do arquivo | SHA-256 de `firmware.bin` |
| --- | --- | ---: | --- |
| `guition-editor-8` | PASS | 1.334.400 | `1301793A92E9E912D89659B9D809E87F68830C67BFE00377EE1C14FB82A3F2A0` |
| `guition-editor-8-bench` | PASS | 1.436.336 | `15055555C66D0131BE042E9E4CA407B0FCA57EB3667F9C2C7B4D1609FC530187` |

As imagens de software foram geradas em `smk-p4/.pio/build/<perfil>/`.
O perfil padrão mantém os gates elétricos desativados. O perfil de bancada
habilita o I²S nos GPIOs 50/51/52 e USB HS0 da montagem existente.

## Qualificação física

**PENDING** — Guition JC4880P433, ESP32-P4 rev. 1.3, com SMK25 V2 e PCM5102A
na montagem de bancada existente. A gravação da imagem de bancada exige
autorização explícita para o arquivo identificado pelo SHA-256. Use um slot
de teste livre na verificação de Save As.

| Métrica | Guition 0.15.2 | 0.15.2a |
| --- | ---: | --- |
| Stack livre do Audio Owner | 3.584 B | PENDING — projeção de 4.096 B. Confirmar mínimo de 4.000 B sob carga |
| RAM interna livre na admissão | 357.763 B | PENDING — projeção de 357.251 B. Confirmar mínimo de 352.256 B |
| Render 256 frames, média / máximo | 239 / 522 µs | PENDING — medir carga com oito vozes e FX |
| Deadlines de render, erros de escrita I²S e timeouts | 0 na captura registrada | PENDING — medir novamente |
| Qualidade auditiva, touchscreen e reconexão USB | — | PENDING — executar roteiro |

O orçamento de render é 5,333 ms por bloco de 256 frames a 48 kHz.
Registre a telemetria `ADMISSION`, `AUDIO`, `ENGINE` e `PATCH08` com o
hash da imagem durante as trocas de patch e ao final do ensaio.

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
4. **Verificação:** O Patch 0 mantém os valores de delay e reverb. Registre separadamente qualquer interrupção ou artefato audível.
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
3. **Verificação:** A indicação fica limpa após confirmação da gravação.
4. Modificar o mix de delay e pressionar **Revert**.
5. **Verificação:** O efeito retorna exatamente ao valor salvo no preset.

### Passo 6: PREV / NEXT Rápido em Performance
1. Pressionar os botões físicos ou touch de PREV e NEXT repetidamente em rápida sucessão.
2. **Verificação:** A interface não trava; requisições intermediárias são ignoradas de forma determinística enquanto o áudio estiver carregando o novo timbre; o patch mostrado corresponde ao confirmado pelo Owner.

### Passo 7: Teste sob Carga Musical Máxima
1. Tocar polifonia de 8 vozes com acordes densos, delay sincronizado e reverb ativos.
2. Alternar timbres durante a sustentação.
3. **Verificação:** Registre render médio/máximo, deadlines, refill, write errors, timeouts e artefatos audíveis. O firmware reporta underruns de DMA como `UNMEASURED`.

### Passo 8: Panic e Recuperação de Emergência
1. Com notas sustentadas e delay soando, pressionar o botão global **PANIC** na UI enquanto o popup está aberto.
2. **Verificação:** Todas as vozes são imediatamente silenciadas; a UI informa a interrupção se a carga ainda estiver pendente e libera CANCEL. Verifique novas notas após confirmar um patch seguro.


### Passo 9: Mesmo patch e recuperação USB
1. Altere o mix do reverb e recarregue o mesmo patch com Keep FX. Confirme
   o valor preservado, dirty state e Compare. Repita com carga limpa e
   confirme a volta aos FX de fábrica.
2. Desconecte o SMK25 com notas ativas. Confirme que as notas cessam e
   registre os contadores de conexão, desconexão e erros USB.
3. Reconecte o SMK25. Toque novas notas, confirme que o instrumento voltou
   a responder e registre se houve reinício.
