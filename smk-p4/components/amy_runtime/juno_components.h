#pragma once
#include <cmath>
#include <cstdint>
namespace smk::p4::edit {
enum class ComponentWave : uint8_t { Other, Control, Pulse, Saw, Noise };
struct ComponentNode {
    ComponentWave wave=ComponentWave::Other;
    int8_t next=-1;
    bool filtered=false,modulator=false;
    float logfreq=0,note=0;
};
struct JunoComponents {
    int8_t root=-1,pulse=-1,sub=-1,noise=-1;
    uint8_t main_mask=0;
};
// Bounded, local graph indices. Never infer a component from its physical slot.
inline JunoComponents identifyJuno(const ComponentNode* nodes,unsigned count){
    JunoComponents result;
    if(!count || count>8)return result;
    for(unsigned i=0;i<count;++i)if(nodes[i].wave==ComponentWave::Control && nodes[i].filtered && !nodes[i].modulator){
        if(result.root>=0)return JunoComponents{};
        result.root=int8_t(i);
    }
    if(result.root<0)return result;
    uint8_t visited=uint8_t(1u<<result.root),members=0;int next=nodes[result.root].next;
    float fundamental=-1000;
    for(unsigned steps=0;next!=-1 && steps<count;++steps){
        if(next<0 || unsigned(next)>=count || (visited&(1u<<next)))return JunoComponents{};
        visited|=uint8_t(1u<<next);members|=uint8_t(1u<<next);
        const auto& n=nodes[next];if(n.modulator)return JunoComponents{};
        if(n.wave==ComponentWave::Pulse || n.wave==ComponentWave::Saw){
            if(!std::isfinite(n.logfreq) || n.note!=1.0f)return JunoComponents{};
            if(n.logfreq>fundamental)fundamental=n.logfreq;
        }else if(n.wave!=ComponentWave::Noise)return JunoComponents{};
        next=n.next;
    }
    if(next!=-1 || fundamental==-1000)return JunoComponents{};
    bool saw=false;
    for(unsigned i=0;i<count;++i)if(members&(1u<<i)){
        const auto& n=nodes[i];
        if(n.wave==ComponentWave::Noise){if(result.noise>=0)return JunoComponents{};result.noise=int8_t(i);}
        else if(std::fabs(n.logfreq-fundamental)<0.0001f){
            result.main_mask|=uint8_t(1u<<i);
            if(n.wave==ComponentWave::Pulse){if(result.pulse>=0)return JunoComponents{};result.pulse=int8_t(i);}
            else {if(saw)return JunoComponents{};saw=true;}
        }else if(n.wave==ComponentWave::Pulse && std::fabs(n.logfreq-fundamental+1.0f)<0.0001f){
            if(result.sub>=0)return JunoComponents{};
            result.sub=int8_t(i);
        }else return JunoComponents{};
    }
    return result;
}
}
