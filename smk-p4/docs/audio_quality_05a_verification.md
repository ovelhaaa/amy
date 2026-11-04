# Audio 0.5A — verificação independente

2026-11-04. Branch feature/smk-p4-polyphony-06, base dd4753af.

O pedido explícito SMK-P4/Guition/800×480 prevalece sobre specs.md S3/284×76.
Apenas o escopo P4 foi alterado, salvo a correção causal mínima descrita abaixo.

## Achado e correção

FAIL (executado): comparação 0.4 cd9ed0ba versus 0.5 dd4753af com fontes e
objetos separados. O probe confirmou 274→18 (128) e 1087→223 (135), primeiro
ataque/release idêntico, zero clipping. Entretanto, a matriz isolada encontrou
regressão no cenário B: após 15.000 blocos de release, PCM silencioso mas operadores
FM com last_amp residual. Patch135: pico536→574, delta180→215. Patch128:
delta386→430. O teste antigo compartilhava um runtime entre cenários e não
verificava resultados, ocultando essa diferença.

Causa: amy_render deixa de executar um pai FM inaudível e limpa seu mod_synthinfo,
mas não limpa last_amp de seus operadores. Esperar mais tempo não resolve.
A guarda 0.5 usa last_amp do operador e ignora que o instrumento já está parado.

Correção restrita a note_on_mod e ao disparo de sua fonte de modulação: permitir trigger_phase também quando last_amp
do pai FM é zero. A guarda para retrigger com amplitude no pai permanece;
nenhuma alteração em I²S, USB, ganho, envelopes ou pools. A aplicação não possui
extensão para a escolha de fase dos operadores, exigindo essa alteração no core.
É candidata upstream após validação em outras plataformas e audição física.

O primeiro ajuste somente em note_on_mod restaurou pico536/delta180 no135,
mas FAIL no128 (delta391 vs386 e diferença na fronteira de Off). A fonte auxiliar
mod_osc também retém last_amp; a mesma condição do dono silencioso foi aplicada
no disparo dessa fonte em play_delta. Logs das tentativas estão preservados.
A matriz completa do ajuste final passou na execução registrada abaixo. Cada cenário possui runtime próprio;
B verifica PCM zero e existência de operador residual. O runner independente
registra commits completos, compilador e SHA256 de cada WAV em manifest.json.
A aceitação compara ataques/releases, clipping e limites de transientes; não
exige WAVs inteiros idênticos entre cores DSP diferentes.

PASS: execução integral independente cd9ed0ba2669cb974c6dec956b020a3d40145140
versus 3fa65331, compilando ambos os cores do zero, em
build/retrigger_ab/8ba24daf5b7549e4813db622071742c8. Onset274→16 (128),
1087→225 (135); primeiro ataque/release byte-idêntico, A/B/H idênticos na matriz
isolada, C/E/F com redução, clipping0. Manifesto contém SHA completo e hashes.
Comando: powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_retrigger_probe.ps1.
GCC14.2.0 x86-64 UCRT; 48k/256/PCM16estéreo, mesmo harness e eventos.
Gate0 PASS em software; limites auditivos e físicos continuam MANUAL.
Até aquele checkpoint de software não houve gravação, reset ou consulta serial. Relatos físicos históricos da0.5
foram anotados com errata sem apagar evidência; não identificam por si só a
imagem atualmente instalada. Melhoria auditiva foi informada pelo operador.

Compatibilidade: o reinício pelo pai silencioso requer operadores/fontes privados
por voz, como nos dois bancos ROM P4 auditados. A modulação AMY genérica permite
fontes compartilhadas; por isso a extensão está atrás de
AMY_EXCLUSIVE_FM_PHASE_RESTART, ativada somente no componente P4 e nos harnesses
correspondentes. Outras plataformas mantêm a política0.5. Não se presume ownership
exclusivo no core genérico. A configuração P4 mantém o resultado verificado.

Verificação final com flag exclusiva e cenários A/B de release completo:
PASS cd9ed0ba versus581570c6, mesmo harness, compilação independente em
build/retrigger_ab/8862ec3a00d04a0daa9ee8685b00bc63. Onsets274→16 e1087→225;
blocos finais silenciosos, comparações A–H aprovadas. Manifesto registra hashes
dos dois harnesses e flags. Comando pwsh -NoProfile -File smk-p4/tests/run_retrigger_probe.ps1.

Checkpoint posterior de preparação6: montagem reconfirmada; leitura de Flash
exigiu reinício temporário e comprovou a imagem0.5 instalada. Nenhuma0.6 gravada;
isso não modifica os resultados A/B de software nem concede qualificação física6.
