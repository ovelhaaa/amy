/**
 * DX7 FM Synthesis Engine & Algorithm Matrix for AMY Studio
 * Synchronized with ESP32-S3 firmware (SMK-S3) AmyAdapter FM architecture.
 * Targets polyphonic instrument i1 (Synth 1, MIDI ch 0).
 */

const DX7_ALGORITHMS_INFO = [
  { id: 1,  carriers: [1, 3], modulators: [[2], [4, 5, 6]], feedbackOp: 6, desc: "Classic 2-stack FM with 3-op modulator chain" },
  { id: 2,  carriers: [1, 3], modulators: [[2], [4, 5, 6]], feedbackOp: 2, desc: "Dual carrier with internal feedback on Op2" },
  { id: 3,  carriers: [1, 4], modulators: [[2, 3], [5, 6]], feedbackOp: 6, desc: "Dual 3-op stacks" },
  { id: 4,  carriers: [1, 4], modulators: [[2, 3], [5, 6]], feedbackOp: 4, desc: "Dual 3-op stacks with carrier feedback" },
  { id: 5,  carriers: [1, 3, 5], modulators: [[2], [4], [6]], feedbackOp: 6, desc: "Triple 2-op pairs for complex bells" },
  { id: 6,  carriers: [1, 3, 5], modulators: [[2], [4], [6]], feedbackOp: 5, desc: "Triple 2-op pairs with Op5 feedback" },
  { id: 7,  carriers: [1, 3], modulators: [[2], [4, 5]], feedbackOp: 6, desc: "Parallel modulation on carrier 3" },
  { id: 8,  carriers: [1, 3], modulators: [[2], [4, 5]], feedbackOp: 4, desc: "Parallel branch with Op4 feedback" },
  { id: 9,  carriers: [1, 3], modulators: [[2], [4, 5]], feedbackOp: 2, desc: "Parallel branch with Op2 feedback" },
  { id: 10, carriers: [1, 4], modulators: [[2, 3], [5]], feedbackOp: 3, desc: "Branching tree modulation" },
  { id: 11, carriers: [1, 4], modulators: [[2, 3], [5]], feedbackOp: 6, desc: "Branching tree with Op6 feedback" },
  { id: 12, carriers: [1, 4], modulators: [[2, 3], [5]], feedbackOp: 2, desc: "Branching tree with Op2 feedback" },
  { id: 13, carriers: [1, 4], modulators: [[2, 3], [5]], feedbackOp: 6, desc: "Cross-modulated tree" },
  { id: 14, carriers: [1, 3], modulators: [[2], [4, 5, 6]], feedbackOp: 6, desc: "Additive tree stack" },
  { id: 15, carriers: [1, 3], modulators: [[2], [4, 5, 6]], feedbackOp: 2, desc: "Additive tree with Op2 feedback" },
  { id: 16, carriers: [1, 3, 4], modulators: [[2], [5, 6]], feedbackOp: 6, desc: "Triple carrier mixed stack" },
  { id: 17, carriers: [1, 3, 4], modulators: [[2], [5, 6]], feedbackOp: 4, desc: "Triple carrier with Op4 feedback" },
  { id: 18, carriers: [1, 2, 4], modulators: [[3], [5, 6]], feedbackOp: 4, desc: "Triple carrier multi-stack" },
  { id: 19, carriers: [1, 4], modulators: [[2, 3], [5, 6]], feedbackOp: 6, desc: "Complex 2-carrier dual tree" },
  { id: 20, carriers: [1, 4], modulators: [[2, 3], [5, 6]], feedbackOp: 3, desc: "Dual tree with Op3 feedback" },
  { id: 21, carriers: [1, 4, 5], modulators: [[2, 3], [6]], feedbackOp: 3, desc: "Multi-carrier additive FM" },
  { id: 22, carriers: [1, 4, 5, 6], modulators: [[2, 3]], feedbackOp: 6, desc: "Quad carrier lush organ" },
  { id: 23, carriers: [1, 4, 5, 6], modulators: [[2, 3]], feedbackOp: 6, desc: "Quad carrier with Op6 feedback" },
  { id: 24, carriers: [1, 3, 4, 5, 6], modulators: [[2]], feedbackOp: 6, desc: "5-carrier rich pad structure" },
  { id: 25, carriers: [1, 3, 4, 5, 6], modulators: [[2]], feedbackOp: 6, desc: "5-carrier additive structure" },
  { id: 26, carriers: [1, 3, 4, 5, 6], modulators: [[2]], feedbackOp: 6, desc: "5-carrier wide brass" },
  { id: 27, carriers: [1, 3, 4, 5, 6], modulators: [[2]], feedbackOp: 3, desc: "5-carrier with Op3 feedback" },
  { id: 28, carriers: [1, 3, 5], modulators: [[2], [4], [6]], feedbackOp: 5, desc: "Triple independent FM pairs" },
  { id: 29, carriers: [1, 3, 4, 5], modulators: [[2], [6]], feedbackOp: 6, desc: "4-carrier mixed organ/bell" },
  { id: 30, carriers: [1, 3, 4, 5], modulators: [[2], [6]], feedbackOp: 5, desc: "4-carrier with Op5 feedback" },
  { id: 31, carriers: [1, 2, 3, 4, 5], modulators: [[6]], feedbackOp: 6, desc: "Single modulator feeding 5 carriers" },
  { id: 32, carriers: [1, 2, 3, 4, 5, 6], modulators: [], feedbackOp: 6, desc: "Pure Additive 6-Sine Organ" }
];

