# SMK-P4 0.13.1 — Evidência de hardware

**SMK-P4 0.13.1 HARDWARE: NOT RUN** para os candidatos deste hardening. Os builds, digests e regressões de host constam no [relatório final](stability_0131_final_hardening.md). SRAM e PSRAM livres, render, underruns e saída PCM5102A na placa dependem do smoke físico.

## Registro histórico

O commit `08a430fb8cdd7ea96b141b1d66969586389813d2` descreve telemetria da 0.13. O commit `7622c2229c4f66bc80abf0ab60715ecccc28c2ec` registra uma gravação e boot anteriores em COM13, com SHA-256 de firmware `1EA1F38ADE5DB0766DD725CCBCE4BE06CEDF1A2C5FB247588C5488759D66E12D`. Esses registros de Git documentam as alegações históricas. A verificação desta entrega não encontrou logs físicos independentes nem autorização vinculada aos novos binários.

## Smoke físico pendente

Com autorização específica para o SHA-256, verificar boot e memória livre, áudio estéreo com oito vozes, sustain e reconexão USB. No editor, verificar timeout de Recall, confirmação de FM com Master intercalado e expiração de Overwrite. Registrar o hash, a telemetria serial, underruns e resultado da escuta.
