#include "fake_nvs.h"
#include "performance_scene_nvs.h"
#include <cassert>
#include <cstdio>

using namespace smk::p4;

static void persistence() {
    NvsSceneStore store;
    assert(store.begin());
    fake_nvs::blobs[{4,"p00a"}]={1,2,3};
    fake_nvs::blobs[{2,"s00a"}]={4,5,6};
    scene::Slot value;
    value.state=seq::SlotState::Valid;
    auto& c=value.config;
    std::strcpy(c.name,"OPENING");
    c.domains=scene::kAll;
    c.patch_id=128;
    c.pattern_slot=7;
    c.bpm=110;
    c.fx.delay_time_ms=120;
    c.harmony.root=5;
    c.arp.enabled=true;
    c.macros[0]=125;
    c.routes={performance::kFactoryRoutes,true};
    c.routes.routes[0][0]={};
    for(unsigned i=0;i<scene::kSlots;++i) {
        assert(store.load(i).state==seq::SlotState::Empty);
        assert(store.commit(i,value));
        const auto read=store.load(i);
        assert(read.state==seq::SlotState::Valid && read.sequence==1);
        assert(read.config.patch_id==128 && read.config.pattern_slot==7 && read.config.bpm==110);
        assert(read.config.harmony.root==5 && read.config.fx.delay_time_ms==120);
        assert(read.config.arp.enabled && read.config.macros==c.macros && read.config.routes==c.routes);
    }
    std::strcpy(c.name,"INTRO");
    fake_nvs::torn=true;
    assert(!store.commit(0,value));
    fake_nvs::torn=false;
    NvsSceneStore reopened;
    assert(reopened.begin());
    assert(std::strcmp(reopened.load(0).config.name,"OPENING")==0);
    assert(reopened.commit(0,value));
    assert(std::strcmp(reopened.load(0).config.name,"INTRO")==0);
    auto& latest=fake_nvs::blobs.at({5,"s00b"});
    latest.back()^=1;
    assert(reopened.load(0).sequence==1);
    assert(reopened.commit(0,{}));
    const auto deleted=reopened.load(0);
    assert(deleted.sequence==2 && deleted.state==seq::SlotState::Empty);
    assert(reopened.commit(1,value));
    auto& future=fake_nvs::blobs.at({5,"s01b"});
    edit::put32(future.data()+4,2);
    edit::put32(future.data()+future.size()-4,edit::crc(future.data(),unsigned(future.size()-4)));
    const auto blobs=fake_nvs::blobs;
    assert(reopened.load(1).state==seq::SlotState::Incompatible);
    assert(!reopened.commit(1,value) && !reopened.commit(1,{}));
    fake_nvs::free_entries=126;
    assert(!reopened.commit(2,value));
    assert(!reopened.commit(scene::kSlots,value));
    assert(fake_nvs::blobs==blobs);
    assert(fake_nvs::blobs.at({4,"p00a"})==std::vector<uint8_t>({1,2,3}));
    assert(fake_nvs::blobs.at({2,"s00a"})==std::vector<uint8_t>({4,5,6}));
    puts("PASS eight isolated NVS slots, sparse routes, rename, tombstone, CRC, torn write, future bank and capacity");
}

int main() {
    persistence();
    scene::Configuration config;
    std::strcpy(config.name,"INTRO");
    config.domains=scene::kAll;
    config.pattern_slot=2;
    config.bpm=110;
    seq::Slot pattern;
    pattern.state=seq::SlotState::Valid;
    const auto caps=edit::familyParameterMask(1);
    const auto check=[&](uint8_t domains,uint8_t locks=0,bool apply_tempo=true) {
        return scene::validate(config,domains,locks,apply_tempo,true,&pattern,1,caps);
    };
    assert(check(scene::kAll).reason==scene::Reject::None);
    for(uint8_t domain=1;domain<=scene::Tempo;domain<<=1) {
        const auto result=check(domain);
        assert(result.reason==scene::Reject::None && result.domains==domain);
    }
    const auto locked=check(scene::kAll,perf_lock::kDefaultLockMask,false);
    assert(locked.reason==scene::Reject::None && locked.domains==(scene::Patch|scene::Pattern|scene::Macros));
    assert(check(scene::Tempo,0,false).reason==scene::Reject::EmptySelection);
    assert(scene::validate(config,scene::Patch,0,true,false,&pattern,1,caps).reason==scene::Reject::PatchUnavailable);
    pattern.state=seq::SlotState::Corrupted;
    assert(check(scene::Pattern|scene::Harmony).reason==scene::Reject::PatternUnavailable);
    assert(check(scene::Harmony).reason==scene::Reject::None);
    config.arp.gate=0;
    assert(check(scene::Arp).reason==scene::Reject::InvalidArp);
    assert(check(scene::Harmony).reason==scene::Reject::None);
    config.macros[0]=128;
    assert(check(scene::Macros).reason==scene::Reject::IncompatibleMacros);
    config.macros=performance::kNeutral;
    config.routes={performance::kFactoryRoutes,true};
    config.routes.routes[0][0]=performance::route(edit::Cutoff,20,18000,3000,performance::Juno);
    assert(check(scene::Macros).reason==scene::Reject::None);
    assert(scene::validate(config,scene::Macros,0,true,true,&pattern,0,edit::familyParameterMask(0))
           .reason==scene::Reject::IncompatibleMacros);
    assert(scene::validate(config,scene::Macros,0,true,true,&pattern,1,caps&~edit::bit(edit::Cutoff))
           .reason==scene::Reject::IncompatibleMacros);
    assert(check(scene::Fx|scene::Macros,perf_lock::Macros).domains==scene::Fx);
    config.domains=scene::Tempo;
    for(const uint16_t bpm:{40,120,240}) {
        config.bpm=bpm;
        assert(check(scene::kAll).reason==scene::Reject::None);
    }
    config.bpm=241;
    assert(check(scene::kAll).reason==scene::Reject::InvalidTempo);
    assert(check(scene::Patch).reason==scene::Reject::EmptySelection);
    std::printf("PASS scene selection, locks, tempo policy and reference validation\n"
                "MEASURE in-memory scene configuration %zu bytes, routes %zu bytes\n",
                sizeof(config),sizeof(config.routes));
}
