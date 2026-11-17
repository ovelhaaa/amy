# Guition JC4880P433 — qualificação da 0.17B

**NO-GO para qualificação como firmware de Performance Scenes.**
A fundação de persistência e validação roda em host. Recall ao vivo e UI permanecem PENDING. O pinout, USB, VBUS, I²S e partições seguem o candidato Guition da [0.17A](pattern_library_017a_hardware_candidate.md).

Quando o Audio Owner e a interface consumirem cenas, qualificar na Guition com SMK25 e saída I²S:

1. Preencher os oito slots, reiniciar e confirmar nomes e referências. Repetir com NVS perto da capacidade e verificar que Patterns e presets V4 continuam acessíveis.
2. Alternar duas cenas com PATCH, PATTERN, FX e TEMPO. Testar Immediate e Next Bar a 40, 120 e 240 BPM. Comparar o instante de aplicação com a fase global e testar Apply Scene Tempo desativado.
3. Segurar um acorde com sustain enquanto o sequenciador e o Arp tocam. Trocar só PATTERN e conferir que o teclado permanece audível. Trocar PATCH, ligar e desligar sustain, depois disparar Stop, Panic e desconectar o SMK25 durante uma troca pendente. Conferir que os indicadores de notas e transporte se recuperam e que não sobram notas presas.
4. Sob carga de oito vozes, registrar render médio e máximo, underruns, jitter, tempo de aplicação, RAM interna, heap e high-water do `amy_owner`. Comparar com a 0.17A na mesma placa e configuração.
5. Conferir cartões, máscara efetiva, falha de Pattern ausente, feedback de fila e acesso ao Panic durante o modal na tela física 800×480.
