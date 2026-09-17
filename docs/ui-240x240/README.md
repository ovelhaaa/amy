# Conceitos de UI 240 × 240

Estes SVGs são estudos gráficos, não capturas do firmware nem confirmação de compatibilidade com um módulo físico.

## Estado da implementação

O firmware agora classifica os displays em `Compact`, `Square` e `Widescreen` e
possui layouts 240 × 240 próprios para Home, parâmetro, sequenciador, pads,
cenas, monitor MIDI, MIDI Learn, diagnósticos e splash. O perfil 284 × 76
continua sendo o padrão.

O perfil `SMKS3_DISPLAY_PROFILE_SQUARE_240X240` é experimental e reutiliza o
driver ST7789 existente. Offset X/Y, rotação, espelhamento, inversão de cor e
polaridade do backlight são configuráveis no Kconfig porque dependem do módulo
1,54 polegada exato. Os valores zero são placeholders seguros de compilação,
não uma pinagem ou sequência de inicialização validada.

A navegação B5/B6 agora percorre apenas Home, Sequencer, Pads e Scenes. As telas
de sistema e MIDI não entram no ciclo de performance. O futuro hub de utilidades
continua planejado; Pad B7 mantém o comportamento atual de abrir MIDI Learn.

Os testes de host renderizam as telas principais em 160 × 128, 240 × 240 e
284 × 76 e rejeitam desenho fora dos limites no perfil quadrado. Ambos os
perfis 284 × 76 e 240 × 240 compilam para o ESP32-S3. Atualização visual,
contraste, ângulo de visão, offsets e polaridade ainda exigem teste físico.

- `home.svg`: tela de performance com oito macros em matriz 4 × 2.
- `home-performance.svg`: alternativa orientada à leitura rápida no palco.
- `home-edit.svg`: alternativa orientada à edição dos oito knobs.
- `parameter-overlay.svg`: edição transitória com valor, posição salva e estado de soft takeover.
- `midi-monitor.svg`: monitor de eventos e integridade da entrada USB MIDI.
- `sequencer.svg`: quatro pistas e 16 passos com os oito pads como entrada contextual.
- `system-diagnostics.svg`: saúde de áudio, USB, filas e memória.
- `navigation-map.svg`: síntese do fluxo proposto entre páginas, overlays e transporte.
- `navigation-plan.md`: regras de navegação baseadas nos eventos atualmente disponíveis no SMK25 V2.

## Direção visual

- Preto como fundo para reduzir brilho aparente e consumo de área iluminada.
- Ciano para o banco A e foco principal.
- Âmbar para o banco B, valores salvos e estados de atenção.
- Verde para estados operacionais saudáveis.
- Tipografia monoespaçada e números grandes para leitura durante performance.
- Componentes dimensionados especificamente para 240 × 240, sem reaproveitar coordenadas do layout 284 × 76.

## Restrições da prova de conceito

- Fontes SVG do sistema aproximam a aparência; o firmware ainda precisa de fontes bitmap equivalentes.
- Ângulo de visão, contraste, brilho e reprodução de cores dependem do painel escolhido.
- A estratégia de atualização e o uso de RAM ainda precisam ser medidos no ESP32-S3 com áudio ativo.
