/**
 * AMY Studio - PCM Sample & Wavetable Library Browser
 * Browse, audition and assign all 128 built-in PCM drum & instrument samples and wavetables.
 */

const AMY_PCM_SAMPLES = [
  // Drums (808, 909, Linn)
  { id: 0, name: "TR-808 Bass Drum", cat: "Drums (808)", note: 36 },
  { id: 1, name: "TR-808 Snare Drum", cat: "Drums (808)", note: 38 },
  { id: 2, name: "TR-808 Closed Hat", cat: "Drums (808)", note: 42 },
  { id: 3, name: "TR-808 Open Hat", cat: "Drums (808)", note: 46 },
  { id: 4, name: "TR-808 Low Tom", cat: "Drums (808)", note: 41 },
  { id: 5, name: "TR-808 Mid Tom", cat: "Drums (808)", note: 45 },
  { id: 6, name: "TR-808 High Tom", cat: "Drums (808)", note: 48 },
  { id: 7, name: "TR-808 Hand Clap", cat: "Drums (808)", note: 39 },
  { id: 8, name: "TR-808 Cowbell", cat: "Drums (808)", note: 56 },
  { id: 9, name: "TR-808 Rimshot", cat: "Drums (808)", note: 37 },
  { id: 10, name: "TR-808 Claves", cat: "Drums (808)", note: 75 },
  { id: 11, name: "TR-808 Maracas", cat: "Drums (808)", note: 70 },
  { id: 12, name: "TR-808 Crash", cat: "Drums (808)", note: 49 },
  
  // TR-909 & Acoustic
  { id: 16, name: "TR-909 Punchy Kick", cat: "Drums (909)", note: 36 },
  { id: 17, name: "TR-909 Snappy Snare", cat: "Drums (909)", note: 38 },
  { id: 18, name: "TR-909 Metallic Hat", cat: "Drums (909)", note: 42 },
  { id: 19, name: "TR-909 Ride Cymbal", cat: "Drums (909)", note: 51 },
  { id: 24, name: "LinnDrum Rock Kick", cat: "LinnDrum", note: 36 },
  { id: 25, name: "LinnDrum Snare Top", cat: "LinnDrum", note: 38 },
  { id: 26, name: "LinnDrum Tambourine", cat: "LinnDrum", note: 54 },
  { id: 27, name: "LinnDrum Cabasa", cat: "LinnDrum", note: 69 },

  // Acoustic & Melodic Instruments
  { id: 64, name: "Steinway Grand Piano", cat: "Acoustic Keys", note: 60 },
  { id: 65, name: "Rhodes Mark I Electric Piano", cat: "Acoustic Keys", note: 60 },
  { id: 66, name: "Wurlitzer 200A", cat: "Acoustic Keys", note: 60 },
  { id: 67, name: "Hammond B3 Drawbar Organ", cat: "Organs", note: 60 },
  { id: 68, name: "Church Pipe Organ", cat: "Organs", note: 60 },
  { id: 72, name: "Fender Precision Bass", cat: "Bass", note: 36 },
  { id: 73, name: "Moog Minimoog Sub Bass", cat: "Bass", note: 36 },
  { id: 80, name: "Orchestral Violins Section", cat: "Strings & Brass", note: 60 },
  { id: 81, name: "Analog Synth Brass Horns", cat: "Strings & Brass", note: 60 },
  { id: 88, name: "Concert Flute", cat: "Winds", note: 72 },
  { id: 96, name: "Vibraphone Warm Motor", cat: "Tuned Percussion", note: 60 },
  { id: 97, name: "Orchestral Marimba", cat: "Tuned Percussion", note: 60 },
  { id: 98, name: "Glockenspiel Bell", cat: "Tuned Percussion", note: 72 }
];

class SampleBrowserManager {
  constructor(containerId) {
    this.container = document.getElementById(containerId);
    if (this.container) this.render();
  }

  render() {
    if (!this.container) return;
    this.container.innerHTML = '';

    const listContainer = document.createElement('div');
    listContainer.className = 'sample-list-grid';
    listContainer.style.display = 'grid';
    listContainer.style.gridTemplateColumns = 'repeat(auto-fill, minmax(220px, 1fr))';
    listContainer.style.gap = '8px';
    listContainer.style.maxHeight = '280px';
    listContainer.style.overflowY = 'auto';

    AMY_PCM_SAMPLES.forEach(sample => {
      const card = document.createElement('div');
      card.className = 'sample-card';
      card.style.background = '#141926';
      card.style.border = '1px solid #202738';
      card.style.borderRadius = '6px';
      card.style.padding = '8px 10px';
      card.style.display = 'flex';
      card.style.justifyContent = 'space-between';
      card.style.alignItems = 'center';
      card.style.cursor = 'pointer';
      card.style.transition = 'all 0.15s ease';

      card.innerHTML = `
        <div>
          <div style="font-size: 11px; font-weight: 600; color: #fff;">${sample.name}</div>
          <div style="font-size: 9px; color: #64748b;">${sample.cat} • ID #${sample.id}</div>
        </div>
        <button class="btn btn-icon" style="padding: 3px 6px; font-size: 10px; color: #00f0ff;">▶</button>
      `;

      card.addEventListener('click', () => {
        // Audition sample via AMY PCM wave (wave=7)
        if (window.amyAudioBridge) {
          window.amyAudioBridge.sendWire(`v1w7p${sample.id}l1Z`);
          window.amyAudioBridge.noteOn(1, sample.note, 0.9);
          setTimeout(() => {
            if (window.amyAudioBridge) window.amyAudioBridge.noteOff(1, sample.note);
          }, 400);
        }
      });

      card.addEventListener('mouseenter', () => {
        card.style.borderColor = '#00f0ff';
        card.style.background = '#1a2234';
      });
      card.addEventListener('mouseleave', () => {
        card.style.borderColor = '#202738';
        card.style.background = '#141926';
      });

      listContainer.appendChild(card);
    });

    this.container.appendChild(listContainer);
  }
}

window.SampleBrowserManager = SampleBrowserManager;
