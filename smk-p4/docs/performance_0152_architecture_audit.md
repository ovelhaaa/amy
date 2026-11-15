# SMK-P4 0.15.2 — Auditoria de Arquitetura e Ciclo de Vida

## 1. Visão Geral

A versão 0.15.2 consolida a estabilidade musical do sintetizador SMK-P4 (Guition ESP32-P4, AMY, 8 vozes, 48 kHz, 256 frames), introduzindo:
1. Recuperação e garantia de margem segura de stack para o Audio Owner Task.
2. Refinamento das transições do delay sincronizado com preservação auditada da integridade DSP.
3. Performance Lock: preservação seletiva de configurações musicais (FX, Harmony, Arpeggiator, Macros) durante a troca de timbres ROM.

Esta auditoria mapeia detalhadamente o ciclo de vida dos parâmetros, ownership de dados, semântica de dirty state e impacto na geração do engine.

---

## 2. Mapeamento do Ciclo de Vida e Estados

### 2.1 O que é reinicializado numa troca de patch ROM (0.15.1 vs 0.15.2)

Em 0.15.1:
- Cancelamento imediato de todas as notas ativas (`silence()`, `all_notes_off()`).
- Incremento da geração de comandos (`commands_.panic(OutputReason::Patch)`), invalidando eventos pendentes.
- Zeração das estruturas de vozes e grupos polifônicos (`voices_`, `groups_`).
- Carga da nova definição de instrumentos AMY (`patches_load_patch_checked`).
- Reinicialização de FX para defaults neutros (`state_.fx.config = fx::FxConfig{}`).
- Limpeza do flag dirty de FX e fontes de modulação de performance.
- Re-extração dos parâmetros da nova ROM (`captureEdit()`), resetando `original`, `manual` e `applied`.
- Reset de rotas e posições de macros para o estado neutro de fábrica.
- Delay restaurado para 350 ms / bypass neutro.

Em 0.15.2 (com Performance Lock):
- **Notas ativas, grupos e buffers de voz continuam sendo 100% reinicializados e silenciados (Regra Inegociável: nenhuma nota antiga soando).**
- Geração é incrementada, descartando eventos e tokens anteriores.
- Carga de instrumentos e captura de parâmetros do novo motor continuam estritas.
- Se **Keep FX** estiver ativo: `state_.fx.config` e `state_.fx.applied` são restaurados; delay mode (FREE/SYNC), division, time, feedback, mix, chorus, reverb e EQ são preservados. O estado é marcado como dirty (customizado em relação ao patch padrão).
- Se **Keep Harmony** estiver ativo: `state_.harmony` (`harmony::Config`) é preservado integralmente (Root, Scale Type, Chord Assist, Inversion, Voicing, Spread).
- Se **Keep ARP** estiver ativo: `arp::Config` (Rate, Direction, Octaves, Gate, Swing, Latch, Restart) é preservado, mas a fila de notas presas e o padrão anterior são purgados. O arpejador permanece armado aguardando novo gesto musical.
- Se **Keep Macros** estiver ativo: Posições M1..M8 e rotas compatíveis com a família do novo patch (Juno vs DX7) são validadas e preservadas. Rotas incompatíveis com a síntese de destino são descartadas com diagnóstico explícito.
- Se **nenhum lock** estiver ativo (clean load): Comportamento 100% idêntico à 0.15.1.

### 2.2 O que é reinicializado durante Recall V4

- O User Preset V4 contém configurações transacionais vinculadas por um `CommitMarker` CRC32 de 32 bytes (`synth_seq` + `fx_seq`).
- `RECALL V4` restaura a totalidade do preset salvo (timbre ROM base, edições de parâmetros, rotas de macro e FX Record V2).
- **Performance Lock não se aplica ao Recall V4.** O Recall permanece integral e atômico.

### 2.3 Parâmetros Globais (intocados na troca de patch)

