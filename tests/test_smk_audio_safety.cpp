#include "audio_task.h"
#include "pcm5102_output.h"
#include "audio_config.h"
#include "diagnostics.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>

namespace smk {
Diagnostics& Diagnostics::instance() { static Diagnostics instance; return instance; }
DiagnosticCounters& Diagnostics::counters() { return counters_; }
}
using namespace smk;

struct Engine : SynthEngine {
    int renders = 0;
    int stop_notifications = 0;
    bool buffered = false;
    bool null_buffer = false;
    int stop_after = 5;
    std::array<int16_t, config::kBlockSize * 2> samples{};
    bool begin(uint32_t) override { return true; }
    void noteOn(uint8_t, uint8_t, uint8_t) override {}
    void noteOff(uint8_t, uint8_t) override {}
    void pitchBend(uint8_t, int16_t) override {}
    void controlChange(uint8_t, uint8_t, uint8_t) override {}
    void allNotesOff() override {}
    void panic() override {}
    int16_t* render() override {
        ++renders;
        assert(renders <= 6); // A write fault must not cause a busy render loop.
        if (renders == stop_after) AudioTask::stop();
        samples.fill(12000);
        return null_buffer ? nullptr : samples.data();
    }
    uint16_t blockSize() const override { return config::kBlockSize; }
    float renderLoad() const override { return 0; }
    uint32_t activeVoices() const override { return 0; }
    bool bufferedOutput() const override { return buffered; }
    void onAudioStopped() override { ++stop_notifications; }
};

struct Output : AudioOutput {
    bool write_ok = true;
    bool stop_ok = true;
    int stops = 0, writes = 0;
    std::vector<int16_t> samples;
    bool begin() override { return true; }
    bool start() override { return true; }
    bool stop() override { ++stops; return stop_ok; }
    bool write(const int16_t* data, size_t frames) override {
        ++writes;
        samples.insert(samples.end(), data, data + frames * 2);
        return write_ok;
    }
    uint32_t underrunCount() const override { return 0; }
    uint32_t framesWritten() const override { return samples.size() / 2; }
};

static void testOutput() {
    mock_i2s::state = {};
    {
        PCM5102Output output(15, 16, 17, 18);
        assert(output.begin() && output.start());
        assert(mock_i2s::state.preloads == config::kDmaBufferCount);
        assert(mock_i2s::state.standard.clk_cfg.sample_rate_hz == config::kSampleRateHz);
        assert(mock_i2s::state.mute_level == 1);
        int16_t silence[config::kBlockSize * 2]{};
        assert(output.write(silence, config::kBlockSize));
        assert(output.framesWritten() == config::kBlockSize);
        assert(mock_i2s::state.last_timeout_ms == config::kAudioWriteTimeoutMs);
        mock_i2s::state.short_write = true;
        assert(!output.write(silence, config::kBlockSize));
        assert(output.underrunCount() == 1);
        assert(output.framesWritten() == config::kBlockSize);
        assert(output.stop() && output.stop());
        assert(mock_i2s::state.mute_level == 0 && mock_i2s::state.disables == 1);
    }
    assert(mock_i2s::state.deletes == 1);
    for (int fault = 0; fault < 3; ++fault) {
        mock_i2s::state = {};
        PCM5102Output output(15, 16, 17, 18);
        mock_i2s::state.fail_init = fault == 0;
        mock_i2s::state.short_preload = fault == 1;
        mock_i2s::state.fail_enable = fault == 2;
        if (fault == 0) assert(!output.begin());
        else assert(output.begin() && !output.start());
        assert(!mock_i2s::state.enabled && mock_i2s::state.mute_level == 0);
    }
}

static void testTask() {
    Engine engine;
    Output output;
    assert(!AudioTask::start(nullptr, &output, 1, 24, 16384));
    mock_task::create_result = pdFALSE;
    assert(!AudioTask::start(&engine, &output, 1, 24, 16384));
    assert(AudioTask::failed() && engine.renders == 0);
    mock_task::create_result = pdPASS;
    assert(AudioTask::start(&engine, &output, 1, 24, 16384));
    mock_task::run();
    assert(!AudioTask::failed() && output.stops == 1 && output.writes == 5);
    assert(output.samples.front() == 0);
    for (size_t i = 0; i < output.samples.size() / 2; ++i) {
        assert(output.samples[i * 2] == output.samples[i * 2 + 1]);
        assert(output.samples[i * 2] >= 0 && output.samples[i * 2] <= 12000);
        if (i) assert(output.samples[i * 2] >= output.samples[(i - 1) * 2]);
    }
    assert(output.samples[config::kAudioFadeInFrames * 2] == 12000);

    Engine write_engine;
    Output bad_output;
    bad_output.write_ok = false;
    assert(AudioTask::start(&write_engine, &bad_output, 1, 24, 16384));
    mock_task::run();
    assert(AudioTask::failed() && write_engine.renders == 1 && bad_output.stops == 1);
    assert(write_engine.stop_notifications == 1);
    Engine null_engine;
    null_engine.null_buffer = true;
    Output null_output;
    assert(AudioTask::start(&null_engine, &null_output, 1, 24, 16384));
    mock_task::run();
    assert(AudioTask::failed() && null_output.writes == 0 && null_output.stops == 1);
    Engine stop_engine;
    stop_engine.stop_after = 1;
    Output stop_output;
    stop_output.stop_ok = false;
    assert(AudioTask::start(&stop_engine, &stop_output, 1, 24, 16384));
    mock_task::run();
    assert(AudioTask::failed() && stop_output.stops == 1);
    Engine buffered_engine;
    buffered_engine.buffered = true;
    buffered_engine.stop_after = 1;
    Output buffered_output;
    auto& diag = Diagnostics::instance().counters();
    diag.max_render_us = 12345;
    diag.frames_rendered = 777;
    assert(AudioTask::start(&buffered_engine, &buffered_output, 1, 24, 16384));
    mock_task::run();
    assert(!AudioTask::failed() && buffered_engine.stop_notifications == 1);
    assert(diag.max_render_us == 12345 && diag.frames_rendered == 777);
    assert(AudioTask::getMaxRenderUs() == 12345);
}

int main() {
    testOutput(); testTask();
    std::puts("PASS: DMA silence before enable, I2S faults, task creation failure, bounded write failure and stereo fade-in");
}
