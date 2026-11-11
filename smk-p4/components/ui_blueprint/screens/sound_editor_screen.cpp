#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

namespace {

constexpr const char* kEditorTabs[7] = {"OSC", "FM", "FILTER", "AMP", "ENV", "LFO", "MOD"};

class SoundEditorScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::SoundEditor; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        const uint8_t tab = nav.subTab(ScreenId::SoundEditor) % 7;

        // 1. Sub-tab bar (y = 4..40)
        createSubTabBar(parent, kEditorTabs, 7, tab, ScreenId::SoundEditor, nav, 8, 4, 784, 36);

        // 2. Contextual Patch & Synthesis Family Banner (y = 44..76)
        lv_obj_t* banner = lv_obj_create(parent);
        applyPanelStyle(banner, false);
        lv_obj_set_pos(banner, 8, 44);
        lv_obj_set_size(banner, 784, 32);
        lv_obj_set_style_pad_all(banner, 4, 0);

        const auto& patch = backend.model().activePatch();
        char btxt[96];
        std::snprintf(btxt, sizeof(btxt),
                      "EDITING: %03u %s  |  FAMILY: %s  |  DEMO STATE (NO AMY DSP WRITES)",
                      patch.id, patch.name, synthFamilyName(patch.family));
        if(backend.hasSoundEditor())std::snprintf(btxt,sizeof(btxt),"%03u %s | %s",patch.id,patch.name,backend.soundEditStatus());
        banner_=createLabel(banner, btxt, 6, 2, 760, &lv_font_montserrat_14, palette::kAccentPrimary);
        if(backend.hasSoundEditor()){
            struct Action { IUiBackend* backend;unsigned action; };
            const char* labels[]={"COMPARE","REVERT","USER / SAVE AS"};
            for(unsigned a=0;a<3;++a){
                auto* button=createActionButton(parent,labels[a],8+int(a)*264,350,256,36,nullptr,nullptr);
                attachLvContext<Action>(button,Action{&backend,a},[](lv_event_t* e){
                    if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;
                    auto* c=static_cast<Action*>(lv_event_get_user_data(e));
                    if(c->action<2)c->backend->requestSoundAction(c->action);
                    else showPresets(static_cast<lv_obj_t*>(lv_event_get_current_target(e)),*c->backend);
                },LV_EVENT_CLICKED);
            }
        }

