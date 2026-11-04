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
A matriz completa do ajuste final ainda está em execução. Cada cenário possui runtime próprio;
B verifica PCM zero e existência de operador residual. O runner independente
registra commits completos, compilador e SHA256 de cada WAV em manifest.json.
A aceitação compara ataques/releases, clipping e limites de transientes; não
exige WAVs inteiros idênticos entre cores DSP diferentes.

A execução integral com commits fixados está pendente nesta revisão do registro.
Nenhuma gravação, reset ou consulta serial. Relatos físicos históricos da0.5
foram anotados com errata sem apagar evidência; não identificam por si só a
imagem atualmente instalada. Melhoria auditiva foi informada pelo operador.
