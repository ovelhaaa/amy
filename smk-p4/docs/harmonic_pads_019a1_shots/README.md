# Harmonic Pads 0.19-A.1 — HOST SIM

Capturas de framebuffer LVGL 9.3, 800×480, com o Audio Owner e AMY reais em host. A fixture principal confirma uma Scene/patch válidos antes dos cenários. ACTIVE e o contador de vozes refletem snapshots do owner; somente a identificação HOST SIM é uma marca de ambiente. Qualificação física adiada.

| PNG | Cenário |
|---|---|
| [01](pads_01_c_major.png) | C maior, limpo |
| [02](pads_02_a_minor.png) | A menor, limpo |
| [03](pads_03_root_selector.png) | ROOT, 12 notas |
| [04](pads_04_scale_selector.png) | SCALE, incompatíveis identificadas |
| [05](pads_05_oct_selector.png) | OCT, escolhas inválidas desabilitadas |
| [06](pads_06_momentary.png) | MOMENTARY |
| [07](pads_07_hold_two_chords.png) | HOLD, dois grupos admitidos |
| [08](pads_08_release_all.png) | RELEASE ALL, rejeição apenas histórica |
| [09](pads_09_capacity.png) | Capacity, rejeição atômica |
| [10](pads_10_ready.png) | READY após liberação |
| [11](pads_11_hold_expired.png) | Expiração confirmada pelo owner |
| [12](pads_12_global_local.png) | Global C maior / local A menor |
| [13](pads_13_incompatible.png) | Escala incompatível |
| [14](pads_14_octave_frozen.png) | Nova oitava, acorde sustentado congelado |
| [15](pads_15_arp.png) | Arp habilitado |
| [16](pads_16_sequencer.png) | Sequencer ativo |
| [17](pads_17_panic.png) | PANIC |
| [18](pads_18_macros.png) | Retorno a Macros |
| [19](pads_19_confirmation.png) | Confirmação destrutiva bloqueando navegação |

Reprodução: `powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_harmonic_pads_019a1.ps1 -Clean -Regressions`, a partir da raiz do repositório.
