# SMK-P4 0.6 — resultados de polifonia

2026-11-04. Branch feature/smk-p4-polyphony-06 em worktree separado
build/worktrees/smk-p4-polyphony-06, base dd4753af. Checkout principal e outras
branches/backups preservados. Nenhum reset/rebase/force-push ou acesso físico.

## Gate0 — PASS software

Ver audio_quality_05a_verification.md. A comparação independente encontrou e
corrigiu um defeito adicional da guarda0.5 em ataques após silêncio: operadores
e fonte auxiliar FM retêm last_amp quando o pai deixa de renderizar. Somente
as condições de trigger_phase foram ampliadas para o dono silencioso; caudas
ativas continuam sem reset de fase. Dois commits pequenos d8cd25b3 e3fa65331.
Comparação0.4→0.5A: onset274→16 (128),1087→225 (135), primeiro ataque/release
idêntico, A–H com verificações efetivas, zero clipping. Nenhuma nova otimização.

## Arquitetura e etapas

25355c55 centraliza capacidade estática em Kconfig/host, deriva osciladores,
vozes AMY e synths, preserva os dois bancos pré-carregados e rejeição sem stealing.
IDs/limites são verificados em compilação e ownership real é auditado no boot/teste.
Driver I²S, DMA, USB, envelopes, ganho e navegação não foram alterados.
A UI recebe a capacidade pelo contrato PerformanceBackend::voiceCapacity,
sem mudar o DTO/UART compartilhado. VX:-- é o placeholder antes do snapshot.
A admissão ampliada é conservadora e fail-closed; não reduz capacidade sozinha.

| Etapa | Software host | Memória rastreada host | Situação física |
|---|---|---|---|
| 4 | PASS suíte e PCM comum idêntico à0.5A |116618 B vivos/116749 B pico;161 alocações | MANUAL para0.5A |
| 6 | PASS suíte completa e PCM comum idêntico ao4 |136738 B vivos/136871 B pico;205 alocações | MANUAL |
| 8 | NOT RUN nesta revisão | NOT RUN | MANUAL |

Seis notas reais foram admitidas com velocidades distintas e instrumentos DSP
independentes. A sétima foi rejeitada sem alterar gates existentes. Release,
sustain por canal, retrigger, pitch/CC1, USB/Touch, desconexão/reconexão lógica,
lease, overflow, troca de patch e Panic passaram; zero heap durante serviço,
zero OOM/clipping e alocações devolvidas no shutdown. Não é USB físico.

Comandos executados (paths relativos ao worktree):

- PASS: powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_retrigger_probe.ps1
- PASS: powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_amy.ps1 -Voices 4
- PASS: powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_amy.ps1 -Voices 6
- PASS: python smk-p4/tests/summarize_polyphony.py --voices 4 6 --output build/polyphony_46_summary.json
- PASS: powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_host.ps1 (23 testes Python mais C++)
- PASS: powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_usb.ps1
- PASS: powershell -ExecutionPolicy Bypass -File tests/run_smk_safety.ps1
- PASS: powershell -ExecutionPolicy Bypass -File tests/run_smk_ui.ps1
- PASS: powershell -ExecutionPolicy Bypass -File tests/run_smk_link.ps1
- PASS: powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_preview.ps1 (20 capturas/700 telas)
- PASS: powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_blueprint_adapter.ps1 -Voices 4 (1400 telas, sem crescimento na segunda janela)

Builds finais, estática/Flash e candidatos serão registrados após conclusão.
A primeira compilação exploratória P4 passou; não é o candidato final, pois a
verificação0.5A e a configuração foram consolidadas depois.

## Limites, decisão e rollback

Heap interno, maior bloco, PSRAM, stacks, render/deadlines P4, DMA, MIDI físico,
LVGL concorrente e janela>=30min: MANUAL/NOT RUN. Os tempos HOST são medidos em
Windows durante compilações concorrentes: máximo pode refletir agendamento do OS;
não são prazo de firmware nem prova de margem DSP. Ver matriz de performance.

Default permanece4. Seis é o primeiro candidato a bancada; oito, caso os testes
passem, será experimental até qualificação progressiva. Não promover por build.
Nenhum pino final inventado: perfil elétrico anterior preservado, gates desativados.
Placa/porta/VBUS/straps/montagem e imagem instalada exigem confirmação atual.
A API amy_start ainda contém alocações iniciais sem checagem; a admissão é
conservadora, não prova formal contra OOM/fragmentação. Ver orçamento de memória.

Rollback de software: compilar perfil4 ou usar a branch0.5 preservada. Rollback
físico: somente bundle/backup previamente validado, hash e layout confirmados e
autorização para aquela imagem; não usar firmware.bin como backup integral.
Próximo menor passo: concluir build/perfis e qualificar6 na Guition antes de8.
Escopo físico permanece aberto; não se declara a missão inteira concluída.
