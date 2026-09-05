# AMY: aviso de falta de osciladores no boot

## Causa reproduzida

O adaptador habilitava `features.default_synths` com um pool de 120 osciladores.
`amy_start()` chamava `amy_default_synths()` em `src/api.c`, criando os quatro
instrumentos de demonstração antes de devolver o controle ao adaptador:

| Synth | Uso | Osciladores solicitados | Alocados na reprodução |
| --- | --- | ---: | ---: |
| 0 | Bleep, uma voz | 1 | 1 |
| 10 | Bateria PCM, uma voz com 32 sons | 32 | 32 |
| 2 | DX7, seis vozes de oito osciladores | 48 | 48 |
| 1 | Juno, seis vozes de seis osciladores | 36 | 30 |
| Total | | 117 | 111 |

O alocador em `src/patches.c` exige um bloco contíguo por voz e alterna o início
da busca entre a metade do pool e o índice zero. A combinação acima deixava
blocos livres em 56–59 e 115–119: nove osciladores livres, mas nenhum bloco de
seis para a última voz Juno. O resultado exato foi:

```text
cannot find 6 oscs for patch 0 for voice 13. not setting this voice
```

Trata-se de fragmentação do pool lógico de osciladores, não de uma falha de
alocação do heap ou evidência de PSRAM insuficiente. AMY ainda registrava seis
vozes no instrumento, embora uma delas não tivesse osciladores. Por isso,
consultar apenas o número de vozes não detectava a inicialização incompleta.

A liberação posterior dos synths 2 e 0 chegava tarde: não refazia a voz que
falhara. O PatchManager recarregava depois o patch principal e podia mascarar
o problema; o aviso, entretanto, correspondia a uma falha real durante o boot.

## Correção implementada

- Desabilitar os instrumentos de demonstração e criar explicitamente o synth 10
  com o patch 258, seguido do synth 1 com o patch inicial configurado.
- Alocar primeiro o bloco permanente da bateria. Preservar seus mapeamentos GM,
  flags, bus 1 e efeitos desativados, bem como o bus 0 e o atraso de 4 ms do synth 1.
- Preservar o hook MIDI que o boot padrão instalava para CC70/CC71 no canal 1.
  Isso mantém a compatibilidade existente e não define um novo perfil do SMK25.
- Compartilhar os limites e patches de boot entre aplicação e adaptador por
  `components/audio/include/synth_config.h`, incluído por `main/app_config.h`.
  O adaptador passa a respeitar as oito vozes já definidas nessa configuração,
  em vez das seis embutidas na demonstração upstream.
- Verificar a propriedade dos osciladores de cada voz no boot. Se a alocação
  estiver incompleta, retornar falha, parar a saída I²S e encerrar a inicialização
  antes de criar a tarefa de áudio e os subsistemas dependentes.

Com a configuração atual, o adaptador usa 48 osciladores para as oito vozes Juno
e 32 para a bateria: **80 utilizados e 40 livres**. O PatchManager pode depois
alterar essa contagem conforme o patch selecionado; os 40 livres descrevem o
fim de `AmyAdapter::begin()`, não uma reserva permanente durante a execução.

O núcleo AMY não foi modificado. A verificação usa tabelas de propriedade já
expostas pelo AMY e fica encapsulada no adaptador, antes da tarefa de áudio.
Não adiciona alocações de heap, logs ou buscas ao caminho de renderização.
Não houve novas dependências, alterações de GPIO, USB, display ou DAC.

## Validação automatizada

Na raiz do repositório, com PowerShell e GCC/G++ MinGW no PATH:

```powershell
./tests/run_smk_amy_boot.ps1 -Legacy
./tests/run_smk_amy_boot.ps1
```

O script compila o adaptador real junto ao núcleo AMY, a 48 kHz e blocos de
256 frames. Somente as interfaces de plataforma são substituídas para execução
no computador; o alocador, os patches, o processamento MIDI e o DSP são reais.
Os objetos, executável e `stderr.log` ficam em `build/smk_amy_boot/`.

O modo `-Legacy` deve reproduzir o aviso e confirmar 117 solicitados/111 alocados.
Sem esse argumento, qualquer diagnóstico do AMY em stderr falha o teste. São
verificados: todas as vozes completas, ausência dos synths 0/2, buses, atraso de
Note On, silêncio inicial, áudio por voz, Note Off e velocity zero, 256 trocas
Juno/DX7 com 10/8 vozes respectivamente, bateria, Panic e CC71.

Build do firmware com a instalação local identificada:

```powershell
C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-devkitc-1
```

Resultados desta correção:

- `PASS`: reprodução do aviso no boot padrão anterior.
- `PASS`: regressão com o adaptador corrigido e DSP real no host.
- `PASS`: build ESP32-S3, ESP-IDF 5.5.1; imagem gerada sem gravar a placa.
- `MANUAL`: áudio físico, underruns, latência, reconexão USB e consumo de heap
  interno/PSRAM exigem a placa. Os testes no host não medem esses resultados.

O compilador MinGW emite avisos preexistentes de conversão de ponteiros para
`long` nos diagnósticos do núcleo AMY; eles não são avisos de alocação em runtime.

## Próximo passo: validação na placa

1. Gravar o firmware pelo procedimento habitual, abrir o monitor a 115200 baud
   e reiniciar dez vezes. Não deve aparecer `cannot find ... oscs`.
2. Confirmar os logs do adaptador antes da tarefa de áudio:
   `Synth 10 ready: voices=1 oscs/voice=32 allocated=32 pool=120` e
   `Synth 1 ready: voices=8 oscs/voice=6 allocated=48 pool=120`.
3. Com o patch inicial, tocar pelo menos 16 notas em sequência e acordes,
   soltar todas as teclas e conferir que nenhuma nota é omitida ou fica presa.
4. Acionar bumbo, caixa e hi-hat; alternar patches Juno e DX7 e repetir.
   Confirmar que a bateria continua respondendo.
5. Testar velocity, pitch, modulation e sustain com o perfil confirmado do
   controlador. Desconectar o USB enquanto segura notas; esperar silêncio,
   reconectar e confirmar que as teclas voltam a responder.
6. Tocar por pelo menos 20 minutos e registrar underruns, máximo de renderização,
   RAM interna/PSRAM livres e falhas de USB antes/depois. Esperado: zero
   underruns em uso normal e nenhum aviso de alocação ou nota presa.

Continuam valendo as configurações de hardware existentes; revisão da placa,
VBUS e straps do PCM5102A não foram inferidos ou validados nesta correção.
Os testes de troca de patch foram sequenciais no host: não certificam a
sincronização entre tarefas nem todos os patches personalizados. Também não
resolvem a limitação geral do alocador upstream quando um futuro patch exceder
o pool; esse caso requer um orçamento de vozes específico, fora deste ajuste de boot.
