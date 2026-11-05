# Candidato audível de oito vozes — 2026-11-05

PASS: preparado depois da qualificação física de seis vozes, documentada em
[qualificação6](polyphony_06_hardware_qualification.md). Oito continua experimental.
NOT RUN: gravação, boot, audição, margem DSP e janela prolongada física de oito.

## Imagem concreta para aprovação

- Ambiente: `guition-standalone-blueprint-8-bench`, fonte `74a586f4`.
- Aplicação: `build/polyphony06_bench8_887cee5de9c0/firmware.bin`,1202400bytes.
- SHA256: `887cee5de9c0f2b6da71caa691e0c5ed34a7e563ba9cde0916cb809eae5b7af7`.
- Proposta: substituir somente aplicação em `0x10000`, partição0x300000.
- Unidade: COM13, VID303a/PID1001, serial80:F1:B2:D3:CC:86; porta novamente
  identificada sem reset. Guition ESrev1.3/16MB conforme preflight anterior.
- Instalada: seis vozes, SHA256
  `41c12553a95136e7f9dbf45bbc1dbf199d119c23489294cd2aad435c62a5b3b8`,
  hash verificado na gravação e boot identificado pelo ELF; sem escrita posterior.
- Rollback: `rollback/firmware_06.bin` no pacote, SHA conferido idêntico ao
  candidato validado. Evidência física6 incluída; backup integral anterior
  preservado no preflight, conforme registro de bancada.
- PASS: partições byte-idênticas ao candidato6. Não gravar bootloader,
  partições, NVS ou backup integral como parte da proposta app-only.

## Validação executada

PASS: `C:/.platformio/penv/Scripts/platformio.exe run -d smk-p4 -e guition-standalone-blueprint-8-bench`
(exit0,642.87s). PASS: `python smk-p4/tests/check_blueprint_build.py --voices 8 --environment guition-standalone-blueprint-8-bench --musical`.
PASS: `C:/.platformio/penv/Scripts/python.exe -m esptool --chip esp32p4 image-info smk-p4/.pio/build/guition-standalone-blueprint-8-bench/firmware.bin`.
Checksum0xc1 válido; validation hash
`d9aaa1a0621852bd298b763aac8ede3e2a714218919d33e05c849bd03af592db` válido.
ELF SHA256 `4f6baa482dd344d305564cffa2c31b55253d96e55c83ff74e6eb32074641d274`.
Descritor legado smk-p4-gate0/0.1.0; identificar perfil por hash e VX, não pelo nome.

PASS: comparação de sdkconfig.h com o pacote6: somente seleção6→8 mudou.
Mesmos PCM50/51/52, USB HS0, gates ligados, diagnóstico PCM/self-test RAM
 desligados,48000Hz/256frames, pool LVGL128KB e correção FM exclusiva.
DRAM0+DRAM1 data/BSS169904B (+24B versus6); IRAM101888B, igual6.
Resumo PIO RAM30988B omite DRAM1; não representa a estática total.
A aplicação ocupa16bytes a mais que6. Recursos dinâmicos e carga DSP exigem
medição na placa; build e testes host8 anteriores não são prova física.

## Arquitetura, riscos e próximo passo

A preparação adiciona somente ambiente de build e aceitação no auditor, usando
perfil8 estático existente. Nenhum DSP, AMY core, GPIO, USB/I²S, ganho, FX,
patch ou UI foi alterado. O padrão seguro continua quatro e seis permanece
instalado/validado. Risco principal: maior custo DSP e memória em oito; guardas
podem recusar inicialização e não há redução automática de vozes.

O item14 do pedido exige "obter aprovação para substituir a imagem concreta".
A autorização anterior referia-se ao candidato6. Este pacote não foi gravado.
Após autorização própria: conferir hashes/unidade, gravar somente0x10000,
smoke com volume baixo/VX0/8, oito notas, rejeição da nona, Panic, ambos patches,
sustain/bend/modulação/retrigger, Touch/USB, hot-plug, UI e janela>=30min.
Se houver falha ou margem insuficiente, retornar ao seis já qualificado.
