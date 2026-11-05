# Qualificação física de seis vozes — 2026-11-05

PASS na montagem Guition JC-ESP32P4-M3 reconfirmada pelo operador: PCM5102A
BCLK50/WS51/DATA52, USB Host HS controlador0, VBUS/proteções sem mudanças.
COM13, unidade 80:F1:B2:D3:CC:86. Imagem instalada e verificada pelo esptool:
`41c12553a95136e7f9dbf45bbc1dbf199d119c23489294cd2aad435c62a5b3b8`.
Gravação autorizada pelo operador, somente aplicação em0x10000.

## Validação

- PASS: captura concluída, processo exit0, 433898 bytes reais.
- PASS: todas as séries >=1872.44s contínuos;
  maior intervalo 5.35s; sem reboot.
- PASS: 1544 novos eventos MIDI, quatro desconexões e quatro
  reconexões, host conectado no fim, 30 transições de tela.
- PASS: amostras de seis notas sustentadas nos patches128 e135; duas rejeições
  de capacidade; nenhuma amostra acima de seis. Rejeição sem cortar notas
  existentes foi confirmada pelo operador.
- PASS: render máximo2736us / período5333us (51.3%); render/compute deadlines0,
  refill risks0, write errors0, timeouts0, clipping0, OOM0, parse errors0,
  dropped MIDI0 e overflows0. Máximo serviço5753us inclui espera DMA e não é
  tempo de computação. Espera DMA máxima5175us, comando máximo1218us,
  patch máximo695us; máximos separados não devem ser somados como mesma amostra.
- PASS: heap interno mínimo188415B, maior bloco mínimo110592B; PSRAM
  mínima31176952B, maior bloco30932992B. Retorno aos valores iniciais após
  carga/hot-plugs; nenhuma queda progressiva observada. Stack livre mínimo:
  áudio7440B, USB4144B, UI3332B. LVGL livre mínimo88240B, final95448B,
  pico usado50164B, fragmentação máxima4%/final1%; telas variam a ocupação.
- MANUAL/PASS: operador confirmou seis notas, soltura, Panic, som sem novos
  estalos/distorção, sustain, retrigger/acordes rápidos, pitch bend, modulação,
  troca com notas ativas e sétima rejeitada. Confirmou repetição atual de quatro
  hot-plugs somente SMK25, coexistência Touch/USB e VX6/6 nos dois patches.
- PASS: `python -m unittest discover -s smk-p4/tests -p test_standalone_capture_report.py -v` (9 testes).
- PASS: `python smk-p4/tests/standalone_capture_report.py build/polyphony06_flash6_42c95326976f/six_voice_window_restarted.log --output build/polyphony06_flash6_42c95326976f/six_voice_qualification_report.json --min-seconds 1800 --hotplugs 4 --voices 6`.

## Limites e registros negativos

A primeira janela foi interrompida pela retirada de PC/alimentação, confirmada
pelo operador; não conta como teste contínuo nem como falha espontânea. O boot
inicial vazio foi recuperado com reset USB JTAG do esptool. Ambos os registros
foram preservados. As primeiras confirmações de hot-plug eram anteriores ao
reboot; os testes foram repetidos na janela válida e cruzados com telemetria.

DMA underruns físicos, latência tecla→saída e CPU total: NOT RUN/não medidos.
A idade máxima da fila19ms não mede latência física. Descartes de MIDI_TRACE
são do diagnóstico limitado; eventos musicais dropped permaneceram0. A janela
inclui repouso e carga musical, não 30min de acorde cheio. Não prova todos os
cenários possíveis nem segurança formal contra OOM do core legado.

## Arquitetura e próximo passo

Esta qualificação não altera DSP, AMY core, USB, I²S, ganho, FX ou patches.
O analisador seleciona explicitamente4/6/8 e mantém quatro como padrão.
Seis é o perfil musical fisicamente validado nesta sessão; o padrão seguro do
projeto continua quatro. Oito permanece experimental e precisa de imagem
concreta, autorização própria e repetição da qualificação antes de recomendação.
Logs e JSON: `build/polyphony06_flash6_42c95326976f/`.
Rollback seis: bundle `build/polyphony06_bench6_41c12553a951_50cdce33.zip`;
rollback0.5 e backup integral seguem preservados conforme registro de bancada.
