# SMK-P4 0.6 — candidato audível de seis vozes

2026-11-04. Operador reconfirmou a montagem0.5 e autorizou build/testes.
**Candidato preparado; gravação desta imagem concreta ainda não autorizada.**
Não há resultado físico do firmware6. A identificação/telemetria abaixo são da0.5.

## Imagem concreta

- Ambiente separado: guition-standalone-blueprint-6-bench.
- Fonte do build: a670c383 (runtime/DSP permanecem os da etapa de software).
- Aplicação:1202384bytes; offset0x10000, factory de0x300000bytes.
- SHA256:41c12553a95136e7f9dbf45bbc1dbf199d119c23489294cd2aad435c62a5b3b8.
- ELF SHA256 informado pelo image-info:1783caf9f8c506392b69944aba7de23a33f10ee824bd37fb8b606084ee35c3e4.
- Checksum0xc2 e validation hash04b48a8c…5315286 válidos.
- IDF5.5.2.260116; projeto/app version continuam smk-p4-gate0/0.1.0 no
  descritor legado. Identificar0.6 pelo SHA e pelo diagnóstico/capacidade, não
  somente por esse campo de versão.
- Seis vozes,96 osciladores,12 vozes AMY/13 posições de synth, dois blocos delta.
- I²S e USB habilitados somente neste ambiente explícito. Sem tom diagnóstico
  ou self-test RAM.48kHz/PCM16estéreo/256frames e mesmo DMA.
- GPIOs reconfirmados: BCLK50,WS51,DATA52. USB HS controlador0, VBUS/proteções
  inalterados. Normalização do sdkconfig difere da referência0.5 somente em
  CONFIG_SMKP4_AMY_VOICES_6 e CONFIG_SMKP4_AMY_VOICES=6.

## Identificação e backup — PASS

COM13, USB303a:1001, serial/MAC80:F1:B2:D3:CC:86. ROM confirmou ESP32-P4
revision1.3; corresponde ao alvo ES<3.0. Modelo/montagem vieram da confirmação
do operador, não foram inferidos apenas do identificador USB.

Backup integral novo, sem sobrescrever anteriores:
`build/polyphony06_preflight_7b71261c3513494aab0a55e59759203e/flash_before_polyphony06.bin`.
16777216bytes; SHA256
7525dbdd6a7b7d5d50af384c79ef87d169df0ef73ff49d53e1cca8b712590df9.
Leitura esptool em133,5s; reinício temporário antes/depois, sem write-flash/eFuses.

Aplicação em0x10000 coincide integralmente com a referência0.5,1201472bytes:
328d510e8820881a3ae9f790cfd3ee0b76f5fc27f8ada01c9dddbb2ba4c5e997.
Checksum0x89/hash de validação válidos. Tabela em0x8000 coincide com a referência
instalada e com a candidata. A configuração normalizada também foi comparada.

Captura anterior à leitura: I2S_PRODUCING, engine ready1, notes0, USB conectado,
render/compute deadlines0, erros/timeouts/refill0, clipping/OOM0. São contadores
históricos da imagem antiga (render máximo2168us), não benchmark6.
Captura após leitura: mesma imagem0.5 reiniciada, I²S produzindo, USB state2,
erros0, notas0. Não foi necessário substituir firmware para restaurar operação.

## Validação executada

- PASS: pio run -d smk-p4 -e guition-standalone-blueprint-6-bench -j 8.
- PASS: python smk-p4/tests/check_blueprint_build.py --voices 6 --environment
  guition-standalone-blueprint-6-bench --musical.
- PASS: esptool image-info para aplicação instalada extraída e candidata.
- PASS: hashes/comparação byte a byte da aplicação0.5 e tabelas de partição.
- PASS: sdkconfig normalizado, somente dois campos novos de polifonia.
- PASS: captura serial antiga antes/depois, sem envio de comandos musicais.
- PASS anterior preservado: regressões host/AMY/LVGL4/6/8 da etapa de software;
  runtime/DSP/drivers não mudaram nesta preparação.
- MANUAL/NOT RUN: boot6, seis notas reais na placa, qualidade auditiva, margem
  DSP, memória com carga, sustain/bend/CC1, USB hot-plug e janela prolongada.

Os artefatos seguros anteriores estão no bundle congelado ac0979c2. Alterar o
platformio.ini fez o PlatformIO invalidar caches .pio antigos: o recheck dos
caches ausentes foi BLOCKED por sdkconfig.h ausente, não uma falha funcional.
Nenhum bundle congelado/backup ou configuração da worktree0.5 foi sobrescrito.

