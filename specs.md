# PROMPT PARA AGENTE — DESENVOLVIMENTO DO SINTETIZADOR SMK-S3

Você é um agente especialista em sistemas embarcados, ESP32-S3, ESP-IDF, DSP de áudio em tempo real, USB MIDI Host, síntese sonora, interfaces gráficas embarcadas e arquitetura de firmware em C/C++.

Sua tarefa é projetar e implementar um sintetizador digital autônomo baseado em uma placa ESP32-S3 DevKitC N16R8, controlado principalmente por um teclado M-VAVE SMK25 V2, com saída de áudio estéreo por DAC PCM5102A e interface gráfica em um display widescreen de 2,25 polegadas com resolução de 284 × 76 pixels.

O projeto deve funcionar sem computador durante o uso normal.

O nome provisório do projeto é:

SMK-S3 Synth

---

# 1. OBJETIVO DO PROJETO

Desenvolver um sintetizador compacto e autônomo que utilize todos os controles disponíveis no M-VAVE SMK25 V2:

- 25 teclas sensíveis à velocidade;
- controle de pitch por toque;
- controle de modulation por toque;
- 8 pads;
- 8 knobs;
- botão Oct+;
- botão Oct−;
- botão Play;
- botão Stop;
- botão Rec;
- botão BT;
- botão Arp;
- botão SC/CH;
- botão KNOB-B;
- botão PAD-B.

O sistema deve oferecer:

- síntese polifônica;
- presets;
- duas camadas de sintetizador;
- modo single, layer e split;
- bateria acionada pelos pads;
- arpejador;
- sequenciador;
- cenas de performance;
- efeitos;
- macros musicais;
- interface gráfica adequada ao display panorâmico;
- armazenamento persistente;
- operação sem computador;
- MIDI Learn;
- monitor MIDI;
- proteção contra travamentos e underruns de áudio.

O motor de síntese inicial deve ser baseado no AMY, integrado como componente do projeto.

O firmware deve ser construído preferencialmente sobre ESP-IDF.

Não desenvolver um novo motor DSP completo do zero na primeira versão. Usar AMY como engine principal e desenvolver ao redor dele a arquitetura de controle, interface, MIDI, sequenciamento, presets e gerenciamento de sistema.

---

# 2. HARDWARE PRINCIPAL

## 2.1 Microcontrolador

Placa:

- ESP32-S3 DevKitC;
- variante N16R8;
- 16 MB de Flash;
- 8 MB de PSRAM;
- processador dual-core;
- USB OTG nativo;
- interface USB-UART separada, quando presente na placa;
- periférico I²S;
- SPI para display;
- possibilidade futura de microSD.

O firmware deve detectar e reportar:

- tamanho da Flash;
- tamanho da PSRAM;
- RAM interna livre;
- PSRAM livre;
- frequência da CPU;
- versão do firmware;
- número de underruns;
- carga aproximada do DSP;
- estado da conexão USB MIDI.

---

## 2.2 Controlador MIDI

Controlador principal:

M-VAVE SMK25 V2

A conexão principal deve ocorrer por USB, com o ESP32-S3 operando como USB Host.

O controlador deve ser tratado como dispositivo USB MIDI class-compliant sempre que possível.

Não presumir antecipadamente os números exatos de CC, notas ou mensagens transmitidas por cada controle.

Implementar:

- MIDI Monitor;
- MIDI Learn;
- assistente de mapeamento;
- perfil persistente do controlador;
- possibilidade de redefinir o perfil;
- tratamento de diferentes revisões do SMK25 V2;
- tratamento de canais MIDI diferentes;
- suporte a controles configurados pelo aplicativo da M-VAVE.

---

## 2.3 Saída de áudio

Usar obrigatoriamente:

PCM5102A

A conexão deve ser feita por I²S.

Sinais mínimos:

- BCLK;
- LRCLK ou WS;
- DATA;
- GND;
- alimentação adequada ao módulo utilizado.

A saída deve ser:

- estéreo;
- em nível de linha;
- com taxa inicial de 48 kHz;
- formato compatível com o PCM5102A;
- preferencialmente 16 ou 32 bits por amostra no barramento I²S;
- clock contínuo e estável;
- sem ruídos causados por atualizações do display ou acesso ao armazenamento.

Não usar MAX98357A como saída principal.

Não depender de DAC interno do ESP32-S3.

Criar uma abstração de saída de áudio para permitir, futuramente, substituição do PCM5102A por outro codec ou DAC.

Interface proposta:

```cpp
class AudioOutput {
public:
    virtual bool begin() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool write(const int16_t* interleavedStereo, size_t frames) = 0;
    virtual uint32_t underrunCount() const = 0;
    virtual ~AudioOutput() = default;
};
```

Implementação inicial:

```cpp
class Pcm5102Output : public AudioOutput {
    // Implementação baseada em I²S e DMA.
};
```

---

## 2.4 Display

Display:

- 2,25 polegadas;
- formato widescreen;
- resolução de 284 × 76 pixels.

O controlador exato do display ainda não deve ser presumido.

Antes de finalizar o driver, obter ou confirmar:

- controlador gráfico;
- interface elétrica;
- SPI, QSPI ou paralela;
- tensão lógica;
- pinagem;
- comando de inicialização;
- offsets físicos;
- orientação;
- sequência de reset;
- controle de backlight;
- ordem de cores;
- formato RGB;
- frequência SPI suportada.

Criar uma camada abstrata que não dependa diretamente de um único controlador.

Exemplo:

```cpp
class DisplayDriver {
public:
    virtual bool begin() = 0;
    virtual void setBrightness(uint8_t value) = 0;
    virtual void fill(uint16_t color) = 0;
    virtual void drawBitmap(
        int x,
        int y,
        int width,
        int height,
        const uint16_t* pixels
    ) = 0;
    virtual void flush() = 0;
    virtual ~DisplayDriver() = default;
};
```

O sistema gráfico deve ser leve.

Preferir:

- driver próprio;
- LovyanGFX, caso compatível;
- framebuffer completo ou parcial;
- fontes bitmap;
- atualização de regiões alteradas;
- widgets próprios.

Evitar inicialmente uma interface LVGL excessivamente pesada, salvo se testes demonstrarem que não prejudica o áudio.

Um framebuffer RGB565 completo ocupa aproximadamente 43 KB e pode ser armazenado em PSRAM, desde que a estratégia de transferência não comprometa o tempo real.

