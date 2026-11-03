# SMK-P4 UI Blueprint 1.1 — Performance Stage v2 Design

**Display:** Guition JC-ESP32P4-M3 (`800 × 480` horizontal, RGB565)  
**Framework:** LVGL 9.3.0  
**Implementação:** [`performance_screen.cpp`](../components/ui_blueprint/screens/performance_screen.cpp)

---

## 1. Objetivo e Hierarquia Visual de Palco

A tela **01 Performance Stage** foi redesenhada na versão 1.1 para operar como um instrumento musical de palco, reduzindo o ruído técnico de desenvolvimento e ampliando os alvos de toque críticos para `800 × 480`.

### Hierarquia de Prioridade Visual
1. **Nome do Patch (`000 NOME DO PATCH`)**: Card superior dominante (`784 × 58 px`) em tipografia `montserrat_20`, com categoria, família de síntese e botões diretos `< PREV`, `NEXT >`, `FAV`, `BROWSER` e `EDIT`.
2. **Macros Principais**: 4 controles amplos por página (com alternância de banco `MACROS 1-4` / `MACROS 5-8` no modo `MACRO FOCUS`) e rótulos contextuais por patch (`M1 CHAR`, `M2 BRIGHT`, `M3 MOTION`, `M4 SHAPE`, `M5 ATTACK`, `M6 RELEASE`, `M7 SPACE`, `M8 DRIVE`).
3. **Master Level**: Controle dedicado de saída sempre visível na Performance (`MASTER LEVEL`).
4. **Octave (`OCT -` / `OCT +`)**: Botões dedicados em ambos os modos da Performance, com indicador central (`OCT +0`) e atualização in-place (sem reconstrução destrutiva durante notas sustentadas).
5. **Layer / Harmony**: Pílulas de acesso rápido e leitura imediata (`LAYER: SINGLE A`, `HARM: C NAT MINOR | MINOR`).
6. **Navegação e Panic**: Barra superior com `PERF` (`58 × 36 px`) e `PANIC` (`66 × 36 px`) + barra inferior com 6 destinos e seletor de banco.
7. **Diagnósticos Secundários**: Mantidos discretos no `StatusStrip` (`UI DEMO`, `MIDI`, `AUD`, `VX:0/4`).

---

## 2. Dois Modos de Palco (`MACRO FOCUS` e `KEYS FOCUS`)

### Modo 0 — `MACRO FOCUS` (Otimizado para controlador físico SMK25 V2)
- **Seletor de Patch Dominante** (`y = 4..62`): `784 × 58 px` em `PresentationLevel::StageMinimal`.
- **Stage Control Strip** (`y = 66..104`):
  - `LAYER: SINGLE A` (`174 × 32 px`) — alterna modo de camadas.
  - `HARM: C NAT MINOR | MINOR` (`266 × 32 px`) — abre a tela Harmony Assist.
  - `BANK: MACROS 1-4` / `BANK: MACROS 5-8` (`158 × 32 px`) — alterna entre os 4 macros primários (1–4) e os 4 macros auxiliares (5–8).
  - `MODE: KEYS FOCUS` (`156 × 32 px`, destaque âmbar) — alterna para o modo de teclado expandido.
- **Grid 2×2 de 4 Macros Grandes** (`y = 108..264`):
  - Cada card mede **`388 × 74 px`** (dobro da largura da versão 1.0), com valor em `montserrat_20` alinhado à direita e barra de slider de `18 px` de altura (`+14 px` de área de clique estendida).
- **Deck Inferior de Performance** (`y = 268..388`):
  - `MASTER LEVEL` (`232 × 72 px`).
  - `OCT -` (`82 × 44 px`), indicador `OCT +0`, `OCT +` (`82 × 44 px`).
  - Teclado de 1 oitava (`544 × 120 px`, `KeyboardDisplayMode::StagePlay`).

### Modo 1 — `KEYS FOCUS` (Otimizado para execução direta no touchscreen)
- **Seletor de Patch Dominante** (`y = 4..62`): `784 × 58 px`.
- **Stage Control Strip** (`y = 66..104`):
  - `LAYER`, `HARM`, indicador de status musical em tempo real e botão `MODE: MACRO FOCUS`.
- **Linha Compacta de 4 Macros + Master + Octave** (`y = 108..214`):
  - 4 knobs compactos (`130 × 106 px`) para `M1..M4` em `PresentationLevel::StageMinimal`.
  - `MASTER LEVEL` (`240 × 64 px`) à direita.
  - Botões `OCT -` (`86 × 38 px`), indicador `OCT +0` e `OCT +` (`86 × 38 px`) logo abaixo do Master.
- **Teclado de Duas Oitavas Maior** (`y = 218..388`):
  - Ocupa toda a largura útil (**`784 × 170 px`**, `KeyboardDisplayMode::StageKeys2Oct`), com 15 teclas brancas e 10 teclas pretas dimensionadas para execução direta no touch.

---

## 3. Redução de Ruído Visual (`StageMinimal` vs `BlueprintVerbose`)

| Aspecto | `StageMinimal` (Tela 01 Performance) | `BlueprintVerbose` (Telas 02..14 Edição/Diagnóstico) |
|---|---|---|
| Badges individuais `AVAIL` / `PLAN` / `N/A` em cada controle | **Omitidos** (0 badges repetitivos nos cards de macro/master) | **Exibidos** em cada card e inspetor técnico |
| Identificação de modo demonstrativo | **Sempre visível** no badge `UI DEMO` do `StatusStrip` | **Sempre visível** no badge `UI DEMO` + banners de tela |
| Tipografia de valores | Destaque em `montserrat_20` nos sliders grandes de palco | `montserrat_14` padrão técnico |
| Abreviações de Macros | Expandidas: `BRIGHT`, `MOTION`, `SHAPE`, `SPACE`, `DRIVE` | Expandidas e documentadas no registro |