- Global BPM (`tempo::Clock`, compartilhado entre ARP e Delay).
- Clock Source interno.
- MIDI Controller Profile (M-VAVE SMK25 V2 mappings, bancos A/B).
- Mapeamentos MIDI Learn persistidos em NVS.
- Preferências globais de interface e calibração touchscreen.
- Master Volume (`state_.master`).

### 2.4 Parâmetros Exclusivos do Timbre (Synth Engine)

- Formas de onda e topologia dos osciladores.
- Algoritmo FM (1..32) e roteamento de operadores OP1..OP6.
- Frequências, ratios e níveis de operadores DX7.
- Cutoff, Ressonância, Keytrack, Envelope Amount do filtro Juno.
- Envelopes ADSR / Breakpoints (taxas e níveis).
- Parâmetros analógicos Juno (SubLevel, NoiseLevel, PulseWidth, Detune).

### 2.5 Parâmetros da Configuração Musical Ativa (Performance Context)

- Configuração de FX (`fx::FxConfig`: Chorus, Delay, Reverb, EQ, Master Tone).
- Modo de Delay (FREE vs SYNC) e divisão rítmica.
- Harmony Assist (`harmony::Config`: Scale Assist, Chord Assist, Voicing, Inversion, Spread).
- Arpeggiator (`arp::Config`: Enabled, Direction, Rate, Octaves, Gate, Swing, Latch, Restart).
- Posições de Macros M1..M8 e rotas customizadas.

### 2.6 Parâmetros Persistidos por Preset (User Preset V4)

- Patch ROM ID.
- Vetor de edições manuais de síntese (`manual[58]`).
- Máscara de parâmetros editados (`mask`).
- Posições de macros e `RouteConfiguration` customizada.
- Registro FX Record V2 transacional (Chorus, Delay, Reverb, EQ, bypass).

### 2.7 Parâmetros Voláteis

- Estado de vozes ativas (`voices_[8]`, `active_notes`).
- Grupos polifônicos e tokens (`groups_[8]`).
- Notas sustentadas (`sustain_[16]`, `bend_[16]`, `modulation_[16]`).
- Fila do arpejador, padrão de notas e cursor de passo.
- Amostras do buffer circular da linha de retardo AMY.
- Filas de comandos e tokens de UI/USB em trânsito.

### 2.8 Estados Seguros para Atravessar a Troca de Patch

- Configuração validada de FX (`FxConfig`).
- Configuração validada de harmonia (`harmony::Config`).
- Configuração validada de arpejador (`arp::Config`).
- Posições de macro neutras ou mapeadas para destinos universais (ex: destinos FX).
- **Inseguros (proibidos de atravessar):** notas ativas, índices de buffer de áudio, tokens de geração obsoletos, held notes do teclado.

### 2.9 Dependências de Família (Juno vs DX7)

- **Juno (Família 1):** Filtro ressonante passa-baixas (Cutoff, Resonance, FilterEnv), oscilador DCO + Sub + Noise, ADSR padrão. Não possui operadores FM nem algoritmo.
- **DX7 (Família 0):** 6 operadores senoidais modulados em frequência, matriz de 32 algoritmos, feedback de operador, envelopes multi-estágio. Não possui filtro passa-baixas analógico nem PWM.
- **Destinos FX universais:** Chorus, Delay, Reverb, Master Tone são válidos em ambas as famílias.
- **Validação de Performance Lock:** Ao alternar Juno $\leftrightarrow$ DX7, rotas de macro direcionadas a parâmetros exclusivos da família anterior não podem ser aplicadas. Se incompatíveis, essas rotas são desarmadas ou o grupo de macros é rejeitado com aviso explícito.

### 2.10 Operações que Alteram a Geração do Owner

- `commands_.panic(OutputReason::Panic)`: disparo explícito de pânico.
- `commands_.panic(OutputReason::Patch)`: início e término do ciclo de patch load.
- Desconexão do USB MIDI Host (`cancelUsb()`).
- Invalidação de lote por estouro ou timeout de comando.
