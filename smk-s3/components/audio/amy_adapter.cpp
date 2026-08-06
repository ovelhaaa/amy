#include "amy_adapter.h"
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <cstdio>

extern "C" {
#include "amy.h"
void amy_platform_init() {}
void amy_platform_deinit() {}
}

namespace smk {

static const char* TAG = "AMY_ADAPTER";

AmyAdapter::AmyAdapter() {}

AmyAdapter::~AmyAdapter() {
    amy_stop();
}

bool AmyAdapter::begin(uint32_t sample_rate_hz) {
    ESP_LOGI(TAG, "Initializing AMY Synth Engine");
    
    amy_config_t config = amy_default_config();
    config.audio = AMY_AUDIO_IS_NONE;
    config.midi = AMY_MIDI_IS_NONE;
    config.platform.multicore = 1;
    config.platform.multithread = 0;
    config.features.default_synths = 1;
    config.features.reverb = 1;
    config.features.chorus = 1;
    config.features.echo = 1;
    config.features.startup_bleep = 0;
    config.ram_caps_events = MALLOC_CAP_SPIRAM;
    config.ram_caps_synth = MALLOC_CAP_SPIRAM;
    config.ram_caps_delay = MALLOC_CAP_SPIRAM;
    config.ram_caps_sample = MALLOC_CAP_SPIRAM;
    config.ram_caps_sysex = MALLOC_CAP_SPIRAM;
    config.overload_threshold = 0.95f;
    config.overload_ms = 500;
    config.max_oscs = 120;
    
    amy_start(config);
    return true;
}

void AmyAdapter::noteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    uint8_t msg[3] = { (uint8_t)(0x90 | (channel & 0x0F)), note, velocity };
    convert_midi_bytes_to_messages(msg, 3, 0);
}

void AmyAdapter::noteOff(uint8_t channel, uint8_t note) {
    uint8_t msg[3] = { (uint8_t)(0x80 | (channel & 0x0F)), note, 0 };
    convert_midi_bytes_to_messages(msg, 3, 0);
}

void AmyAdapter::pitchBend(uint8_t channel, int16_t value) {
    uint8_t lsb = value & 0x7F;
    uint8_t msb = (value >> 7) & 0x7F;
    uint8_t msg[3] = { (uint8_t)(0xE0 | (channel & 0x0F)), lsb, msb };
    convert_midi_bytes_to_messages(msg, 3, 0);
}

void AmyAdapter::controlChange(uint8_t channel, uint8_t controller, uint8_t value) {
    uint8_t msg[3] = { (uint8_t)(0xB0 | (channel & 0x0F)), controller, value };
    convert_midi_bytes_to_messages(msg, 3, 0);
}

void AmyAdapter::allNotesOff() {
    all_notes_off();
}

void AmyAdapter::panic() {
    all_notes_off();
    amy_reset_oscs();
}

int16_t* AmyAdapter::render() {
    return amy_simple_fill_buffer();
}

uint16_t AmyAdapter::blockSize() const {
    return AMY_BLOCK_SIZE;
}

float AmyAdapter::renderLoad() const {
    return amy_get_render_load();
}

uint32_t AmyAdapter::activeVoices() const {
    return 0; // Not strictly tracked in simple API
}

void AmyAdapter::setFilter(uint8_t osc_id, float cutoff_hz, float resonance) {
    char msg[64];
    snprintf(msg, sizeof(msg), "v%dF%.2f,0Q%.2f", osc_id, cutoff_hz, resonance);
    amy_play_message(msg);
}

void AmyAdapter::setOscillatorWaveform(uint8_t osc_id, uint8_t wave_type) {
    char msg[32];
    snprintf(msg, sizeof(msg), "v%dw%d", osc_id, wave_type);
    amy_play_message(msg);
}

void AmyAdapter::setEnvelope(uint8_t osc_id, float attack_ms, float decay_ms, float sustain_level, float release_ms) {
    char msg[128];
    snprintf(msg, sizeof(msg), "v%dA%.0f,1,%.0f,%.2f,%.0f,0", osc_id, attack_ms, decay_ms, sustain_level, release_ms);
    amy_play_message(msg);
}

void AmyAdapter::setChorus(float depth, float rate, float level) {
    char msg[64];
    snprintf(msg, sizeof(msg), "k0k%.2f,%.2f,%.2f", level, depth, rate);
    amy_play_message(msg);
}

void AmyAdapter::setReverb(float room_size, float damp, float mix) {
    char msg[64];
    snprintf(msg, sizeof(msg), "H0H%.2f,%.2f,%.2f", mix, room_size, damp);
    amy_play_message(msg);
}

void AmyAdapter::setDelay(float delay_ms, float feedback, float mix) {
    char msg[64];
    snprintf(msg, sizeof(msg), "M0M%.2f,%.0f,%.2f", mix, delay_ms, feedback);
    amy_play_message(msg);
}

void AmyAdapter::setSendLevels(uint8_t osc_id, float reverb_send, float chorus_send, float echo_send) {
    char msg[64];
    snprintf(msg, sizeof(msg), "v%dR%.2f,%.2f,%.2f", osc_id, reverb_send, chorus_send, echo_send);
    amy_play_message(msg);
}

} // namespace smk
