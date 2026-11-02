#include "../components/audio_pcm5102/pcm_test.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>

using namespace smk::p4::audio;
int main() {
    float left[kBlockFrames]{}, right[kBlockFrames]{};
    int16_t pcm[kBlockSamples];
    left[0] = 1; right[0] = -1;
    left[1] = 0.5f; right[1] = -0.5f;
    left[2] = 2; right[2] = -2;
    assert(interleave(left, right, pcm, kBlockFrames, kBlockSamples));
    assert(pcm[0] == 32767 && pcm[1] == -32768);
    assert(pcm[2] == 16384 && pcm[3] == -16384);
    assert(pcm[4] == 32767 && pcm[5] == -32768);
    assert(interleave(left, nullptr, pcm, kBlockFrames, kBlockSamples));
    for (size_t i = 0; i < kBlockFrames; ++i) assert(pcm[2*i] == pcm[2*i+1]);
    assert(!interleave(nullptr, right, pcm, kBlockFrames, kBlockSamples));
    for (auto value : pcm) assert(value == 0);
    assert(!interleave(left, right, nullptr, kBlockFrames, kBlockSamples));
    assert(!interleave(left, right, pcm, 0, kBlockSamples));
    assert(!interleave(left, right, pcm, kBlockFrames-1, kBlockSamples));
    assert(!interleave(left, right, pcm, kBlockFrames, kBlockSamples-1));
    assert(!interleave(left, right, pcm, SIZE_MAX, SIZE_MAX));
    right[200] = std::numeric_limits<float>::quiet_NaN();
    assert(!interleave(left, right, pcm, kBlockFrames, kBlockSamples));
    for (auto value : pcm) assert(value == 0);
    right[200] = std::numeric_limits<float>::infinity();
    assert(!interleave(left, right, pcm, kBlockFrames, kBlockSamples));

    TestTone tone;
    assert(!tone.configure(0, 0.05f));
    assert(!tone.configure(24000, 0.05f));
    assert(!tone.configure(440, -0.1f));
    assert(!tone.configure(440, 0.2f));
    assert(!tone.configure(std::numeric_limits<float>::quiet_NaN(), 0.05f));
    assert(tone.configure(440, kTestAmplitude));
    assert(!tone.render(nullptr, kBlockFrames, true));
    assert(!tone.render(pcm, kBlockFrames+1, true));
    int peaks[5]{};
    unsigned crossings = 0;
    int16_t previous = 0;
    uint32_t absolute = 0;
    // Two complete cycles verify wrap, segment routing and block-boundary continuity.
    for (unsigned block = 0; block < (10*kSegmentFrames)/kBlockFrames; ++block) {
        assert(tone.render(pcm, kBlockFrames, true));
        for (size_t i = 0; i < kBlockFrames; ++i, ++absolute) {
            const uint32_t frame = absolute % (5*kSegmentFrames);
            const unsigned segment = frame/kSegmentFrames;
            const int16_t l = pcm[2*i], r = pcm[2*i+1];
            if (segment == 0 || segment == 4) assert(l == 0 && r == 0);
            if (segment == 1) assert(r == 0);
            if (segment == 2) assert(l == 0);
            if (segment == 3) assert(l == r);
            assert(std::abs(int(l)) <= 1639 && std::abs(int(r)) <= 1639);
            peaks[segment] = std::max({peaks[segment], std::abs(int(l)), std::abs(int(r))});
            if (frame % kSegmentFrames == 0 || frame % kSegmentFrames == kSegmentFrames-1)
                assert(l == 0 && r == 0);
            if (absolute >= kSegmentFrames && absolute < 2*kSegmentFrames && previous <= 0 && l > 0)
                ++crossings;
            previous = l;
        }
    }
    assert(crossings >= 879 && crossings <= 881); // 440Hz * 2 seconds
    assert(peaks[0] == 0 && peaks[4] == 0);
    assert(peaks[1] > 1600 && peaks[2] > 1600 && peaks[3] > 1600);
    assert(tone.configure(880, 0.01f)); // Restart resets phase and silence segment.
    assert(tone.render(pcm, kBlockFrames, true));
    for (auto value : pcm) assert(value == 0);
    for (unsigned i = 0; i < 400; ++i) {
        assert(tone.render(pcm, kBlockFrames, false));
        for (auto value : pcm) assert(value == 0);
    }
}
