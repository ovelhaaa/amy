# SMK-P4 0.14.1 — imagens do incremento de rotas no owner

**NO-GO para qualificação física da 0.14.1.** As imagens incluem o painel legado e a composição dinâmica no owner. O editor visual e o V4 seguem pendentes.

Código compilado: 9299ba73fa2848ac8bf17cc120173e9e8ec78271. Os commits posteriores de documentação não entram no build.

| Perfil | Bytes do binário | SHA-256 | Flash reportada | RAM estática reportada |
|---|---:|---|---:|---:|
| guition-editor-8 | 1.271.072 | `c47172dac23470eadef737e037f5f7289f1ed4b7c10d4ef63e50382cc3a1784b` | 1.270.338 B | 48.200 B |
| guition-editor-8-bench | 1.372.096 | `e3f8ca8f109d0d8836ea6ae312c64797c4afdd7afccf1a3fc51aeb79e23d57c2` | 1.371.376 B | 48.584 B |

Os dois builds e as duas auditorias de configuração retornaram PASS. A RAM estática aumentou 6.960 B em cada perfil. Os perfis configuram flash de 16 MB e PSRAM Hex a 200 MHz. PSRAM livre, RAM interna livre, margem de stack, latência, underruns e deadlines na placa estão sem medições.

Os binários estão em `smk-p4/.pio/build/<perfil>/firmware.bin`. A [matriz de aceitação](performance_0141_test_matrix.md) registra o ensaio físico pendente. Nenhuma imagem foi gravada na placa. Se houver qualificação autorizada posteriormente, preservar a imagem e a NVS da unidade antes do ensaio. Para voltar ao estado exato anterior, usar esse par de backups. Qualquer gravação, inclusive de rollback, requer autorização vinculada ao SHA-256 da imagem.