---

# 3. CONEXÕES USB

A placa possui duas conexões USB com funções potencialmente diferentes.

Arquitetura esperada:

## USB nativo do ESP32-S3

Usar para:

- USB Host;
- conexão com o SMK25 V2;
- recepção de USB MIDI.

## USB-UART da DevKitC

Usar para:

- alimentação da placa;
- programação;
- logs;
- depuração;
- console serial.

Considerar que a única interface USB OTG nativa do ESP32-S3 não deve ser presumida como host e device simultaneamente.

Enquanto o USB nativo estiver operando como host para o SMK25, não depender dele simultaneamente como:

- USB MIDI Device;
- USB Audio Device;
- porta serial USB nativa.

O firmware deve funcionar mesmo quando a porta USB-UART estiver conectada a um computador apenas para alimentação e depuração.

---

# 4. ALIMENTAÇÃO USB DO SMK25

O sistema deve considerar que o SMK25 precisa receber 5 V pelo VBUS USB.

Não presumir que a DevKitC fornece automaticamente VBUS adequado em modo host.

A solução final deve prever:

- 5 V estáveis;
- chave de alimentação USB;
- limitação de corrente;
- proteção contra curto;
- proteção contra corrente reversa;
- desacoplamento;
- detecção de sobrecorrente, se possível.

Para prototipagem, permitir o uso de:

- hub USB alimentado externamente;
- cabo OTG apropriado;
- alimentação separada e segura.

Para hardware final, considerar componentes como:

- AP22802;
- TPS2051;
- SY6280;
- MIC2005;
- equivalente adequado.

O firmware deve detectar desconexão do dispositivo USB e se recuperar sem reinicialização completa.

---

# 5. MOTOR DE SÍNTESE

Usar AMY como engine de áudio inicial.

Criar um adaptador para evitar que o restante do firmware dependa diretamente da API interna do AMY.

Exemplo:

```cpp
class SynthEngine {
public:
    virtual bool begin(uint32_t sampleRate) = 0;

    virtual void noteOn(
        uint8_t channel,
        uint8_t note,
        uint8_t velocity
    ) = 0;

    virtual void noteOff(
        uint8_t channel,
        uint8_t note
    ) = 0;

    virtual void pitchBend(
        uint8_t channel,
        int16_t value
    ) = 0;

    virtual void controlChange(
        uint8_t channel,
        uint8_t controller,
        uint8_t value
    ) = 0;

    virtual void setParameter(
        uint16_t parameterId,
        float value
    ) = 0;

    virtual void render(
        float* left,
        float* right,
        size_t frames
    ) = 0;

    virtual ~SynthEngine() = default;
};
```

Implementação inicial:

```cpp
class AmySynthEngine : public SynthEngine {
    // Adaptador para o AMY.
};
```

O restante do sistema deve se comunicar com o motor por:

- eventos;
- parâmetros normalizados;
- IDs estáveis;
- estruturas intermediárias;
- filas thread-safe.

Não permitir que a interface gráfica ou o USB alterem diretamente estruturas internas usadas pelo DSP.

---

# 6. CONFIGURAÇÃO DE ÁUDIO

Configuração inicial recomendada:

- sample rate: 48 kHz;
- saída estéreo;
- PCM5102A via I²S;
- blocos DSP de 128 amostras inicialmente;
- testar posteriormente 64 amostras;
- múltiplos buffers DMA;
- buffers críticos em RAM interna compatível com DMA;
- PSRAM para dados não críticos.

O sistema deve medir:

- duração de cada renderização;
- tempo máximo de renderização;
- tempo médio;
- uso aproximado da CPU;
- número de underruns;
- quantidade de vozes;
- memória livre;
- maior bloco livre de memória interna;
- maior bloco livre de PSRAM.

A interface, USB, armazenamento e logs não podem bloquear o processamento de áudio.

Nenhuma operação lenta deve ocorrer diretamente na callback ou tarefa crítica de áudio.

Evitar nessa tarefa:

- acesso a arquivos;
- alocação dinâmica frequente;
- logs;
- desenho de display;
- parsing de JSON;
- chamadas bloqueantes;
- locks prolongados.

---

# 7. POLIFONIA

Definir inicialmente metas conservadoras:

- 8 vozes para patches complexos;
- 12 a 16 vozes para patches simples;
- 6 a 8 vozes por layer em modo dual;
- vozes reservadas para bateria;
- voice stealing configurável;
- prioridade para notas recentes;
- possibilidade de prioridade para graves ou agudos;
- proteção contra sobrecarga.

O sistema deve degradar de forma controlada.

Se a carga do DSP atingir nível crítico:

1. impedir novas vozes acima do limite;
2. reduzir vozes de release mais antigas;
3. opcionalmente reduzir efeitos mais caros;
4. nunca interromper o áudio abruptamente;
5. registrar a condição para diagnóstico.

---

# 8. ARQUITETURA DO FIRMWARE

Separar o projeto em módulos.

Estrutura recomendada:

```text
smk-s3/
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
│
├── main/
│   ├── app_main.cpp
│   ├── app_config.h
│   ├── app_state.cpp
│   └── app_state.h
│
├── components/
│   ├── amy/
│   ├── audio/
│   │   ├── audio_engine.cpp
│   │   ├── audio_engine.h
│   │   ├── amy_adapter.cpp
│   │   ├── amy_adapter.h
│   │   ├── pcm5102_output.cpp
│   │   └── pcm5102_output.h
│   │
│   ├── midi/
│   │   ├── midi_event.h
│   │   ├── midi_router.cpp
│   │   ├── midi_router.h
│   │   ├── usb_midi_host.cpp
│   │   ├── usb_midi_host.h
│   │   ├── midi_parser.cpp
│   │   ├── midi_parser.h
│   │   ├── midi_learn.cpp
│   │   ├── midi_learn.h
│   │   ├── controller_profile.cpp
│   │   └── controller_profile.h
│   │
│   ├── synth/
│   │   ├── patch.cpp
│   │   ├── patch.h
│   │   ├── patch_manager.cpp
│   │   ├── patch_manager.h
│   │   ├── macro_engine.cpp
│   │   ├── macro_engine.h
│   │   ├── modulation_matrix.cpp
│   │   ├── modulation_matrix.h
│   │   ├── voice_manager.cpp
│   │   └── voice_manager.h
│   │
│   ├── sequencer/
│   │   ├── transport.cpp
│   │   ├── transport.h
│   │   ├── clock.cpp
│   │   ├── clock.h
│   │   ├── arpeggiator.cpp
│   │   ├── arpeggiator.h
│   │   ├── pattern.cpp
│   │   ├── pattern.h
│   │   ├── sequencer.cpp
│   │   └── sequencer.h
│   │
│   ├── ui/
│   │   ├── display_driver.h
│   │   ├── display_driver.cpp
│   │   ├── ui_manager.cpp
│   │   ├── ui_manager.h
│   │   ├── widgets.cpp
│   │   ├── widgets.h
│   │   ├── fonts/
│   │   └── screens/
│   │       ├── home_screen.cpp
│   │       ├── parameter_screen.cpp
│   │       ├── sequencer_screen.cpp
│   │       ├── midi_monitor_screen.cpp
│   │       ├── system_screen.cpp
│   │       └── mapping_screen.cpp
│   │
│   ├── storage/
│   │   ├── preset_store.cpp
│   │   ├── preset_store.h
│   │   ├── settings_store.cpp
│   │   ├── settings_store.h
│   │   ├── file_format.cpp
│   │   └── file_format.h
│   │
│   └── system/
│       ├── diagnostics.cpp
│       ├── diagnostics.h
│       ├── watchdog.cpp
│       ├── watchdog.h
│       ├── event_bus.cpp
│       └── event_bus.h
│
├── presets/
├── controller_profiles/
├── tools/
├── docs/
└── tests/
```

---

# 9. DIVISÃO DE TAREFAS ENTRE NÚCLEOS

Proposta inicial:

## Core dedicado prioritariamente ao áudio

Responsabilidades:

- renderização do AMY;
- gerenciamento de vozes;
- atualização de parâmetros DSP;
- preenchimento dos buffers;
- I²S;
- DMA;
- conversão de formato;
- contagem de underruns;
- proteção de tempo real.

## Outro core

Responsabilidades:

- USB Host;
- MIDI;
- sequenciador;
- arpejador;
- interface;
- display;
- armazenamento;
- presets;
- diagnóstico;
- comandos de console.

A divisão real deve ser validada por testes.

Usar prioridades adequadas de FreeRTOS.

A tarefa de áudio deve ter prioridade superior às tarefas de:

- display;
- armazenamento;
- interface;
- logs;
- MIDI Learn;
- serial.

A tarefa USB deve ter prioridade suficiente para evitar perda de eventos.

---

# 10. BARRAMENTO INTERNO DE EVENTOS

Não usar mensagens MIDI cruas em toda a aplicação.

Converter entradas MIDI em eventos internos normalizados.

Exemplo:

```cpp
enum class EventType : uint8_t {
    NoteOn,
    NoteOff,
    PolyAftertouch,
    ChannelAftertouch,
    PitchBend,
    Modulation,
    ControlChange,
    ProgramChange,
    PadHit,
    TransportPlay,
    TransportStop,
    TransportRecord,
    Clock,
    Start,
    Continue,
    SongPosition,
    ParameterChange,
    PatchChange,
    SceneChange,
    AllNotesOff,
    Panic
};

struct SynthEvent {
    EventType type;
    uint8_t source;
    uint8_t channel;
    uint16_t id;
    int32_t value;
    uint32_t timestampUs;
};
```

Criar fontes de evento para:

- USB MIDI;
- sequenciador;
- arpejador;
- interface;
- automação;
- console de testes;
- futuras entradas BLE MIDI;
- futuras entradas DIN MIDI.

O motor deve consumir eventos com timestamp.

---

# 11. MODOS DO SINTETIZADOR

Implementar:

## Single

Somente Layer A.

## Layer

Layer A e Layer B tocadas simultaneamente.

## Split

Layer A e Layer B divididas por uma nota configurável.

## Chord

Uma nota pode disparar um acorde armazenado.

## Arpeggio

Notas pressionadas alimentam o arpejador.

## Sequencer Transpose

As teclas transpõem um padrão em reprodução.

Cada layer deve possuir:

- patch AMY;
- volume;
- pan;
- transpose;
- key range;
- velocity range;
- MIDI channel;
- número máximo de vozes;
- prioridade;
- envio para efeitos;
- resposta ao pitch bend;
- resposta à modulation;
- resposta ao sustain;
- mute;
- solo.

---

# 12. MODELO DE PATCH

Criar um formato de patch versionado.

Um patch deve conter:

- nome;
- categoria;
- autor;
- versão;
- Layer A;
- Layer B;
- modo do teclado;
- split point;
- kit de bateria;
- oito macros;
- matriz de modulação;
- parâmetros do arpejador;
- parâmetros do sequenciador;
- efeitos;
- faixa de pitch bend;
- comportamento da modulation;
- configuração dos pads;
- configuração dos knobs;
- parâmetros do display;
- checksum ou CRC.

Estrutura conceitual:

```cpp
struct LayerConfig {
    uint16_t enginePatch;
    int8_t transpose;
    uint8_t volume;
    int8_t pan;
    uint8_t lowKey;
    uint8_t highKey;
    uint8_t lowVelocity;
    uint8_t highVelocity;
    uint8_t maxVoices;
    uint8_t midiChannel;
    uint16_t flags;
};

struct PatchHeader {
    uint32_t magic;
    uint16_t formatVersion;
    uint16_t dataSize;
    uint32_t crc32;
};

struct SynthPatch {
    PatchHeader header;
    char name[24];
    char category[16];

    LayerConfig layerA;
    LayerConfig layerB;

    uint8_t keyboardMode;
    uint8_t splitPoint;
    uint8_t pitchBendRange;
    uint8_t reserved;

    // Macros, matriz, FX, arpejador,
    // sequenciador, pads e outros parâmetros.
};
```

Não usar ponteiros dentro do arquivo persistente.

Não salvar structs diretamente sem controlar:

- alinhamento;
- endianness;
- tamanho;
- versão;
- compatibilidade.

Criar serialização explícita.

---

# 13. MACROS DOS OITO KNOBS

Os oito knobs devem controlar macros musicais.

Banco principal sugerido:

1. Character
2. Brightness
3. Motion
4. Shape
5. Attack
6. Release
7. Space
8. Drive/FX

Cada macro deve poder controlar múltiplos destinos.

Exemplo:

```text
Macro Brightness
├── cutoff do filtro do Layer A
├── cutoff do filtro do Layer B
├── envelope amount
├── índice de FM
├── posição de wavetable
└── compensação de ganho
```

