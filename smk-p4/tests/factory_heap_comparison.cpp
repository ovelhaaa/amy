#include "amy_runtime.h"
#include <cassert>
#include <cstdio>
extern "C" {size_t probe_bytes();uint32_t probe_live();}
int main(){auto before=probe_bytes();auto live=probe_live();smk::p4::amy::Runtime r;assert(r.begin());std::printf("HOST_HEAP engine=%zu live=%u runtime_object=%zu resident_instruments=%u capacity_oscs=%u catalog=%zu\n",probe_bytes()-before,probe_live()-live,sizeof(r),smk::p4::amy::kAmyVoices,smk::p4::amy::kOscillators,smk::p4::amy::kPatchCount);r.end();assert(probe_bytes()==before && probe_live()==live);}
