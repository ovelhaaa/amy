#pragma once
#include <cstddef>
#include <cstdint>

namespace smk::link {
constexpr uint8_t kVersion = 1;
constexpr size_t kPayloadBytes = 192;
constexpr size_t kHeaderBytes = 12;
constexpr size_t kPacketBytes = kHeaderBytes + kPayloadBytes + 2;
constexpr size_t kWireBytes = 2 * kPacketBytes + 2;
constexpr uint32_t kFrameTimeoutMs = 100;
constexpr uint32_t kLinkTimeoutMs = 1500;
constexpr uint32_t kNoteLeaseMs = 600;
constexpr uint32_t kStatePollMs = 200;
constexpr size_t kSnapshotBytes = 154;
constexpr size_t kDiagnosticsBytes = 38;
constexpr uint8_t kNoNote = 255;

enum class Type : uint8_t {
    Hello = 1, GetState, SetPatch, SetMacro, SetMasterVolume, Panic,
    AuditionNoteOn, AuditionNoteOff, GetPatch,
    HelloAck = 0x81, StateSnapshot, CommandAck, CommandError, PatchState, AudioDiagnostics
};
enum class Error : uint8_t { None, Version, Session, Sequence, Payload, Unsupported, Busy };
enum class Ack : uint8_t { Received = 1, Applied = 2 };
struct Frame {
    uint8_t version = kVersion;
    Type type = Type::Hello;
    uint16_t sequence = 0;
    uint32_t session = 0;
    uint16_t size = 0;
    uint8_t payload[kPayloadBytes]{};
};
inline void put16(uint8_t* out, uint16_t value) { out[0]=uint8_t(value); out[1]=uint8_t(value>>8); }
inline void put32(uint8_t* out, uint32_t value) { put16(out,uint16_t(value)); put16(out+2,uint16_t(value>>16)); }
inline uint16_t get16(const uint8_t* in) { return uint16_t(in[0]) | (uint16_t(in[1])<<8); }
inline uint32_t get32(const uint8_t* in) { return uint32_t(get16(in)) | (uint32_t(get16(in+2))<<16); }
uint16_t crc16(const uint8_t* data, size_t size);
// SLIP END prefix/suffix; DB/DC/DD escaping. No struct layout crosses the wire.
size_t encode(const Frame& frame, uint8_t* wire, size_t capacity);
bool validCommand(const Frame& frame);
bool sameRequest(const Frame& a, const Frame& b);

class Decoder {
public:
    enum class Result { None, Packet, Invalid };
    Result feed(uint8_t byte, uint32_t now_ms, Frame& out);
    bool expire(uint32_t now_ms);
    void reset();
    uint32_t errors() const { return errors_; }
    uint32_t timeouts() const { return timeouts_; }
private:
    uint8_t bytes_[kPacketBytes]{};
    size_t size_ = 0;
    uint32_t last_ms_ = 0, errors_ = 0, timeouts_ = 0;
    bool started_ = false, escaped_ = false, dropping_ = false;
};
}
