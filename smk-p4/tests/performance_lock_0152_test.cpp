#include "performance_lock.h"
#include <cassert>
#include <cstdio>

namespace {
using namespace smk::p4::perf_lock;

void testPerformanceLockPolicies() {
    assert(Flag::None == 0);
    assert(Flag::Fx == 1);
    assert(Flag::Harmony == 2);
    assert(Flag::Arp == 4);
    assert(Flag::Macros == 8);

    Options opt{};
    assert(!opt.keepFx() && !opt.keepHarmony() && !opt.keepArp() && !opt.keepMacros());

    opt.preserve_mask = kDefaultLockMask;
    assert(opt.keepFx());
    assert(opt.keepHarmony());
    assert(opt.keepArp());
    assert(!opt.keepMacros());

    opt.preserve_mask = Flag::Macros;
    assert(!opt.keepFx() && !opt.keepHarmony() && !opt.keepArp() && opt.keepMacros());

    std::printf("PASS performance_lock_0152_test: policies and options validated\n");
}
} // namespace

int main() {
    testPerformanceLockPolicies();
    return 0;
}
