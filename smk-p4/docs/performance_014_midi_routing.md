# SMK-P4 0.14 — rota MIDI

O parser publica CC válidos do cabo 0 na fila USB → UI com 32 elementos. Cada evento carrega canal, cabo, timestamp, número, valor e gerações da conexão e da fila musical. A fila descarta o evento mais novo ao encher e contabiliza em control_dropped no diagnóstico USB. A UI verifica ambas as gerações e drena até 32 eventos por ciclo. Cada knob publica no máximo o último movimento elegível do ciclo.

CC1 mantém modulação musical e pode ser observado para Learn autorizado. CC64, CC120 e CC123 permanecem na fila musical. CC genéricos deixam a fila musical, preservando espaço para as liberações. Pitch Bend, notas, reset e Panic mantêm suas rotas existentes. A fila de diagnóstico permanece independente. A UI é a única produtora dos pedidos de edição.