Cada rota de macro deve possuir:

- parâmetro de destino;
- valor mínimo;
- valor máximo;
- profundidade;
- curva;
- polaridade;
- layer alvo;
- habilitação;
- suavização.

Curvas possíveis:

- linear;
- exponencial;
- logarítmica;
- S;
- invertida;
- bipolar.

Estrutura sugerida:

```cpp
enum class MacroCurve : uint8_t {
    Linear,
    Exponential,
    Logarithmic,
    SCurve,
    Inverted
};

struct MacroRoute {
    uint16_t destination;
    float minimum;
    float maximum;
    float amount;
    MacroCurve curve;
    uint8_t targetLayer;
    uint8_t flags;
};

struct MacroDefinition {
    char name[12];
    MacroRoute routes[6];
    uint8_t routeCount;
};
```

---

# 14. BANCOS DOS KNOBS

O botão KNOB-B deve alternar bancos.

Bancos iniciais:

## Banco A — Macros

- Character;
- Brightness;
- Motion;
- Shape;
- Attack;
- Release;
- Space;
- Drive.

## Banco B — Osciladores

- oscillator mix;
- waveform;
- detune;
- octave;
- sub;
- noise;
- FM amount;
- oscillator modulation.

## Banco C — Filtro e envelopes

- cutoff;
- resonance;
- filter envelope;
- attack;
- decay;
- sustain;
- release;
- key tracking.

## Banco D — Efeitos

- chorus;
- delay time;
- delay feedback;
- delay mix;
- reverb size;
- reverb mix;
- drive;
- master tone.

## Banco E — Sequenciador

- tempo;
- swing;
- gate;
- probability;
- ratchet;
- pattern length;
- transpose;
- pattern selection.

KNOB-B:

- toque curto: próximo banco;
- pressão longa: menu de bancos;
- KNOB-B pressionado enquanto gira: ajuste fino;
- duplo toque: retornar ao valor salvo ou padrão;
- combinação com SC/CH: banco anterior, se necessário.

---

# 15. SOFT TAKEOVER

Implementar soft takeover para knobs absolutos.

Ao trocar:

- patch;
- banco;
- cena;
- página;
- layer;

o valor interno pode não corresponder à posição física do knob.

O parâmetro não deve saltar imediatamente.

O knob deve “capturar” o valor quando cruzar o valor atual.

Estados:

- desacoplado;
- aproximando por baixo;
- aproximando por cima;
- capturado.

A interface deve mostrar:

- valor físico;
- valor armazenado;
- direção necessária;
- indicação de captura.

Também implementar opção configurável de comportamento:

- pickup;
- jump;
- relative emulation;
- scaled catch-up.

Pickup deve ser o padrão.

---

# 16. MAPEAMENTO DO CONTROLADOR

Criar um arquivo de perfil persistente.

Exemplo conceitual:

```cpp
struct MidiBinding {
    uint8_t messageType;
    uint8_t channel;
    uint16_t number;
    uint16_t targetAction;
    int16_t minimum;
    int16_t maximum;
    uint8_t flags;
};

struct ControllerProfile {
    char name[24];
    MidiBinding keys;
    MidiBinding pitch;
    MidiBinding modulation;
    MidiBinding knobs[8];
    MidiBinding pads[8];
    MidiBinding buttons[10];
};
```

O assistente deve solicitar ao usuário:

1. pressione uma tecla;
2. mova o pitch;
3. mova modulation;
4. gire knob 1;
5. gire knob 2;
6. continue até knob 8;
7. pressione pad 1;
8. continue até pad 8;
9. pressione Oct+;
10. pressione Oct−;
11. pressione Play;
12. pressione Stop;
13. pressione Rec;
14. pressione BT;
15. pressione Arp;
16. pressione SC/CH;
17. pressione KNOB-B;
18. pressione PAD-B.

O sistema deve identificar:

- tipo de mensagem;
- canal;
- número;
- faixa;
- comportamento momentâneo ou alternado;
- possível uso de Note, CC ou Program Change;
- mensagens duplicadas;
- controles que enviam múltiplas mensagens.

Permitir edição manual posterior.

---

# 17. MAPEAMENTO FUNCIONAL PADRÃO

## Teclas

- tocar notas;
- velocity aplicada à amplitude;
- velocity opcionalmente aplicada ao filtro;
- aftertouch, se disponível, roteável;
- suporte a sustain por CC.

## Pitch

- pitch bend;
- faixa configurável:
  - ±2;
  - ±3;
  - ±5;
  - ±7;
  - ±12;
  - ±24 semitons.

Cada patch pode definir sua própria faixa.

Modos especiais opcionais:

- tape stop;
- oscillator drift;
- effect bend;
- crossfade entre layers.

## Modulation

Por padrão:

- vibrato;
- abertura de filtro;
- aumento de movimento.

Destinos opcionais:

- FM amount;
- wavetable position;
- chorus;
- delay feedback;
- reverb;
- crossfade A/B;
- LFO depth;
- macro morph.

## Oct+ e Oct−

Toque curto:

- mudança de oitava.

Pressionar ambos:

- retornar à oitava zero.

Pressão longa:

- transpose em semitons.

Faixa sugerida:

- −4 a +4 oitavas.

## Play

- iniciar;
- pausar;
- continuar;
- iniciar arpejador ou sequenciador.

## Stop

- parar;
- enviar All Notes Off;
- cancelar latch;
- pressionar duas vezes: voltar ao início do padrão;
- pressão longa: Panic.

## Rec

- ativar gravação;
- overdub;
- step record;
- pressão longa: alternar modo de gravação;
- Rec + Stop: limpar padrão com confirmação;
- Rec + pad: gravar ou selecionar pista.

## Arp

- ativar ou desativar arpejador;
- pressão longa: abrir página do arpejador;
- Arp + knob: editar parâmetros rápidos.

## BT

Na primeira versão, usar como botão de conexão e sistema.

Possíveis funções:

- abrir menu de conexão;
- mostrar estado USB;
- futura ativação BLE MIDI;
- futura seleção USB, BLE ou DIN;
- pressão longa: iniciar pareamento BLE no futuro.

Não implementar BLE MIDI antes de estabilizar USB, áudio e interface.

## SC/CH

Usar para:

- seleção de cena;
- seleção de canal;
- acesso a funções secundárias.