        // 3. Tab-specific parameter grid (y = 82..386)
        const int row2=backend.hasSoundEditor()?170:184,row3=backend.hasSoundEditor()?258:286;
        const int cell_h=backend.hasSoundEditor()?78:94;
        switch (tab) {
            case 0: // OSC
                if(backend.hasSoundEditor()){
                    createParameterCard(parent, backend, "osc.1.level", 8,82,256,cell_h);
                    createParameterCard(parent, backend, "osc.sub_level",272,82,256,cell_h);
                    createParameterCard(parent, backend, "osc.noise_level",536,82,256,cell_h);
                    createParameterCard(parent, backend, "osc.1.detune_ct",8,row2,388,cell_h);
                    createParameterCard(parent, backend, "osc.1.pulse_width",404,row2,388,cell_h);
                    auto* note=lv_obj_create(parent);applyPanelStyle(note,false);lv_obj_set_pos(note,8,row3);lv_obj_set_size(note,784,cell_h);
                    createLabel(note,"NATIVE COMPONENT MIX",8,6,740,&lv_font_montserrat_14,palette::kAccentPrimary);
                    createLabel(note,"MAIN = % OF ROM MIX; SUB/NOISE = NATIVE LEVEL. N/A: INCOMPATIBLE TOPOLOGY / PWM.",8,34,756,&lv_font_montserrat_14,palette::kTextSecondary);
                    break;
                }
                createParameterCard(parent, backend, "osc.1.wave",        8, 82, 256, cell_h);
                createParameterCard(parent, backend, "osc.1.pitch_st",    272, 82, 256, cell_h);
                createParameterCard(parent, backend, "osc.1.detune_ct",   536, 82, 256, cell_h);

                createParameterCard(parent, backend, "osc.1.level",       8, row2, 256, cell_h);
                createParameterCard(parent, backend, "osc.1.pan",         272, row2, 256, cell_h);
                createParameterCard(parent, backend, "osc.1.pulse_width", 536, row2, 256, cell_h);

                createParameterCard(parent, backend, "osc.1.unison",      8, row3, 256, cell_h);
                createParameterCard(parent, backend, "osc.sub_level",     272, row3, 256, cell_h);
                createParameterCard(parent, backend, "osc.noise_level",   536, row3, 256, cell_h);
                break;

            case 1: // FM
                createParameterCard(parent, backend, backend.hasSoundEditor()?"fm.depth":"fm.algo", 8, 82, 190, cell_h);
                createParameterCard(parent, backend, "fm.selected_op", 206, 82, 190, cell_h);
                createParameterCard(parent, backend, "fm.op.ratio",    404, 82, 190, cell_h);
                createParameterCard(parent, backend, "fm.op.level",    602, 82, 190, cell_h);

                createParameterCard(parent, backend, "fm.feedback",    8, row2, 190, cell_h);
                createParameterCard(parent, backend, "fm.op.attack",   206, row2, 190, cell_h);
                createParameterCard(parent, backend, "fm.op.decay",    404, row2, 190, cell_h);
                createParameterCard(parent, backend, "fm.op.vel_sens", 602, row2, 190, cell_h);

                createFmAlgorithmSelector(parent, backend, 8, row3, 200, cell_h);
                createFmTopologyDiagram(parent, backend, 216, row3, 576, cell_h);
                break;

            case 2: // FILTER
                createParameterCard(parent, backend, "filter.type",       8, 82, 256, cell_h);
                createParameterCard(parent, backend, "filter.cutoff_hz",  272, 82, 256, cell_h);
                createParameterCard(parent, backend, "filter.resonance",  536, 82, 256, cell_h);

                createParameterCard(parent, backend, "filter.drive",      8, row2, 256, cell_h);
                createParameterCard(parent, backend, "filter.keytrack",   272, row2, 256, cell_h);
                createParameterCard(parent, backend, "filter.env_amount", 536, row2, 256, cell_h);

                {
                    lv_obj_t* fcard = lv_obj_create(parent);
                    applyPanelStyle(fcard, false);
                    lv_obj_set_pos(fcard, 8, row3);
                    lv_obj_set_size(fcard, 784, cell_h);
                    createLabel(fcard, "BIQUAD / ZDF FILTER TOPOLOGY NOTES", 8, 6, 420,
                                &lv_font_montserrat_14, palette::kAccentPrimary);
                    createLabel(fcard,
                                "Cutoff, Q, Keytrack & Env Amount: native AMY controls on Juno. Filter Type/Drive PLANNED.",
                                8, 34, 756, &lv_font_montserrat_14, palette::kTextSecondary);
                }
                break;

            case 3: // AMP
                createParameterCard(parent, backend, "amp.level",          8,   82, 190, 110);
                createParameterCard(parent, backend, "amp.pan",            206, 82, 190, 110);
                createParameterCard(parent, backend, "amp.vel_sens",       404, 82, 190, 110);
                createParameterCard(parent, backend, "amp.portamento_ms",  602, 82, 190, 110);
                createParameterCard(parent, backend, "perf.master_volume", 8,   202, 388, 110);
                break;

            case 4: // ENV
                createEnvelopeEditor(parent, backend, 8, 82, 784, 148);
                createParameterCard(parent, backend, "env.amp.attack_ms",  8,   238, 150, backend.hasSoundEditor()?104:144);
                createParameterCard(parent, backend, "env.amp.decay_ms",   166, 238, 150, backend.hasSoundEditor()?104:144);
                createParameterCard(parent, backend, "env.amp.sustain",    324, 238, 150, backend.hasSoundEditor()?104:144);
                createParameterCard(parent, backend, "env.amp.release_ms", 482, 238, 150, backend.hasSoundEditor()?104:144);
                createParameterCard(parent, backend, "env.amp.curve",      640, 238, 152, backend.hasSoundEditor()?104:144);
                break;

            case 5: // LFO
                createParameterCard(parent, backend, "lfo.1.wave",      8,   82, 256, 110);
                createParameterCard(parent, backend, "lfo.1.rate_hz",   272, 82, 256, 110);
                createParameterCard(parent, backend, "lfo.1.depth",     536, 82, 256, 110);

                createParameterCard(parent, backend, "lfo.1.phase_deg", 8,   202, 256, 110);
                createParameterCard(parent, backend, "lfo.1.sync",      272, 202, 256, 110);
                createParameterCard(parent, backend, "lfo.1.dest",      536, 202, 256, 110);
                break;

            case 6: // MOD
                createModulationMatrixWidget(parent, backend, nav, 0, 8, 82, 784, backend.hasSoundEditor()?260:298);
                break;
        }

