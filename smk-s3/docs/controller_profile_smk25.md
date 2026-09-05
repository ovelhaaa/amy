# Perfil MIDI confirmado — M-VAVE SMK25 V2

Este perfil usa apenas mensagens MIDI observáveis pelo ESP32-S3. Os seletores
físicos `SC/CH`, `KNOB-B`, `PAD-B`, `Oct+`, `Oct-` e `BT` não emitem mensagens
MIDI; o firmware não deve tratá-los como botões recebidos.

## Controles que chegam ao firmware

| Grupo | Tipo | Canal MIDI | Mensagens |
| --- | --- | --- | --- |
| Pads, banco A | Note | 10 | 36–43 |
| Pads, banco B | Note | 10 | 44–51 |
| Knobs, banco A | CC | 1 | 21–28 |
| Knobs, banco B | CC | 1 | 29–36 |
| Play | CC | 1 | 114 |
| Stop | CC | 1 | 115 |
| Rec | CC | 1 | 117 |

Os seletores de banco são silenciosos. A UI exibe o banco somente após receber
uma mensagem de um knob ou pad daquele banco. No boot, o estado é desconhecido.
Na tela de pads, `PAD A` identifica os disparos 36–43 e `PAD B [SHORTCUTS]`
identifica os comandos 44–51 observados mais recentemente; isso não pressupõe
que a tecla física de seleção tenha enviado um evento.

## Atalhos do banco B de pads

| Nota | Ação |
| --- | --- |
| 44 | Patch anterior |
| 45 | Próximo patch |
| 46 | Home |
| 47 | Arpejador liga/desliga |
| 48 | Página anterior |
| 49 | Próxima página |
| 50 | MIDI Learn |
| 51 | Salvar patch, perfil e cenas após manter pressionado por 1,2 s |

`Stop` é a ação inequívoca para interromper reprodução. `Play` inicia a
reprodução e `Rec` controla a gravação. A validação em hardware continua
necessária para confirmar o envio de Note Off do pad 16, usado pela confirmação
por pressão longa.

## Estado mostrado pela UI

A splash apresenta SMK-S3, uma onda decorativa animada e crédito ao AMY.
A duração padrão é de 2,5 segundos, com fade-in de 350 ms; uma nota recebida
pode antecipar a Home após 600 ms. A linha inferior indica apenas o tempo até
a Home, sem afirmar que áudio ou USB foram inicializados com sucesso.

Na Home de 284 × 76, patch, modo/bancos, BPM e vozes têm áreas separadas.
Nomes que excedem o espaço disponível terminam em `~`. Os indicadores M (MIDI)
e U (USB) ficam junto à borda direita, com margem de 2 pixels.
Ao montar o cabeçalho panorâmico, a Home ignora um prefixo idêntico ao número
formatado do preset seguido de espaço (`000 `, `042 ` etc.) no nome armazenado.
Assim, ID 0 e nome `000 A11 Brass Set 1` aparecem como `000 A11 Brass Set 1`.
A identificação `A11` é preservada; nomes sem prefixo correspondente permanecem
intactos. Essa regra não modifica o catálogo nem os arquivos de presets.
Espaços de preenchimento no fim dos nomes também são ignorados na exibição,
evitando um `~` quando somente esses espaços ultrapassariam a largura disponível.
Os valores de banco `A`, `B` e `?` aparecem em branco, em fonte 5×7 com
traço reforçado; os rótulos `K` e `P` continuam em ciano. O valor do andamento
aparece em branco com pixels duplicados (10 pixels de altura), acompanhado
de `BPM` em âmbar, em fonte 3×5 normal. Conferir a legibilidade na placa com
bancos conhecidos/desconhecidos e andamentos de dois e três dígitos.

A tela do sequenciador lê BPM e swing do `ClockManager`, em vez de apresentar
um BPM fixo. Essas leituras são apenas de interface, no task de UI, e não
alteram o caminho de áudio.

## Validação da tipografia da Home

`PASS`: testes existentes de UI, incluindo separação dos campos e cores dos
valores/rótulos; prévia do framebuffer com `?/?`, `A/B`, `B/A` e 80/120/300 BPM;
build ESP32-S3. `MANUAL`: leitura no display físico, ainda sem gravação da placa.
A alteração fica na renderização da Home panorâmica, sem alterar AMY ou GPIOs.
Os campos maiores deixam menos espaço para nomes de patches longos, que continuam
usando `~` quando truncados.

Comandos executados na raiz, em PowerShell com G++ no PATH:

```powershell
$ui = 'smk-s3/components/ui'
g++ -std=c++17 -O2 -Itests/mock -I"$ui/include" -I"$ui/include/screens" -Ismk-s3/components/sequencer/include -Ismk-s3/components/synth/include -Ismk-s3/components/storage/include -Ismk-s3/components/midi/include -Ismk-s3/components/audio/include tests/test_smk_ui.cpp "$ui/display_driver.cpp" "$ui/dummy_display_driver.cpp" "$ui/font_renderer.cpp" "$ui/widgets.cpp" "$ui/screens/home_screen.cpp" "$ui/screens/sequencer_screen.cpp" "$ui/screens/parameter_screen.cpp" "$ui/screens/pad_screen.cpp" "$ui/screens/midi_monitor_screen.cpp" "$ui/screens/scene_screen.cpp" "$ui/screens/splash_screen.cpp" -o build/test_smk_ui.exe
./build/test_smk_ui.exe
C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-devkitc-1
```

Validação posterior da remoção do número duplicado: `PASS` na mesma suíte de UI,
com comparação do framebuffer para prefixos correspondentes/divergentes,
nomes curtos/vazios, espaços de preenchimento e IDs de três a cinco dígitos;
`PASS` no build e no upload pela COM8, com hash verificado. Comando de upload:

```powershell
$env:PYTHONIOENCODING = 'utf-8'
$env:PYTHONUTF8 = '1'
C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-devkitc-1 -t upload
```

`MANUAL`: após o reboot, confirmar `000 A11 Brass Set 1` na Home e trocar de
preset para conferir a leitura; nomes que realmente excedam a largura ainda
terminam em `~`. Nenhuma nova hipótese de hardware ou alteração no áudio.
