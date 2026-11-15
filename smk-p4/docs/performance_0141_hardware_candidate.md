# SMK-P4 0.14.1 — candidatos Guition

**PASS para gravação, boot e admissão do perfil bench com a correção de RAM.** Ensaio musical sob carga permanece pendente.

Código de firmware compilado em 650ab199f76787781d70d487e78e21072773d296. Os commits posteriores de testes ou documentação não entram nesses binários.

| Perfil | Bytes do binário | SHA-256 | Flash reportada | RAM estática reportada |
|---|---:|---|---:|---:|
| guition-editor-8 | 1.300.624 | `f5520ea2b5bfaf99adc6e48476b079e8ef089228e9a55fe187368cfb9db72337` | 1.299.892 B | 51.032 B |
| guition-editor-8-bench | 1.401.648 | `664f2c3488f6546665c1d0cf56b91c54a0988768da1440b1d8b57bcd46bfcc8f` | 1.400.936 B | 51.416 B |

Os perfis preservam oito vozes, 48 kHz, 256 frames e a partição factory de 3 MiB. A configuração indica flash de 16 MB e PSRAM Hex a 200 MHz. A telemetria de áudio sob carga e a margem de stack do owner permanecem pendentes. Nenhuma alteração de pinagem, partições ou hardware foi necessária.

A imagem bench deste ensaio foi preservada em `build/p4-0141-com13-bench/firmware-verified.bin` antes da recompilação. Antes do ensaio, guardar firmware e NVS da mesma unidade. O retorno à 0.14 ou 0.13.1 usa o par pré-ensaio verificado. Essas versões não leem V4. Qualquer gravação, inclusive de rollback, exige autorização vinculada ao SHA-256 da imagem. A NVS conferiu pelo SHA-256 após a gravação inicial.

[Procedimento de hardware](performance_0141_test_matrix.md) · [Persistência e rollback](performance_0141_persistence.md).

## Gravação e boot em 2026-11-12

- **PASS — gravação e verificação.** Guition ESP32-P4 rev. 1.3, COM13, USB 303A:1001, serial 80:F1:B2:D3:CC:86. Autorização vinculada ao SHA-256 do perfil bench listado acima. Esptool 5.1.0 gravou e verificou 1.401.648 bytes somente em 0x10000.
- **PASS — backup e NVS.** Leitura integral de 16 MiB em build/backups/p4-0141-com13-preflash. A tabela de partições do backup coincide com a do candidato. O SHA-256 da NVS lida após a gravação é 2e9e858b22c13f60d3c8c9a8bafc2a23f85925b69a1be43bf176e0429b3bd04c, igual ao backup.
- **PASS — boot e inicialização do display.** Captura de 30 s, 14.302 bytes. O GT911 e o driver 800×480 inicializaram. O chip reportou 32 MiB de PSRAM a 200 MHz e o teste de memória passou.
- **FAIL — admissão de áudio.** O owner encontrou 348.867 bytes livres de RAM interna, 3.389 abaixo do mínimo de 352.256. O maior bloco livre era 245.760 bytes, acima do mínimo de 196.608. A admissão retornou ESP_ERR_NO_MEM, o engine permaneceu ready=0, os contadores de blocos e frames ficaram em zero e o USB Host não iniciou.
- **NOT RUN — ensaio musical.** Áudio físico, MIDI, interação de toque, rotas e Save/Recall na placa permanecem pendentes. O perfil editor sem bench não foi gravado.

Comandos usados no diretório raiz, com o Python do PlatformIO e esptool 5.1.0:

```text
python -m esptool --chip esp32p4 --port COM13 --baud 460800 write-flash 0x10000 smk-p4/.pio/build/guition-editor-8-bench/firmware.bin
python -m esptool --chip esp32p4 --port COM13 --baud 460800 verify-flash 0x10000 smk-p4/.pio/build/guition-editor-8-bench/firmware.bin
python -m esptool --chip esp32p4 --port COM13 --baud 460800 read-flash 0x9000 0x6000 build/p4-0141-com13-bench/nvs-after.bin
python smk-p4/tests/capture_serial.py --port COM13 --output build/p4-0141-com13-bench/boot.log --seconds 30 --reset
```

Logs locais em build/p4-0141-com13-bench. SHA-256 de boot.log: e96271ac7c5d205e35f89de757a3a35a8b0f917c0160bb9178a923b17414b632. O próximo passo é reduzir a pressão de RAM interna e manter a guarda de admissão.

## Correção de RAM interna

O backend de controle do Sound Editor passa a ocupar 9.928 bytes de PSRAM reservados uma vez na inicialização. Ele contém estado de edição e presets e pertence à task de controle. A alocação alternativa em RAM interna acontece antes da admissão para que o custo entre na medição. A guarda de 352.256 bytes permanece ativa. O runtime AMY, o mailbox, as tarefas e os buffers de áudio permanecem no caminho anterior.

| Perfil | RAM estática anterior | RAM estática nova | Bytes do firmware | SHA-256 |
|---|---:|---:|---:|---|
| guition-editor-8 | 51.032 B | 41.100 B | 1.300.864 | `451c48c999044192cc08b56e3ba77ba9eb984ce73401b8dc4e7be32d09d7953b` |
| guition-editor-8-bench | 51.416 B | 41.484 B | 1.401.904 | `eb40674e617c8122ed61c705d1d9733e282431c5b255298ce53695ac8e0ae86c` |

A redução estática de 9.932 bytes excede o déficit observado de 3.389 bytes. A admissão real depende da medição de heap no boot. O ELF confirma a remoção do objeto estático do backend.

**PASS** para `powershell -File smk-p4/tests/run_host.ps1`, os dois builds `pio run -d smk-p4 -e <perfil> -j 8` e as duas auditorias `check_blueprint_build.py`. Os novos binários ficam em `smk-p4/.pio/build/<perfil>/firmware.bin`. A validação física da nova imagem está abaixo.

## Ensaio da correção de RAM

- **PASS — gravação e verificação.** Guition ESP32-P4 rev. 1.3 em COM13, com o SHA-256 eb40674e617c8122ed61c705d1d9733e282431c5b255298ce53695ac8e0ae86c autorizado. Esptool verificou 1.401.904 bytes da aplicação em 0x10000.
- **PASS — NVS preservada.** Leituras anterior e posterior têm SHA-256 2e9e858b22c13f60d3c8c9a8bafc2a23f85925b69a1be43bf176e0429b3bd04c, igual ao backup original.
- **PASS — admissão do AMY.** Backend de controle em PSRAM. RAM interna livre na admissão de 358.819 bytes, contra 348.867 antes da correção. Margem de 6.563 bytes sobre a guarda de 352.256. Maior bloco de 245.760 bytes.
- **PASS — I²S em repouso.** Captura de 30 s com 1.253.632 frames escritos na última amostra. Render médio de 219 µs e máximo de 362 µs. Zero deadlines de render e computação, OOM, erros de escrita e timeouts. Stack livre do owner de 1.188 bytes, RAM interna livre de 134.219 bytes e PSRAM livre de 30.482.328 bytes. Underruns físicos não instrumentados.
- **PASS — USB Host iniciado.** A captura indica estado de espera, com zero conexões. **NOT RUN** para enumeração do controlador, audição analógica, toque, execução sob carga e Save/Recall físico.

A imagem corrigida permanece instalada. Logs locais em build/p4-0141-com13-bench-ram. SHA-256 de boot.log: 5035b2b84e90a866a718c8a4d4f9040f0c76148a7abcf7c972a0e98efd3955be.
