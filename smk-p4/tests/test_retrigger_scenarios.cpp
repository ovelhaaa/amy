#include "amy_runtime.h"
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>

extern "C" {
#include "amy.h"
}

using namespace smk::p4::amy;

struct ScenarioResult {
    std::string name;
    int onset_jump = 0;
    int off_jump = 0;
    int max_delta = 0;
    int max_amp = 0;
    double rms = 0.0;
    int clipped = 0;
};

class ScenarioRunner {
public:
    explicit ScenarioRunner(uint16_t patch) : patch_(patch) {
        assert(rt_.begin());
        rt_.commands().push({CommandType::Patch, patch_, 0, 0, 0, 0});
        rt_.apply(0);
        // warm up
        for (int b = 0; b < 10; ++b) {
            now_ms_ += 256 * 1000 / 48000;
            rt_.apply(now_ms_);
            rt_.render();
        }
    }

    ~ScenarioRunner() {
        rt_.end();
    }

    void noteOn(uint8_t note, uint8_t vel = 100) {
        uint8_t pkt[] = {9, 0x90, note, vel};
        now_ms_ += 1;
        rt_.usbInput().transfer(pkt, 4, now_ms_);
    }

    void noteOff(uint8_t note) {
        uint8_t pkt[] = {8, 0x80, note, 0};
        now_ms_ += 1;
        rt_.usbInput().transfer(pkt, 4, now_ms_);
    }

    void cc(uint8_t ctrl, uint8_t val) {
        uint8_t pkt[] = {0xB, 0xB0, ctrl, val};
        now_ms_ += 1;
        rt_.usbInput().transfer(pkt, 4, now_ms_);
    }

    void blocks(int count, int edge_type, ScenarioResult& res) {
        for (int b = 0; b < count; ++b) {
            now_ms_ += 256 * 1000 / 48000;
            rt_.apply(now_ms_);
            const int16_t* pcm = rt_.render();
            assert(pcm);

            if (b == 0 && edge_type != 0) {
                int jump = std::abs(pcm[0] - prev_sample_);
                if (edge_type > 0) res.onset_jump = std::max(res.onset_jump, jump);
                else res.off_jump = std::max(res.off_jump, jump);
            }

            last_block_silent_ = true;
            for (int f = 0; f < 256; ++f) {
                for (int c = 0; c < 2; ++c) {
                    int val = pcm[f * 2 + c];
                    if(val != 0) last_block_silent_ = false;
                    int delta = std::abs(val - (c == 0 ? prev_sample_ : prev_sample_r_));
                    res.max_delta = std::max(res.max_delta, delta);
                    res.max_amp = std::max(res.max_amp, std::abs(val));
                    sum_sq_ += double(val) * double(val);
                    total_samples_++;
                    if (c == 0) prev_sample_ = val;
                    else prev_sample_r_ = val;
                }
            }
        }
    }

    ScenarioResult runA() {
        // Scenario A: Isolated note -> sustain -> note off -> complete release
        ScenarioResult r{"Scenario A: Isolated Note"};
        resetStats();
        noteOn(60);
        blocks(30, 1, r);
        noteOff(60);
        blocks(15000, -1, r);
        assert(last_block_silent_);
        finishRms(r);
        return r;
    }

    ScenarioResult runB() {
        // Scenario B: Slow repetition (tail finished before next note)
        ScenarioResult r{"Scenario B: Slow Repetition"};
        resetStats();
        for (int n = 0; n < 4; ++n) {
            noteOn(60);
            blocks(20, 1, r);
            noteOff(60);
            blocks(15000, -1, r); // FM tails can exceed one second.
            assert(last_block_silent_);
            // Output silence is not equivalent to operator last_amp == 0.
            uint16_t voice; assert(instrument_get_num_voices(patchIndex(patch_)*kVoices+1,&voice)==1);
            unsigned residual=0;
            for(unsigned o=0;o<kOscillators;++o)
                if(osc_to_voice[o]==voice && msynth[o] && msynth[o]->last_amp!=0) ++residual;
            assert(residual>0); // Exercise the frozen-operator case explicitly.
        }
        finishRms(r);
        return r;
    }

    ScenarioResult runC() {
        // Scenario C: Rapid repetition (new note before tail finished)
        ScenarioResult r{"Scenario C: Rapid Repetition"};
        resetStats();
        for (int n = 0; n < 8; ++n) {
            noteOn(60);
            blocks(12, 1, r);
            noteOff(60);
            blocks(3, -1, r); // 16 ms release only
        }
        blocks(50, 0, r);
        finishRms(r);
        return r;
    }

