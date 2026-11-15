#pragma once
#include <algorithm>
#include <array>
#include "tempo.h"

namespace smk::p4::tempo {
class TapTempo {
public:
    // Returns zero until three consistent intervals establish a tempo.
    unsigned tap(uint32_t timestamp_ms) {
        const uint32_t elapsed=timestamp_ms-last_;
        if(!started_ || elapsed>60000/kMinBpm) {
            started_=true;
            last_=timestamp_ms;
            count_=next_=0;
            return 0;
        }
        if(elapsed<60000/kMaxBpm) return 0;
        last_=timestamp_ms;
        intervals_[next_]=elapsed;
        next_=(next_+1)%intervals_.size();
        if(count_<intervals_.size()) ++count_;
        if(count_<3) return 0;
        auto sorted=intervals_;
        if(count_==3) sorted[3]=UINT32_MAX;
        std::sort(sorted.begin(),sorted.end());
        const auto median=(sorted[(count_-1)/2]+sorted[count_/2]+1)/2;
        if(sorted[count_-1]-sorted[0]>median/4) return 0;
        return (60000+median/2)/median;
    }
private:
    std::array<uint32_t,4> intervals_{};
    uint32_t last_=0;
    unsigned count_=0,next_=0;
    bool started_=false;
};
}
