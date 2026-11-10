#pragma once
#include <cstdint>

extern "C" {
#include "amy.h"
// FmAlgorithm ABI from algorithms.c
struct FmAlgorithm;
extern const struct FmAlgorithm algorithms[33];
}

namespace smk::p4::fm {

enum FmOpFlags : uint8_t {
    kOutBusOne = 1 << 0,
    kOutBusTwo = 1 << 1,
    kOutBusAdd = 1 << 2,
    kInBusOne  = 1 << 4,
    kInBusTwo  = 1 << 5,
    kFbIn      = 1 << 6,
    kFbOut     = 1 << 7
};

struct Topology {
    uint8_t carriers = 0;         // bitmask: bit (op-1) is 1 if operator op (1..6) is carrier
    uint8_t modulators = 0;       // bitmask: bit (op-1) is 1 if operator op (1..6) is modulator
    uint8_t feedback_in = 0;      // 1..6, or 0 if none
    uint8_t feedback_out = 0;     // 1..6, or 0 if none
    uint8_t modulates[6] = {};    // bitmask of operators modulated by operator i (0..5 -> OP1..OP6)
    uint8_t modulated_by[6] = {}; // bitmask of operators that modulate operator i

    constexpr bool isCarrier(unsigned op1to6) const {
        return op1to6 >= 1 && op1to6 <= 6 && ((carriers & (1u << (op1to6 - 1))) != 0);
    }
    constexpr bool isModulator(unsigned op1to6) const {
        return op1to6 >= 1 && op1to6 <= 6 && ((modulators & (1u << (op1to6 - 1))) != 0);
    }
    constexpr bool hasFeedback(unsigned op1to6) const {
        return op1to6 >= 1 && op1to6 <= 6 && (feedback_in == op1to6 || feedback_out == op1to6);
    }
    constexpr bool connectsTo(unsigned src1to6, unsigned dst1to6) const {
        return src1to6 >= 1 && src1to6 <= 6 && dst1to6 >= 1 && dst1to6 <= 6 &&
               ((modulates[src1to6 - 1] & (1u << (dst1to6 - 1))) != 0);
    }
    constexpr unsigned carrierCount() const {
        unsigned c = 0;
        for (unsigned i = 0; i < 6; ++i) if (carriers & (1u << i)) ++c;
        return c;
    }
    constexpr unsigned modulatorCount() const {
        unsigned c = 0;
        for (unsigned i = 0; i < 6; ++i) if (modulators & (1u << i)) ++c;
        return c;
    }
};

inline Topology deriveTopology(unsigned algorithm) {
    Topology topo{};
    if (algorithm < 1 || algorithm > 32) return topo;
    const uint8_t* ops = reinterpret_cast<const uint8_t*>(&algorithms[algorithm]);
    uint8_t bus_one = 0;
    uint8_t bus_two = 0;

    for (unsigned native_op = 0; native_op < 6; ++native_op) {
        const unsigned ui_op = 6 - native_op; // native 0..5 -> OP6..OP1
        const uint8_t flags = ops[native_op];
        const uint8_t op_bit = 1u << (ui_op - 1);

        if (flags & kFbIn) topo.feedback_in = ui_op;
        if (flags & kFbOut) topo.feedback_out = ui_op;

        // Check modulation inputs from preceding operators
        if (flags & kInBusOne) {
            for (unsigned src = 1; src <= 6; ++src) {
                if (bus_one & (1u << (src - 1))) {
                    topo.modulates[src - 1] |= op_bit;
                    topo.modulated_by[ui_op - 1] |= (1u << (src - 1));
                }
            }
        }
        if (flags & kInBusTwo) {
            for (unsigned src = 1; src <= 6; ++src) {
                if (bus_two & (1u << (src - 1))) {
                    topo.modulates[src - 1] |= op_bit;
                    topo.modulated_by[ui_op - 1] |= (1u << (src - 1));
                }
            }
        }

        // Check output routing
        if (flags & kOutBusOne) {
            topo.modulators |= op_bit;
            if (flags & kOutBusAdd) {
                bus_one |= op_bit;
            } else {
                bus_one = op_bit;
            }
        } else if (flags & kOutBusTwo) {
            topo.modulators |= op_bit;
            if (flags & kOutBusAdd) {
                bus_two |= op_bit;
            } else {
                bus_two = op_bit;
            }
        } else {
            // Neither BUS_ONE nor BUS_TWO -> writes to audio buf (Carrier)
            topo.carriers |= op_bit;
        }
    }
    return topo;
}

} // namespace smk::p4::fm
