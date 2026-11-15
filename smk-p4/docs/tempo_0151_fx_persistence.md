# Persistência do Delay Sync e tempo global

O registro FX12 V2 ocupa 32 bytes. Os bytes 21 e 22 guardam modo e divisão. O byte 23 permanece zero. O tempo manual mantém a posição e quantização em ms da V1. O decoder valida versão, ROM, CRC e intervalo dos campos. Registros V1 preservam seu tempo manual e carregam em FREE. Firmware 0.15 pode rejeitar registros V2 escritos pela 0.15.1, por isso o backup pré-gravação é relevante para rollback.

O coordenador V4 continua publicando Synth e FX juntos pelo marcador de commit e usa o codec atualizado em cada banco. Compare, Revert e verificação de alterações incluem modo e divisão. O tamanho dos bancos e do marcador permanece o mesmo.

A chave `global` no namespace `perf0151tempo` guarda o BPM global em oito bytes (identificador, versão, BPM, reservado e CRC32). A carga ocorre após admissão do AMY e usa o caminho do owner antes das operações de performance. Na ausência de dados válidos, o default é 120 BPM. O botão SAVE GLOBAL TEMPO grava somente o último BPM confirmado, com barreira de áudio ocioso e verificação de readback. Os presets V4 não contêm BPM.
