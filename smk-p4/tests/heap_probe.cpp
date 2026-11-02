#include <cstddef>
#include <cstdint>
#include <cassert>
#include <cstdio>
extern "C" {
void* __real_malloc(size_t);
void* __real_calloc(size_t,size_t);
void* __real_realloc(void*,size_t);
void __real_free(void*);
static void* allocations[4096]{};
static size_t sizes[4096]{};
static void* callers[4096]{};
static uint32_t live=0,calls=0;
static bool realtime=false;
static void remember(void* p,size_t bytes,void* caller) {
    if(!p) return;
    assert(!realtime); ++calls;
    for(unsigned i=0;i<4096;++i) if(!allocations[i]) { allocations[i]=p; sizes[i]=bytes; callers[i]=caller; ++live; return; }
    assert(false);
}
static void forget(void* p) {
    if(!p) return;
    assert(!realtime);
    for(auto& entry:allocations) if(entry==p) { entry=nullptr; --live; return; }
}
void* __wrap_malloc(size_t n) { void* p=__real_malloc(n); remember(p,n,__builtin_return_address(0)); return p; }
void* __wrap_calloc(size_t n,size_t bytes) { void* p=__real_calloc(n,bytes); remember(p,n*bytes,__builtin_return_address(0)); return p; }
void* __wrap_realloc(void* old,size_t bytes) {
    void* p=__real_realloc(old,bytes);
    if(p || !bytes) { forget(old); remember(p,bytes,__builtin_return_address(0)); }
    return p;
}
void __wrap_free(void* p) { forget(p); __real_free(p); }
void probe_realtime(bool enabled) { realtime=enabled; }
uint32_t probe_live() { return live; }
void probe_dump() { for(unsigned i=0;i<4096;++i) if(allocations[i]) std::fprintf(stderr,"LIVE size=%zu caller=%p\n",sizes[i],callers[i]); }
uint32_t probe_calls() { return calls; }
}
