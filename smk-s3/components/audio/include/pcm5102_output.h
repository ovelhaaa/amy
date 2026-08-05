#pragma once
#include "audio_output.h"
#include <driver/i2s_std.h>
#include <atomic>

namespace smk {




class PCM5102Output : public AudioOutput {
public:
    PCM5102Output(int bclk_pin, int lrclk_pin, int data_pin);
    virtual ~PCM5102Output();

    bool begin() override;
    bool start() override;
    bool stop() override;
    bool write(const int16_t* interleaved_stereo, size_t frames) override;
    
    uint32_t underrunCount() const override { return _underruns.load(); }
    uint32_t framesWritten() const override { return _frames_written.load(); }

private:
    int _bclk_pin;
    int _lrclk_pin;
    int _data_pin;
    i2s_chan_handle_t _tx_handle;
    std::atomic<uint32_t> _underruns;
    std::atomic<uint32_t> _frames_written;
    int32_t* _conversion_buffer;
    size_t _conversion_buffer_size;
};


} // namespace smk
