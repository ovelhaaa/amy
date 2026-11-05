# SMK-P4 0.7 — roteiro de qualificação (MANUAL / NOT RUN)

Este candidato não herda aprovação física da 0.6. Gravação/startup já executados
na unidade do [registro físico](sound_library_07_hardware_qualification.md);
smoke e janela longa continuam pendentes. Não gravar sem autorização
explícita para a imagem concreta, após os passos abaixo. O build seguro tem
I²S/USB desligados. O ambiente `guition-sound-library-8-bench` usa o perfil
histórico 0.6, ainda sujeito à reconfirmação da montagem atual.

1. Identificar Guition JC-ESP32P4-M3, revisão ES, serial e porta reais; fechar
   monitores concorrentes. Usar `C:/.platformio/penv/Scripts/python.exe -m
   serial.tools.list_ports -v`. Não presumir que COM13 continua sendo a porta.
2. Identificar imagem instalada e verificar SHA256 contra bundle qualificado.
   Preservar backup integral e dados, sem sobrescrever backups anteriores.
   Conferir rollback 4/6/8 da 0.6 e layout Flash. Firmware.bin não é backup integral.
3. Reconfirmar PCM5102A, alimentação/straps/saída de linha, BCLK50/WS51/DATA52,
   USB HS0 e VBUS/proteções da montagem 0.6. Esses pinos não são um novo pinout.
4. Auditar sdkconfig.h e comandos reais de compilação com
   `python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library
   --environment guition-sound-library-8-bench --musical`. Conferir hashes do
   manifesto, ELF/mapa e SDK; obter autorização do operador para esta imagem.
5. Após gravação autorizada, capturar boot: AMY ready, limite8, quatro bancos,
   memória admitida, sem OOM; áudio/I²S, USB e touchscreen funcionais. Se a guarda
   de memória recusar boot, manter falha segura e voltar ao bundle autorizado;
   não diminuir reservas nem vozes para esconder a falha.
6. Em cada patch128/135/138/142, tocar uma, quatro, seis e oito notas (por exemplo
   48,51,54,57,60,63,66,69); conferir VX e nona nota rejeitada sem cortar as oito.
   Soltar em ordem inversa. Repetir velocity baixa/alta e Note On velocity0.
7. Repetir sustain, soltura parcial, retrigger antes de acabar release,
   acordes rápidos, bend e CC1. Sustentar notas iguais por Touch/USB e soltar em
   ambas ordens; nenhuma fonte pode liberar a nota da outra indevidamente.
8. Trocar entre todos os pares de bancos com nota/acorde/sustain/release/bend/CC1.
   Esperado: cancelamento seguro das notas e controles anteriores, novo nome
   somente após snapshot aplicado, sem replay de USB antigo. Não há crossfade.
   Conferir Prev/Next (só os quatro residentes), categorias, favoritos, paginação,
   planejados desabilitados e nomes completos. Layer B continua planejada.
9. Panic com notas e releases deve produzir silêncio e permitir tocar novamente.
   Desconectar/reconectar apenas SMK25 ao menos quatro vezes, incluindo troca
   de patch e Touch ativo. USB ausente não deve impedir UI/áudio.
10. Operador avalia ataques, releases, diferença musical, ruído, clicks, volume,
    velocity, bend/CC1. Host RMS/PCM não substitui esta audição.
11. Só após smoke aprovado, executar >=30min contínuos com oito notas, sustain,
    retrigger, trocas, navegação e hot-plug. Capturar logs sem reset e registrar
    cada patch/cena/observação. Exigir zero OOM, clipping, erros I²S, perdas MIDI,
    notas presas e violações compute; render deve caber em5333,33us com margem.
    Medir heap interno/PSRAM/maior bloco, stacks, fila, render médio/máximo e troca.
    Guardas originais após begin, antes de I²S/UI: interno>=65536 B e maior
    bloco>=32768 B. A revisão observou31744 B de maior bloco após UI: registrar
    essa limitação e demonstrar margem real dos endpoints sob hot-plug, sem
    declarar que32KiB continuam disponíveis nesse ponto.
12. Preservar evidência de qualquer falha, rejeitar o candidato e usar rollback
    previamente autorizado. Não declarar Gate E PASS com build/host apenas.

Latência tecla→saída, underrun DMA físico e qualidade auditiva continuam MANUAL.
O máximo histórico4234us é referência de outro binário/configuração, não limite
medido desta biblioteca. Não atualizar silenciosamente a imagem instalada.
