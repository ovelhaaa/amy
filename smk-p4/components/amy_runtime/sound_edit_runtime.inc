// Included inside the platform adapter translation unit; never part of AMY core.
extern "C" {
// Native routing table ABI from algorithms.c; no copied algorithms or AMY patch.
struct FmAlgorithm { uint8_t ops[MAX_ALGO_OPS]; };
extern const struct FmAlgorithm algorithms[33];
}
namespace smk::p4::amy {
void Runtime::captureEdit() {
    const uint32_t revision=state_.edit.revision+1,epoch=state_.edit.epoch+1;
    edit_components_=edit::JunoComponents{};
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
        // Gated Juno VCA moves its musical ADSR from EG0 to EG1.
        b.filter_env_coef=(s->filter_logfreq_coefs[COEF_EG0]==0 &&
            (s->filter_logfreq_coefs[COEF_EG1]!=0 ||
             (s->breakpoint_times[1] && AMY_IS_SET(s->breakpoint_times[1][0]))))?COEF_EG1:COEF_EG0;
        b.filter_env=s->filter_logfreq_coefs[b.filter_env_coef];b.filter_note=s->filter_logfreq_coefs[COEF_NOTE];
        b.freq=s->logfreq_coefs[COEF_CONST];b.duty=s->duty_coefs[COEF_CONST];
        b.pan=s->pan_coefs[COEF_CONST];b.velocity=s->amp_coefs[COEF_VEL];
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
            if(t->role!=s->role || std::memcmp(t->logfreq_coefs,s->logfreq_coefs,sizeof(s->logfreq_coefs)) ||
                std::memcmp(t->duty_coefs,s->duty_coefs,sizeof(s->duty_coefs)) ||
                std::memcmp(t->pan_coefs,s->pan_coefs,sizeof(s->pan_coefs)) ||
                std::memcmp(t->amp_coefs,s->amp_coefs,sizeof(s->amp_coefs)) || t->wave!=s->wave || t->filter_logfreq_coefs[COEF_CONST]!=b.cutoff ||
                t->filter_type!=s->filter_type || t->filter_logfreq_coefs[b.filter_env_coef]!=b.filter_env || t->filter_logfreq_coefs[COEF_NOTE]!=b.filter_note ||
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
        edit::JunoComponents maps[kVoices];bool matching=true;
        for(unsigned v=0;v<kVoices;++v){
            edit::ComponentNode nodes[8];
            for(unsigned o=0;o<edit_count_;++o){
                const auto* osc=synth[edit_map_[v][o]];auto& n=nodes[o];
                n.wave=osc->wave==SILENT?edit::ComponentWave::Control:osc->wave==PULSE?edit::ComponentWave::Pulse:
                    (osc->wave==SAW_UP || osc->wave==SAW_DOWN)?edit::ComponentWave::Saw:osc->wave==NOISE?edit::ComponentWave::Noise:edit::ComponentWave::Other;
                n.filtered=osc->filter_type!=FILTER_NONE;n.modulator=osc->role==SYNTH_IS_MOD_SOURCE;
                n.logfreq=osc->logfreq_coefs[COEF_CONST];n.note=osc->logfreq_coefs[COEF_NOTE];
                if(AMY_IS_SET(osc->chained_osc)){
                    n.next=-2;for(unsigned j=0;j<edit_count_;++j)if(edit_map_[v][j]==osc->chained_osc)n.next=int8_t(j);
                }
            }
            maps[v]=edit::identifyJuno(nodes,edit_count_);
            if(v && (maps[v].root!=maps[0].root || maps[v].main_mask!=maps[0].main_mask ||
                maps[v].pulse!=maps[0].pulse || maps[v].sub!=maps[0].sub || maps[v].noise!=maps[0].noise))matching=false;
        }
        if(matching && maps[0].root>=0){
            edit_components_=maps[0];const auto& map=edit_components_;
            bool main_audible=false;
            for(unsigned o=0;o<edit_count_;++o)if((map.main_mask&(1u<<o)) && edit_oscs_[o].amp>0)main_audible=true;
            if(main_audible){out.original[edit::OscLevel]=100;out.original[edit::Detune]=0;
                out.available|=(1u<<edit::OscLevel)|(1u<<edit::Detune);}
            for(unsigned param:{unsigned(edit::SubLevel),unsigned(edit::NoiseLevel)}){
                const int local=param==edit::SubLevel?map.sub:map.noise;
                if(local>=0 && edit::valid(param,100*edit_oscs_[local].amp)){
                    out.original[param]=100*edit_oscs_[local].amp;out.available|=1u<<param;
                }
            }
            if(map.pulse>=0){
                const auto* pulse=synth[edit_map_[0][map.pulse]];bool static_duty=true;
                for(unsigned c=1;c<NUM_COMBO_COEFS;++c)if(pulse->duty_coefs[c]!=0)static_duty=false;
                if(static_duty && edit_oscs_[map.pulse].amp>0 && edit::valid(edit::PulseWidth,100*edit_oscs_[map.pulse].duty)){
                    out.original[edit::PulseWidth]=100*edit_oscs_[map.pulse].duty;out.available|=1u<<edit::PulseWidth;
                }
            }
            out.original[edit::Pan]=(edit_oscs_[map.root].pan-0.5f)*128;
            out.original[edit::Velocity]=edit_oscs_[map.root].velocity*100;
            if(edit::valid(edit::Pan,out.original[edit::Pan]))out.available|=1u<<edit::Pan;
            if(edit::valid(edit::Velocity,out.original[edit::Velocity]))out.available|=1u<<edit::Velocity;
        }
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
    bool levels_valid=true;
    for(unsigned v=0;v<kVoices;++v){edit_instrument_levels_[v]=instrument_get_level(v+1);
        if(!std::isfinite(edit_instrument_levels_[v]) || edit_instrument_levels_[v]<0)levels_valid=false;}
    if(levels_valid){out.original[edit::PatchLevel]=100;out.available|=1u<<edit::PatchLevel;}
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
    if(touched&(1u<<edit::PatchLevel))for(unsigned v=0;v<kVoices;++v)
        instrument_set_level(v+1,edit_instrument_levels_[v]*((effective&(1u<<edit::PatchLevel))?value(edit::PatchLevel)/100.0f:1.0f));
    for(unsigned v=0;v<kVoices;++v)for(unsigned o=0;o<edit_count_;++o){
        const auto& b=edit_oscs_[o];const auto* s=synth[edit_map_[v][o]];
        auto e=amy_default_event();e.osc=edit_map_[v][o];e.time=0;bool emit=false;
        if(out.family==uint8_t(library::SynthFamily::Subtractive)){
            const auto& components=edit_components_;
            if(components.main_mask&(1u<<o)){
                if(touched&(1u<<edit::OscLevel)){e.amp_coefs[COEF_CONST]=b.amp*((effective&(1u<<edit::OscLevel))?value(edit::OscLevel)/100.0f:1.0f);emit=true;}
                if(touched&(1u<<edit::Detune)){
                    struct delta d{};d.osc=e.osc;d.param=FREQ;
                    d.data.f=(effective&(1u<<edit::Detune))?b.freq+value(edit::Detune)/1200.0f:b.freq;add_delta_to_queue(&d,&amy_global.delta_queue);
                }
            }
            for(unsigned param:{unsigned(edit::SubLevel),unsigned(edit::NoiseLevel)}){
                const int local=param==edit::SubLevel?components.sub:components.noise;
                if(int(o)==local && (touched&(1u<<param))){e.amp_coefs[COEF_CONST]=(effective&(1u<<param))?value(param)/100.0f:b.amp;emit=true;}
            }
            if(int(o)==components.pulse && (touched&(1u<<edit::PulseWidth))){e.duty_coefs[COEF_CONST]=(effective&(1u<<edit::PulseWidth))?value(edit::PulseWidth)/100.0f:b.duty;emit=true;}
            if(int(o)==components.root){
                if(touched&(1u<<edit::Pan)){e.pan_coefs[COEF_CONST]=(effective&(1u<<edit::Pan))?0.5f+value(edit::Pan)/128.0f:b.pan;emit=true;}
                if(touched&(1u<<edit::Velocity)){e.amp_coefs[COEF_VEL]=(effective&(1u<<edit::Velocity))?value(edit::Velocity)/100.0f:b.velocity;emit=true;}
            }
            if(s->filter_type!=FILTER_NONE){
                if(touched&(1u<<edit::Cutoff)){
                    if(effective&1){e.filter_freq_coefs[COEF_CONST]=value(edit::Cutoff);emit=true;}
                    else { // Hz/log lookup conversion is lossy: restore the captured native value exactly.
                        struct delta d{};d.osc=edit_map_[v][o];d.param=FILTER_FREQ;d.data.f=b.cutoff;
                        add_delta_to_queue(&d,&amy_global.delta_queue);
                    }
                }
                if(touched&(1u<<edit::FilterEnv)){
                    struct delta d{};d.osc=e.osc;d.param=static_cast<params>(int(FILTER_FREQ)+int(b.filter_env_coef));
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

