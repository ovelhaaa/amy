# Perfil SMK25 V2 — Legacy S3

O perfil de fábrica fica ativo quando o P4 não encontra um registro válido. Na tela MIDI, a guia de mapeamento mostra o perfil, o estado USB, o último CC ou pad observado e a fonte, o destino e o Pickup quando a fonte tem binding. Em PROFILE / PADS / TRANSPORT, é possível restaurar o perfil SMK25, recarregar o perfil salvo ou iniciar um perfil P4 Custom.

A identificação usa o VID/PID 4353:4B4D registrado no ensaio anterior do P4, na interface USB MIDI selecionada, cabo virtual 0. Controladores com outro VID/PID podem usar o perfil P4 Custom. Os eventos de teclado conservam o canal recebido e o filtro MIDI existente. Pitch Bend, Mod Wheel CC1 e Sustain CC64 mantêm o caminho musical.

| Knob | Banco A, canal 1 | Destino | Banco B, canal 1 | Destino |
|---|---:|---|---:|---|
| K1 | CC21 | M1 Character | CC29 | Chorus Depth |
| K2 | CC22 | M2 Brightness | CC30 | Delay Time |
| K3 | CC23 | M3 Motion | CC31 | Delay Feedback |
| K4 | CC24 | M4 Shape | CC32 | Delay Mix |
| K5 | CC25 | M5 Attack | CC33 | Reverb Size |
| K6 | CC26 | M6 Release | CC34 | Reverb Mix |
| K7 | CC27 | M7 Space | CC35 | Master Tone |
| K8 | CC28 | M8 Edge | CC36 | EQ Low |

As macros usam posições de 0–127 no backend. Chorus Depth, Delay Mix, Reverb Size e Reverb Mix usam 0–100%. Delay Time usa 10–1000 ms, Delay Feedback 0–95%, Master Tone −100–100%, EQ Low −12–+12 dB. EQ Low usa o ganho de banda existente, com passos de 1 dB.
