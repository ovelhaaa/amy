#include "amy_adapter.h"
#include "audio_config.h"
#include "synth_config.h"
#include "diagnostics.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_timer.h>
#include <cstring>
#include <cstdlib>
#include <new>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif

namespace smk {
namespace {
enum CommandType : uint8_t {
    NoteOn, NoteOff, Bend, CC, AllOff, Panic, Filter, Wave, Envelope,
    Portamento, Preset, Message, FmIndex, FmFeedback, FmRatio, FmAlgorithm,
    Chorus, Reverb, Delay, Mono
};
struct Command {
    uint8_t type;
    uint8_t channel;
    uint16_t id;
    uint32_t generation;
    uint32_t timestamp_us;
    union { float values[4]; char message[config::kSynthMessageBytes]; } data;
};
bool release(const Command& c) {
    return c.type == NoteOff || c.type == AllOff ||
           (c.type == NoteOn && c.data.values[0] == 0) ||
           (c.type == CC && ((c.id == 64 && c.data.values[0] < 64) || c.id == 120 || c.id == 123));
}
bool musical(const Command& c) {
    return c.type <= AllOff || c.type == Message;
}
constexpr size_t kSamples = config::kBlockSize * 2;
static_assert(config::kSynthReleaseReserve < config::kSynthCommandCapacity);
static_assert(config::kSynthPcmBlocks > 0);
}

struct AmyAdapter::State {
    // Multiple task producers; no ISR callers. Copies and counters only under mux.
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    Command commands[config::kSynthCommandCapacity]{};
    size_t head = 0, count = 0;
    std::atomic<uint32_t> requested_generation{0};
    uint32_t applied_generation = 0; // Synthesis owner only.
    uint32_t panic_timestamp_us = 0; // Protected by mux.
    SemaphoreHandle_t wake = xSemaphoreCreateBinary();
    TaskHandle_t worker = nullptr;
    std::atomic<bool> running{false}, active{false}, failed{false};
    std::atomic<bool> recovery_requested{false};
    bool started = false, prepared = false;
    struct Block { int16_t samples[kSamples]; uint32_t generation; } pcm[config::kSynthPcmBlocks]{};
    // Single producer/consumer. A slot is reused only after consumer's release.
    std::atomic<uint32_t> written{0}, read{0};
    int16_t output[kSamples]{}; // Output task owns this copy, including fade-in.
    uint64_t average_us = 0;