Sugestão:

- toque curto: próxima cena;
- pressão longa: menu de cenas;
- SC/CH + pad 1–8: selecionar cena 1–8;
- SC/CH + knob: editar mixer ou canais.

## KNOB-B

- trocar banco de knobs;
- ajuste fino;
- retorno ao valor salvo;
- funções secundárias.

## PAD-B

- trocar banco dos pads;
- pressionado com pad: função secundária;
- mostrar banco atual no display.

---

# 18. BANCOS DOS PADS

Implementar pelo menos quatro bancos.

## Banco A — Drums

1. Kick
2. Snare
3. Closed Hat
4. Open Hat
5. Clap
6. Tom ou Percussion
7. FX
8. Accent ou Alternate

## Banco B — Chord Memory

Cada pad dispara um acorde armazenado.

Permitir:

- aprender acorde;
- definir inversão;
- transposição pelas teclas;
- latch;
- velocity scaling.

## Banco C — Patterns

Pads selecionam ou lançam patterns 1–8.

Com quantização configurável:

- imediata;
- próximo step;
- próximo beat;
- próximo compasso;
- final do pattern.

## Banco D — Performance FX

Sugestões:

1. filter momentary;
2. stutter;
3. tape stop;
4. delay throw;
5. reverb freeze;
6. beat repeat;
7. mute drums;
8. transition effect.

Os efeitos devem ser seguros quanto à carga de CPU.

---

# 19. ARPEJADOR

Implementar:

- Up;
- Down;
- Up/Down;
- Down/Up;
- Played Order;
- Random;
- Chord;
- Converge;
- Diverge;
- Euclidean opcional.

Parâmetros:

- rate;
- gate;
- octave range;
- swing;
- latch;
- hold;
- transpose;
- probability;
- ratchet;
- velocity mode;
- note length;
- sync source;
- restart behavior;
- pattern de acentos.

Divisões:

- 1/1;
- 1/2;
- 1/4;
- 1/8;
- 1/16;
- 1/32;
- tercinas;
- pontuadas, se viável.

Mapeamento rápido:

- Arp + knob 1: mode;
- Arp + knob 2: rate;
- Arp + knob 3: gate;
- Arp + knob 4: octave range;
- Arp + knob 5: swing;
- Arp + knob 6: probability;
- Arp + knob 7: ratchet;
- Arp + knob 8: transpose.

---

# 20. CLOCK E TRANSPORTE

Criar um clock interno estável.

Suportar:

- BPM;
- tap tempo;
- MIDI Clock externo;
- Start;
- Stop;
- Continue;
- Song Position Pointer, se viável;
- clock interno como master;
- clock externo como slave;
- detecção de perda de clock;
- retorno seguro ao clock interno, configurável.

Faixa inicial de BPM:

- 30 a 300 BPM.

Resolução interna deve ser superior à resolução MIDI Clock, preferencialmente baseada em timestamp de microssegundos.

Evitar usar apenas delays de tarefa como base temporal.

---

# 21. SEQUENCIADOR

Primeira versão:

- 8 patterns;
- 16 ou 32 steps;
- pelo menos 4 pistas de bateria;
- 1 pista melódica;
- 1 pista de automação;
- gravação em tempo real;
- step recording;
- overdub;
- quantização;
- swing;
- ties;
- rests;
- ratchets;
- probability;
- microtiming;
- velocity por step;
- comprimento independente quando possível;
- mute;
- solo;
- pattern chaining em versão posterior.

Cada step pode conter:

```cpp
struct StepEvent {
    uint8_t note;
    uint8_t velocity;
    uint8_t gate;
    uint8_t probability;
    int8_t microTiming;
    uint8_t ratchetCount;
    uint8_t flags;
};
```

Automação:

```cpp
struct ParameterLock {
    uint16_t parameterId;
    int16_t value;
    uint8_t step;
    uint8_t flags;
};
```

O sequenciador deve gerar eventos no mesmo barramento interno usado pelo MIDI.

---

# 22. CENAS

Implementar oito cenas.

Cada cena armazena:

- patch do Layer A;
- patch do Layer B;
- modo single, layer ou split;
- drum kit;
- pattern selecionado;
- tempo;
- estado do arpejador;
- transpose;
- valores dos macros;
- estados dos efeitos;
- volumes;
- mutes;
- banco de pads;
- banco de knobs.

Uso sugerido:

- Scene 1: intro;
- Scene 2: verso;
- Scene 3: pré-refrão;
- Scene 4: refrão;
- Scene 5: bridge;
- Scene 6: solo;
- Scene 7: breakdown;
- Scene 8: final.

Mudanças de cena devem poder ocorrer:

- imediatamente;
- no próximo beat;
- no próximo compasso;
- ao final do pattern.

Evitar cortes abruptos de release e efeitos.

Permitir tails de reverb e delay quando possível.

---

# 23. EFEITOS

Utilizar inicialmente os efeitos disponíveis no AMY, quando adequados.

Priorizar:

- chorus;
- delay;
- reverb;
- equalização;
- drive.

Criar parâmetros normalizados independentes da API do AMY.

Exemplo:

```cpp
enum class EffectParameter : uint16_t {
    ChorusDepth,
    ChorusRate,
    ChorusMix,
    DelayTime,
    DelayFeedback,
    DelayMix,
    ReverbSize,
    ReverbDamping,
    ReverbMix,
    DriveAmount,
    MasterLow,
    MasterMid,
    MasterHigh
};
```

Efeitos devem possuir:

- bypass;
- suavização;
- limites seguros;
- proteção contra feedback excessivo;
- compensação de ganho quando necessário.

---

# 24. INTERFACE GRÁFICA

O display possui apenas 76 pixels de altura.

A interface deve ser desenhada especificamente para formato panorâmico.

Evitar:

- menus profundos;
- textos longos;
- fontes pequenas demais;
- excesso de ícones;
- animações pesadas;
- redesenho completo desnecessário.

## Tela principal

Deve mostrar:

- número do patch;
- nome;
- modo do teclado;
- layer ativo;
- estado do arpejador;
- estado do sequenciador;
- BPM;
- banco de knobs;
- oito macros;
- valores resumidos;
- estado USB;
- indicador de atividade MIDI;
- indicador de clipping ou carga, quando necessário.

Layout conceitual:

```text
┌──────────────────────────────────────────────────────────┐
│ 042 GLASS HORIZON   LYR   ARP 1/16   120 BPM   USB ●    │
│ CHAR BRITE MOTION SHAPE  ATK  REL  SPACE DRIVE          │
│  62    78     31    45    08   67    34    22           │
│ ▂▄▆█  ▃▅▇█   ▂▃▅▆   ◢◣    ╱╲   ╲__   ≋≋≋   ▓▓░         │
└──────────────────────────────────────────────────────────┘
```

## Tela contextual de parâmetro

Ao girar um knob:

- mostrar nome;
- valor;
- unidade;
- barra;
- layer afetado;
- valor salvo;
- indicação de soft takeover;
- retornar automaticamente após intervalo configurável.

## Tela do sequenciador

Mostrar:

- pattern;
- BPM;
- divisão;
- swing;
- cursor;
- steps;
- pista selecionada;
- estados de mute e rec.

## Tela MIDI Monitor

Mostrar:

- tipo da mensagem;
- canal;
- número;
- valor;
- dispositivo;
- timestamp resumido;
- nome da ação mapeada.

## Tela de sistema

Mostrar:

- CPU;
- DSP load;
- vozes;
- RAM;
- PSRAM;
- underruns;
- sample rate;
- USB state;
- versão;
- temperatura, caso disponível e confiável.

---

# 25. SISTEMA DE NAVEGAÇÃO

Como não há encoder dedicado, a navegação deve usar combinações claras.

Sugestão:

- KNOB-B: troca banco de knobs;
- PAD-B: troca banco dos pads;
- SC/CH: troca cena ou página;
- SC/CH + knob: navegação ou edição de menu;
- SC/CH + pad: seleção direta;
- Arp pressionado: página do arpejador;
- Rec pressionado: página do sequenciador;
- BT pressionado: página de conexões;
- Stop longo: Panic;
- Play longo: tap tempo ou menu de transporte;
- Oct+ e Oct− simultâneos: home ou reset de oitava.

Toda combinação deve ser documentada e apresentada na interface quando possível.

Não criar combinações impossíveis de descobrir.

---

# 26. ARMAZENAMENTO

Usar inicialmente:

- NVS para configurações pequenas;
- partição de arquivos ou LittleFS para patches e perfis;
- microSD como recurso futuro.

Armazenar:

- configurações;
- último patch;
- última cena;
- perfis MIDI;
- patches do usuário;
- patterns;
- kits;
- calibração;
- brilho do display;
- preferências de soft takeover;
- contadores de diagnóstico.

Não gravar Flash continuamente.

Usar:

- debounce de salvamento;
- dirty flags;
- salvamento após período de inatividade;
- escrita atômica;
- arquivo temporário;
- rename;
- CRC;
- versão.

Evitar perda de dados se a alimentação for removida durante a gravação.

---

# 27. FORMATO DE ARQUIVOS

Criar formatos versionados.

Sugestões de extensões:

- `.s3p` para patches;
- `.s3s` para scenes;
- `.s3q` para sequências;
- `.s3k` para kits;
- `.s3m` para controller mappings;
- `.s3c` para configurações.

Cada arquivo deve conter:

- magic;
- versão;
- tamanho;
- tipo;
- CRC;
- payload.

Permitir futuramente uma ferramenta de computador para:

- ler;
- editar;
- converter;
- validar;
- organizar;
- transferir presets.

---

# 28. CONSOLE DE DIAGNÓSTICO

Implementar console serial pela USB-UART.

Comandos mínimos:

```text
help
status
audio status
audio reset
midi monitor on
midi monitor off
midi devices
midi mapping show
midi mapping reset
patch list
patch load <id>
patch save <id>
scene list
scene load <id>
panic
memory
tasks
cpu
display test
storage test
usb reset
reboot
factory-reset
```

O console não deve imprimir continuamente durante a reprodução normal.

Logs devem possuir níveis:

- Error;
- Warning;
- Info;
- Debug;
- Trace.

Build de release deve reduzir logs.

---

# 29. SEGURANÇA DE TEMPO REAL

Regras obrigatórias:

- não alocar memória no caminho crítico de áudio;
- não usar mutex demorado na tarefa de áudio;
- não acessar arquivos durante renderização;
- não fazer logs na callback I²S;
- não atualizar display na tarefa de áudio;
- não chamar parsing pesado no áudio;
- não copiar buffers grandes desnecessariamente;
- evitar fragmentação;
- pré-alocar vozes;
- pré-alocar eventos;
- usar ring buffers;
- usar filas de tamanho conhecido;
- definir comportamento para overflow;
- implementar watchdog;
- implementar Panic.

Quando uma fila de eventos encher:

- preservar Note Off sempre que possível;
- preservar Stop e Panic;
- descartar primeiro eventos de baixa prioridade;
- registrar contador de overflow;
- não travar.

---

# 30. TRATAMENTO DE NOTE OFF E PANIC

Implementar tratamento robusto contra notas presas.

Executar All Notes Off quando:

- USB for desconectado;
- dispositivo MIDI desaparecer;
- Stop longo for acionado;
- ocorrer reset do motor;
- troca crítica de configuração;
- watchdog detectar falha;
- perfil MIDI for trocado;
- usuário acionar Panic.

Panic deve:

- desligar todas as vozes;
- zerar sustain;
- limpar latch;
- limpar notas do arpejador;
- parar notas pendentes do sequenciador;
- preservar, opcionalmente, tails de efeitos por curto período;
- atualizar a interface.

---

# 31. LATÊNCIA

Meta inicial de latência percebida:

- baixa o suficiente para performance musical;
- idealmente abaixo de aproximadamente 10 ms do evento MIDI até a saída;
- buscar valor menor após estabilização.

Medir separadamente:

- recepção USB;
- parsing MIDI;
- fila interna;
- renderização;
- buffer DSP;
- DMA I²S;
- DAC.

Criar ferramenta de teste ou instrumentação para timestamps.

Não declarar latência sem medição.

---

# 32. INICIALIZAÇÃO DO SISTEMA

Sequência recomendada:

1. inicializar logs;
2. validar Flash e PSRAM;
3. montar armazenamento;
4. carregar configurações;
5. inicializar barramento de eventos;
6. inicializar PCM5102A e I²S;
7. inicializar motor AMY;
8. iniciar tarefa de áudio;
9. inicializar display;
10. mostrar splash minimalista;
11. inicializar USB Host;
12. procurar SMK25;
13. carregar perfil MIDI;
14. carregar último patch;
15. liberar áudio com fade-in;
16. entrar na tela principal.