    ScenarioResult runD() {
        // Scenario D: Retrigger without Note Off
        ScenarioResult r{"Scenario D: Retrigger Without Off"};
        resetStats();
        noteOn(60);
        blocks(12, 1, r);
        for (int n = 0; n < 4; ++n) {
            noteOn(60); // same note retrigger
            blocks(12, 1, r);
        }
        noteOff(60);
        blocks(50, -1, r);
        finishRms(r);
        return r;
    }

    ScenarioResult runE() {
        // Scenario E: Alternating notes C4 -> D4 -> C4 -> D4
        ScenarioResult r{"Scenario E: Alternating Notes"};
        resetStats();
        uint8_t notes[] = {60, 62};
        for (int n = 0; n < 8; ++n) {
            uint8_t nt = notes[n % 2];
            noteOn(nt);
            blocks(8, 1, r);
            noteOff(nt);
            blocks(3, -1, r);
        }
        blocks(50, 0, r);
        finishRms(r);
        return r;
    }

    ScenarioResult runF() {
        // Scenario F: Four voices (chord of 4 notes, rapid repetition)
        ScenarioResult r{"Scenario F: Four-Voice Chords"};
        resetStats();
        uint8_t chord[] = {60, 64, 67, 72};
        for (int n = 0; n < 4; ++n) {
            for (auto c : chord) noteOn(c);
            blocks(12, 1, r);
            for (auto c : chord) noteOff(c);
            blocks(3, -1, r);
        }
        blocks(50, 0, r);
        finishRms(r);
        return r;
    }

    ScenarioResult runG() {
        // Scenario G: Sustain (CC64 on, repeated notes, CC64 off)
        ScenarioResult r{"Scenario G: Sustain Pedal"};
        resetStats();
        cc(64, 127); // pedal down
        for (int n = 0; n < 4; ++n) {
            noteOn(60 + n * 2);
            blocks(8, 1, r);
            noteOff(60 + n * 2);
            blocks(4, -1, r);
        }
        cc(64, 0); // pedal up
        blocks(100, 0, r);
        finishRms(r);
        return r;
    }

    ScenarioResult runH() {
        // Scenario H: Patch change during active sound
        ScenarioResult r{"Scenario H: Patch Change"};
        resetStats();
        noteOn(60);
        blocks(12, 1, r);
        // Switch patch to the other patch
        uint16_t other = (patch_ == 135) ? 128 : 135;
        rt_.commands().push({CommandType::Patch, other, 0, 0, 0, 0});
        rt_.apply(now_ms_);
        blocks(10, 0, r);
        noteOn(64);
        blocks(12, 1, r);
        noteOff(64);
        blocks(50, -1, r);
        finishRms(r);
        return r;
    }

private:
    void resetStats() {
        sum_sq_ = 0.0;
        total_samples_ = 0;
    }
    void finishRms(ScenarioResult& r) {
        if (total_samples_ > 0) r.rms = std::sqrt(sum_sq_ / total_samples_);
        r.clipped = rt_.snapshot().clipped;
    }

    Runtime rt_;
    uint16_t patch_ = 135;
    uint32_t now_ms_ = 0;
    int16_t prev_sample_ = 0;
    int16_t prev_sample_r_ = 0;
    double sum_sq_ = 0.0;
    size_t total_samples_ = 0;
    bool last_block_silent_ = true;
};

void run_all_scenarios(uint16_t patch) {
    std::printf("\n=======================================================================\n");
    std::printf("REPRODUCTION MATRIX - PATCH %d BASELINE\n", patch);
    std::printf("=======================================================================\n");

    auto print_res = [](const ScenarioResult& r) {
        assert(r.max_amp > 0 && r.rms > 0 && std::isfinite(r.rms));
        assert(r.clipped == 0);
        std::printf("%-32s | OnsetJump: %4d | OffJump: %4d | MaxDelta: %4d | Peak: %5d | RMS: %6.1f | Clip: %d\n",
                    r.name.c_str(), r.onset_jump, r.off_jump, r.max_delta, r.max_amp, r.rms, r.clipped);
    };

    { ScenarioRunner runner(patch); print_res(runner.runA()); }
    { ScenarioRunner runner(patch); print_res(runner.runB()); }
    { ScenarioRunner runner(patch); print_res(runner.runC()); }
    { ScenarioRunner runner(patch); print_res(runner.runD()); }
    { ScenarioRunner runner(patch); print_res(runner.runE()); }
    { ScenarioRunner runner(patch); print_res(runner.runF()); }
    { ScenarioRunner runner(patch); print_res(runner.runG()); }
    { ScenarioRunner runner(patch); print_res(runner.runH()); }
}

int main() {
    run_all_scenarios(135);
    run_all_scenarios(128);
    return 0;
}