    ~State() { if (wake) vSemaphoreDelete(wake); }
    void enqueue(Command& c) {
        auto& diag = Diagnostics::instance().counters();
        c.timestamp_us = static_cast<uint32_t>(esp_timer_get_time());
        portENTER_CRITICAL(&mux);
        if (c.type == Panic) {
            panic_timestamp_us = c.timestamp_us;
            requested_generation.fetch_add(1, std::memory_order_release);
        } else {
            const size_t limit = release(c) ? config::kSynthCommandCapacity :
                                 config::kSynthCommandCapacity - config::kSynthReleaseReserve;
            if (count >= limit) {
                diag.synth_commands_dropped.fetch_add(1, std::memory_order_relaxed);
                if (release(c)) {
                    panic_timestamp_us = c.timestamp_us;
                    requested_generation.fetch_add(1, std::memory_order_release);
                    recovery_requested.store(true, std::memory_order_release);
                }
            } else {
                c.generation = requested_generation.load(std::memory_order_relaxed);
                commands[(head + count) % config::kSynthCommandCapacity] = c;
                ++count;
                if (count > diag.synth_queue_high_water.load(std::memory_order_relaxed))
                    diag.synth_queue_high_water.store(count, std::memory_order_relaxed);
            }
        }
        portEXIT_CRITICAL(&mux);
        xSemaphoreGive(wake);
    }
    bool take(Command& c) {
        portENTER_CRITICAL(&mux);
        const uint32_t generation = requested_generation.load(std::memory_order_relaxed);
        if (applied_generation != generation) {
            c = {};
            c.type = Panic;
            c.generation = generation;
            c.timestamp_us = panic_timestamp_us;
        } else if (count) {
            c = commands[head];
            head = (head + 1) % config::kSynthCommandCapacity;
            --count;
        } else {
            portEXIT_CRITICAL(&mux);
            return false;
        }
        portEXIT_CRITICAL(&mux);
        return true;
    }
};

AmyAdapter::AmyAdapter() = default;

AmyAdapter::~AmyAdapter() {
    onAudioStopped();
    // Lifecycle only: caller must stop/join the PCM consumer before destruction.
    if (state_) {
        while (state_->active.load(std::memory_order_acquire)) vTaskDelay(1);
    }
    if (initialized_) endEngine();
    if (state_) { state_->~State(); std::free(state_); }
}

bool AmyAdapter::begin(uint32_t sample_rate_hz) {
    if (state_ || sample_rate_hz != config::kSampleRateHz) return false;
#ifdef ESP_PLATFORM
    void* memory = heap_caps_malloc(sizeof(State), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
    void* memory = std::malloc(sizeof(State));
#endif
    if (!memory) return false;
    state_ = new (memory) State;
    if (!state_->wake) return false;
    (void)Diagnostics::instance();
    initialized_ = true; // beginEngine may partially initialize AMY.
    state_->prepared = beginEngine(sample_rate_hz);
    return state_->prepared;
}

bool AmyAdapter::startWorker(uint8_t core, uint8_t priority, uint32_t stack_bytes) {
    if (!state_ || !state_->prepared || !state_->wake || state_->started || !stack_bytes) return false;
    state_->started = true;
    state_->active.store(true);
    state_->running.store(true);
    if (xTaskCreatePinnedToCore(workerRoutine, "amy_owner", stack_bytes, this,
                               priority, &state_->worker, core) != pdPASS) {
        state_->active.store(false);
        state_->running.store(false);
        state_->failed.store(true);
        return false;
    }
    return true;
}

void AmyAdapter::onAudioStopped() {
    if (!state_) return;
    state_->running.store(false, std::memory_order_release);
    if (state_->wake) xSemaphoreGive(state_->wake);
}

bool AmyAdapter::takeRecoveryRequest() {
    return state_ && state_->recovery_requested.exchange(false, std::memory_order_acq_rel);
}

void AmyAdapter::workerRoutine(void* arg) {
    auto& self = *static_cast<AmyAdapter*>(arg);
    auto& s = *self.state_;
    while (s.running.load(std::memory_order_acquire)) {
        if (!self.serviceBlock()) xSemaphoreTake(s.wake, portMAX_DELAY);
    }
    self.executePanic();
    s.active.store(false, std::memory_order_release);
    vTaskDelete(nullptr);
}

void AmyAdapter::submit(uint8_t type, uint8_t channel, uint16_t id, float a, float b, float c, float d) {
    if (!state_ || !state_->wake) return;
    Command command{};
    command.type = type; command.channel = channel; command.id = id;
    command.data.values[0] = a; command.data.values[1] = b;
    command.data.values[2] = c; command.data.values[3] = d;
    state_->enqueue(command);
}

void AmyAdapter::noteOn(uint8_t ch, uint8_t note, uint8_t vel) { submit(NoteOn, ch, note, vel); }
void AmyAdapter::noteOff(uint8_t ch, uint8_t note) { submit(NoteOff, ch, note); }
void AmyAdapter::pitchBend(uint8_t ch, int16_t bend) { submit(Bend, ch, 0, bend); }
void AmyAdapter::controlChange(uint8_t ch, uint8_t cc, uint8_t value) { submit(CC, ch, cc, value); }
void AmyAdapter::allNotesOff() { submit(AllOff, 0, 0); }
void AmyAdapter::panic() { submit(Panic, 0, 0); }
void AmyAdapter::setFilter(uint8_t id, float cutoff, float res) { submit(Filter, id, 0, cutoff, res); }
void AmyAdapter::setOscillatorWaveform(uint8_t id, uint8_t wave) { submit(Wave, id, wave); }
void AmyAdapter::setEnvelope(uint8_t id, float a, float d, float s, float r) { submit(Envelope, id, 0, a, d, s, r); }
void AmyAdapter::setPortamento(uint8_t id, uint16_t ms) { submit(Portamento, id, ms); }
void AmyAdapter::loadPreset(uint8_t id, uint16_t preset, uint8_t voices) { submit(Preset, id, preset, voices); }
void AmyAdapter::setFmModIndex(uint8_t id, float value) { submit(FmIndex, id, 0, value); }
void AmyAdapter::setFmFeedback(uint8_t id, float value) { submit(FmFeedback, id, 0, value); }
void AmyAdapter::setFmRatio(uint8_t id, float value) { submit(FmRatio, id, 0, value); }
void AmyAdapter::setFmAlgorithm(uint8_t id, uint8_t value) { submit(FmAlgorithm, id, value); }
void AmyAdapter::setChorus(float depth, float rate, float level) { submit(Chorus, 0, 0, depth, rate, level); }
void AmyAdapter::setReverb(float room, float damp, float mix) { submit(Reverb, 0, 0, room, damp, mix); }
void AmyAdapter::setDelay(float ms, float feedback, float mix) { submit(Delay, 0, 0, ms, feedback, mix); }
void AmyAdapter::setMonoMode(bool enable) { submit(Mono, 0, 0, enable ? 1 : 0); }

void AmyAdapter::sendAmyMessage(const char* message) {
    if (!state_ || !state_->wake || !message) return;
    Command command{};
    command.type = Message;
    size_t length = 0;
    while (length < sizeof(command.data.message) && message[length]) ++length;
    if (length == sizeof(command.data.message)) {
        Diagnostics::instance().counters().synth_commands_dropped.fetch_add(1, std::memory_order_relaxed);
        return; // Reject entire message; truncation could change wire semantics.
    }
    std::memcpy(command.data.message, message, length + 1);
    state_->enqueue(command);
}

bool AmyAdapter::serviceBlock() {
    auto& s = *state_;
    const uint32_t written = s.written.load(std::memory_order_relaxed);
    if (written - s.read.load(std::memory_order_acquire) >= config::kSynthPcmBlocks) return false;
    const int64_t start_us = esp_timer_get_time();
    auto& diag = Diagnostics::instance().counters();
    // Command phase belongs to the synthesis owner, outside the output task.
    // AMY may allocate/load patches here; I2S never waits for that work.
    for (uint16_t i = 0; i < config::kSynthCommandsPerBlock; ++i) {
        Command c{};
        if (!s.take(c)) break;
        if (c.type != Panic && musical(c) && c.generation != s.applied_generation) {
            diag.synth_commands_dropped.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        const uint32_t wait_us = static_cast<uint32_t>(esp_timer_get_time()) - c.timestamp_us;
        if (wait_us > diag.synth_max_command_wait_us.load(std::memory_order_relaxed))
            diag.synth_max_command_wait_us.store(wait_us, std::memory_order_relaxed);
        const float* v = c.data.values;
        switch (c.type) {
            case NoteOn: executeNoteOn(c.channel, static_cast<uint8_t>(c.id), static_cast<uint8_t>(v[0])); break;
            case NoteOff: executeNoteOff(c.channel, static_cast<uint8_t>(c.id)); break;
            case Bend: executePitchBend(c.channel, static_cast<int16_t>(v[0])); break;
            case CC: executeControlChange(c.channel, static_cast<uint8_t>(c.id), static_cast<uint8_t>(v[0])); break;
            case AllOff: executeAllNotesOff(); break;
            case Panic: executePanic(); s.applied_generation = c.generation;
                        diag.synth_panics.fetch_add(1, std::memory_order_relaxed); break;
            case Filter: executeFilter(c.channel, v[0], v[1]); break;
            case Wave: executeWaveform(c.channel, static_cast<uint8_t>(c.id)); break;
            case Envelope: executeEnvelope(c.channel, v[0], v[1], v[2], v[3]); break;
            case Portamento: executePortamento(c.channel, c.id); break;
            case Preset: executePreset(c.channel, c.id, static_cast<uint8_t>(v[0])); break;
            case Message: executeMessage(c.data.message); break;
            case FmIndex: executeFmModIndex(c.channel, v[0]); break;
            case FmFeedback: executeFmFeedback(c.channel, v[0]); break;
            case FmRatio: executeFmRatio(c.channel, v[0]); break;
            case FmAlgorithm: executeFmAlgorithm(c.channel, static_cast<uint8_t>(c.id)); break;
            case Chorus: executeChorus(v[0], v[1], v[2]); break;
            case Reverb: executeReverb(v[0], v[1], v[2]); break;
            case Delay: executeDelay(v[0], v[1], v[2]); break;
            case Mono: mono_mode_ = v[0] != 0; mono_stack_size_ = 0; break;
        }
    }
    mono_snapshot_.store(mono_mode_, std::memory_order_relaxed);
    int16_t* samples = renderEngine();
    if (!samples) {
        s.failed.store(true, std::memory_order_release);
        s.running.store(false, std::memory_order_release);
        return true;
    }
    auto& block = s.pcm[written % config::kSynthPcmBlocks];
    std::memcpy(block.samples, samples, sizeof(block.samples));
    block.generation = s.applied_generation;
    const uint32_t elapsed_us = static_cast<uint32_t>(esp_timer_get_time() - start_us);
    if (elapsed_us > diag.max_render_us.load(std::memory_order_relaxed))
        diag.max_render_us.store(elapsed_us, std::memory_order_relaxed);
    s.average_us = s.average_us ? (s.average_us * 15 + elapsed_us) / 16 : elapsed_us;
    diag.avg_render_us.store(static_cast<uint32_t>(s.average_us), std::memory_order_relaxed);
    diag.frames_rendered.fetch_add(config::kBlockSize, std::memory_order_relaxed);
    s.written.store(written + 1, std::memory_order_release);
    return true;
}

int16_t* AmyAdapter::render() {
    if (!state_ || state_->failed.load(std::memory_order_acquire)) return nullptr;
    auto& s = *state_;
    bool copied = false;
    // At most two blocks inspected; never wait, parse commands or call AMY.
    for (uint8_t i = 0; i < config::kSynthPcmBlocks; ++i) {
        const uint32_t read = s.read.load(std::memory_order_relaxed);
        if (read == s.written.load(std::memory_order_acquire)) break;
        const auto& block = s.pcm[read % config::kSynthPcmBlocks];
        if (block.generation == s.requested_generation.load(std::memory_order_acquire)) {
            std::memcpy(s.output, block.samples, sizeof(s.output));
            copied = true;
        }
        s.read.store(read + 1, std::memory_order_release);
        if (copied) break;
    }
    if (!copied) {
        std::memset(s.output, 0, sizeof(s.output));
        Diagnostics::instance().counters().synth_pcm_starvations.fetch_add(1, std::memory_order_relaxed);
    }
    xSemaphoreGive(s.wake);
    return s.output;
}
} // namespace smk