Em caso de falha do display:

- áudio e MIDI devem continuar funcionando;
- reportar erro no console.

Em caso de falha do armazenamento:

- carregar patch padrão embutido;
- permitir funcionamento temporário.

Em caso de falha USB:

- continuar inicializado;
- mostrar aguardando controlador;
- tentar reconexão.

---

# 33. COMPORTAMENTO DE BOOT

Evitar estalos no PCM5102A.

Implementar:

- buffers zerados antes de iniciar I²S;
- fade-in;
- mute lógico inicial;
- ativação controlada;
- volume master iniciado em nível seguro;
- tratamento de reset;
- fade-out quando possível.

A tela inicial deve indicar:

- inicializando áudio;
- carregando engine;
- iniciando USB;
- aguardando controlador;
- pronto.

---

# 34. PRIMEIRO MARCO FUNCIONAL

O primeiro marco deve implementar somente:

```text
SMK25 V2
    ↓ USB MIDI Host
ESP32-S3
    ↓ AMY
I²S
    ↓
PCM5102A
    ↓
Saída estéreo
```

Requisitos do marco 1:

- reconhecer o SMK25;
- receber Note On;
- receber Note Off;
- receber velocity;
- receber pitch bend;
- receber modulation;
- tocar pelo menos 8 vozes;
- saída estéreo estável;
- sem notas presas em desconexão;
- console de diagnóstico;
- contador de underruns;
- patch fixo;
- funcionamento contínuo mínimo de teste.

Não implementar sequenciador antes de esse marco estar estável.

---

# 35. FASES DE IMPLEMENTAÇÃO

## Fase 0 — Validação de hardware

- confirmar modelo exato da DevKitC;
- confirmar pinos disponíveis;
- confirmar circuito das duas USBs;
- confirmar alimentação VBUS;
- confirmar controlador do display;
- confirmar pinagem do PCM5102A;
- testar alimentação;
- testar ruído;
- documentar todas as conexões.

## Fase 1 — Áudio básico

- configurar I²S;
- gerar seno de teste;
- validar canais esquerdo e direito;
- medir sample rate;
- validar ausência de glitches;
- implementar buffers DMA;
- implementar fade-in e fade-out.

## Fase 2 — AMY

- integrar AMY como componente;
- renderizar patch fixo;
- validar polifonia;
- medir CPU;
- medir memória;
- testar efeitos;
- criar AmySynthEngine.

## Fase 3 — USB MIDI Host

- enumerar SMK25;
- listar descriptors;
- receber pacotes MIDI;
- parser robusto;
- Note On/Off;
- velocity;
- pitch;
- modulation;
- desconexão e reconexão;
- Panic automático.

## Fase 4 — Display e diagnóstico

- driver do display;
- framebuffer;
- fontes;
- tela de sistema;
- MIDI Monitor;
- medidores;
- atualização sem interferir no áudio.

## Fase 5 — Perfis e MIDI Learn

- assistente de mapeamento;
- salvar perfil;
- editar perfil;
- validar todos os controles;
- detectar mensagens duplicadas;
- implementar ações.

## Fase 6 — Patches e macros

- Patch Manager;
- oito macros;
- bancos de knobs;
- soft takeover;
- salvamento;
- navegação;
- seleção rápida.

## Fase 7 — Layers

- Layer A;
- Layer B;
- Single;
- Layer;
- Split;
- mixer;
- voice allocation;
- limites de CPU.

## Fase 8 — Pads e bateria

- drum kit;
- quatro bancos;
- chord memory;
- pattern select;
- performance FX.

## Fase 9 — Arpejador

- clock;
- modos;
- rate;
- gate;
- swing;
- latch;
- probability;
- ratchet.

## Fase 10 — Sequenciador

- patterns;
- pistas;
- step recording;
- realtime recording;
- automação;
- parameter locks;
- cenas.

## Fase 11 — Estabilização

- soak tests;
- reconnect tests;
- testes de memória;
- testes de energia;
- testes de Flash;
- testes de underrun;
- documentação;
- builds de release.

---

# 36. CRITÉRIOS DE ACEITAÇÃO DO MVP

O MVP será considerado funcional quando:

1. o ESP32-S3 iniciar sem computador;
2. o PCM5102A produzir áudio estéreo limpo;
3. o SMK25 for reconhecido por USB;
4. teclas dispararem notas corretamente;
5. velocity funcionar;
6. pitch bend funcionar;
7. modulation funcionar;
8. os oito knobs forem mapeados;
9. os oito pads forem mapeados;
10. todos os botões forem reconhecidos;
11. o display mostrar patch, controles e status;
12. houver pelo menos 16 patches utilizáveis;
13. houver oito macros por patch;
14. soft takeover funcionar;
15. houver bateria nos pads;
16. houver arpejador;
17. houver pelo menos um sequenciador básico de 16 steps;
18. presets e configurações forem persistentes;
19. desconectar o SMK25 não gerar notas presas;
20. o sistema operar continuamente sem crashes;
21. o contador de underruns permanecer em zero em uso normal;
22. o sistema se recuperar da reconexão USB;
23. o projeto puder ser compilado a partir de documentação limpa.

---

# 37. TESTES OBRIGATÓRIOS

## Teste de áudio

- seno de 1 kHz;
- canais L/R;
- amplitude;
- ruído;
- clipping;
- sample rate;
- funcionamento por período prolongado;
- mudanças de patch;
- carga máxima.

## Teste MIDI

- todas as teclas;
- Note On com velocity zero;
- Note Off explícito;
- pitch máximo e mínimo;
- modulation;
- oito knobs;
- oito pads;
- todos os botões;
- conexão;
- desconexão;
- reconexão;
- mensagens rápidas;
- acordes;
- repetição;
- sustain.

## Teste de interface

- atualizações rápidas;
- troca de páginas;
- parâmetros;
- soft takeover;
- sem glitches de áudio;
- brilho;
- tela de erro.

## Teste de armazenamento

- salvar;
- carregar;
- CRC inválido;
- arquivo truncado;
- versão incompatível;
- falta de espaço;
- interrupção durante salvamento;
- factory reset.

## Teste de carga

- máxima polifonia;
- efeitos ligados;
- display atualizando;
- sequenciador rodando;
- USB ativo;
- logs mínimos;
- gravação de automação;
- troca de cena.

## Teste prolongado