        return true;
    }

    void refresh(IUiBackend& backend) override {
        if(banner_ && backend.hasSoundEditor()){
            char text[96];std::snprintf(text,sizeof(text),"%s | %s",backend.soundEditName(),backend.soundEditStatus());lv_label_set_text(banner_,text);
        }
        if(s_active_preset_modal_){
            lv_obj_send_event(s_active_preset_modal_, LV_EVENT_REFRESH, nullptr);
        }
    }
private:
    lv_obj_t* banner_=nullptr;
    static inline lv_obj_t* s_active_preset_modal_ = nullptr;
    struct PresetPanel {
        IUiBackend* backend = nullptr;
        lv_obj_t* root = nullptr;
        lv_obj_t* slot = nullptr;
        lv_obj_t* name = nullptr;
        lv_obj_t* info = nullptr;
        lv_obj_t* save_btn = nullptr;
        lv_obj_t* recall_btn = nullptr;
        lv_obj_t* close_btn = nullptr;
        bool overwrite = false;
        unsigned overwrite_slot = 0;
        char overwrite_cur_name[25]{};
        char overwrite_new_name[25]{};
        PresetOperationState last_seen_op_state = PresetOperationState::Idle;
    };

    static void updateSlotDropdown(lv_obj_t* slot, IUiBackend& backend) {
        if(!slot) return;
        char* options = static_cast<char*>(lv_malloc(800));
        if(!options) return;
        std::memset(options,0,800);
        unsigned used=0;
        for(unsigned i=0;i<16;++i){
            char name[25]{};uint16_t origin=0;
            const bool valid=backend.userPreset(i,name,sizeof(name),origin);
            const int n=std::snprintf(options+used,800-used,"%s%02u %s",i?"\n":"",i+1,valid?name:"EMPTY / INVALID");
            if(n<0 || unsigned(n)>=800-used)break;
            used+=unsigned(n);
        }
        lv_dropdown_set_options(slot,options);
        lv_free(options);
    }
