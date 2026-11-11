# SMK-P4 0.13.1 — Evidência de hardware

**SMK-P4 0.13.1 HARDWARE: NOT RUN** para os candidatos deste hardening. Os builds, digests e regressões de host constam no [relatório final](stability_0131_final_hardening.md). SRAM e PSRAM livres, render, underruns e saída PCM5102A na placa dependem do smoke físico.

## Registro histórico

O commit `08a430fb8cdd7ea96b141b1d66969586389813d2` descreve telemetria da 0.13. O commit `7622c2229c4f66bc80abf0ab60715ecccc28c2ec` registra uma gravação e boot anteriores em COM13, com SHA-256 de firmware `1EA1F38ADE5DB0766DD725CCBCE4BE06CEDF1A2C5FB247588C5488759D66E12D`. Esses registros de Git documentam as alegações históricas. A verificação desta entrega não encontrou logs físicos independentes nem autorização vinculada aos novos binários.

## Smoke físico pendente

Depois de o operador autorizar e instalar o SHA-256 específico:

1. Conectar o PCM5102A a uma entrada estéreo e o SMK25 V2 à porta USB Host. Capturar o log serial de boot a 115200 baud e registrar o hash, SRAM e PSRAM livres, sample rate e tamanho de bloco.
2. Tocar oito notas, acionar e soltar sustain, desconectar o controlador com notas ativas e reconectar. Esperar áudio nos dois canais, liberação das notas e retomada sem reboot. Registrar underruns e tempo máximo de render.
3. Usar um slot descartável para Save As. Aguardar mais de 10 segundos após o primeiro toque de Overwrite e tocar novamente. Esperar um novo pedido de confirmação com o texto digitado preservado. Confirmar, mudar de patch e executar Recall. Esperar `Confirmed` após a aplicação de síntese e efeitos.
4. Com as notas soltas, selecionar um algoritmo FM e alterar Master durante a solicitação. Esperar o algoritmo confirmado pelo owner e observar `FM_ALGO_DIAG` e `PRESET_DIAG`. Registrar o resultado da escuta e qualquer falha ou lacuna audível.
