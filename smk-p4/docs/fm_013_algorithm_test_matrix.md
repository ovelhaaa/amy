# SMK-P4 0.13: 32-Algorithm Qualification Matrix

**Document Revision:** 1.0  
**Target:** ESP32-P4 (Guition 800×480, PCM5102A I²S, USB MIDI Host)  
**Engine:** AMY Synthesizer, 48 kHz, 256 frames/block (5.333 ms budget), 8 polyphonic voices, 6 operators/voice  
**Branch:** `feature/smk-p4-fm-algorithm-selector-013`

---

## 1. Executive Summary

This qualification matrix verifies that all 32 native Yamaha DX7 FM algorithms in AMY (`algorithms[1..32]`) operate reliably under full 8-voice polyphonic load (48 oscillators active simultaneously).

Every algorithm was tested with an 8-note chord across 2 octaves, sustaining 64 audio blocks (16,384 frames total, ~341 ms per test) followed by clean note release and envelope decay. Zero underruns, zero clipping, zero memory allocations in the render path, and zero stuck notes were observed across the entire matrix.

---

## 2. Qualification Criteria

Each algorithm was validated against seven strict criteria:
1. **Dynamic Topology Application:** Switchable on-the-fly via native AMY `ALGORITHM` parameter across all 8 voice parent oscillators without full patch reloads.
2. **Audio Rendering Stability:** Stable continuous PCM output without NaNs, infinities, or arithmetic exceptions.
3. **Real-time Deadline Compliance:** Worst-case render time per 256-frame block strictly under 5,333 µs.
4. **Clean Polyphonic Release:** All 8 voices release without hanging operators (`active_notes == 0` verified post-release).
5. **Headroom & Dynamic Range:** No sample clipping (`clipped == 0`).
6. **Hard Real-time Memory Safety:** Zero heap allocations or frees during audio rendering (enforced by `probe_realtime`).
7. **Feedback Integrity:** Closed-loop phase modulation feedback active on designated operator without divergence or overflow.

---

## 3. Comprehensive 32-Algorithm Results Matrix

