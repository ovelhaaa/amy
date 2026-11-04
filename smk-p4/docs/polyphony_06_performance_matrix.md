# SMK-P4 0.6 — matriz de performance e aceitação física

48 kHz, estéreo PCM16,256 frames: orçamento5333,33 us. Mesmos driver e DMA.
Os números históricos0.4 (render máximo2113 us,0 deadlines em30min) não aprovam
6/8 nem são comparáveis a host x86-64. Host mede apply+render e render separados,
sem I²S, USB físico ou LVGL concorrente. Não traduzir esses tempos para margem P4.

Executar os perfis em ordem4→6→8. Oito só entra na bancada depois de seis aprovado.
Para cada perfil e patch128/135, capturar amostras sem reiniciar contadores e
identificar build, SHA256, sdkconfig, relógio e duração. Registrar:

| Cenário | Estímulo | Critério |
|---|---|---|
| Repouso | sem notas | PCM silencioso, contadores estáveis |
| Uma nota | velocity30/100, release | ataque/timbre preservados, sem clipping |
| Metade/total | N/2 e N alturas/canais, velocidades diferentes | N gates reais |
| Capacidade | N+1, soltar rejeitada primeiro | uma rejeição, nenhum gate corrompido |
| Sustain | CC64 por canal, notas repetidas | soltar um canal não libera outro |
| Caudas | acorde, Off conjunto, novo acorde imediato, sequência | sem cliques regressivos/deadlines |
| Controles | bend por canal e CC1 durante acorde | Touch independente |
| UI | navegar/sliders/oitava, múltiplos pressionamentos | navegação intacta, VX acompanha perfil |
| USB | cancelar com notas/sustain e reconectar>=4 vezes | somente USB cancelado, sem reboot |
| Segurança | Panic ativo/release, patch ativo, lease Touch10s | sem notas presas |
| Longa janela | >=30min, carga musical, UI, sustain, hot-plug | sem degradação progressiva |

Capturar render médio/máximo, compute/render deadlines, serviço completo, espera
DMA, refill risks, write errors/timeouts, latência máxima da fila, parse/dropped,
polyphony_rejected, clipping/OOM, heap interno/maior bloco, PSRAM e stacks/uso LVGL.
Contagem exata de underrun analógico não está disponível: refill risk não equivale
a dropout. Não afirmar latência física a partir de idade da fila.

Não aprovar somente por máximo<5333 us. Exigir margem operacional significativa
repetida nos piores cenários, sem deadlines, sem crescimento de erros e sem queda
progressiva de heap/maior bloco/pilhas. Se8 quase consumir o orçamento, manter6;
se6 falhar, manter4. Audição do operador é obrigatória, com ganho baixo inicial,
comparando ataques, releases, volume, distorção e retrigger nos dois patches.

## Antes de gravar — MANUAL

Identificar unidade/porta atuais sem presumir COM13; confirmar montagem elétrica,
GPIOs, VBUS e straps qualificados. Identificar imagem instalada, preservar backup
com hash e layout. Verificar binário candidato/hash e header sdkconfig gerado,
rejeitar gates/configuração incompatíveis. Obter autorização explícita para
substituir aquela imagem concreta. Nenhuma gravação está autorizada pelo build.
Preservar logs negativos. Rollback usa bundle físico previamente validado e seus
offsets documentados, nunca confundir firmware.bin com backup integral da Flash.

## Execução host final, sem builds Codex concorrentes

Microssegundos em Windows x86-64, GCC14.2.0 -O2. Não são tempos P4.
Serviço inclui apply+render; render isolado não revela o custo da fila cheia.

|Vozes|Render cheio128 média/máximo|Render cheio135 média/máximo|Pior render em todos cenários|Pior serviço em todos cenários|
|---|---|---|---|---|
|4|31/50|29/45|99|1703|
|6|43/87|44/52|172|2849|
|8|57/78|59/72|258|4223|

Pior serviço ocorreu com as duas filas de32 cheias (Touch retrigger+CC1 para
N-1 USB), em contraste com o render regular. Essa carga precisa de medição no
P4: não inferir margem do orçamento5333,33 us a partir do render host isolado.
As amostras anteriores, sob compilação concorrente, incluíram10558 us de render;
logs preservados. Diferenças de ambiente não são otimização nem regressão DSP
comprovada. A reserva mudou alocação no boot, não o algoritmo de ordenação de
eventos nem o trabalho por controle. JSON final contém cada cena/patch, memória,
RMS/pico/hashes e a comparação PCM. Deadlines/CPUload/latência P4: MANUAL.
