# SMK-P4 0.14.1 — persistência

## Compatibilidade

| Formato | Leitura em 0.14.1 | Posições | Rotas |
|---|---|---|---|
| V1 | Sim | Três legadas e neutros das oito novas | Fábrica |
| V2 | Sim | Três legadas e neutros das oito novas | Fábrica |
| V3 | Sim | Três legadas e oito novas | Fábrica |
| V4 | Sim | Três legadas e oito novas | Diferenças frente à fábrica |

O V4 contém magic de 4 bytes, contagem de 1 byte, V3 integral de 176 bytes, entradas ordenadas de 15 bytes e CRC externo de 4 bytes. Cada entrada contém índice do slot, ID do destino, flags e três floats IEEE-754 little-endian. Rotas omitidas usam a fábrica e uma entrada com rota vazia remove uma rota original. Nome, sequência, identidade ROM, IDs Synth e posições continuam no V3 encapsulado. Presets sem rotas customizadas continuam usando V2/V3. Abrir um preset antigo não o regrava.

## Orçamento

O tamanho é 185 bytes mais 15 por slot alterado. Uma alteração usa 200 bytes e 48 usam 905 bytes. A NVS tem 24 KiB. Só os 32 registros V4 máximos totalizariam 28.960 bytes, antes dos bancos FX, marcadores, chaves legadas, perfil MIDI e reserva NVS. A capacidade máxima de edição é 48 por preset. A capacidade persistida é esparsa e depende da ocupação real. O Save verifica entradas livres para o candidato, metadados, FX, marcadores e uma página de GC. A memória física ainda precisa ser medida. Se a transação não couber, o Save falha e o commit anterior fica recuperável.

## Commit e rollback

O marcador vigente referencia a versão, banco e sequência do Synth, que inclui as rotas, e o banco e sequência dos FX. A escrita e o readback de ambos precedem o marcador. Um registro corrompido recua para o par anterior íntegro. Uma nova tentativa parte da geração íntegra recuperável, preservando esse banco até o commit.

A 0.14 e a 0.13.1 não leem V4. Um firmware antigo pode recuar para gerações anteriores e sobrescrever marcadores se gravar sobre uma NVS usada pela 0.14.1. Antes da qualificação física, guardar firmware e NVS da mesma unidade. Para voltar à versão anterior, restaurar a imagem anterior verificada e a NVS pré-ensaio da unidade. Cada imagem gravada exige autorização vinculada ao SHA-256. Nenhum passo de rollback foi executado.
