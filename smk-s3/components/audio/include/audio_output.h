#pragma once
#include <cstdint>
#include <cstddef>

namespace smk {




class AudioOutput {
public:
    virtual ~AudioOutput() = default;
    virtual bool begin() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool write(const int16_t* interleaved_stereo, size_t frames) = 0;
    virtual uint32_t underrunCount() const = 0;
    virtual uint32_t framesWritten() const = 0;
};


} // namespace smk
