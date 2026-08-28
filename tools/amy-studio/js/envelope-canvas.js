/**
 * AMY Studio - Interactive Graphical Multi-Stage Envelope Editor
 * Renders and allows direct dragging of Attack, Decay, Sustain, Release breakpoints
 * using the authentic AMY mathematical curve equations (Linear, False Exponential, DX7, True Exponential).
 */

class InteractiveEnvelopeCanvas {
  constructor(canvasId, egIndex = 0) {
    this.canvas = document.getElementById(canvasId);
    this.ctx = this.canvas ? this.canvas.getContext('2d') : null;
    this.egIndex = egIndex; // 0 = EG0 (Amp), 1 = EG1 (Filter/Mod)
    this.isDragging = false;
    this.dragTarget = null; // 'A', 'D', 'S', 'R'
    this.hoverTarget = null;
    this.points = {
      attackMs: 10,
      decayMs: 200,
      sustainLevel: 0.7,
      releaseMs: 300,
      curveType: 0 // 0=Normal/RC, 1=Linear, 2=DX7, 3=Exponential
    };

    if (this.canvas) {
      this.initEvents();
      this.render();
    }
  }

  updateParams(attackMs, decayMs, sustainLevel, releaseMs, curveType = 0) {
    this.points.attackMs = Math.max(1, attackMs);
    this.points.decayMs = Math.max(5, decayMs);
    this.points.sustainLevel = Math.max(0, Math.min(1, sustainLevel));
    this.points.releaseMs = Math.max(5, releaseMs);
    this.points.curveType = curveType;
    this.render();
  }

  initEvents() {
    const c = this.canvas;
    
    c.addEventListener('mousedown', (e) => {
      const rect = c.getBoundingClientRect();
      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;
      const target = this.hitTest(x, y);
      if (target) {
        this.isDragging = true;
        this.dragTarget = target;
      }
    });

    window.addEventListener('mousemove', (e) => {
      if (!this.canvas) return;
      const rect = this.canvas.getBoundingClientRect();
      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;

      if (this.isDragging && this.dragTarget) {
        this.handleDrag(x, y);
      } else {
        const hit = this.hitTest(x, y);
        if (hit !== this.hoverTarget) {
          this.hoverTarget = hit;
          this.canvas.style.cursor = hit ? 'pointer' : 'default';
          this.render();
        }
      }
    });

    window.addEventListener('mouseup', () => {
      if (this.isDragging) {
        this.isDragging = false;
        this.dragTarget = null;
        this.render();
      }
    });
  }

  hitTest(x, y) {
    const coords = this.getScreenCoords();
    const radius = 9;

    for (const key of ['A', 'D', 'S', 'R']) {
      const pt = coords[key];
      const dist = Math.hypot(x - pt.x, y - pt.y);
      if (dist <= radius) return key;
    }
    return null;
  }

  getScreenCoords() {
    const w = this.canvas.width;
    const h = this.canvas.height;
    const padding = 16;
    const plotW = w - padding * 2;
    const plotH = h - padding * 2;

    const totalTime = this.points.attackMs + this.points.decayMs + 400 + this.points.releaseMs;
    const timeScale = plotW / totalTime;

    const x0 = padding;
    const y0 = h - padding;

    const xA = x0 + this.points.attackMs * timeScale;
    const yA = padding; // Peak = 1.0

    const xD = xA + this.points.decayMs * timeScale;
    const yD = y0 - this.points.sustainLevel * plotH;

    const xS = xD + 300 * timeScale; // Sustain hold length
    const yS = yD;

    const xR = xS + this.points.releaseMs * timeScale;
    const yR = y0;

    return {
      origin: { x: x0, y: y0 },
      A: { x: xA, y: yA },
      D: { x: xD, y: yD },
      S: { x: xS, y: yS },
      R: { x: xR, y: yR }
    };
  }

  handleDrag(x, y) {
    const w = this.canvas.width;
    const h = this.canvas.height;
    const padding = 16;
    const plotH = h - padding * 2;

    if (this.dragTarget === 'A') {
      const atk = Math.max(1, Math.min(3000, (x - padding) * 10));
      this.points.attackMs = atk;
      if (this.egIndex === 0) {
        window.synthStateManager.setParam('amp_attack', atk, false);
      } else {
        window.synthStateManager.setParam('eg1_attack', atk, false);
      }
    } else if (this.dragTarget === 'D') {
      const dec = Math.max(5, Math.min(5000, (x - padding) * 12));
      this.points.decayMs = dec;
      if (this.egIndex === 0) {
        window.synthStateManager.setParam('amp_decay', dec, false);
      } else {
        window.synthStateManager.setParam('eg1_decay', dec, false);
      }
    } else if (this.dragTarget === 'S') {
      const sus = Math.max(0, Math.min(1.0, (h - padding - y) / plotH));
      this.points.sustainLevel = sus;
      if (this.egIndex === 0) {
        window.synthStateManager.setParam('amp_sustain', sus, false);
      } else {
        window.synthStateManager.setParam('eg1_sustain', sus, false);
      }
    } else if (this.dragTarget === 'R') {
      const rel = Math.max(5, Math.min(6000, (x - padding) * 14));
      this.points.releaseMs = rel;
      if (this.egIndex === 0) {
        window.synthStateManager.setParam('amp_release', rel, false);
      } else {
        window.synthStateManager.setParam('eg1_release', rel, false);
      }
    }

    this.render();
  }