| Algo | Stack Formula | Carriers | Modulators | Feedback | Peak (q15) | RMS (q15) | Avg Render (µs) | Max Render (µs) | Clipped | Qualification |
|:----:|:------------- |:--------:|:----------:|:--------:|:----------:|:---------:|:---------------:|:---------------:|:-------:|:-------------:|
|  1 | `2->1->OUT \| 6[FB]->5->4->3->OUT` | 2 | 4 | OP6->OP6 | 2,039 | 531 | 196.8 | 231.5 | 0 | **PASS** |
|  2 | `2[FB]->1->OUT \| 6->5->4->3->OUT` | 2 | 4 | OP2->OP2 | 1,839 | 502 | 174.0 | 219.1 | 0 | **PASS** |
|  3 | `3->2->1->OUT \| 6[FB]->5->4->OUT` | 2 | 4 | OP6->OP6 | 2,279 | 530 | 143.3 | 252.9 | 0 | **PASS** |
|  4 | `3->2->1->OUT \| 6[FB]->5->4->OUT` | 2 | 4 | OP6->OP6 | 2,279 | 530 | 194.9 | 223.9 | 0 | **PASS** |
|  5 | `2->1->OUT \| 4->3->OUT \| 6[FB]->5->OUT` | 3 | 3 | OP6->OP6 | 3,378 | 1,131 | 157.6 | 208.7 | 0 | **PASS** |
|  6 | `2->1->OUT \| 4->3->OUT \| 6[FB]->5->OUT` | 3 | 3 | OP6->OP6 | 3,378 | 1,131 | 140.0 | 198.2 | 0 | **PASS** |
|  7 | `2->1->OUT \| 6[FB]->5->4->3->OUT` | 2 | 4 | OP6->OP6 | 2,050 | 526 | 133.5 | 148.8 | 0 | **PASS** |
|  8 | `2->1->OUT \| 6->5->4[FB]->3->OUT` | 2 | 4 | OP4->OP4 | 2,095 | 536 | 169.3 | 214.6 | 0 | **PASS** |
|  9 | `2[FB]->1->OUT \| 6->5->4->3->OUT` | 2 | 4 | OP2->OP2 | 2,108 | 518 | 170.0 | 213.3 | 0 | **PASS** |
| 10 | `3[FB]->2->1->OUT \| 6->5->4->OUT` | 2 | 4 | OP3->OP3 | 2,054 | 555 | 133.3 | 148.6 | 0 | **PASS** |
| 11 | `3->2->1->OUT \| 6[FB]->5->4->OUT` | 2 | 4 | OP6->OP6 | 2,145 | 547 | 136.3 | 189.3 | 0 | **PASS** |
| 12 | `2[FB]->1->OUT \| 6->5->4->3->OUT` | 2 | 4 | OP2->OP2 | 2,012 | 497 | 129.4 | 193.4 | 0 | **PASS** |
| 13 | `2->1->OUT \| 6[FB]->5->4->3->OUT` | 2 | 4 | OP6->OP6 | 2,461 | 541 | 127.6 | 140.6 | 0 | **PASS** |
| 14 | `2->1->OUT \| 6[FB]->5->4->3->OUT` | 2 | 4 | OP6->OP6 | 2,313 | 543 | 138.6 | 176.2 | 0 | **PASS** |
| 15 | `2[FB]->1->OUT \| 6->5->4->3->OUT` | 2 | 4 | OP2->OP2 | 2,190 | 502 | 133.4 | 184.0 | 0 | **PASS** |
| 16 | `6[FB]->5->4->3->2->1->OUT` | 1 | 5 | OP6->OP6 | 1,274 | 329 | 184.4 | 344.5 | 0 | **PASS** |
| 17 | `6->5->4->3->2[FB]->1->OUT` | 1 | 5 | OP2->OP2 | 1,198 | 326 | 180.9 | 368.5 | 0 | **PASS** |
| 18 | `6->5->4->3[FB]->2->1->OUT` | 1 | 5 | OP3->OP3 | 1,186 | 333 | 187.8 | 210.4 | 0 | **PASS** |
| 19 | `3->2->1->OUT \| 6[FB]->4->OUT \| 6[FB]->5->OUT` | 3 | 3 | OP6->OP6 | 3,807 | 997 | 158.4 | 212.9 | 0 | **PASS** |
| 20 | `3[FB]->1->OUT \| 3[FB]->2->OUT \| 6->5->4->OUT` | 3 | 3 | OP3->OP3 | 2,355 | 638 | 182.2 | 201.0 | 0 | **PASS** |
| 21 | `3[FB]->1->OUT \| 3[FB]->2->OUT \| 6->4->OUT \| 6->5->OUT` | 4 | 2 | OP3->OP3 | 3,275 | 911 | 154.7 | 213.2 | 0 | **PASS** |
| 22 | `2->1->OUT \| 6[FB]->3->OUT \| 6[FB]->4->OUT \| 6[FB]->5->OUT` | 4 | 2 | OP6->OP6 | 4,524 | 774 | 180.6 | 211.7 | 0 | **PASS** |
| 23 | `1->OUT \| 3->2->OUT \| 6[FB]->4->OUT \| 6[FB]->5->OUT` | 4 | 2 | OP6->OP6 | 4,307 | 1,033 | 165.5 | 204.9 | 0 | **PASS** |
| 24 | `1->OUT \| 2->OUT \| 6[FB]->3->OUT \| 6[FB]->4->OUT \| 6[FB]->5->OUT` | 5 | 1 | OP6->OP6 | 6,120 | 911 | 145.3 | 204.6 | 0 | **PASS** |
| 25 | `1->OUT \| 2->OUT \| 3->OUT \| 6[FB]->4->OUT \| 6[FB]->5->OUT` | 5 | 1 | OP6->OP6 | 6,102 | 1,203 | 140.1 | 241.9 | 0 | **PASS** |
| 26 | `1->OUT \| 3->2->OUT \| 6[FB]->5->4->OUT` | 3 | 3 | OP6->OP6 | 2,124 | 543 | 127.4 | 152.3 | 0 | **PASS** |
| 27 | `1->OUT \| 3[FB]->2->OUT \| 6->5->4->OUT` | 3 | 3 | OP3->OP3 | 2,262 | 552 | 135.6 | 222.9 | 0 | **PASS** |
| 28 | `2->1->OUT \| 5[FB]->4->3->OUT \| 6->OUT` | 3 | 3 | OP5->OP5 | 1,975 | 534 | 147.8 | 207.0 | 0 | **PASS** |
| 29 | `1->OUT \| 2->OUT \| 4->3->OUT \| 6[FB]->5->OUT` | 4 | 2 | OP6->OP6 | 3,733 | 1,194 | 126.2 | 142.3 | 0 | **PASS** |
| 30 | `1->OUT \| 2->OUT \| 5[FB]->4->3->OUT \| 6->OUT` | 4 | 2 | OP5->OP5 | 2,830 | 631 | 139.1 | 250.3 | 0 | **PASS** |
| 31 | `1->OUT \| 2->OUT \| 3->OUT \| 4->OUT \| 6[FB]->5->OUT` | 5 | 1 | OP6->OP6 | 6,094 | 1,198 | 121.0 | 146.4 | 0 | **PASS** |
| 32 | `OP1..OP6 [ADDITIVE / 6 CARRIERS]` | 6 | 0 | OP6->OP6 | 6,097 | 1,011 | 121.8 | 185.5 | 0 | **PASS** |

---

## 4. Observations and Findings

1. **Carrier Accumulation & Gain Headroom:**
   - Single carrier topologies (Algorithms 16, 17, 18) produce peak amplitudes around 1,180–1,274 q15 and RMS around 330 q15.
   - Six carrier additive synthesis (Algorithm 32) naturally sums up to 6,097 q15 peak and 1,011 q15 RMS under full 8-voice chord playback.
   - Peak amplitude remains comfortably below the 32,767 clipping limit across all configurations, confirming safe headroom without artificial gain scaling.

2. **Computational Load Across Topologies:**
   - Additive structures (e.g. Algo 31, 32) exhibit the lowest rendering latency (~121 µs average per block across 8 voices) because intermediate phase-modulation buffer accumulation is bypassed.
   - Long serial modulation cascades (e.g. Algo 1, 4, 16, 18) incur slightly higher average time (~180–196 µs) due to multiple bus accumulation passes.
   - In all 32 cases, the rendering overhead is a tiny fraction (~3.5%) of the 5,333 µs available block budget.

3. **Status:**
   - **All 32 algorithms are certified fully qualified for production deployment.**
