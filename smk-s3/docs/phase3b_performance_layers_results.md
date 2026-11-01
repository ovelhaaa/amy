# Fase 3B — performance e layers (entrega parcial segura)

## A — auditoria antes das alterações

Base: `6fd57c5a3673a6d77c65d373c29f1c1004738970`, merge da Fase 3A,
branch inicial main, árvore limpa. Branch de trabalho:
`feature/smk-s3-performance-layers`.

Leitura: AGENTS.md, specs.md, README, manifests/builds/CI, relatórios 2.5/3A,
amy_boot_allocation, amy_ownership, FX M3, smoke 20261101, adapter/mailbox,
NoteOwnership, app_main, PatchManager, StorageWorker, formatos v6 e UIManager,
alocadores src/instrument.c e src/patches.c e suites host.

O README conserva conflitos antigos AMY/P4; o projeto isolado smk-s3 continua
sendo a referência de build. Specs pede 284×76; o perfil local usa 240×240.
Nenhuma decisão de hardware será inferida. O pedido explícito desta fase
autoriza a infraestrutura de layers, mas permite entrega parcial segura.

### Pré-requisitos e gate

- Comprovado: Fase 3A integrada; identidade origem/instância/canal/entrada/token;
  64 identidades, fan-out 4, sustain por canal, releases capturados no onset,
  Stop seletivo e Panic geracional. Destinos aceitos hoje: synth 1 e drums 10.
- PASS antes de editar: run_smk_safety.ps1 (inclui timing) e
  run_smk_amy_boot.ps1, engine e owner reais, inclusive 256 trocas de patch.
  Logs locais ignorados: build/phase3b_baseline_safety.log e
  build/phase3b_baseline_amy.log. Demais regressões em execução durante auditoria.
- Não atendido: bancada/soak musical da Fase 3A. O smoke 20261101 registrou
  38.479 bytes internos livres e maior bloco de 21.504 bytes em repouso, mas
  também 63 lacunas PCM, três deadlines perdidos e render máximo 241.039 us,
  de origem não determinada. Não é orçamento de performance aprovado.
- Bloqueio de integração: executePreset faz AllNotesOff global; fila de preset
  avança geração global. Mono stack/modo e snapshots FM são únicos. Rotas
  convertem todo destino não-drums para synth 1. Habilitar synth 2 sem separar
  esses estados corromperia isolamento entre layers.
- Bloqueio de recursos: pool 120; bateria patch 258 reserva 32; Juno patch 0
  usa seis oscs/voz, DX7 oito. A8 Juno+B8 DX7+drums =144; dois DX7 de oito
  vozes =160. Mesmo 4+4 DX7+drums =96 é apenas limite aritmético, não prova
  contiguidade, heap, PSRAM ou deadline. Alocador exige bloco contíguo por voz.
- instrument_init/voice_fifo_init alocam heap; troca de patch pode materializar
  recursos na fase de comandos do owner. Não há transação/ACK de capacidade
  que garanta habilitação B sem alocação tardia. Não duplicar esse caminho.

### Orçamento e fronteiras atuais

Synth 1: principal, bus 0, oito vozes no boot (patch pode mudar orçamento),
delay de stealing 4 ms, última nota em mono/legato. Synth 10: container de
uma voz/32 osciladores PCM, bus 1, IGNORE_NOTE_OFFS e mapa GM. Limite upstream
por instrumento: 32 vozes; não equivale a 32 vozes disponíveis neste produto.
AMY faz stealing e agrega pitches iguais por instrumento. Referências lógicas
do adapter não medem vozes físicas nem caudas de release.

FX/EQ/drive são compartilhados por bus; bus 1 é seco. A e B no bus 0
compartilhariam FX. Macros/patch v6, scenes, perfis MIDI e UI hoje operam sobre
um principal. StorageWorker usa snapshots copiados e único worker de I/O;
UI/console ainda têm acessos paralelos a estado de controle (risco prévio).

### Escopo seguro escolhido antes de editar

Entregar A/B portáteis e testes: configuração validada, comandos por valor,
snapshot copiado, roteador limitado e codec explícito versionado/CRC. Arquivos
previstos: novos headers em audio/include (junto ao NoteRoute), teste/script,
CI e este relatório. Sem mudança no core AMY, hardware, filas ou presets
existentes. O firmware permanece Single; não expor controles B que não possam
ser aplicados. C e integração runtime de D ficam bloqueadas pelas condições
acima. Não declarar a fase completa.

Tempo real: roteador tem duas iterações, sem heap/I/O/locks; codec não pertence
ao render. Compatibilidade: preservar APIs/formatos atuais, nenhuma dependência.
Aceitação deste incremento: modelo/codec/routing host, regressões existentes,
AMY real preservado, builds 128/256 e diff limitado. Não alegar áudio dual.
