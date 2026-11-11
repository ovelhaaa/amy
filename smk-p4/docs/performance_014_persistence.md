# SMK-P4 0.14 — persistência de presets

O V3 encapsula os 160 bytes do V2 e acrescenta oito posições de performance protegidas por CRC externo. Os formatos e as chaves legados continuam legíveis. Um preset com todas as posições neutras pode usar V2. V1/V2 decodificam o trio legado e inicializam as novas posições nos neutros de fábrica, preservando o som histórico.

O coordenador escreve e verifica os dados Synth e FX antes de publicar o marcador com versão, banco e sequência de Synth e FX. A leitura V3 depende de um marcador válido. A inspeção de presets legados ignora V3 sem marcador, de modo que uma gravação interrompida não confirma posições novas ao lado de FX antigos. Os parâmetros manuais, os IDs 0–57 e o algoritmo FM permanecem no payload Synth. Os perfis MIDI globais têm seu próprio armazenamento.

O firmware anterior reconhece marcadores V1/V2. Para voltar a ele após Save As V3, use o backup prévio de NVS se precisar do estado exato anterior. A versão antiga não recupera os registros V3.
