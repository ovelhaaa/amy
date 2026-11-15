# Bancos físicos e Pickup no SMK-P4 0.14.2

O banco de knobs começa em estado desconhecido. O recebimento de CH1 CC21–28 indica A e CC29–36 indica B. O banco de pads é observado independentemente por CH10 Note 36–43 ou Note 44–51. O firmware recebe o resultado do banco do controlador pelas mensagens MIDI. KNOB-B, PAD-B, ARP, SC/CH, BT e Oct+/Oct− operam localmente.

No perfil legado, BANK alterna a visualização dos bindings A/B. As 16 fontes mantêm seus destinos ativos em ambas as visualizações. No perfil P4 Custom, BANK escolhe um dos quatro bancos virtuais A–D de oito destinos. MIDI Learn e a edição de destino continuam disponíveis por fonte.

O Pickup mantém valor físico e estado separados por binding. O estado inicial espera aproximação ou cruzamento do valor interno, com tolerância de três passos. Na captura, emite o valor interno. Movimentos posteriores enviam a nova posição e amostras repetidas são ignoradas. Patch, Recall, Compare/Revert, reconexão, nova associação e alterações de destino armam a captura novamente. Edições touchscreen também rearmam as fontes do parâmetro alterado.
