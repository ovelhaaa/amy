# Integração ARP

O agendador usa a posição do clock global. O primeiro passo de uma frase vazia inicia na próxima fronteira de render, estabelecendo uma origem relativa à fase global. Os seguintes preservam o excedente de fração de bloco, as seis divisões, o swing e a ordem musical. O Gate de uma nota já emitida mantém o prazo que foi calculado no seu onset.

Stop, Panic e troca de patch limpam a frase. Com Latch, a troca de acorde mantém a progressão e a origem quando a frase continua. O teste real AMY compara pitch, velocity, onset e gate com uma partitura direta. O fixture ARP desligado continua com PCM idêntico à referência 0.15.

Clear Latch remove entradas já soltas e libera o grupo gerado. Teclas USB fisicamente pressionadas continuam na captura e notas Touch independentes mantêm seu ownership. Restart on New Chord reinicia a origem, cursor e semente aleatória na primeira tecla depois de soltar todas as teclas capturadas. Notas adicionais e Note On repetido no mesmo gesto não reiniciam a sequência. Sem Restart, a substituição de um acorde latched mantém a progressão.
