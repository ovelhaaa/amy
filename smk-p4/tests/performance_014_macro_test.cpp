#include "performance_macros.h"
#include <cassert>
#include <limits>

using namespace smk::p4::performance;

int main() {
    Routes routes{};
    for (unsigned m = 0; m < kMacroCount; ++m) {
        routes[m][0] = {uint16_t(m), 0, 100, 20, Curve::Linear, Polarity::Bipolar, Both, true};
        auto positions = kNeutral;
        positions[m] = 127;
        assert(valid(positions));
        for (unsigned d = 0; d < kMacroCount; ++d) {
            const auto output = compose(routes, positions, Juno, d, 50, 0, 100);
            assert(output.active == (m == d));
            assert(output.value == (m == d ? 70 : 50));
        }
    }

    routes[0][1] = {8, 0, 100, 50, Curve::Exponential, Polarity::Positive, Juno, true};
    routes[1][1] = {8, 0, 100, 30, Curve::Logarithmic, Polarity::Bipolar, Both, true, true};
    auto positions = kNeutral;
    positions[0] = positions[1] = 127;
    assert(compose(routes, positions, Juno, 8, 50, 0, 100).value == 70);
    assert(compose(routes, positions, Fm, 8, 50, 0, 100).value == 20);
    positions[1] = kNeutral[1];
    assert(compose(routes, positions, Juno, 8, 80, 0, 100).value == 100);
    positions[0] = 0;
    assert(!compose(routes, positions, Juno, 8, 50, 0, 100).active);
    for (const uint8_t value : {64, 100, 64, 100, 64}) {
        positions[0] = value;
        const auto output = compose(routes, positions, Juno, 8, 50, 0, 100);
        if (value == 64) assert(!output.active && output.value == 50);
        else assert(output.active && output.value > 50 && output.value < 100);
    }
    assert(compose(routes, positions, Juno, 8, 72, 0, 100).value == 72);
    positions[0] = 100;
    assert(!compose(routes, positions, Juno, 8, std::numeric_limits<float>::infinity(), 0, 100).active);
    positions[0] = 128;
    assert(!valid(positions));
}