Executar por muitas horas com:

- sequência contínua;
- mudanças de patch;
- entrada MIDI;
- efeitos;
- atualização de display;
- reconexões USB ocasionais.

Registrar:

- crashes;
- resets;
- watchdog;
- underruns;
- memória mínima;
- fragmentação;
- falhas USB.

---

# 38. DOCUMENTAÇÃO OBRIGATÓRIA

Entregar:

- README;
- diagrama de arquitetura;
- diagrama de tarefas;
- diagrama de fluxo de eventos;
- pinagem completa;
- esquema de conexão do PCM5102A;
- esquema da alimentação USB Host;
- configuração do display;
- instruções de build;
- instruções de flash;
- instruções de uso;
- tabela dos controles;
- formato dos arquivos;
- API dos módulos;
- guia para criar patches;
- guia para criar controller profiles;
- guia de diagnóstico;
- lista de limitações;
- roadmap.

O README deve permitir que outra pessoa:

1. clone o projeto;
2. instale as dependências;
3. configure o ESP-IDF;
4. compile;
5. grave a placa;
6. conecte o PCM5102A;
7. conecte o display;
8. conecte o SMK25;
9. obtenha áudio.

---

# 39. QUALIDADE DO CÓDIGO

O código deve:

- ser C++ moderno compatível com o toolchain do ESP-IDF;
- evitar exceções, caso estejam desabilitadas;
- evitar RTTI, se desnecessário;
- ter responsabilidades bem separadas;
- evitar singletons globais excessivos;
- usar interfaces;
- permitir mocks;
- permitir testes unitários;
- usar tipos explícitos;
- evitar números mágicos;
- validar argumentos;
- tratar erros;
- registrar falhas importantes;
- não esconder erros de inicialização;
- possuir comentários apenas onde agregam contexto;
- ter nomes consistentes;
- ter configuração centralizada.

---

# 40. RESTRIÇÕES DE PROJETO

Não:

- implementar tudo em `app_main.cpp`;
- misturar USB, display e DSP na mesma classe;
- acessar diretamente AMY a partir da interface;
- bloquear a tarefa de áudio;
- depender de computador;
- depender de aplicativo de celular;
- presumir CCs do SMK sem validação;
- presumir o controlador do display;
- gravar Flash a cada movimento de knob;
- usar delays bloqueantes para clock musical;
- alocar memória continuamente no DSP;
- ignorar desconexão USB;
- deixar Note Off sujeito a descarte comum;
- usar o PCM5102A sem validar mute, clocks e alimentação;
- declarar metas de polifonia sem benchmark;
- declarar latência sem medição.

---

# 41. DECISÕES TÉCNICAS FIXADAS

As seguintes decisões já estão tomadas:

- microcontrolador: ESP32-S3 DevKitC N16R8;
- controlador principal: M-VAVE SMK25 V2;
- MIDI principal: USB MIDI Host;
- DAC: PCM5102A;
- protocolo do DAC: I²S;
- sample rate inicial: 48 kHz;
- display: 284 × 76;
- motor inicial: AMY;
- framework principal: ESP-IDF;
- funcionamento autônomo;
- oito macros;
- soft takeover;
- presets persistentes;
- arpejador;
- sequenciador;
- cenas;
- monitor MIDI;
- MIDI Learn.

---

# 42. PONTOS A CONFIRMAR ANTES DA PINAGEM FINAL

Solicitar ao responsável pelo hardware somente quando necessário:

- modelo exato da DevKitC;
- revisão ou link da placa;
- foto ou esquema das portas USB;
- modelo exato do display;
- controlador do display;
- pinagem do display;
- tensão do display;
- modelo do módulo PCM5102A;
- pinagem disponível;
- necessidade de microSD;
- tipo de alimentação final;
- existência de saída para fones;
- necessidade de potenciômetro físico de volume;
- tipo de conector de áudio.

Na ausência dessas informações, desenvolver abstrações e usar configurações por arquivo, sem inventar pinagem definitiva.

---

# 43. ENTREGAS DO AGENTE

O agente deve produzir, progressivamente:

1. arquitetura do sistema;
2. lista de riscos;
3. mapa de dependências;
4. diagrama de hardware;
5. pinagem provisória;
6. projeto ESP-IDF compilável;
7. driver I²S para PCM5102A;
8. teste de seno estéreo;
9. integração AMY;
10. USB MIDI Host;
11. MIDI Monitor;
12. sistema de eventos;
13. driver de display;
14. interface inicial;
15. controller profile;
16. MIDI Learn;
17. Patch Manager;
18. macros;
19. soft takeover;
20. arpejador;
21. sequenciador;
22. cenas;
23. testes;
24. documentação;
25. release estável.

Cada entrega deve incluir:

- código;
- instruções;
- decisões;
- limitações;
- testes realizados;
- resultados;
- próximos riscos.

---

# 44. PRIMEIRA TAREFA DO AGENTE

Comece pela arquitetura e pela prova de conceito.

Produza inicialmente:

1. análise de viabilidade;
2. arquitetura de tarefas FreeRTOS;
3. arquitetura de módulos;
4. proposta de barramento de eventos;
5. configuração do I²S para o PCM5102A;
6. lista de pinos necessários;
7. estratégia de USB MIDI Host;
8. plano de integração do AMY;
9. estratégia de buffers;
10. estratégia de medição de underruns;
11. riscos técnicos;
12. critérios de teste;
13. esqueleto de diretórios;
14. arquivos iniciais do projeto;
15. código para gerar tom estéreo pelo PCM5102A.

Não avance para sequenciador ou interface completa antes de validar:

- áudio I²S;
- PCM5102A;
- estabilidade dos buffers;
- AMY;
- USB MIDI Host;
- Note On;
- Note Off;
- pitch bend;
- modulation;
- desconexão e reconexão.

O resultado da primeira etapa deve ser um firmware compilável que gere áudio pelo PCM5102A e forneça logs claros de diagnóstico.

---

# 45. PRINCÍPIO CENTRAL

A prioridade máxima do projeto é:

Áudio estável e tocável.

A ordem de prioridade é:

1. ausência de travamentos;
2. ausência de notas presas;
3. ausência de underruns;
4. baixa latência;
5. resposta musical;
6. estabilidade USB;
7. clareza da interface;
8. recursos adicionais.

Sempre prefira uma implementação simples, testável e estável a uma implementação extensa, complexa e frágil.