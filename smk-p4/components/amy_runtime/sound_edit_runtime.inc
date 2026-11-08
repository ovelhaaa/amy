// Included inside the platform adapter translation unit; never part of AMY core.
extern "C" {
// Native routing table ABI from algorithms.c; no copied algorithms or AMY patch.
struct FmAlgorithm { uint8_t ops[MAX_ALGO_OPS]; };
extern const struct FmAlgorithm algorithms[33];
}
namespace smk::p4::amy {
void Runtime::captureEdit() {
    const uint32_t revision=state_.edit.revision+1,epoch=state_.edit.epoch+1;
    state_.edit=edit::State{};auto& out=state_.edit;
    out.patch=state_.patch_id;out.revision=revision;out.epoch=epoch;
    const auto* patch=library::find(out.patch);if(!patch)return;
    out.family=uint8_t(patch->family);edit_count_=patch->oscillators;
    if(edit_count_>8){edit_count_=0;return;}
    for(unsigned v=0;v<kVoices;++v){
        uint16_t voice;
        if(instrument_get_num_voices(v+1,&voice)!=1)return;
        const unsigned base=voice_to_base_osc[voice];
        for(unsigned o=0;o<edit_count_;++o){
            if(base+o>=kOscillators || !synth[base+o] || osc_to_voice[base+o]!=voice)return;
            edit_map_[v][o]=uint16_t(base+o);
        }
    }
    for(unsigned o=0;o<edit_count_;++o){
        const auto* s=synth[edit_map_[0][o]];auto& b=edit_oscs_[o];b=EditOsc{};
        b.wave=s->wave;b.cutoff=s->filter_logfreq_coefs[COEF_CONST];
        b.filter_env=s->filter_logfreq_coefs[COEF_EG0];b.filter_note=s->filter_logfreq_coefs[COEF_NOTE];
        b.resonance=s->resonance;b.amp=s->amp_coefs[COEF_CONST];b.ratio=s->logratio;
        unsigned n=0;
        if(s->breakpoint_times[0])while(n<s->max_num_breakpoints[0] && AMY_IS_SET(s->breakpoint_times[0][n]))++n;
        // Juno ROM uses three ADSR breakpoints. Do not grow or reinterpret others.
        if(n==3 && s->breakpoint_times[0] && s->breakpoint_values[0]){
            b.envelope=true;b.release_index=2;b.attack_samples=s->breakpoint_times[0][0];
            b.decay_samples=s->breakpoint_times[0][1];b.sustain=s->breakpoint_values[0][1];
            b.release_samples=s->breakpoint_times[0][2];
        }
        for(unsigned v=1;v<kVoices;++v){
            const auto* t=synth[edit_map_[v][o]];
            if(b.envelope) {
                unsigned count=0;
                if(t->breakpoint_times[0] && t->breakpoint_values[0])
                    while(count<t->max_num_breakpoints[0] && AMY_IS_SET(t->breakpoint_times[0][count]))++count;
                if(count!=3 || t->breakpoint_times[0][0]!=b.attack_samples ||
                   t->breakpoint_times[0][1]!=b.decay_samples || t->breakpoint_times[0][2]!=b.release_samples ||
                   t->breakpoint_values[0][1]!=b.sustain) b.envelope=false;
            }
            if(t->wave!=s->wave || t->filter_logfreq_coefs[COEF_CONST]!=b.cutoff ||
                t->filter_logfreq_coefs[COEF_EG0]!=b.filter_env || t->filter_logfreq_coefs[COEF_NOTE]!=b.filter_note ||
                t->resonance!=b.resonance || t->amp_coefs[COEF_CONST]!=b.amp || std::memcmp(&t->logratio,&b.ratio,sizeof(float))!=0)return;
        }
    }
    if(patch->family==library::SynthFamily::Subtractive){
        int filter=-1,env=-1;
        for(unsigned o=0;o<edit_count_;++o){
            const auto* s=synth[edit_map_[0][o]];
            if(s->filter_type!=FILTER_NONE){if(filter<0)filter=int(o);}
            if(edit_oscs_[o].envelope && s->amp_coefs[COEF_EG0]!=0 && env<0)env=int(o);
        }
        if(filter>=0){out.original[edit::Cutoff]=freq_of_logfreq(edit_oscs_[filter].cutoff);
            out.original[edit::Resonance]=edit_oscs_[filter].resonance;
            out.available|=3;
            out.original[edit::FilterEnv]=edit_oscs_[filter].filter_env;
            out.original[edit::FilterKeytrack]=100.0f*edit_oscs_[filter].filter_note;
            if(edit::valid(edit::FilterEnv,out.original[edit::FilterEnv]))out.available|=1u<<edit::FilterEnv;
            if(edit::valid(edit::FilterKeytrack,out.original[edit::FilterKeytrack]))out.available|=1u<<edit::FilterKeytrack;}
        edit_env_=env<0?255:uint8_t(env);
        if(env>=0){out.original[edit::Attack]=edit_oscs_[env].attack_samples*1000.0f/kSampleRateHz;
            out.original[edit::Release]=edit_oscs_[env].release_samples*1000.0f/kSampleRateHz;
            out.original[edit::Decay]=edit_oscs_[env].decay_samples*1000.0f/kSampleRateHz;
            out.original[edit::Sustain]=edit_oscs_[env].sustain*100.0f;
            out.available|=(1u<<edit::Attack)|(1u<<edit::Release);
            if(edit::valid(edit::Decay,out.original[edit::Decay]) && edit::valid(edit::Sustain,out.original[edit::Sustain]))
                out.available|=(1u<<edit::Decay)|(1u<<edit::Sustain);}
    } else if(patch->family==library::SynthFamily::FM){
        const auto* algo=synth[edit_map_[0][0]];
        if(algo->wave!=ALGO || algo->algorithm>32)return;
        for(unsigned op=0;op<6;++op){
            const unsigned native_op=5-op; // Native render order is DX7 OP6..OP1.
            edit_modulators_[op]=(algorithms[algo->algorithm].ops[native_op]&3)!=0;
            const int physical=algo->algo_source[native_op];int local=-1;
            for(unsigned o=0;o<edit_count_;++o)if(edit_map_[0][o]==physical)local=int(o);
            if(local<0)return;
            edit_ops_[op]=uint8_t(local);
            for(unsigned v=1;v<kVoices;++v)
                if(synth[edit_map_[v][0]]->algo_source[native_op]!=edit_map_[v][local])return;
            // Levels are relative percentages of the real, preserved ROM gain.
            out.original[edit::OpLevel0+op]=100;
            out.original[edit::OpRatio0+op]=std::exp2(edit_oscs_[local].ratio);
            out.available|=(1u<<(edit::OpLevel0+op));
            if(edit::valid(edit::OpRatio0+op,out.original[edit::OpRatio0+op]))
                out.available|=(1u<<(edit::OpRatio0+op));
        }
        out.original[edit::Depth]=1;out.available|=1u<<edit::Depth;
    }
    for(unsigned p=0;p<edit::Count;++p)out.manual[p]=out.applied[p]=out.original[p];
}
void Runtime::applyEdit() {
    edit::Request request;if(!commands_.edits.pop(request))return;
    auto& out=state_.edit;
#ifdef SMKP4_HARMONY
    AdmissionTimer edit_timer(out.apply_max_us);
#endif
    out.request=request.request;++out.revision;
    if(!state_.ready || state_.patch_state!=PatchState::Ready || request.patch!=out.patch ||
       request.generation!=state_.generation || (request.mask&~out.available)) {++out.errors;return;}
    for(unsigned p=0;p<edit::Count;++p)
        if((request.mask&(1u<<p)) && !edit::valid(p,request.values[p])){++out.errors;return;}
    for(unsigned m=0;m<3;++m)if(request.macros[m]>127){++out.errors;return;}
    const auto macroMask=[&](const uint8_t* m){
        uint32_t mask=0;
        if(m[0]!=64)mask|=1u<<(out.family==1?edit::Cutoff:edit::Depth);
        if(out.family==1){if(m[1]!=64)mask|=1u<<edit::Attack;if(m[2]!=64)mask|=1u<<edit::Release;}
        return mask&out.available;
    };
    if(out.family==0 && (request.macros[1]!=64 || request.macros[2]!=64)){++out.errors;return;}
    const uint32_t effective=request.compare?0:(request.mask|macroMask(request.macros));
    // Preflight the complete eight-voice topology before issuing any event.
    for(unsigned v=0;v<kVoices;++v)for(unsigned o=0;o<edit_count_;++o)
        if(!synth[edit_map_[v][o]] || synth[edit_map_[v][o]]->wave!=edit_oscs_[o].wave){++out.errors;return;}
    const auto value=[&](unsigned p){
        float v=!request.compare && (request.mask&(1u<<p))?request.values[p]:out.original[p];
        if(!request.compare){
            if(p==edit::Cutoff && request.macros[0]!=64)v=std::fmax(20.0f,std::fmin(18000.0f,v*std::exp2((int(request.macros[0])-64)/32.0f)));
            if(p==edit::Depth)v*=std::fmin(1.0f,request.macros[0]/64.0f);
            if((p==edit::Attack || p==edit::Release) && request.macros[p==edit::Attack?1:2]!=64)
                v=std::fmax(1.0f,std::fmin(8000.0f,v*std::exp2((int(request.macros[p==edit::Attack?1:2])-64)/16.0f)));
        }
        return v;
    };
    // Emit only changed destinations; at most 8 * 6 bounded native events.
    const uint32_t previous=out.compare?0:(out.mask|macroMask(out.macros));
    const uint32_t touched=previous|effective;
    for(unsigned v=0;v<kVoices;++v)for(unsigned o=0;o<edit_count_;++o){
        const auto& b=edit_oscs_[o];const auto* s=synth[edit_map_[v][o]];
        auto e=amy_default_event();e.osc=edit_map_[v][o];e.time=0;bool emit=false;
        if(out.family==uint8_t(library::SynthFamily::Subtractive)){
            if(s->filter_type!=FILTER_NONE){
                if(touched&(1u<<edit::Cutoff)){
                    if(effective&1){e.filter_freq_coefs[COEF_CONST]=value(edit::Cutoff);emit=true;}
                    else { // Hz/log lookup conversion is lossy: restore the captured native value exactly.
                        struct delta d{};d.osc=edit_map_[v][o];d.param=FILTER_FREQ;d.data.f=b.cutoff;
                        add_delta_to_queue(&d,&amy_global.delta_queue);
                    }
                }
                if(touched&(1u<<edit::FilterEnv)){
                    struct delta d{};d.osc=e.osc;d.param=static_cast<params>(int(FILTER_FREQ)+int(COEF_EG0));
                    d.data.f=(effective&(1u<<edit::FilterEnv))?value(edit::FilterEnv):b.filter_env;add_delta_to_queue(&d,&amy_global.delta_queue);
                }
                if(touched&(1u<<edit::FilterKeytrack)){
                    struct delta d{};d.osc=e.osc;d.param=static_cast<params>(int(FILTER_FREQ)+int(COEF_NOTE));
                    d.data.f=(effective&(1u<<edit::FilterKeytrack))?value(edit::FilterKeytrack)/100.0f:b.filter_note;add_delta_to_queue(&d,&amy_global.delta_queue);
                }
                if(touched&(1u<<edit::Resonance)){e.resonance=(effective&2)?value(edit::Resonance):b.resonance;emit=true;}
            }
            if(b.envelope && s->amp_coefs[COEF_EG0]!=0){
                if(touched&((1u<<edit::Attack)|(1u<<edit::Release)|(1u<<edit::Decay)|(1u<<edit::Sustain)))e.bp_is_set[0]=1;
                if(touched&(1u<<edit::Attack)){e.eg0_times[0]=(effective&(1u<<edit::Attack))?uint32_t(value(edit::Attack)):b.attack_samples*1000/kSampleRateHz;emit=true;}
                if(o==edit_env_ && (touched&(1u<<edit::Decay))){
                    if(effective&(1u<<edit::Decay)){e.eg0_times[1]=uint32_t(value(edit::Decay));emit=true;}
                    else {struct delta d{};d.osc=e.osc;d.param=static_cast<params>(BP_START+2);d.data.i=b.decay_samples;add_delta_to_queue(&d,&amy_global.delta_queue);}
                }
                if(o==edit_env_ && (touched&(1u<<edit::Sustain))){e.eg0_values[1]=(effective&(1u<<edit::Sustain))?value(edit::Sustain)/100.0f:b.sustain;emit=true;}
                if(touched&(1u<<edit::Release)){e.eg0_times[b.release_index]=(effective&(1u<<edit::Release))?uint32_t(value(edit::Release)):b.release_samples*1000/kSampleRateHz;emit=true;}
            }
        }else{
            for(unsigned op=0;op<6;++op)if(edit_ops_[op]==o){
                if(touched&((1u<<(edit::OpLevel0+op))|(1u<<edit::Depth))){
                    e.amp_coefs[COEF_CONST]=b.amp*(value(edit::OpLevel0+op)/100.0f)*(edit_modulators_[op]?value(edit::Depth):1.0f);emit=true;
                }
                if(touched&(1u<<(edit::OpRatio0+op))){
                    if(effective&(1u<<(edit::OpRatio0+op))){e.ratio=value(edit::OpRatio0+op);emit=true;}
                    else {struct delta d{};d.osc=edit_map_[v][o];d.param=RATIO;d.data.f=b.ratio;add_delta_to_queue(&d,&amy_global.delta_queue);}
                }
            }
        }
        if(emit)amy_add_event(&e);
    }
    amy_execute_deltas();
    if(amy_get_oom_count()!=state_.oom_count){++out.errors;failRendering();return;}
    out.mask=request.mask;out.compare=request.compare;out.dirty=request.mask!=0 || request.macros[0]!=64 || request.macros[1]!=64 || request.macros[2]!=64;out.user_slot=request.slot;
    for(unsigned m=0;m<3;++m)out.macros[m]=request.macros[m];
    for(unsigned p=0;p<edit::Count;++p){out.manual[p]=request.mask&(1u<<p)?request.values[p]:out.original[p];out.applied[p]=value(p);}
}
}