## Escopo proposto após aprovação

Gravar somente a aplicação candidata em0x10000. Preservar bootloader, tabela e
NVS; tabela é idêntica e framework/perfil elétrico foram mantidos. O backup
integral e a aplicação0.5 extraída ficam disponíveis para rollback. Não usar o
backup integral como firmware.bin nem gravar todos os segmentos por hábito.

O item14 do pedido exige aprovação para substituir a imagem concreta, além da
confirmação elétrica. “Build e testes autorizados” não foi interpretado como
aprovação para gravar um hash ainda não apresentado. Primeiro smoke com volume
baixo: boot/ready6/VX0/6, I²S/USB sem erros; depois operador toca seis notas,
sétima rejeitada, sustain, patches128/135, retrigger, bend/CC1, Touch+USB e Panic.
Capturar memória/deadlines e só ampliar a janela se não houver falha crítica.
Oito permanece experimental e não faz parte desta proposta de gravação.

## Congelamento do candidato — PASS

Bundle imutável local:
`build/polyphony06_bench6_41c12553a951_50cdce33.zip`.
SHA256ZIP:a08ba9ff211bf76587c47be1a0f3a8a5e30433fd5bdd8e391fa1b37e1ea911c4.
26 arquivos com SHA conferido, integridade ZIP validada. Contém aplicação/ELF/
mapa/configuração, logs de build e integridade, preflight e aplicação0.5 de
rollback (328d510e…4c5e997). Backup integral16MB permanece no diretório de
preflight, sem ser duplicado no ZIP. DRAM data+BSS do ELF candidato169880bytes;
o resumoPIO30956bytes omite DRAM1, como documentado no orçamento.
Manifesto registra fonte do builda670c383, congelamentoada6085e e autorização
de gravação=false naquele instante. Nenhum comando write-flash foi executado.

## Atualização de bancada — 2026-11-05

Após o congelamento acima, o operador autorizou explicitamente a gravação
("pode gravar"). PASS: aplicação SHA256
`41c12553a95136e7f9dbf45bbc1dbf199d119c23489294cd2aad435c62a5b3b8`
gravada somente em `0x10000`, com verificação de hash pelo esptool. Bootloader,
tabela de partições e NVS foram preservados. Evidências locais:
`build/polyphony06_flash6_42c95326976f/{authorization.json,write_flash.log,write_success.json}`.
O manifesto congelado continua registrando corretamente o estado anterior à autorização.

MANUAL/PASS informado pelo operador: VX 0/6, seis notas simultâneas, soltura,
Panic e som corretos; patches 128/135 com sustain, retrigger/acordes rápidos,
bend, modulação, troca com notas ativas e rejeição da sétima nota sem cortar
as seis existentes. Essas confirmações não substituem a janela instrumentada.

A primeira captura prolongada (`six_voice_musical_window.log`) foi interrompida
por perda da COM13. O operador confirmou ter removido também o cabo do PC ou a
alimentação. A recuperação mostrou novo uptime; essa tentativa NÃO conta como
30 minutos contínuos e não foi classificada como falha espontânea do firmware.
A primeira captura de boot vazia também foi preservada: o reset RTS simples não
saiu do bootloader; o reset USB JTAG do esptool iniciou a aplicação corretamente.

NOT RUN/conclusão pendente: janela contínua reiniciada em
`six_voice_window_restarted.log`, quatro ciclos de hot-plug somente do SMK25,
coexistência Touch/USB e navegação sob carga. Até concluir a captura e analisar
os critérios, seis vozes permanecem candidato em validação física; oito não
foi gravado nem autorizado por esta aprovação de seis vozes.


## Qualificação física6 concluída — 2026-11-05

PASS: janela contínua >30min, seis notas nos dois patches, quatro hot-plugs,
Touch/USB, 30 mudanças de tela e audição aprovada pelo operador. Render máximo
2736us, deadlines/clipping/OOM/drops0, heap interno mínimo188415B.
Detalhes e limites: [qualificação física](polyphony_06_hardware_qualification.md).
O checkpoint pré-autorização acima é histórico. Oito ainda não foi gravado.


## Qualificação8 concluída — 2026-11-05

PASS: captura contínua>=30min, oito notas nos dois patches, quatro hot-plugs,
Touch/USB,39 transições, reataques com release e audição aprovada pelo operador.
Render máximo4234us, deadlines/clipping/OOM/drops0, heap mínimo169075B.
[Resultados, limites e recomendação](polyphony_06_eight_hardware_qualification.md).
Os estados pendentes acima são checkpoints históricos. Nenhuma nova gravação.
