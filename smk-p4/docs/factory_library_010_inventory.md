# Factory library 0.10 inventory

ROM SHA256 `3e76efb6519174663fb81073fa4e8332130489b991b4d3c6825535e1e8516a5e`. 256 Juno/DX7 IDs; 391 total positions, 266 populated, 125 reserved.

Names and costs come directly from ROM. Categories are deterministic name heuristics (FX is the fallback), not historical factory classifications. Juno bank A/B is explicit in ROM names; DX7 source bank is recorded as AMY DX7, because ROM1–4 provenance is not established here. No invented historical bank mapping.

Memory requires oscillator/filter state, eight instrument/FIFO maps and shared delta storage; actual requested bytes are in the generated qualification matrix. FX disabled: chorus requests are ignored, factory EQ retained. Mono/unison in a name does not force monophony: eight independent gates remain. All rows are ROM_PRESENT; host/support/failure states are separate flags.

|ID|Original name|Family|Category|Bank|Osc/voice|Filters|Effects/global|Command letters|Structure|
|---|---|---|---|---|---:|---|---|---|---|
|0|Juno A11 Brass Set 1|Subtractive|Brass|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|1|Juno A12 Brass Swell|Subtractive|Brass|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|2|Juno A13 Trumpet|Subtractive|Brass|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|3|Juno A14 Flutes|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|4|Juno A15 Moving Strings|Subtractive|Strings|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|5|Juno A16 Brass & Strings|Subtractive|Brass|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|6|Juno A17 Choir|Subtractive|Strings|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|7|Juno A18 Piano I|Subtractive|Piano|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|8|Juno A21 Organ I|Subtractive|Organ|JUNO A|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|9|Juno A22 Organ II|Subtractive|Organ|JUNO A|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|10|Juno A23 Combo Organ|Subtractive|Organ|JUNO A|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|11|Juno A24 Calliope|Subtractive|Organ|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|12|Juno A25 Donald Pluck|Subtractive|Plucks|JUNO A|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|13|Juno A26 Celeste* (1 oct.up)|Subtractive|Percussion|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|14|Juno A27 Elect. Piano I|Subtractive|ElectricPiano|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|15|Juno A28 Elect. Piano II|Subtractive|ElectricPiano|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|16|Juno A31 Clock Chimes* (1 oct. up)|Subtractive|Percussion|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|17|Juno A32 Steel Drums|Subtractive|Percussion|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|18|Juno A33 Xylophone|Subtractive|Percussion|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|19|Juno A34 Brass III|Subtractive|Brass|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|20|Juno A35 Fanfare|Subtractive|Brass|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|21|Juno A36 String III|Subtractive|Strings|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|22|Juno A37 Pizzicato|Subtractive|Plucks|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|23|Juno A38 High Strings|Subtractive|Strings|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|24|Juno A41 Bass clarinet|Subtractive|Bass|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|25|Juno A42 English Horn|Subtractive|Brass|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|26|Juno A43 Brass Ensemble|Subtractive|Brass|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|27|Juno A44 Guitar|Subtractive|Plucks|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|28|Juno A45 Koto|Subtractive|Plucks|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|29|Juno A46 Dark Pluck|Subtractive|Plucks|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|30|Juno A47 Funky I|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|31|Juno A48 Synth Bass I (unison)|Subtractive|Bass|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|32|Juno A51 Lead I|Subtractive|Leads|JUNO A|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|33|Juno A52 Lead II|Subtractive|Leads|JUNO A|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|34|Juno A53 Lead III|Subtractive|Leads|JUNO A|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|35|Juno A54 Funky II|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|36|Juno A55 Synth Bass II|Subtractive|Bass|JUNO A|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|37|Juno A56 Funky III|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|38|Juno A57 Thud Wah|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|39|Juno A58 Going Up|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|40|Juno A61 Piano II|Subtractive|Piano|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|41|Juno A62 Clav|Subtractive|Plucks|JUNO A|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|42|Juno A63 Frontier Organ|Subtractive|Organ|JUNO A|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|43|Juno A64 Snare Drum (unison)|Subtractive|Percussion|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|44|Juno A65 Tom Toms (unison)|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|45|Juno A66 Timpani (unison)|Subtractive|Percussion|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|46|Juno A67 Shaker|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|47|Juno A68 Synth Pad|Subtractive|Pads|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|48|Juno A71 Sweep I|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|49|Juno A72 Pluck Sweep|Subtractive|Plucks|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|50|Juno A73 Repeater|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|51|Juno A74 Sweep II|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|52|Juno A75 Pluck Bell|Subtractive|Plucks|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|53|Juno A76 Dark Synth Piano|Subtractive|Piano|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|54|Juno A77 Sustainer|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|55|Juno A78 Wah Release|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|56|Juno A81 Gong (play low chords)|Subtractive|Percussion|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|57|Juno A82 Resonance Funk|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|58|Juno A83 Drum Booms* (1 oct. down)|Subtractive|Percussion|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|59|Juno A84 Dust Storm|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|60|Juno A85 Rocket Men|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|61|Juno A86 Hand Claps|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|62|Juno A87 FX Sweep|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|63|Juno A88 Caverns|Subtractive|FX|JUNO A|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|64|Juno B11 Strings|Subtractive|Strings|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|65|Juno B12 Violin|Subtractive|Strings|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|66|Juno B13 Chorus Vibes|Subtractive|Percussion|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|67|Juno B14 Organ 1|Subtractive|Organ|JUNO B|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|68|Juno B15 Harpsichord 1|Subtractive|Plucks|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|69|Juno B16 Recorder|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; RELEASE_TAIL|
|70|Juno B17 Perc. Pluck|Subtractive|Plucks|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|71|Juno B18 Noise Sweep|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|72|Juno B21 Space Chimes|Subtractive|Percussion|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|73|Juno B22 Nylon Guitar|Subtractive|Plucks|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|74|Juno B23 Orchestral Pad|Subtractive|Pads|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|75|Juno B24 Bright Pluck|Subtractive|Plucks|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|76|Juno B25 Organ Bell|Subtractive|Organ|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|77|Juno B26 Accordion|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|78|Juno B27 FX Rise 1|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|79|Juno B28 FX Rise 2|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|80|Juno B31 Brass|Subtractive|Brass|JUNO B|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|81|Juno B32 Helicopter|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING,HOST_DEADLINE_REVIEW|
|82|Juno B33 Lute|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|83|Juno B34 Chorus Funk|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|84|Juno B35 Tomita|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|85|Juno B36 FX Sweep 1|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|86|Juno B37 Sharp Reed|Subtractive|Plucks|JUNO B|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|87|Juno B38 Bass Pluck|Subtractive|Bass|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|88|Juno B41 Resonant Rise|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|89|Juno B42 Harpsichord 2|Subtractive|Plucks|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|90|Juno B43 Dark Ensemble|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|91|Juno B44 Contact Wah|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|92|Juno B45 Noise Sweep 2|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|93|Juno B46 Glassy Wah|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|94|Juno B47 Phase Ensemble|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|95|Juno B48 Chorused Bell|Subtractive|Percussion|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|96|Juno B51 Clav|Subtractive|Plucks|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|97|Juno B52 Organ 2|Subtractive|Organ|JUNO B|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|98|Juno B53 Bassoon|Subtractive|Bass|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|99|Juno B54 Auto Release Noise Sweep|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|100|Juno B55 Brass Ensemble|Subtractive|Brass|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|101|Juno B56 Ethereal|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|102|Juno B57 Chorus Bell 2|Subtractive|Percussion|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|103|Juno B58 Blizzard|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING,HOST_DEADLINE_REVIEW|
|104|Juno B61 E. Piano with Tremolo|Subtractive|Piano|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|105|Juno B62 Clarinet|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|106|Juno B63 Thunder|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|107|Juno B64 Reedy Organ|Subtractive|Organ|JUNO B|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|108|Juno B65 Flute / Horn|Subtractive|Brass|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|109|Juno B66 Toy Rhodes|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|110|Juno B67 Surf's Up|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|111|Juno B68 OW Bass|Subtractive|Bass|JUNO B|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|112|Juno B71 Piccolo|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; RELEASE_TAIL|
|113|Juno B72 Melodic Taps|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|114|Juno B73 Meow Brass|Subtractive|Brass|JUNO B|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|115|Juno B74 Violin (high)|Subtractive|Strings|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|116|Juno B75 High Bells|Subtractive|Percussion|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|117|Juno B76 Rolling Wah|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|118|Juno B77 Ping Bell|Subtractive|Percussion|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; HARD_CLIPPING|
|119|Juno B78 Brassy Organ|Subtractive|Organ|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|120|Juno B81 Low Dark Strings|Subtractive|Strings|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|121|Juno B82 Piccolo Trumpet|Subtractive|Brass|JUNO B|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|122|Juno B83 Cello|Subtractive|Strings|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|123|Juno B84 High Strings|Subtractive|Strings|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|124|Juno B85 Rocket Men|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|125|Juno B86 Forbidden Planet|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|126|Juno B87 Froggy|Subtractive|FX|JUNO B|6|LPF24 G4|EQ/chorus dry|ABFGLRZacdfkmvwx|local references within voice; NOT RUN|
|127|Juno B88 Owgan|Subtractive|Organ|JUNO B|6|LPF24 G4|EQ/chorus dry|AFGLRZacdfkmvwx|local references within voice; NOT RUN|
|128|DX7 BRASS   1|FM|Brass|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|129|DX7 BRASS   2|FM|Brass|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|130|DX7 BRASS   3|FM|Brass|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|131|DX7 STRINGS 1|FM|Strings|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|132|DX7 STRINGS 2|FM|Strings|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|133|DX7 STRINGS 3|FM|Strings|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|134|DX7 ORCHESTRA|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|135|DX7 PIANO   1|FM|Piano|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|136|DX7 PIANO   2|FM|Piano|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|137|DX7 PIANO   3|FM|Piano|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|138|DX7 E.PIANO 1|FM|ElectricPiano|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|139|DX7 GUITAR  1|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|140|DX7 GUITAR  2|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|141|DX7 SYN-LEAD 1|FM|Leads|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|142|DX7 BASS    1|FM|Bass|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|143|DX7 BASS    2|FM|Bass|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|144|DX7 E.ORGAN 1|FM|Organ|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; HARD_CLIPPING|
|145|DX7 PIPES   1|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|146|DX7 HARPSICH 1|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|147|DX7 CLAV    1|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|148|DX7 VIBE    1|FM|Percussion|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|149|DX7 MARIMBA|FM|Percussion|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|150|DX7 KOTO|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|151|DX7 FLUTE   1|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|152|DX7 ORCH-CHIME|FM|Percussion|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|153|DX7 TUB BELLS|FM|Percussion|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|154|DX7 STEEL DRUM|FM|Percussion|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|155|DX7 TIMPANI|FM|Percussion|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|156|DX7 REFS WHISL|FM|FX|AMY DX7|8|none|none|ALOPZabfovw|local references within voice; NOT RUN|
|157|DX7 VOICE   1|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|158|DX7 TRAIN|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; HARD_CLIPPING|
|159|DX7 TAKE OFF|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|160|DX7 PIANO   4|FM|Piano|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|161|DX7 PIANO   5|FM|Piano|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|162|DX7 E.PIANO 2|FM|ElectricPiano|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|163|DX7 E.PIANO 3|FM|ElectricPiano|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|164|DX7 E.PIANO 4|FM|ElectricPiano|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|165|DX7 PIANO 5THS|FM|Piano|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|166|DX7 CELESTE|FM|Percussion|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|167|DX7 TOY PIANO|FM|Piano|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|168|DX7 HARPSICH 2|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|169|DX7 HARPSICH 3|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|170|DX7 CLAV    2|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|171|DX7 CLAV    3|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|172|DX7 E.ORGAN 2|FM|Organ|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|173|DX7 E.ORGAN 3|FM|Organ|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|174|DX7 E.ORGAN 4|FM|Organ|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|175|DX7 E.ORGAN 5|FM|Organ|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|176|DX7 PIPES   2|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|177|DX7 PIPES   3|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|178|DX7 PIPES   4|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|179|DX7 CALIOPE|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|180|DX7 ACCORDION|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|181|DX7 SITAR|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|182|DX7 GUITAR  3|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|183|DX7 GUITAR  4|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|184|DX7 GUITAR  5|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|185|DX7 GUITAR  6|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|186|DX7 LUTE|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|187|DX7 BANJO|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|188|DX7 HARP    1|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|189|DX7 HARP    2|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|190|DX7 BASS    3|FM|Bass|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|191|DX7 BASS    4|FM|Bass|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|192|DX7 PICCOLO|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|193|DX7 FLUTE   2|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|194|DX7 OBOE|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|195|DX7 CLARINET|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|196|DX7 SAX BC|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|197|DX7 BASSOON|FM|Bass|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|198|DX7 STRINGS 4|FM|Strings|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|199|DX7 STRINGS 5|FM|Strings|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|200|DX7 STRINGS 6|FM|Strings|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|201|DX7 STRINGS 7|FM|Strings|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|202|DX7 STRINGS 8|FM|Strings|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|203|DX7 BRASS   4|FM|Brass|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|204|DX7 BRASS   5|FM|Brass|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|205|DX7 BRASS 6 BC|FM|Brass|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|206|DX7 BRASS   7|FM|Brass|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|207|DX7 BRASS   8|FM|Brass|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|208|DX7 RECORDER|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|209|DX7 HARMONICA1|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|210|DX7 HRMNCA2 BC|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|211|DX7 VOICE   2|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|212|DX7 VOICE   3|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|213|DX7 GLOKENSPL|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|214|DX7 VIBE    2|FM|Percussion|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|215|DX7 XYLOPHONE|FM|Percussion|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|216|DX7 CHIMES|FM|Percussion|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|217|DX7 GONG    1|FM|Percussion|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|218|DX7 GONG    2|FM|Percussion|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|219|DX7 BELLS|FM|Percussion|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|220|DX7 COW BELL|FM|Percussion|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|221|DX7 BLOCK|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|222|DX7 FLEXATONE|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|223|DX7 LOG DRUM|FM|Percussion|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|224|DX7 SYN-LEAD 2|FM|Leads|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|225|DX7 SYN-LEAD 3|FM|Leads|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|226|DX7 SYN-LEAD 4|FM|Leads|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|227|DX7 SYN-LEAD 5|FM|Leads|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|228|DX7 SYN-CLAV 1|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|229|DX7 SYN-CLAV 2|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|230|DX7 SYN-CLAV 3|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|231|DX7 SYN-PIANO|FM|Piano|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|232|DX7 SYNBRASS 1|FM|Brass|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; HARD_CLIPPING|
|233|DX7 SYNBRASS 2|FM|Brass|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|234|DX7 SYNORGAN 1|FM|Organ|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|235|DX7 SYNORGAN 2|FM|Organ|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|236|DX7 SYN-VOX|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|237|DX7 SYN-ORCH|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|238|DX7 SYN-BASS 1|FM|Bass|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|239|DX7 SYN-BASS 2|FM|Bass|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|240|DX7 HARP-FLUTE|FM|Plucks|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|241|DX7 BELL-FLUTE|FM|Percussion|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|242|DX7 E.P-BRS BC|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|243|DX7 T.BL-EXPA|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|244|DX7 CHIME-STRG|FM|Percussion|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|245|DX7 B.DRM-SNAR|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|246|DX7 SHIMMER|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|247|DX7 EVOLUTION|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|248|DX7 WATER GDN|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; HARD_CLIPPING|
|249|DX7 WASP STING|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|250|DX7 LASER GUN|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|251|DX7 DESCENT|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|252|DX7 OCTAVE WAR|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|253|DX7 GRAND PRIX|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
|254|DX7 ST.HELENS|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; RELEASE_TAIL,HOST_DEADLINE_REVIEW|
|255|DX7 EXPLOSION|FM|FX|AMY DX7|8|none|none|AILOPZabfovw|local references within voice; NOT RUN|
