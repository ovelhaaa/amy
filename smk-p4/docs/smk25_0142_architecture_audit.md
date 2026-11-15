# Auditoria MIDI do SMK-P4 0.14.2

O SMK-S3 registra knobs CH1 CC21–36, pads CH10 notas 36–51 e transporte CH1 CC114/115/117. KNOB-B e PAD-B alteram o controlador localmente. A captura anterior do P4 registra VID/PID 4353:4B4D.

O cliente USB é o produtor da fila musical e da fila de controle. O owner de áudio consome a primeira, com geração própria para cancelar só as vozes USB. A UI consome a segunda e aplica parâmetros pelo backend existente. Note Off, Sustain, Pitch Bend e CC1 permanecem no caminho musical. Pulsos de Stop reconhecidos podem invalidar somente a geração musical USB no produtor.

Pads requerem identidade USB, perfil ativo, cabo 0, CH10 e nota dentro da faixa. A classificação de cada nota deve permanecer estável até a soltura. Eventos de UI devem carregar a geração da conexão e da fila USB. A perda de qualquer evento de controle invalida gestos pendentes. A UI valida também a geração do owner antes de executar atalhos.

Os perfis P4 V1/V2 armazenam oito fontes e quatro bancos de destino. O perfil legado usa dois conjuntos independentes de oito fontes e destinos. A extensão persistente precisa de uma versão própria.

Referências do SMK-S3 usadas na auditoria: [perfil documentado](../../smk-s3/docs/controller_profile_smk25.md), [bindings](../../smk-s3/components/midi/controller_profile.cpp), [modelo](../../smk-s3/components/midi/include/controller_profile.h), [MIDI Learn](../../smk-s3/components/midi/midi_learn.cpp), [parser](../../smk-s3/components/midi/midi_parser.cpp) e [tela de pads](../../smk-s3/components/ui/screens/pad_screen.cpp). O alvo P4 desta integração usa a Guition 800×480 e a configuração elétrica do perfil bench do P4.
