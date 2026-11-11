# SMK-P4 0.13.1 — Evidência de hardware

**Gravação e boot: PASS. Smoke musical: NOT RUN.**

O operador autorizou a gravação do perfil `guition-editor-8` com SHA-256 `6BB85D5115CC340BA782C390494EB019CE8E6FC4D0CD012338793C9927DBBC9B` pela COM13. O esptool 5.1.0 identificou o ESP32-P4 rev. 1.3, leu e conferiu a tabela de partições existente e gravou 1.245.472 bytes no endereço `0x10000`, com verificação de hash e reset. A operação gravou somente a partição de aplicação.

A captura serial de 8 segundos registrou engine AMY pronta com patch 128, zero deadlines de render, 354 µs de render máximo observado, 158.155 B de RAM interna livre e 30.493.192 B de PSRAM livre. O display reportou zero erros de flush. O perfil estava em `RAM_ONLY`, com zero frames de saída, `DMA_underruns=UNMEASURED` e nenhum evento USB MIDI. Esses dados qualificam o boot em repouso. A escuta pelo PCM5102A, o USB MIDI, os controles e os fluxos de preset ainda precisam de teste físico.

Logs locais: `build/editor011/guition_com13_flash.log` e `build/editor011/guition_com13_boot.log`.

## Registro histórico

O commit `08a430fb8cdd7ea96b141b1d66969586389813d2` descreve telemetria da 0.13. O commit `7622c2229c4f66bc80abf0ab60715ecccc28c2ec` registra uma gravação e boot anteriores em COM13, com SHA-256 de firmware `1EA1F38ADE5DB0766DD725CCBCE4BE06CEDF1A2C5FB247588C5488759D66E12D`. Esses registros de Git documentam as alegações históricas. A verificação desta entrega não encontrou logs físicos independentes nem autorização vinculada aos novos binários.

## Smoke físico pendente

Para o smoke musical, usar um perfil com saída física habilitada após autorização específica para o SHA-256:

1. Conectar o PCM5102A a uma entrada estéreo e o SMK25 V2 à porta USB Host. Capturar o log serial de boot a 115200 baud e registrar o hash, SRAM e PSRAM livres, sample rate e tamanho de bloco.
2. Tocar oito notas, acionar e soltar sustain, desconectar o controlador com notas ativas e reconectar. Esperar áudio nos dois canais, liberação das notas e retomada sem reboot. Registrar underruns e tempo máximo de render.
3. Usar um slot descartável para Save As. Aguardar mais de 10 segundos após o primeiro toque de Overwrite e tocar novamente. Esperar um novo pedido de confirmação com o texto digitado preservado. Confirmar, mudar de patch e executar Recall. Esperar `Confirmed` após a aplicação de síntese e efeitos.
4. Com as notas soltas, selecionar um algoritmo FM e alterar Master durante a solicitação. Esperar o algoritmo confirmado pelo owner e observar `FM_ALGO_DIAG` e `PRESET_DIAG`. Registrar o resultado da escuta e qualquer falha ou lacuna audível.
