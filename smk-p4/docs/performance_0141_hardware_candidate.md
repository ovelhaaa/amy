# SMK-P4 0.14.1 — imagens do incremento legado

**NO-GO para qualificação física da 0.14.1.** As imagens abaixo correspondem ao painel legado, sem o editor de rotas do escopo de release.

Código compilado: 203a77ff4ecd8113eac23eaa84d5c973bcc98e07. Os commits posteriores de documentação não entram no build.

| Perfil | Bytes do binário | SHA-256 | Flash reportada | RAM estática reportada |
|---|---:|---|---:|---:|
| guition-editor-8 | 1.264.080 | `40ce04e677357986154f6b6ab77a2c35bc7738c7363c20b89bb8f25cbc456980` | 1.263.346 B | 41.240 B |
| guition-editor-8-bench | 1.365.040 | `319efb677a066f28fbd42e1437c5464c74d86f9c42976432cfed30c53b4e9376` | 1.364.324 B | 41.624 B |

Os dois builds e as duas auditorias de configuração retornaram PASS. PSRAM livre, RAM interna livre, latência, underruns e deadlines na placa estão sem medições.

Os binários estão em `smk-p4/.pio/build/<perfil>/firmware.bin`. A [matriz de aceitação](performance_0141_test_matrix.md) registra o ensaio físico pendente. Nenhuma imagem foi gravada na placa. Se houver qualificação autorizada posteriormente, preservar a imagem e a NVS da unidade antes do ensaio. Para voltar ao estado exato anterior, usar esse par de backups. Qualquer gravação, inclusive de rollback, requer autorização vinculada ao SHA-256 da imagem.
