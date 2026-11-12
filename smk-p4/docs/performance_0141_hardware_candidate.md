# SMK-P4 0.14.1 — candidatos Guition

**GO para iniciar a qualificação física mediante autorização vinculada ao hash.** Validação em placa permanece MANUAL / NOT RUN.

Código de firmware compilado em 650ab199f76787781d70d487e78e21072773d296. Os commits posteriores de testes ou documentação não entram nesses binários.

| Perfil | Bytes do binário | SHA-256 | Flash reportada | RAM estática reportada |
|---|---:|---|---:|---:|
| guition-editor-8 | 1.300.624 | `f5520ea2b5bfaf99adc6e48476b079e8ef089228e9a55fe187368cfb9db72337` | 1.299.892 B | 51.032 B |
| guition-editor-8-bench | 1.401.648 | `664f2c3488f6546665c1d0cf56b91c54a0988768da1440b1d8b57bcd46bfcc8f` | 1.400.936 B | 51.416 B |

Os perfis preservam oito vozes, 48 kHz, 256 frames e a partição factory de 3 MiB. A configuração indica flash de 16 MB e PSRAM Hex a 200 MHz. RAM interna e PSRAM livres, margem de stack, underruns, latência e deadlines na placa ainda precisam de medição. Nenhuma alteração de pinagem, partições ou hardware foi necessária.

Os binários ficam em `smk-p4/.pio/build/<perfil>/firmware.bin`. Antes do ensaio, guardar firmware e NVS da mesma unidade. O retorno à 0.14 ou 0.13.1 usa o par pré-ensaio verificado. Essas versões não leem V4. Qualquer gravação, inclusive de rollback, exige autorização vinculada ao SHA-256 da imagem. Nenhum firmware foi gravado e nenhuma NVS foi apagada.

[Procedimento de hardware](performance_0141_test_matrix.md) · [Persistência e rollback](performance_0141_persistence.md).
