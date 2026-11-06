#pragma once
#include "runtime_config.h"
#include <atomic>
#include <cstring>
namespace smk::p4::amy {
// One synthesis producer and one output consumer. No heap or waiting here.
template<unsigned Blocks=3>
class PcmHandoff {
public:
    bool push(const int16_t* samples,uint32_t generation) {
        const uint32_t w=write_.load(std::memory_order_relaxed);
        if(w-read_.load(std::memory_order_acquire)>=Blocks) return false;
        auto& block=blocks_[w%Blocks];
        std::memcpy(block.pcm,samples,sizeof(block.pcm));block.generation=generation;
        write_.store(w+1,std::memory_order_release);return true;
    }
    bool take(int16_t* output,uint32_t generation) {
        bool copied=false;
        for(unsigned i=0;i<Blocks;++i) {
            const uint32_t r=read_.load(std::memory_order_relaxed);
            if(r==write_.load(std::memory_order_acquire)) break;
            const auto& block=blocks_[r%Blocks];
            if(block.generation==generation) {
                std::memcpy(output,block.pcm,sizeof(block.pcm));copied=true;
            }
            read_.store(r+1,std::memory_order_release);
            if(copied)break;
        }
        if(!copied) std::memset(output,0,kBlockFrames*2*sizeof(int16_t));
        return copied;
    }
private:
    static_assert(Blocks>0 && std::atomic<uint32_t>::is_always_lock_free);
    struct Block { int16_t pcm[kBlockFrames*2]{};uint32_t generation=0; } blocks_[Blocks]{};
    std::atomic<uint32_t> read_{0},write_{0};
};
}