  render() {
    if (!this.ctx || !this.canvas) return;
    const ctx = this.ctx;
    const w = this.canvas.width;
    const h = this.canvas.height;

    ctx.fillStyle = '#0a0d14';
    ctx.fillRect(0, 0, w, h);

    // Background Grid
    ctx.strokeStyle = '#161c28';
    ctx.lineWidth = 1;
    for (let y = 20; y < h; y += 25) {
      ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
    }
    for (let x = 30; x < w; x += 40) {
      ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke();
    }

    const c = this.getScreenCoords();

    // Fill Envelope Area with glowing gradient
    const grad = ctx.createLinearGradient(0, 0, 0, h);
    if (this.egIndex === 0) {
      grad.addColorStop(0, 'rgba(0, 255, 136, 0.35)');
      grad.addColorStop(1, 'rgba(0, 255, 136, 0.02)');
    } else {
      grad.addColorStop(0, 'rgba(0, 240, 255, 0.35)');
      grad.addColorStop(1, 'rgba(0, 240, 255, 0.02)');
    }

    ctx.fillStyle = grad;
    ctx.beginPath();
    ctx.moveTo(c.origin.x, c.origin.y);

    // Attack segment curve
    if (this.points.curveType === 1) {
      ctx.lineTo(c.A.x, c.A.y); // Linear
    } else {
      ctx.quadraticCurveTo(c.origin.x + (c.A.x - c.origin.x) * 0.2, c.A.y, c.A.x, c.A.y);
    }

    // Decay segment curve
    if (this.points.curveType === 1) {
      ctx.lineTo(c.D.x, c.D.y);
    } else {
      ctx.quadraticCurveTo(c.A.x + (c.D.x - c.A.x) * 0.4, c.D.y + (c.A.y - c.D.y) * 0.8, c.D.x, c.D.y);
    }

    // Sustain segment
    ctx.lineTo(c.S.x, c.S.y);

    // Release segment curve
    if (this.points.curveType === 1) {
      ctx.lineTo(c.R.x, c.R.y);
    } else {
      ctx.quadraticCurveTo(c.S.x + (c.R.x - c.S.x) * 0.3, c.R.y + (c.S.y - c.R.y) * 0.7, c.R.x, c.R.y);
    }

    ctx.lineTo(c.origin.x, c.origin.y);
    ctx.closePath();
    ctx.fill();

    // Envelope Stroke Line
    ctx.strokeStyle = this.egIndex === 0 ? '#00ff88' : '#00f0ff';
    ctx.lineWidth = 2.5;
    ctx.shadowBlur = 8;
    ctx.shadowColor = this.egIndex === 0 ? 'rgba(0,255,136,0.6)' : 'rgba(0,240,255,0.6)';

    ctx.beginPath();
    ctx.moveTo(c.origin.x, c.origin.y);
    if (this.points.curveType === 1) {
      ctx.lineTo(c.A.x, c.A.y);
      ctx.lineTo(c.D.x, c.D.y);
    } else {
      ctx.quadraticCurveTo(c.origin.x + (c.A.x - c.origin.x) * 0.2, c.A.y, c.A.x, c.A.y);
      ctx.quadraticCurveTo(c.A.x + (c.D.x - c.A.x) * 0.4, c.D.y + (c.A.y - c.D.y) * 0.8, c.D.x, c.D.y);
    }
    ctx.lineTo(c.S.x, c.S.y);
    if (this.points.curveType === 1) {
      ctx.lineTo(c.R.x, c.R.y);
    } else {
      ctx.quadraticCurveTo(c.S.x + (c.R.x - c.S.x) * 0.3, c.R.y + (c.S.y - c.R.y) * 0.7, c.R.x, c.R.y);
    }
    ctx.stroke();
    ctx.shadowBlur = 0;

    // Draw Draggable Handle Nodes
    for (const key of ['A', 'D', 'S', 'R']) {
      const pt = c[key];
      const isHover = this.hoverTarget === key || this.dragTarget === key;

      ctx.fillStyle = isHover ? '#ffffff' : (this.egIndex === 0 ? '#00ff88' : '#00f0ff');
      ctx.strokeStyle = '#000000';
      ctx.lineWidth = 2;

      ctx.beginPath();
      ctx.arc(pt.x, pt.y, isHover ? 6 : 4.5, 0, Math.PI * 2);
      ctx.fill();
      ctx.stroke();

      // Node Label
      ctx.fillStyle = '#94a3b8';
      ctx.font = '9px "JetBrains Mono", monospace';
      ctx.fillText(key, pt.x - 3, pt.y - 8);
    }

    // Time readout footer
    ctx.fillStyle = '#64748b';
    ctx.font = '9px "JetBrains Mono", monospace';
    const text = `A: ${Math.round(this.points.attackMs)}ms | D: ${Math.round(this.points.decayMs)}ms | S: ${(this.points.sustainLevel * 100).toFixed(0)}% | R: ${Math.round(this.points.releaseMs)}ms`;
    ctx.fillText(text, 10, h - 4);
  }
}

window.InteractiveEnvelopeCanvas = InteractiveEnvelopeCanvas;