class Dx7MatrixEditor {
  constructor() {
    this.currentAlgo = 1;
    this.feedback = 0;
    this.operators = [
      { id: 1, ratio: 1.00, detune: 0, amp: 1.0, env: [10, 100, 0.8, 200] },
      { id: 2, ratio: 2.00, detune: 0, amp: 0.8, env: [10, 150, 0.4, 250] },
      { id: 3, ratio: 1.00, detune: 1, amp: 0.0, env: [20, 200, 0.5, 300] },
      { id: 4, ratio: 1.00, detune: 0, amp: 0.0, env: [10, 100, 0.7, 200] },
      { id: 5, ratio: 3.00, detune: -1, amp: 0.5, env: [15, 120, 0.3, 220] },
      { id: 6, ratio: 1.00, detune: 0, amp: 0.7, env: [10, 80, 0.2, 180] }
    ];
  }

  setAlgorithm(algoId) {
    if (algoId < 1 || algoId > 32) return;
    this.currentAlgo = algoId;
    // Set algorithm on Synth 1
    const wire = `i1o${algoId}Z`;
    if (window.amyAudioBridge) window.amyAudioBridge.sendWire(wire);
    if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(wire);
  }

  setOperatorParam(opId, param, value) {
    const op = this.operators.find(o => o.id === opId);
    if (!op) return;
    op[param] = value;
    
    let wire = "";
    if (param === 'ratio') {
      // In AMY, frequency ratio is 'I'
      const osc = 8 - opId; // Maps op 1..6 to voices 7..2
      wire = `v${osc}I${value.toFixed(3)}Z`;
    } else if (param === 'amp') {
      // In AMY, amplitude is 'a'
      const osc = 8 - opId;
      wire = `v${osc}a${value.toFixed(3)}Z`;
    } else if (param === 'feedback') {
      // Feedback on Synth 1
      wire = `i1b${value.toFixed(3)}Z`;
    }
    
    if (wire.length > 0) {
      if (window.amyAudioBridge) window.amyAudioBridge.sendWire(wire);
      if (window.esp32HardwareSync) window.esp32HardwareSync.sendWireIfConnected(wire);
    }
  }
}

window.dx7MatrixEditor = new Dx7MatrixEditor();

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { Dx7MatrixEditor, DX7_ALGORITHMS_INFO };
}
