# Guition JC4880P433 — qualificação da 0.17B

**Qualificação física PENDING.** O pinout, USB, VBUS, I²S e partições seguem o candidato Guition da [0.17A](pattern_library_017a_hardware_candidate.md). Consulte o [release gate de software](performance_scenes_017b_test_results.md) para o resultado dos testes.

Qualificar na Guition com SMK25 e saída I²S, após autorização de gravação e identificação do binário pelo SHA-256.

1. Preencher os oito slots, reiniciar e confirmar nomes e referências. Repetir com NVS perto da capacidade e verificar que Patterns e presets V4 continuam acessíveis.
2. Alternar duas cenas com PATCH, PATTERN, FX e TEMPO. Testar Immediate e Next Bar a 40, 120 e 240 BPM. Comparar o instante de aplicação com a fase global e testar Apply Scene Tempo desativado.
3. Segurar um acorde com sustain enquanto o sequenciador e o Arp tocam. Trocar só PATTERN e conferir que o teclado permanece audível. Trocar PATCH, ligar e desligar sustain, depois disparar Stop, Panic e desconectar o SMK25 durante uma troca pendente. Conferir que os indicadores de notas e transporte se recuperam e que não sobram notas presas.
4. Sob carga de oito vozes, registrar render médio e máximo, underruns, jitter, tempo de aplicação, RAM interna, heap e high-water do `amy_owner`. Comparar com a 0.17A na mesma placa e configuração.
5. Conferir cartões, máscara efetiva, falha de Pattern ausente, feedback de fila e acesso ao Panic durante o modal na tela física 800×480. Usar os atalhos numerados no modo Macro Focus.
6. Após cada falha, conferir a máscara aplicada e a última referência de cena confirmada. Se o loader falhar, escolher explicitamente um patch válido e testar um novo Note On/Off antes de retomar o ensaio.

## Builds da fundação

| Perfil | Resultado | RAM PIO | Flash PIO | SHA-256 |
| --- | --- | ---: | ---: | --- |
| guition-editor-8 | PASS | 45.968 B (+544 B sobre 0.17A) | 1.404.242 B (−428 B) | `DB5A2196FF398F0B890D142F5E6A3EBC0E02C6070B1731D72E1F91DFE5F5C78B` |
| guition-editor-8-bench | PASS | 45.816 B (−544 B sobre 0.17A) | 1.506.160 B (−428 B) | `F8EA364124F66E87079E375758E03F12517FF6B760F041D6860ACEB9035F0311` |

Os deltas são em relação aos builds publicados da 0.17A. A aprovação dos builds cobre o firmware atual e o hardening de Patterns. A persistência de cenas ainda é exercitada em host. Nenhuma gravação na Guition foi executada.

## Builds do recall parcial

| Perfil | Resultado | RAM PIO | Flash PIO | SHA-256 |
| --- | --- | ---: | ---: | --- |
| guition-editor-8 | PASS | 47.500 B | 1.410.838 B | `538CDAE74D9979698906E86D5002C19355A6E468FEB16AD9C59F19CE18E49ED0` |
| guition-editor-8-bench | PASS | 47.360 B | 1.512.772 B | `5E9CFBEE21166FAC505CB4999351AC5C48EA08476EAB3FC3149BC4C4CC1397CC` |

Frente à fundação da 0.17B, os perfis normal e bench cresceram 1.532 B e 1.544 B em RAM estática e 6.596 B e 6.612 B em Flash. Frente à 0.17A, cresceram 2.076 B e 1.000 B em RAM estática e 6.168 B e 6.184 B em Flash. A qualificação física permanece PENDING.

## Builds da integração final

Comandos executados com `C:/.platformio/penv/Scripts/pio.exe`, usando `run -d smk-p4 -j 8 -e` para cada perfil.

| Perfil | Resultado | RAM PIO | Flash PIO | SHA-256 de firmware.bin |
| --- | --- | ---: | ---: | --- |
| guition-editor-8 | PASS | 61.608 B | 1.449.948 B | `BE6601DE3C2F088264D59497973007975C27CA32A35B091DACAD2E1A7BC64629` |
| guition-editor-8-bench | PASS | 61.992 B | 1.551.848 B | `042AC632749D0694835CA47F036FF0A9FC85041F60FF84C8C930EA7415B05AA2` |

Em relação ao HEAD inicial da PR, editor e bench cresceram 14.108 B e 14.632 B em RAM e 39.110 B e 39.076 B em Flash. Em relação à 0.17A, os deltas foram +16.184 B e +15.632 B de RAM e +45.278 B e +45.260 B de Flash. A maior parte do aumento de RAM vem do catálogo de oito Scenes e dos buffers fixos de captura e comparação.

O código de integração não altera o esquema de partições nem as configurações de pinos. MANUAL na Guition com SMK25: duração de silêncio na troca, underruns e refill DMA sob oito vozes, headroom de NVS, heap LVGL físico e high-water real da tarefa.
