# Plano inicial de navegação — display 240 × 240 e SMK25 V2

Status: implementação incremental. O anel Home → Sequencer → Pads → Scenes e o
retorno à Home a partir de páginas externas já estão implementados em B5/B6.
O hub de utilidades ainda é uma proposta; atualmente Pad B7 abre MIDI Learn.

## 1. Entradas disponíveis

### Confirmadas pelo perfil e pelo firmware atual

| Controle físico | Evento disponível | Uso seguro na navegação |
|---|---|---|
| 8 knobs, banco A | CC 21–28 no perfil padrão | edição direta de oito parâmetros |
| 8 knobs, banco B | CC 29–36 no perfil padrão | segundo conjunto de oito parâmetros |
| 8 pads, banco A | notas 36–43 no canal 10 | disparo musical ou oito escolhas contextuais |
| 8 pads, banco B | notas 44–51 no canal 10 | atalhos e ações contextuais |
| Play | CC 114 no perfil padrão | transporte global e confirmação positiva contextual |
| Stop | CC 115 no perfil padrão | parar, cancelar e Panic por pressão longa |
| Rec | CC 117 no perfil padrão | gravação e funções contextuais do sequenciador |

Os números acima pertencem ao perfil padrão e continuam substituíveis por MIDI Learn. A navegação deve trabalhar com ações semânticas, nunca com CCs ou notas codificados diretamente na UI.

### Controles locais silenciosos confirmados

- `ARP`, `SC/CH`, `KNOB-B`, `PAD-B`, `BT`, `Oct+` e `Oct−` não emitem MIDI.
- Os oito knobs emitem 16 comandos distintos ao considerar os bancos A e B.
- Os oito pads emitem 16 comandos distintos ao considerar os bancos A e B.
- O firmware só infere o banco depois de receber o CC ou Note correspondente.

Consequência: nenhum dos sete controles silenciosos pode ser modificador,
atalho, confirmação ou requisito de navegação. A troca de banco não é mostrada
antecipadamente; a UI mostra `A`, `B` ou `?` conforme o último evento recebido.

## 2. Modelo proposto

### Anel de performance

O ciclo principal deve conter apenas páginas usadas enquanto se toca:

1. Home
2. Sequencer
3. Pads
4. Scenes

- Pad B5: página anterior.
- Pad B6: próxima página.
- Pad B3: retorno direto à Home.

### Hub de utilidades

System, MIDI Monitor e MIDI Learn devem ficar fora do anel principal. Pad B7 abre o hub; dentro dele, pads A1–A3 fazem seleção direta. Isso reduz a quantidade de passos necessária para voltar a uma tela musical.

### Camada transitória

- Movimento de knob abre `ParameterOverlay` sobre a página atual.
- O overlay expira e devolve a página anterior sem alterar a posição no anel.
- Mensagens de salvar, conexão, erro e Panic usam a mesma camada, com prioridade maior.
- Um Note Off, Stop ou Panic nunca pode ser consumido pela interface.

## 3. Mapeamento global sugerido

| Ação semântica | Controle | Comportamento |
|---|---|---|
| Patch anterior/próximo | Pad B1 / B2 | atua em qualquer página não modal |
| Home | Pad B3 | saída de qualquer página ou cancelamento de overlay |
| Arpejador | Pad B4 | alterna estado; é a única entrada MIDI observável dedicada a essa função |
| Página anterior/próxima | Pad B5 / B6 | navega no anel de performance |
| Utilidades | Pad B7 | abre/fecha o hub de sistema e MIDI |
| Salvar | Pad B8 longo | 1,2 s; confirmação visual somente após sucesso |
| Iniciar/continuar | Play | global |
| Parar | Stop | global; também envia All Notes Off conforme o contexto existente |
| Panic | Stop longo | global e sempre prioritário |
| Gravar | Rec | global no sequenciador; fora dele pode abrir o sequenciador após validação de gesto |

## 4. Contexto do sequenciador

### Pad bank A

- Pads 1–8 alteram os oito passos da página ativa.
- O display mostra simultaneamente os 16 passos, mas destaca claramente a metade controlável.
- Note On fornece velocity para o passo e Note Off continua chegando ao caminho musical.

### Pad bank B

O comportamento já usado no firmware é adequado para uma camada contextual:

1. selecionar BD;
2. selecionar SD;
3. selecionar CH;
4. selecionar OH;
5. alternar passos 1–8 / 9–16;
6. mute da pista;
7. solo da pista;
8. limpar pista.

Limpar pista deve exigir confirmação: Pad B8 arma a ação e Play confirma; Stop cancela. Não executar limpeza instantânea.

### Transporte

- Play inicia ou continua.
- Stop para imediatamente.
- Rec alterna gravação/overdub.
- Rec seguido de pad A pode selecionar ou gravar a pista somente depois que a sequência for validada; não depender de chord/combinação simultânea.

## 5. Papel dos knobs

- Knobs devem editar oito parâmetros paralelos da página atual; não devem simular um encoder de navegação.
- Home Edit mostra os oito parâmetros em matriz 4 × 2.
- Sequencer pode mapear K1–K8 para volume, pan, pitch, decay, probability, ratchet, swing e length da pista selecionada.
- Todo contexto novo aplica pickup/soft takeover.
- A UI mostra qual knob se moveu, valor físico, valor salvo e direção de captura.

## 6. Regras de feedback

- Rodapé persistente mostra somente os atalhos válidos na página atual.
- Estados Play, Rec, USB e Panic têm posição fixa em todas as telas.
- Cor nunca é o único indicador: usar rótulo e forma junto com a cor.
- Ações destrutivas exigem estado `ARMADO`, confirmação explícita e timeout.
- Mudanças produzidas por pads devem gerar feedback em até um frame de UI, sem bloquear áudio.

## 7. Controles fora do contrato de navegação

- `ARP`, `SC/CH`, `KNOB-B`, `PAD-B`, `BT`, `Oct+` e `Oct−` são locais ao SMK25.
- A interface pode explicar essas funções, mas não pode reagir diretamente a elas.
- Caso uma revisão futura do controlador emita MIDI para algum desses controles,
  ela deverá usar um perfil de hardware versionado; o comportamento atual não
  deve mudar por inferência.

## 8. Critérios de aceitação

- Home alcançável com uma ação a partir de qualquer tela.
- Nenhuma função musical crítica depende de botão silencioso.
- No máximo duas ações para alcançar qualquer página de performance.
- MIDI Learn e diagnóstico não aparecem no ciclo normal de performance.
- Stop e Panic funcionam mesmo com modal, confirmação ou MIDI Learn aberto.
- Troca de banco desconhecida aparece como `?`, nunca como estado inventado.
- Nenhum Note Off é descartado por navegação.
- Teste de usuário consegue trocar patch, editar macro, abrir sequenciador, editar passo, salvar e voltar à Home sem consultar manual.
