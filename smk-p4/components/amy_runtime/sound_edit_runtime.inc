// Included inside the platform adapter translation unit; never part of AMY core.
extern "C" {
// Native routing table ABI from algorithms.c; no copied algorithms or AMY patch.
struct FmAlgorithm { uint8_t ops[MAX_ALGO_OPS]; };
extern const struct FmAlgorithm algorithms[33];
}
namespace smk::p4::amy {
static uint16_t waveFromLfoIndex(unsigned idx) {
    return idx==0?SINE:idx==1?TRIANGLE:idx==2?SAW_DOWN:idx==3?SAW_UP:PULSE;
}
static int lfoIndexFromWave(uint16_t w) {
    return w==SINE?0:w==TRIANGLE?1:w==SAW_DOWN?2:w==SAW_UP?3:w==PULSE?4:-1;
}
void Runtime::updateModWheelFilter() {
    if(state_.edit.family!=uint8_t(library::SynthFamily::Subtractive) || edit_filter_>=edit_count_) return;
    const auto& out=state_.edit;
    const bool cutoff_edited=!out.compare && ((out.mask&edit::bit(edit::Cutoff)) || out.macros[0]!=64);
    const float base_logfreq=cutoff_edited?logfreq_of_freq(out.applied[edit::Cutoff]):edit_oscs_[edit_filter_].cutoff;
    const bool mod_flt=!out.compare && (out.available&edit::bit(edit::ModEnable)) &&
        std::lround(out.applied[edit::ModEnable])==1 &&
        std::lround(out.applied[edit::ModSource])==1 &&
        std::lround(out.applied[edit::ModDest])==3;
    const float raw_amt=out.applied[edit::ModAmount]/100.0f;
    const int pol=int(std::lround(out.applied[edit::ModPolarity]));
    const float eff_amt=(pol==0)?std::fabs(raw_amt):(pol==2)?(-std::fabs(raw_amt)):raw_amt;
    bool queued=false;
    for(unsigned v=0;v<kVoices;++v) {
        const uint16_t osc_id=edit_map_[v][edit_filter_];
        if(osc_id>=kOscillators || !synth[osc_id]) continue;
        const uint8_t cc1=voices_[v].used?modulation_[voices_[v].channel]:modulation_[0];
        const float oct=mod_flt?(cc1/127.0f)*eff_amt*3.0f:0.0f;
        const float target=(!cutoff_edited && oct==0.0f)?edit_oscs_[edit_filter_].cutoff:(base_logfreq+oct);
        if(synth[osc_id]->filter_logfreq_coefs[COEF_CONST]!=target) {
            struct delta d{}; d.osc=osc_id; d.param=FILTER_FREQ; d.data.f=target;
            add_delta_to_queue(&d,&amy_global.delta_queue);
            queued=true;
        }
    }
    if(queued) amy_execute_deltas();
}
void Runtime::captureEdit() {
    const uint32_t revision=state_.edit.revision+1,epoch=state_.edit.epoch+1;
    edit_components_=edit::JunoComponents{};
    edit_env_=255;edit_lfo_=255;edit_filter_=255;
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
        b.feedback=s->feedback;
        b.freq_mod=s->logfreq_coefs[COEF_MOD];b.filter_mod=s->filter_logfreq_coefs[COEF_MOD];
        b.amp_mod=s->amp_coefs[COEF_MOD];b.duty_mod=s->duty_coefs[COEF_MOD];
        b.amp_eg0=s->amp_coefs[COEF_EG0];
        unsigned n=0;
        if(s->breakpoint_times[0])while(n<s->max_num_breakpoints[0] && AMY_IS_SET(s->breakpoint_times[0][n]))++n;
        // Juno ROM uses three ADSR breakpoints. Do not grow or reinterpret others.
        if(n==3 && s->breakpoint_times[0] && s->breakpoint_values[0]){
            b.envelope=true;b.release_index=2;b.attack_samples=s->breakpoint_times[0][0];
            b.decay_samples=s->breakpoint_times[0][1];b.sustain=s->breakpoint_values[0][1];
            b.release_samples=s->breakpoint_times[0][2];
        }
        // DX7 operator envelopes use 5 breakpoints (seg 1 = attack, seg 2/3 = decay).
        if(n==5 && s->eg_type[0]==ENVELOPE_DX7 && s->breakpoint_times[0] && s->breakpoint_values[0] && s->breakpoint_times[0][0]==0){
            b.op_attack_samples=s->breakpoint_times[0][1];
            b.op_attack_valid=std::fabs(s->breakpoint_values[0][1]-s->breakpoint_values[0][0])>1e-4f;
            if(std::fabs(s->breakpoint_values[0][2]-s->breakpoint_values[0][1])>1e-4f){
                b.op_decay_index=2;b.op_decay_samples=s->breakpoint_times[0][2];
            }else if(s->breakpoint_times[0][2]==0 && std::fabs(s->breakpoint_values[0][3]-s->breakpoint_values[0][2])>1e-4f){
                b.op_decay_index=3;b.op_decay_samples=s->breakpoint_times[0][3];
            }
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
            if(b.op_attack_valid || b.op_decay_index) {
                unsigned count=0;
                if(t->breakpoint_times[0] && t->breakpoint_values[0])
                    while(count<t->max_num_breakpoints[0] && AMY_IS_SET(t->breakpoint_times[0][count]))++count;
                if(count!=5 || t->eg_type[0]!=ENVELOPE_DX7 ||
                   std::memcmp(t->breakpoint_times[0],s->breakpoint_times[0],5*sizeof(uint32_t))!=0 ||
                   std::memcmp(t->breakpoint_values[0],s->breakpoint_values[0],5*sizeof(float))!=0) {
                    b.op_attack_valid=false;b.op_decay_index=0;
                }
            }
            if(t->role!=s->role || std::memcmp(t->logfreq_coefs,s->logfreq_coefs,sizeof(s->logfreq_coefs)) ||
                std::memcmp(t->duty_coefs,s->duty_coefs,sizeof(s->duty_coefs)) ||
                std::memcmp(t->pan_coefs,s->pan_coefs,sizeof(s->pan_coefs)) ||
                std::memcmp(t->amp_coefs,s->amp_coefs,sizeof(s->amp_coefs)) || t->wave!=s->wave || t->filter_logfreq_coefs[COEF_CONST]!=b.cutoff ||
                t->filter_type!=s->filter_type || t->filter_logfreq_coefs[b.filter_env_coef]!=b.filter_env || t->filter_logfreq_coefs[COEF_NOTE]!=b.filter_note ||
                t->resonance!=b.resonance || t->amp_coefs[COEF_CONST]!=b.amp || t->feedback!=b.feedback ||
                std::memcmp(&t->logratio,&b.ratio,sizeof(float))!=0)return;
        }
    }
    for(unsigned o=0;o<edit_count_;++o)if(synth[edit_map_[0][o]]->role==SYNTH_IS_MOD_SOURCE){
        bool same=true;
        for(unsigned v=1;v<kVoices;++v)if(synth[edit_map_[v][o]]->role!=SYNTH_IS_MOD_SOURCE)same=false;
        if(same && lfoIndexFromWave(edit_oscs_[o].wave)>=0){edit_lfo_=uint8_t(o);break;}
    }
    if(patch->family==library::SynthFamily::Subtractive){
        int filter=-1,env=-1;
        for(unsigned o=0;o<edit_count_;++o){
            const auto* s=synth[edit_map_[0][o]];
            if(s->filter_type!=FILTER_NONE){if(filter<0)filter=int(o);}
            if(edit_oscs_[o].envelope && s->amp_coefs[COEF_EG0]!=0 && env<0)env=int(o);
        }
        edit_filter_=filter<0?255:uint8_t(filter);
        if(filter>=0){out.original[edit::Cutoff]=freq_of_logfreq(edit_oscs_[filter].cutoff);
            out.original[edit::Resonance]=edit_oscs_[filter].resonance;
            out.available|=edit::bit(edit::Cutoff)|edit::bit(edit::Resonance);
            out.original[edit::FilterEnv]=edit_oscs_[filter].filter_env;
            out.original[edit::FilterKeytrack]=100.0f*edit_oscs_[filter].filter_note;
            if(edit::valid(edit::FilterEnv,out.original[edit::FilterEnv]))out.available|=edit::bit(edit::FilterEnv);
            if(edit::valid(edit::FilterKeytrack,out.original[edit::FilterKeytrack]))out.available|=edit::bit(edit::FilterKeytrack);}
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
                out.available|=edit::bit(edit::OscLevel)|edit::bit(edit::Detune);}
            for(unsigned param:{unsigned(edit::SubLevel),unsigned(edit::NoiseLevel)}){
                const int local=param==edit::SubLevel?map.sub:map.noise;
                if(local>=0 && edit::valid(param,100*edit_oscs_[local].amp)){
                    out.original[param]=100*edit_oscs_[local].amp;out.available|=edit::bit(param);
                }
            }
            if(map.pulse>=0){
                const auto* pulse=synth[edit_map_[0][map.pulse]];bool static_duty=true;
                for(unsigned c=1;c<NUM_COMBO_COEFS;++c)if(pulse->duty_coefs[c]!=0)static_duty=false;
                if(static_duty && edit_oscs_[map.pulse].amp>0 && edit::valid(edit::PulseWidth,100*edit_oscs_[map.pulse].duty)){
                    out.original[edit::PulseWidth]=100*edit_oscs_[map.pulse].duty;out.available|=edit::bit(edit::PulseWidth);
                }
            }
            out.original[edit::Pan]=(edit_oscs_[map.root].pan-0.5f)*128;
            out.original[edit::Velocity]=edit_oscs_[map.root].velocity*100;
            if(edit::valid(edit::Pan,out.original[edit::Pan]))out.available|=edit::bit(edit::Pan);
            if(edit::valid(edit::Velocity,out.original[edit::Velocity]))out.available|=edit::bit(edit::Velocity);
        }
        edit_env_=env<0?255:uint8_t(env);
        if(env>=0){out.original[edit::Attack]=edit_oscs_[env].attack_samples*1000.0f/kSampleRateHz;
            out.original[edit::Release]=edit_oscs_[env].release_samples*1000.0f/kSampleRateHz;
            out.original[edit::Decay]=edit_oscs_[env].decay_samples*1000.0f/kSampleRateHz;
            out.original[edit::Sustain]=edit_oscs_[env].sustain*100.0f;
            out.available|=edit::bit(edit::Attack)|edit::bit(edit::Release);
            if(edit::valid(edit::Decay,out.original[edit::Decay]) && edit::valid(edit::Sustain,out.original[edit::Sustain]))
                out.available|=edit::bit(edit::Decay)|edit::bit(edit::Sustain);}
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
            for(unsigned v=0;v<kVoices;++v)synth[edit_map_[v][local]]->velocity=1.0f;
            // Levels are relative percentages of the real, preserved ROM gain.
            out.original[edit::OpLevel0+op]=100;
            out.original[edit::OpRatio0+op]=std::exp2(edit_oscs_[local].ratio);
            out.available|=edit::bit(edit::OpLevel0+op);
            if(edit::valid(edit::OpRatio0+op,out.original[edit::OpRatio0+op]))
                out.available|=edit::bit(edit::OpRatio0+op);
            if(edit_oscs_[local].op_attack_valid){
                const float att_ms=edit_oscs_[local].op_attack_samples*1000.0f/kSampleRateHz;
                if(edit::valid(edit::OpAttack0+op,att_ms)){
                    out.original[edit::OpAttack0+op]=att_ms;
                    out.available|=edit::bit(edit::OpAttack0+op);
                }
            }
            if(edit_oscs_[local].op_decay_index){
                const float dec_ms=edit_oscs_[local].op_decay_samples*1000.0f/kSampleRateHz;
                if(edit::valid(edit::OpDecay0+op,dec_ms)){
                    out.original[edit::OpDecay0+op]=dec_ms;
                    out.available|=edit::bit(edit::OpDecay0+op);
                }
            }
            const float vel_pct=std::fmin(100.0f,std::fmax(0.0f,edit_oscs_[local].velocity*50.0f));
            out.original[edit::OpVel0+op]=vel_pct;
            if(edit::valid(edit::OpVel0+op,vel_pct))out.available|=edit::bit(edit::OpVel0+op);
        }
        out.original[edit::Depth]=1;out.available|=edit::bit(edit::Depth);
        out.original[edit::FmFeedback]=std::fmin(100.0f,std::fmax(0.0f,edit_oscs_[0].feedback*625.0f));
        if(edit::valid(edit::FmFeedback,out.original[edit::FmFeedback]))out.available|=edit::bit(edit::FmFeedback);
        out.original[edit::FmAlgorithm]=float(algo->algorithm);
        if(edit::valid(edit::FmAlgorithm,out.original[edit::FmAlgorithm]))
            out.available|=edit::bit(edit::FmAlgorithm);
    }
    if(edit_lfo_<edit_count_){
        const float rate_hz=freq_of_logfreq(edit_oscs_[edit_lfo_].freq);
        const int wave_idx=lfoIndexFromWave(edit_oscs_[edit_lfo_].wave);
        float depth_pct=0.0f,dest_idx=1.0f;
        if(patch->family==library::SynthFamily::Subtractive){
            const float fmod=edit_filter_<edit_count_?std::fabs(edit_oscs_[edit_filter_].filter_mod):0.0f;
            float pmod=0.0f;
            for(unsigned o=0;o<edit_count_;++o)if((edit_components_.main_mask&(1u<<o)) || int(o)==edit_components_.sub)
                pmod=std::fmax(pmod,std::fabs(edit_oscs_[o].freq_mod));
            if(fmod>pmod && fmod>0.0f){dest_idx=2.0f;depth_pct=std::fmin(100.0f,std::fmax(1.0f,std::round(fmod*50.0f)));}
            else if(pmod>0.0f){dest_idx=1.0f;depth_pct=std::fmin(100.0f,std::fmax(1.0f,std::round(pmod*1200.0f)));}
        }else{
            const float pmod=std::fabs(edit_oscs_[0].freq_mod);
            if(pmod>0.0f){dest_idx=1.0f;depth_pct=std::fmin(100.0f,std::fmax(1.0f,std::round(pmod*1200.0f)));}
        }
        if(edit::valid(edit::LfoRate,rate_hz) && wave_idx>=0){
            out.original[edit::LfoRate]=rate_hz;
            out.original[edit::LfoDepth]=depth_pct;
            out.original[edit::LfoWave]=float(wave_idx);
            out.original[edit::LfoDest]=dest_idx;
            out.available|=edit::bit(edit::LfoRate)|edit::bit(edit::LfoDepth)|edit::bit(edit::LfoWave)|edit::bit(edit::LfoDest);
        }
    }
    out.original[edit::ModEnable]=1.0f;
    out.original[edit::ModSource]=1.0f;
    out.original[edit::ModDest]=1.0f;
    out.original[edit::ModAmount]=50.0f;
    out.original[edit::ModPolarity]=1.0f;
    out.available|=edit::bit(edit::ModEnable)|edit::bit(edit::ModSource)|
        edit::bit(edit::ModDest)|edit::bit(edit::ModAmount)|edit::bit(edit::ModPolarity);
    bool levels_valid=true;
    for(unsigned v=0;v<kVoices;++v){edit_instrument_levels_[v]=instrument_get_level(v+1);
        if(!std::isfinite(edit_instrument_levels_[v]) || edit_instrument_levels_[v]<0)levels_valid=false;}
    if(levels_valid){out.original[edit::PatchLevel]=100;out.available|=edit::bit(edit::PatchLevel);}
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
        if((request.mask&edit::bit(p)) && !edit::valid(p,request.values[p])){++out.errors;return;}
    if((request.mask&edit::bit(edit::LfoDest)) && std::lround(request.values[edit::LfoDest])==2 &&
       (out.family==0 || edit_filter_>=edit_count_)) {++out.errors;return;}
    if(request.mask&edit::bit(edit::ModSource)) {
        const int ms=int(std::lround(request.values[edit::ModSource]));
        if(ms!=0 && ms!=1) {++out.errors;return;}
    }
    if(request.mask&edit::bit(edit::ModDest)) {
        const int md=int(std::lround(request.values[edit::ModDest]));
        if(md==2 || (md==3 && (out.family==0 || edit_filter_>=edit_count_))) {++out.errors;return;}
    }
    for(unsigned m=0;m<3;++m)if(request.macros[m]>127){++out.errors;return;}
    const auto macroMask=[&](const uint8_t* m){
        edit::Mask mask=0;
        if(m[0]!=64)mask|=edit::bit(out.family==1?edit::Cutoff:edit::Depth);
        if(out.family==1){if(m[1]!=64)mask|=edit::bit(edit::Attack);if(m[2]!=64)mask|=edit::bit(edit::Release);}
        return mask&out.available;
    };
    if(out.family==0 && (request.macros[1]!=64 || request.macros[2]!=64)){++out.errors;return;}
    const edit::Mask effective=request.compare?0:(request.mask|macroMask(request.macros));
    // Preflight the complete eight-voice topology before issuing any event.
    for(unsigned v=0;v<kVoices;++v)for(unsigned o=0;o<edit_count_;++o)
        if(!synth[edit_map_[v][o]] || (o!=edit_lfo_ && synth[edit_map_[v][o]]->wave!=edit_oscs_[o].wave)){++out.errors;return;}
    const auto value=[&](unsigned p){
        float v=!request.compare && (request.mask&edit::bit(p))?request.values[p]:out.original[p];
        if(!request.compare){
            if(p==edit::Cutoff && request.macros[0]!=64)v=std::fmax(20.0f,std::fmin(18000.0f,v*std::exp2((int(request.macros[0])-64)/32.0f)));
            if(p==edit::Depth)v*=std::fmin(1.0f,request.macros[0]/64.0f);
            if((p==edit::Attack || p==edit::Release) && request.macros[p==edit::Attack?1:2]!=64)
                v=std::fmax(1.0f,std::fmin(8000.0f,v*std::exp2((int(request.macros[p==edit::Attack?1:2])-64)/16.0f)));
        }
        return v;
    };
    const edit::Mask previous=out.compare?0:(out.mask|macroMask(out.macros));
    const edit::Mask touched=previous|effective;
    if(touched&edit::bit(edit::PatchLevel))for(unsigned v=0;v<kVoices;++v)
        instrument_set_level(v+1,edit_instrument_levels_[v]*((effective&edit::bit(edit::PatchLevel))?value(edit::PatchLevel)/100.0f:1.0f));
    for(unsigned v=0;v<kVoices;++v)for(unsigned o=0;o<edit_count_;++o){
        const auto& b=edit_oscs_[o];const auto* s=synth[edit_map_[v][o]];
        auto e=amy_default_event();e.osc=edit_map_[v][o];e.time=0;bool emit=false;
        if(o==edit_lfo_){
            if(touched&edit::bit(edit::LfoRate)){
                if(effective&edit::bit(edit::LfoRate)){e.freq_coefs[COEF_CONST]=value(edit::LfoRate);emit=true;}
                else{struct delta d{};d.osc=e.osc;d.param=FREQ;d.data.f=b.freq;add_delta_to_queue(&d,&amy_global.delta_queue);}
            }
            if(touched&edit::bit(edit::LfoWave)){
                struct delta d{};d.osc=e.osc;d.param=WAVE;
                d.data.i=(effective&edit::bit(edit::LfoWave))?waveFromLfoIndex(unsigned(std::lround(value(edit::LfoWave)))):b.wave;
                add_delta_to_queue(&d,&amy_global.delta_queue);
            }
            if(touched&(edit::bit(edit::LfoDepth)|edit::bit(edit::LfoDest))){
                const bool lfo_edited=(effective&(edit::bit(edit::LfoDepth)|edit::bit(edit::LfoDest)))!=0;
                const int ldest=int(std::lround(value(edit::LfoDest)));
                const float ldepth=value(edit::LfoDepth)/100.0f;
                struct delta d0{};d0.osc=e.osc;d0.param=static_cast<params>(int(AMP)+int(COEF_CONST));
                d0.data.f=lfo_edited?((ldest==0 || ldepth<=0.0f)?0.0f:1.0f):b.amp;
                add_delta_to_queue(&d0,&amy_global.delta_queue);
                struct delta d1{};d1.osc=e.osc;d1.param=static_cast<params>(int(AMP)+int(COEF_EG0));
                d1.data.f=lfo_edited?0.0f:b.amp_eg0;
                add_delta_to_queue(&d1,&amy_global.delta_queue);
            }
        }else if(touched&(edit::bit(edit::LfoDepth)|edit::bit(edit::LfoDest))){
            const bool lfo_edited=(effective&(edit::bit(edit::LfoDepth)|edit::bit(edit::LfoDest)))!=0;
            const int ldest=int(std::lround(value(edit::LfoDest)));
            const float ldepth=value(edit::LfoDepth)/100.0f;
            float fmod=b.freq_mod,fltmod=b.filter_mod,amod=b.amp_mod;
            if(lfo_edited){
                fmod=0.0f;fltmod=0.0f;amod=0.0f;
                if(out.family==uint8_t(library::SynthFamily::Subtractive)){
                    const auto& comp=edit_components_;
                    if(ldest==1 && ((comp.main_mask&(1u<<o)) || int(o)==comp.sub))fmod=ldepth*(1.0f/12.0f);
                    if(ldest==2 && s->filter_type!=FILTER_NONE)fltmod=ldepth*1.5f;
                    if(ldest==3 && int(o)==comp.root)amod=ldepth*0.08f;
                }else{
                    if(o==0 && ldest==1)fmod=ldepth*(1.0f/12.0f);
                    if(o==0 && ldest==3)amod=ldepth*0.08f;
                }
            }
            if(s->logfreq_coefs[COEF_MOD]!=fmod){
                struct delta df{};df.osc=e.osc;df.param=static_cast<params>(int(FREQ)+int(COEF_MOD));df.data.f=fmod;add_delta_to_queue(&df,&amy_global.delta_queue);
            }
            if(s->filter_type!=FILTER_NONE && s->filter_logfreq_coefs[COEF_MOD]!=fltmod){
                struct delta dfl{};dfl.osc=e.osc;dfl.param=static_cast<params>(int(FILTER_FREQ)+int(COEF_MOD));dfl.data.f=fltmod;add_delta_to_queue(&dfl,&amy_global.delta_queue);
            }
            if(s->amp_coefs[COEF_MOD]!=amod){
                struct delta da{};da.osc=e.osc;da.param=static_cast<params>(int(AMP)+int(COEF_MOD));da.data.f=amod;add_delta_to_queue(&da,&amy_global.delta_queue);
            }
        }
        if(out.family==uint8_t(library::SynthFamily::Subtractive)){
            const auto& components=edit_components_;
            if(components.main_mask&(1u<<o)){
                if(touched&edit::bit(edit::OscLevel)){e.amp_coefs[COEF_CONST]=b.amp*((effective&edit::bit(edit::OscLevel))?value(edit::OscLevel)/100.0f:1.0f);emit=true;}
                if(touched&edit::bit(edit::Detune)){
                    struct delta d{};d.osc=e.osc;d.param=FREQ;
                    d.data.f=(effective&edit::bit(edit::Detune))?b.freq+value(edit::Detune)/1200.0f:b.freq;add_delta_to_queue(&d,&amy_global.delta_queue);
                }
            }
            for(unsigned param:{unsigned(edit::SubLevel),unsigned(edit::NoiseLevel)}){
                const int local=param==edit::SubLevel?components.sub:components.noise;
                if(int(o)==local && (touched&edit::bit(param))){e.amp_coefs[COEF_CONST]=(effective&edit::bit(param))?value(param)/100.0f:b.amp;emit=true;}
            }
            if(int(o)==components.pulse && (touched&edit::bit(edit::PulseWidth))){e.duty_coefs[COEF_CONST]=(effective&edit::bit(edit::PulseWidth))?value(edit::PulseWidth)/100.0f:b.duty;emit=true;}
            if(int(o)==components.root){
                if(touched&edit::bit(edit::Pan)){e.pan_coefs[COEF_CONST]=(effective&edit::bit(edit::Pan))?0.5f+value(edit::Pan)/128.0f:b.pan;emit=true;}
                if(touched&edit::bit(edit::Velocity)){e.amp_coefs[COEF_VEL]=(effective&edit::bit(edit::Velocity))?value(edit::Velocity)/100.0f:b.velocity;emit=true;}
            }
            if(s->filter_type!=FILTER_NONE){
                if(touched&edit::bit(edit::Cutoff)){
                    if(effective&edit::bit(edit::Cutoff)){e.filter_freq_coefs[COEF_CONST]=value(edit::Cutoff);emit=true;}
                    else { // Hz/log lookup conversion is lossy: restore the captured native value exactly.
                        struct delta d{};d.osc=edit_map_[v][o];d.param=FILTER_FREQ;d.data.f=b.cutoff;
                        add_delta_to_queue(&d,&amy_global.delta_queue);
                    }
                }
                if(touched&edit::bit(edit::FilterEnv)){
                    struct delta d{};d.osc=e.osc;d.param=static_cast<params>(int(FILTER_FREQ)+int(b.filter_env_coef));
                    d.data.f=(effective&edit::bit(edit::FilterEnv))?value(edit::FilterEnv):b.filter_env;add_delta_to_queue(&d,&amy_global.delta_queue);
                }
                if(touched&edit::bit(edit::FilterKeytrack)){
                    struct delta d{};d.osc=e.osc;d.param=static_cast<params>(int(FILTER_FREQ)+int(COEF_NOTE));
                    d.data.f=(effective&edit::bit(edit::FilterKeytrack))?value(edit::FilterKeytrack)/100.0f:b.filter_note;add_delta_to_queue(&d,&amy_global.delta_queue);
                }
                if(touched&edit::bit(edit::Resonance)){e.resonance=(effective&edit::bit(edit::Resonance))?value(edit::Resonance):b.resonance;emit=true;}
            }
            if(b.envelope && s->amp_coefs[COEF_EG0]!=0){
                if(touched&(edit::bit(edit::Attack)|edit::bit(edit::Release)|edit::bit(edit::Decay)|edit::bit(edit::Sustain)))e.bp_is_set[0]=1;
                if(touched&edit::bit(edit::Attack)){e.eg0_times[0]=(effective&edit::bit(edit::Attack))?uint32_t(value(edit::Attack)):b.attack_samples*1000/kSampleRateHz;emit=true;}
                if(o==edit_env_ && (touched&edit::bit(edit::Decay))){
                    if(effective&edit::bit(edit::Decay)){e.eg0_times[1]=uint32_t(value(edit::Decay));emit=true;}
                    else {struct delta d{};d.osc=e.osc;d.param=static_cast<params>(BP_START+2);d.data.i=b.decay_samples;add_delta_to_queue(&d,&amy_global.delta_queue);}
                }
                if(o==edit_env_ && (touched&edit::bit(edit::Sustain))){e.eg0_values[1]=(effective&edit::bit(edit::Sustain))?value(edit::Sustain)/100.0f:b.sustain;emit=true;}
                if(touched&edit::bit(edit::Release)){e.eg0_times[b.release_index]=(effective&edit::bit(edit::Release))?uint32_t(value(edit::Release)):b.release_samples*1000/kSampleRateHz;emit=true;}
            }
        }else{
            if(o==0 && (touched&edit::bit(edit::FmFeedback))){
                struct delta d{};d.osc=e.osc;d.param=FEEDBACK;
                d.data.f=(effective&edit::bit(edit::FmFeedback))?value(edit::FmFeedback)/625.0f:b.feedback;
                add_delta_to_queue(&d,&amy_global.delta_queue);
            }
            for(unsigned op=0;op<6;++op)if(edit_ops_[op]==o){
                if(touched&(edit::bit(edit::OpLevel0+op)|edit::bit(edit::Depth))){
                    e.amp_coefs[COEF_CONST]=b.amp*(value(edit::OpLevel0+op)/100.0f)*(edit_modulators_[op]?value(edit::Depth):1.0f);emit=true;
                }
                if(touched&edit::bit(edit::OpRatio0+op)){
                    if(effective&edit::bit(edit::OpRatio0+op)){e.ratio=value(edit::OpRatio0+op);emit=true;}
                    else {struct delta d{};d.osc=edit_map_[v][o];d.param=RATIO;d.data.f=b.ratio;add_delta_to_queue(&d,&amy_global.delta_queue);}
                }
                if(b.op_attack_valid && (touched&edit::bit(edit::OpAttack0+op))){
                    struct delta d{};d.osc=e.osc;d.param=static_cast<params>(BP_START+2);
                    d.data.i=(effective&edit::bit(edit::OpAttack0+op))?
                        uint32_t(std::lround(value(edit::OpAttack0+op)*kSampleRateHz/1000.0f)):b.op_attack_samples;
                    add_delta_to_queue(&d,&amy_global.delta_queue);
                }
                if(b.op_decay_index && (touched&edit::bit(edit::OpDecay0+op))){
                    struct delta d{};d.osc=e.osc;d.param=static_cast<params>(BP_START+2*b.op_decay_index);
                    d.data.i=(effective&edit::bit(edit::OpDecay0+op))?
                        uint32_t(std::lround(value(edit::OpDecay0+op)*kSampleRateHz/1000.0f)):b.op_decay_samples;
                    add_delta_to_queue(&d,&amy_global.delta_queue);
                }
                if(touched&edit::bit(edit::OpVel0+op)){
                    struct delta d{};d.osc=e.osc;d.param=static_cast<params>(int(AMP)+int(COEF_VEL));
                    d.data.f=(effective&edit::bit(edit::OpVel0+op))?value(edit::OpVel0+op)/50.0f:b.velocity;
                    add_delta_to_queue(&d,&amy_global.delta_queue);
                }
            }
        }
        if(emit)amy_add_event(&e);
    }
    amy_execute_deltas();
    if(amy_get_oom_count()!=state_.oom_count){++out.errors;failRendering();return;}
    out.mask=request.mask;out.compare=request.compare;out.dirty=request.mask!=0 || request.macros[0]!=64 || request.macros[1]!=64 || request.macros[2]!=64;out.user_slot=request.slot;
    for(unsigned m=0;m<3;++m)out.macros[m]=request.macros[m];
    for(unsigned p=0;p<edit::Count;++p){out.manual[p]=request.mask&edit::bit(p)?request.values[p]:out.original[p];out.applied[p]=value(p);}
    const edit::Mask mod_mask=edit::bit(edit::Cutoff)|edit::bit(edit::LfoRate)|edit::bit(edit::ModEnable)|
        edit::bit(edit::ModSource)|edit::bit(edit::ModDest)|edit::bit(edit::ModAmount)|edit::bit(edit::ModPolarity);
    if((touched&mod_mask) || out.compare!=request.compare){
        updateModWheelFilter();
        for(unsigned i=0;i<kVoices;++i)if(voices_[i].used && voices_[i].usb)tune(i);
        amy_execute_deltas();
    }
}
}
