# Correção de memória do display 240×240 — 2026-09-07

## Problema observado

O boot do perfil quadrado registrava:

```text
Failed to allocate UI Framebuffer (115200 bytes)
ST7789 display initialization failed; using DummyDisplayDriver fallback
deltas_add_pool_block: out of memory - events will be dropped
```

O painel já estava ligado quando a alocação falhava. Como nenhum quadro válido
era enviado depois do fallback, sua GRAM permanecia visível com pixels
indefinidos. O perfil 284×76 podia falhar pelo mesmo motivo: seu framebuffer de
43.168 bytes também excedia o maior bloco interno medido em algumas partidas.

## Implementação

- O framebuffer RGB565 completo do ST7789 fica exclusivamente em PSRAM.
- Uma janela de 16 linhas fica em RAM interna com capacidade DMA: 7.680 bytes
  em 240×240 e 9.088 bytes em 284×76.
- A tarefa de UI copia blocos limitados da PSRAM para essa janela e aguarda a
  conclusão do DMA antes de reutilizá-la. Não há acesso ao driver na tarefa de
  áudio.
- O backlight permanece apagado até um quadro preto completo chegar ao painel.
- Cores RGB565 são convertidas para a ordem de bytes exigida pela transmissão
  SPI ao serem escritas no framebuffer.
- O painel quadrado usa intervalo de 66 ms, aproximadamente 15 FPS. Os perfis
  menores mantêm 33 ms. Um quadro 240×240 requer idealmente cerca de 23 ms a
  40 MHz, portanto 30 FPS deixaria pouca margem para outras atividades.

O estado de osciladores, buffers de bloco e mistura do AMY continua em RAM
interna. O primeiro bloco de deltas também continua interno. Somente quando ele
se esgota, blocos adicionais podem usar PSRAM por uma alternativa específica
do build SMK-S3. A alteração no núcleo AMY é protegida por
`AMY_DELTA_POOL_PSRAM_FALLBACK` e não muda outros builds. `ram_caps_events`
permanece em RAM interna porque também armazena o estado quente dos osciladores.

## Critérios de validação em hardware

1. O boot deve registrar `ST7789 initialized`, informando framebuffer em PSRAM
   e janela DMA interna; não deve ativar `DummyDisplayDriver`.
2. Não deve aparecer `out of memory - events will be dropped`.
3. Executar `audio_status` e `memory` depois de pelo menos 30 segundos.
4. Comparar média e máximo de render, gaps PCM, descartes e underruns antes e
   depois de dez minutos tocando acordes e movendo controles.
5. Confirmar manualmente orientação, offsets, inversão, cores e ausência de
   tearing. Esses itens dependem do módulo ST7789 exato e não são certificados
   pelo build.

## Perfil físico confirmado e Home quadrada

O módulo 1,54 polegada 240×240 conectado à placa foi confirmado com offsets
zero, orientação normal, backlight ativo em nível alto e comando de inversão
de cores do ST7789 habilitado. A opção continua configurável para permitir
outros módulos com o mesmo controlador e vidro diferente.

Na Home quadrada, a contagem e as oito barras de vozes ativas ficam na segunda
linha do cabeçalho. O rodapé passa a mostrar o andamento em fonte 5×7 ampliada
em 3×, seguido de `BPM` em 2×; a carga de CPU permanece como diagnóstico
secundário no canto inferior direito. A mudança reutiliza o snapshot da UI e
não adiciona leitura, sincronização ou alocação ao caminho de áudio.

## Validação executada na placa

O firmware foi compilado, gravado e verificado na COM8. O boot confirmou o
framebuffer de 115.200 bytes em PSRAM e a janela DMA de 7.680 bytes em RAM
interna. Não houve fallback para `DummyDisplayDriver`, mensagem de OOM, panic,
watchdog ou brownout na captura.

| Medida em repouso | Firmware anterior, display fictício | Correção, ST7789 ativo |
| --- | ---: | ---: |
| Média de comandos + render | 431 µs | 419–423 µs |
| Audio underruns | 0 | 0 |
| Descartes de comandos | 0 | 0 |
| RAM interna livre | 55.231 bytes | 47.347 bytes |
| PSRAM livre | 8.242.524 bytes | 8.087.472 bytes |

O contador de lacunas PCM chegou a 75 durante a inicialização e permaneceu
inalterado em uma janela observada de 15 segundos com a tela atualizando. O
máximo acumulado de 250.416 µs também ocorreu no boot, durante carregamento e
configuração inicial. Esses valores não são regressões em regime permanente,
mas mostram que a sequência de boot ainda precisa ser reorganizada para
preencher os buffers antes de iniciar a saída I²S. A aparência física da tela e
o teste musical sob carga continuam manuais.

Após habilitar a inversão confirmada do módulo e reorganizar a Home, uma nova
gravação na COM8 mediu 416–418 µs de média, zero underruns e zero comandos
descartados. As lacunas PCM ficaram estáveis em 77 entre 27 e 78 segundos de
uptime, confirmando que a alteração de layout não adicionou falhas em regime
permanente. A correção visual das cores ainda requer confirmação humana no
painel, e o teste musical prolongado continua manual.
