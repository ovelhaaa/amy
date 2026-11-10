#pragma once
#include <cstdint>
#include <cstdio>

extern "C" {
#include "amy.h"
#ifndef FM_ALGORITHM_STRUCT_DEFINED
#define FM_ALGORITHM_STRUCT_DEFINED
struct FmAlgorithm { uint8_t ops[MAX_ALGO_OPS]; };
#endif
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

inline void formatRoutingFormula(const Topology& topo, char* buf, size_t cap) {
    if (!buf || cap == 0) return;
    buf[0] = '\0';
    if (topo.carrierCount() == 6) {
        std::snprintf(buf, cap, "OP1..OP6 [ADDITIVE / 6 CARRIERS]  FB: OP%u", topo.feedback_in ? topo.feedback_in : 6);
        return;
    }
    size_t pos = 0;
    bool first_carrier = true;

    for (unsigned c = 1; c <= 6; ++c) {
        if (!topo.isCarrier(c)) continue;
        if (!first_carrier && pos + 4 < cap) {
            pos += std::snprintf(buf + pos, cap - pos, " | ");
        }
        first_carrier = false;

        char chain[64];
        chain[0] = '\0';
        size_t cpos = 0;

        for (unsigned m = 6; m >= 1; --m) {
            if (topo.connectsTo(m, c)) {
                for (unsigned m2 = 6; m2 >= 1; --m2) {
                    if (topo.connectsTo(m2, m)) {
                        for (unsigned m3 = 6; m3 >= 1; --m3) {
                            if (topo.connectsTo(m3, m2)) {
                                cpos += std::snprintf(chain + cpos, sizeof(chain) - cpos,
                                    (topo.hasFeedback(m3) ? "%u[FB]->" : "%u->"), m3);
                            }
                        }
                        cpos += std::snprintf(chain + cpos, sizeof(chain) - cpos,
                            (topo.hasFeedback(m2) ? "%u[FB]->" : "%u->"), m2);
                    }
                }
                cpos += std::snprintf(chain + cpos, sizeof(chain) - cpos,
                    (topo.hasFeedback(m) ? "%u[FB]->" : "%u->"), m);
            }
        }
        if (cpos > 0) {
            pos += std::snprintf(buf + pos, cap - pos, "%s%u", chain, c);
        } else {
            pos += std::snprintf(buf + pos, cap - pos, (topo.hasFeedback(c) ? "%u[FB]" : "%u"), c);
        }
        pos += std::snprintf(buf + pos, cap - pos, "->OUT");
    }
}

} // namespace smk::p4::fm
