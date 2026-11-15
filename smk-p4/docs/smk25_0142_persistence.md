# Persistência do perfil MIDI 0.14.2

A NVS continua usando o namespace perf014midi e a chave profile. A leitura valida integralmente V1 (36 bytes), V2 (68) e V3 (100), incluindo CRC32, tamanho e versão, os IDs de destino e a unicidade das fontes ativas. V1 e V2 carregam como P4 Custom e conservam as fontes, o banco selecionado e os destinos previamente armazenados.

V3 conserva os campos de V2 nas posições originais até o byte 63. O byte 7 identifica o perfil legado. Os bytes 64–87 guardam oito fontes adicionais, 88–95 os destinos personalizados do banco físico B e 96–99 o CRC. O perfil legado seleciona também os bindings fixos de pads e transporte. O estado físico e o Pickup ficam em RAM.

RESTORE SMK25 DEFAULTS pede confirmação e restaura o perfil ativo em RAM. LOAD SAVED PROFILE recupera a versão em NVS. SAVE PROFILE usa a operação existente de espera por áudio ocioso, grava V3 e exige leitura de volta válida e equivalente. A configuração de fábrica é utilizável a partir do primeiro boot, inclusive com NVS indisponível ou registro inválido.

Em rollback, versões a partir de 0.14.1 anteriores a 0.14.2 leem V1/V2. Preserve um backup da NVS antes de sobrescrever um perfil com V3 caso o firmware anterior precise recuperar seus bindings. User Presets e transações de síntese/FX são separados do perfil MIDI.