public:
    static void showPresets(lv_obj_t* button,IUiBackend& backend){
        lv_obj_t* screen = button ? lv_obj_get_screen(button) : lv_screen_active();
        if(!screen) screen = lv_screen_active();
        if(!screen) return;
        auto* root=lv_obj_create(screen);
        if(!root) return;
        applyPanelStyle(root,true);
        lv_obj_set_pos(root,40,50);lv_obj_set_size(root,720,380);
        createLabel(root,"USER PRESETS | SAVE ONLY WITH NOTES RELEASED",12,8,680);
        auto* slot=lv_dropdown_create(root);lv_obj_set_pos(slot,12,42);lv_obj_set_size(slot,260,40);
        updateSlotDropdown(slot, backend);
        auto* name=lv_textarea_create(root);lv_obj_set_pos(name,284,42);lv_obj_set_size(name,396,42);
        lv_textarea_set_one_line(name,true);lv_textarea_set_max_length(name,24);lv_textarea_set_text(name,"MY SOUND 01");
        auto* info=createLabel(root,"SELECT SLOT; SAVE AS ALSO RENAMES. OCCUPIED SLOT REQUIRES TWO PRESSES.",12,88,680);
        auto* keyboard=lv_keyboard_create(root);lv_obj_set_align(keyboard,LV_ALIGN_TOP_LEFT);lv_obj_set_pos(keyboard,12,166);lv_obj_set_size(keyboard,680,190);lv_keyboard_set_textarea(keyboard,name);
        auto* save=createActionButton(root,"SAVE AS",12,118,210,40,nullptr,nullptr);
        auto* recall=createActionButton(root,"RECALL",244,118,210,40,nullptr,nullptr);
        auto* close=createActionButton(root,"CLOSE",476,118,204,40,nullptr,nullptr);
        auto* context=attachLvContext<PresetPanel>(root,PresetPanel{&backend,root,slot,name,info,save,recall,close,false,0,{},{},PresetOperationState::Idle},nullptr);
        if(!context){lv_obj_delete(root);return;}
        s_active_preset_modal_ = root;
        lv_obj_add_event_cb(root, [](lv_event_t* e){
            (void)e;
            s_active_preset_modal_ = nullptr;
        }, LV_EVENT_DELETE, nullptr);
        lv_obj_add_event_cb(root, [](lv_event_t* e){
            auto* c = static_cast<PresetPanel*>(lv_event_get_user_data(e));
            if(!c || !c->backend) return;
            const auto op = c->backend->presetOperationStatus();
            const bool pending = (op.state == PresetOperationState::Requested ||
                                  op.state == PresetOperationState::WaitingForQuiet ||
                                  op.state == PresetOperationState::Writing ||
                                  op.state == PresetOperationState::Verifying);
            if(pending){
                if(c->save_btn) lv_obj_add_state(c->save_btn, LV_STATE_DISABLED);
                if(c->recall_btn) lv_obj_add_state(c->recall_btn, LV_STATE_DISABLED);
                if(c->info && op.message && op.message[0]){
                    lv_label_set_text(c->info, op.message);
                    lv_obj_set_style_text_color(c->info, lv_color_hex(palette::kAccentPrimary), 0);
                }
            } else {
                if(c->save_btn) lv_obj_clear_state(c->save_btn, LV_STATE_DISABLED);
                if(c->recall_btn) lv_obj_clear_state(c->recall_btn, LV_STATE_DISABLED);
                if(op.state != c->last_seen_op_state){
                    if(op.state == PresetOperationState::Confirmed){
                        if(op.action == 2 && c->slot){
                            updateSlotDropdown(c->slot, *c->backend);
                        }
                        if(c->info && op.message && op.message[0]){
                            lv_label_set_text(c->info, op.message);
                            lv_obj_set_style_text_color(c->info, lv_color_hex(palette::kActive), 0);
                        }
                    } else if(op.state == PresetOperationState::Failed ||
                              op.state == PresetOperationState::OutcomeUnknown ||
                              op.state == PresetOperationState::Rejected){
                        if(c->info && op.message && op.message[0]){
                            lv_label_set_text(c->info, op.message);
                            lv_obj_set_style_text_color(c->info, lv_color_hex(palette::kWarning), 0);
                        }
                    }
                }
            }
            c->last_seen_op_state = op.state;
        }, LV_EVENT_REFRESH, context);

        lv_obj_add_event_cb(slot,[](lv_event_t* e){auto* c=static_cast<PresetPanel*>(lv_event_get_user_data(e));
            if(!c || !c->backend) return;
            c->overwrite=false;char name[25]{};uint16_t origin=0;
            if(c->backend->userPreset(lv_dropdown_get_selected(c->slot),name,sizeof(name),origin)){
                char text[80];std::snprintf(text,sizeof(text),"%s | FACTORY %03u",name,origin);lv_label_set_text(c->info,text);lv_textarea_set_text(c->name,name);
            }else lv_label_set_text(c->info,"EMPTY OR INVALID SLOT");
            lv_obj_set_style_text_color(c->info, lv_color_hex(palette::kTextSecondary), 0);
        },LV_EVENT_VALUE_CHANGED,context);

        lv_obj_add_event_cb(name,[](lv_event_t* e){
            auto* c=static_cast<PresetPanel*>(lv_event_get_user_data(e));
            if(c) c->overwrite = false;
        },LV_EVENT_VALUE_CHANGED,context);

        lv_obj_add_event_cb(save,[](lv_event_t* e){auto* c=static_cast<PresetPanel*>(lv_event_get_user_data(e));
            if(!c || !c->backend) return;
            const unsigned slot=lv_dropdown_get_selected(c->slot);
            const char* new_name = lv_textarea_get_text(c->name);
            char cur_name[25]{};uint16_t origin=0;
            const bool occupied = c->backend->userPreset(slot,cur_name,sizeof(cur_name),origin);
            if(occupied && (!c->overwrite || c->overwrite_slot != slot || std::strcmp(c->overwrite_new_name, new_name) != 0)){
                c->overwrite=true;
                c->overwrite_slot = slot;
                std::snprintf(c->overwrite_cur_name, sizeof(c->overwrite_cur_name), "%s", cur_name);
                std::snprintf(c->overwrite_new_name, sizeof(c->overwrite_new_name), "%s", new_name);
                char conf_msg[160];
                std::snprintf(conf_msg, sizeof(conf_msg),
                    "OVERWRITE SLOT %02u?\nCURRENT: %s | NEW: %s\nPRESS SAVE AS AGAIN TO CONFIRM",
                    slot + 1, cur_name, new_name);
                lv_label_set_text(c->info, conf_msg);
                lv_obj_set_style_text_color(c->info, lv_color_hex(palette::kWarning), 0);
                return;
            }
            c->overwrite=false;
            const auto r=c->backend->requestSoundAction(2,slot,new_name);
            if(r.notice) lv_label_set_text(c->info, r.notice);
            else lv_label_set_text(c->info, r.accepted() ? "SAVE REQUESTED" : "SAVE REJECTED");
            lv_obj_set_style_text_color(c->info, lv_color_hex(r.accepted() ? palette::kAccentPrimary : palette::kWarning), 0);
        },LV_EVENT_CLICKED,context);

        lv_obj_add_event_cb(recall,[](lv_event_t* e){auto* c=static_cast<PresetPanel*>(lv_event_get_user_data(e));
            if(!c || !c->backend) return;
            c->overwrite = false;
            const auto r=c->backend->requestSoundAction(3,lv_dropdown_get_selected(c->slot));
            if(r.notice) lv_label_set_text(c->info, r.notice);
            else lv_label_set_text(c->info, r.accepted() ? "RECALL REQUESTED" : "RECALL REJECTED");
            lv_obj_set_style_text_color(c->info, lv_color_hex(r.accepted() ? palette::kAccentPrimary : palette::kWarning), 0);
        },LV_EVENT_CLICKED,context);

        lv_obj_add_event_cb(close,[](lv_event_t* e){auto* c=static_cast<PresetPanel*>(lv_event_get_user_data(e));
            if(c && c->root) lv_obj_delete(c->root);
        },LV_EVENT_CLICKED,context);

        lv_obj_send_event(slot,LV_EVENT_VALUE_CHANGED,nullptr);
    }
};

} // namespace

void showUserPresetPanel(lv_obj_t* anchor,IUiBackend& backend){SoundEditorScreen::showPresets(anchor,backend);}

ScreenView* createSoundEditorScreen() {
    return new SoundEditorScreen();
}

} // namespace smk::p4::ui
