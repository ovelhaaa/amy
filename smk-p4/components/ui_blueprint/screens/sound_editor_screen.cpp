#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

namespace {

constexpr uint8_t mapSoundEditorModule(uint8_t raw_tab) {
    if (raw_tab <= 4) return raw_tab;
    return 3; // Legacy tabs 5 (LFO) and 6 (MOD) map cleanly to LFO / MOD
}

class SoundEditorScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::SoundEditor; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        parent_ = parent;
        backend_ = &backend;
        nav_ = &nav;
        confirm_dialog_.close(false);
        compare_btn_ = nullptr;
        const auto& patch = backend.model().activePatch();
        shown_family_ = patch.family;
        const bool is_fm = (patch.family == SynthFamily::FM);
        const uint8_t mod = mapSoundEditorModule(nav.subTab(ScreenId::SoundEditor));

        // 1. Left Contextual Sidebar (x = 4..160, y = 4..384)
        lv_obj_t* sidebar = lv_obj_create(parent);
        applyPanelStyle(sidebar, false);
        lv_obj_set_pos(sidebar, 4, 4);
        lv_obj_set_size(sidebar, 156, 380);
        lv_obj_set_style_pad_all(sidebar, 4, 0);
        lv_obj_remove_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);

        createLabel(sidebar, "SINTESE", 4, 2, 140, &lv_font_montserrat_14, palette::kAccentPrimary);
        createLabel(sidebar, is_fm ? "MOTOR: FM 6-OP" : "MOTOR: SUBTRATIVO", 4, 20, 140,
                    &lv_font_montserrat_14, palette::kActive);

        const char* mod_labels[5] = {
            "GERAL",
            is_fm ? "FM / OPS" : "OSCILADORES",
            "FILTRO / ENV",
            "LFO / MOD",
            "AVANCADO"
        };
        struct ModCtx { UiNavigationManager* nav; uint8_t mod; };
        for (uint8_t i = 0; i < 5; ++i) {
            lv_obj_t* btn = createActionButton(
                sidebar, mod_labels[i], 2, 44 + i * 48, 144, 42,
                nullptr, nullptr, mod == i, false);
            if (btn) {
                attachLvContext<ModCtx>(btn, {&nav, i}, [](lv_event_t* e) {
                    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                    auto* c = static_cast<ModCtx*>(lv_event_get_user_data(e));
                    if (c && c->nav) c->nav->requestNavigateTo(ScreenId::SoundEditor, c->mod);
                }, LV_EVENT_CLICKED);
            }
        }

        lv_obj_t* lib_btn = createActionButton(sidebar, "BIBLIOTECA >", 2, 296, 144, 34, nullptr, nullptr);
        lv_obj_t* perf_btn = createActionButton(sidebar, "TOCAR >", 2, 336, 144, 34, nullptr, nullptr);
        if (lib_btn) {
            attachLvContext<UiNavigationManager*>(lib_btn, &nav, [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* n = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
                if (n) n->requestNavigateTo(ScreenId::PatchBrowser);
            }, LV_EVENT_CLICKED);
        }
        if (perf_btn) {
            attachLvContext<UiNavigationManager*>(perf_btn, &nav, [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* n = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
                if (n) n->requestReturnToPerformance();
            }, LV_EVENT_CLICKED);
        }

        // 2. Top Context Banner (x = 166..794, y = 4..38)
        lv_obj_t* banner = lv_obj_create(parent);
        applyPanelStyle(banner, false);
        lv_obj_set_pos(banner, 166, 4);
        lv_obj_set_size(banner, 628, 34);
        lv_obj_set_style_pad_all(banner, 4, 0);
        lv_obj_remove_flag(banner, LV_OBJ_FLAG_SCROLLABLE);

        char btxt[96];
        if (backend.hasSoundEditor()) {
            std::snprintf(btxt, sizeof(btxt), "%03u %s (%s) | %s",
                          patch.id, patch.name, is_fm ? "FM" : "SUB", backend.soundEditStatus());
        } else {
            std::snprintf(btxt, sizeof(btxt), "DEMO: %03u %s | %s (SEM AMY DSP)",
                          patch.id, patch.name, synthFamilyName(patch.family));
        }
        banner_ = createLabel(banner, btxt, 6, 2, 612, &lv_font_montserrat_14, palette::kAccentPrimary);
        if (banner_) lv_label_set_long_mode(banner_, LV_LABEL_LONG_MODE_DOTS);

        // 3. Bottom Action Strip (x = 166..794, y = 344..384)
        if (backend.hasSoundEditor()) {
            struct ActionCtx { SoundEditorScreen* screen; IUiBackend* backend; unsigned action; };
            compare_btn_ = createActionButton(parent, "COMPARE", 166, 344, 146, 38, nullptr, nullptr);
            lv_obj_t* rev_btn = createActionButton(parent, "REVERT", 318, 344, 146, 38, nullptr, nullptr);
            lv_obj_t* usr_btn = createActionButton(parent, "USER / SAVE AS", 470, 344, 182, 38, nullptr, nullptr, false, true);
            lv_obj_t* mod_btn = createActionButton(parent, "MOD >", 658, 344, 136, 38, nullptr, nullptr);

            const auto on_act = [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* c = static_cast<ActionCtx*>(lv_event_get_user_data(e));
                if (!c || !c->backend || !c->screen) return;
                if (c->action == 0) {
                    c->backend->requestSoundAction(0);
                } else if (c->action == 1) {
                    if (c->backend->patchNeedsDiscardConfirmation()) {
                        c->screen->openRevertConfirm();
                    } else {
                        c->backend->requestSoundAction(1);
                    }
                } else if (c->action == 2) {
                    showPresets(static_cast<lv_obj_t*>(lv_event_get_current_target(e)), *c->backend);
                }
            };
            if (compare_btn_) attachLvContext<ActionCtx>(compare_btn_, {this, &backend, 0}, on_act, LV_EVENT_CLICKED);
            if (rev_btn) attachLvContext<ActionCtx>(rev_btn, {this, &backend, 1}, on_act, LV_EVENT_CLICKED);
            if (usr_btn) attachLvContext<ActionCtx>(usr_btn, {this, &backend, 2}, on_act, LV_EVENT_CLICKED);
            if (mod_btn) {
                attachLvContext<UiNavigationManager*>(mod_btn, &nav, [](lv_event_t* e) {
                    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                    auto* n = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
                    if (n) n->requestNavigateTo(ScreenId::Modulation);
                }, LV_EVENT_CLICKED);
            }
        } else {
            lv_obj_t* demo_bar = lv_obj_create(parent);
            applyPanelStyle(demo_bar, false);
            lv_obj_set_pos(demo_bar, 166, 344);
            lv_obj_set_size(demo_bar, 628, 38);
            lv_obj_set_style_pad_all(demo_bar, 4, 0);
            createLabel(demo_bar, "DEMO RAM: Compare / Revert / User Presets requerem Audio Owner", 6, 4, 610,
                        &lv_font_montserrat_14, palette::kTextSecondary);
        }

        // 4. Contextual Module Body (x = 166..794, y = 42..338, 628x296)
        constexpr int32_t kX = 166;
        constexpr int32_t kY = 42;
        switch (mod) {
            case 0: // GERAL
                createParameterCard(parent, backend, "amp.level",      kX,       kY,       310, 92);
                createParameterCard(parent, backend, "amp.pan",        kX + 318, kY,       310, 92);
                createParameterCard(parent, backend, "amp.vel_sens",   kX,       kY + 100, 310, 92);
                if (is_fm) {
                    createParameterCard(parent, backend, "fm.depth",          kX + 318, kY + 100, 310, 92);
                    createParameterCard(parent, backend, "fm.feedback",       kX,       kY + 200, 310, 92);
                    createParameterCard(parent, backend, "perf.master_volume",kX + 318, kY + 200, 310, 92);
                } else {
                    createParameterCard(parent, backend, "osc.1.level",       kX + 318, kY + 100, 310, 92);
                    createParameterCard(parent, backend, "filter.cutoff_hz",  kX,       kY + 200, 310, 92);
                    createParameterCard(parent, backend, "perf.master_volume",kX + 318, kY + 200, 310, 92);
                }
                break;

            case 1: // FM / OPS or OSCILADORES
                if (is_fm) {
                    createParameterCard(parent, backend, backend.hasSoundEditor() ? "fm.depth" : "fm.algo", kX, kY, 152, 88);
                    createParameterCard(parent, backend, "fm.selected_op", kX + 158, kY, 152, 88);
                    createParameterCard(parent, backend, "fm.op.ratio",    kX + 316, kY, 152, 88);
                    createParameterCard(parent, backend, "fm.op.level",    kX + 474, kY, 154, 88);

                    createParameterCard(parent, backend, "fm.feedback",    kX,       kY + 94, 152, 88);
                    createParameterCard(parent, backend, "fm.op.attack",   kX + 158, kY + 94, 152, 88);
                    createParameterCard(parent, backend, "fm.op.decay",    kX + 316, kY + 94, 152, 88);
                    createParameterCard(parent, backend, "fm.op.vel_sens", kX + 474, kY + 94, 154, 88);

                    createFmAlgorithmSelector(parent, backend, kX, kY + 188, 144, 108);
                    createFmTopologyDiagram(parent, backend, kX + 148, kY + 188, 480, 108);
                } else if (backend.hasSoundEditor()) {
                    createParameterCard(parent, backend, "osc.1.level",       kX,       kY,      204, 92);
                    createParameterCard(parent, backend, "osc.sub_level",     kX + 212, kY,      204, 92);
                    createParameterCard(parent, backend, "osc.noise_level",   kX + 424, kY,      204, 92);
                    createParameterCard(parent, backend, "osc.1.detune_ct",   kX,       kY + 100,310, 92);
                    createParameterCard(parent, backend, "osc.1.pulse_width", kX + 318, kY + 100,310, 92);

                    lv_obj_t* note = lv_obj_create(parent);
                    applyPanelStyle(note, false);
                    lv_obj_set_pos(note, kX, kY + 200);
                    lv_obj_set_size(note, 628, 92);
                    createLabel(note, "MIX DE OSCILADORES (SUBTRATIVO)", 8, 6, 420,
                                &lv_font_montserrat_14, palette::kAccentPrimary);
                    createLabel(note, "MAIN = % do mix ROM; SUB/NOISE = nivel nativo; PWM/DETUNE ativos no Juno.",
                                8, 34, 608, &lv_font_montserrat_14, palette::kTextSecondary);
                } else {
                    createParameterCard(parent, backend, "osc.1.wave",        kX,       kY,      204, 92);
                    createParameterCard(parent, backend, "osc.1.pitch_st",    kX + 212, kY,      204, 92);
                    createParameterCard(parent, backend, "osc.1.detune_ct",   kX + 424, kY,      204, 92);
                    createParameterCard(parent, backend, "osc.1.level",       kX,       kY + 100,204, 92);
                    createParameterCard(parent, backend, "osc.1.pulse_width", kX + 212, kY + 100,204, 92);
                    createParameterCard(parent, backend, "osc.sub_level",     kX + 424, kY + 100,204, 92);
                    createParameterCard(parent, backend, "osc.1.pan",         kX,       kY + 200,204, 92);
                    createParameterCard(parent, backend, "osc.1.unison",      kX + 212, kY + 200,204, 92);
                    createParameterCard(parent, backend, "osc.noise_level",   kX + 424, kY + 200,204, 92);
                }
                break;

            case 2: // FILTRO / ENV
                if (is_fm) {
                    lv_obj_t* fnote = lv_obj_create(parent);
                    applyPanelStyle(fnote, false);
                    lv_obj_set_pos(fnote, kX, kY);
                    lv_obj_set_size(fnote, 628, 36);
                    lv_obj_set_style_pad_all(fnote, 4, 0);
                    createLabel(fnote, "MOTOR FM: ENVELOPE DE AMPLITUDE (FILTRO ANALOGICO N/A NO DX7)",
                                6, 4, 610, &lv_font_montserrat_14, palette::kWarning);
                    createEnvelopeEditor(parent, backend, kX, kY + 42, 628, 134);
                } else {
                    createParameterCard(parent, backend, "filter.cutoff_hz",  kX,       kY, 204, 86);
                    createParameterCard(parent, backend, "filter.resonance",  kX + 212, kY, 204, 86);
                    createParameterCard(parent, backend, "filter.env_amount", kX + 424, kY, 204, 86);
                    createParameterCard(parent, backend, "filter.keytrack",   kX,       kY + 92, 176, 88);
                    createEnvelopeEditor(parent, backend, kX + 182, kY + 92, 446, 88);
                }
                createParameterCard(parent, backend, "env.amp.attack_ms",  kX,       kY + 186, 152, 110);
                createParameterCard(parent, backend, "env.amp.decay_ms",   kX + 158, kY + 186, 152, 110);
                createParameterCard(parent, backend, "env.amp.sustain",    kX + 316, kY + 186, 152, 110);
                createParameterCard(parent, backend, "env.amp.release_ms", kX + 474, kY + 186, 154, 110);
                break;

            case 3: // LFO / MOD
                createParameterCard(parent, backend, "lfo.1.wave",    kX,       kY, 152, 88);
                createParameterCard(parent, backend, "lfo.1.rate_hz", kX + 158, kY, 152, 88);
                createParameterCard(parent, backend, "lfo.1.depth",   kX + 316, kY, 152, 88);
                createParameterCard(parent, backend, "lfo.1.dest",    kX + 474, kY, 154, 88);
                createModulationMatrixWidget(parent, backend, nav, 0, kX, kY + 94, 628, 202);
                break;

            case 4: // AVANCADO (preserves all secondary & planned parameters)
                createParameterCard(parent, backend, "filter.type",       kX,       kY,       152, 92);
                createParameterCard(parent, backend, "filter.drive",      kX + 158, kY,       152, 92);
                createParameterCard(parent, backend, "amp.portamento_ms", kX + 316, kY,       152, 92);
                createParameterCard(parent, backend, "env.amp.curve",     kX + 474, kY,       154, 92);

                createParameterCard(parent, backend, "lfo.1.phase_deg",   kX,       kY + 100, 152, 92);
                createParameterCard(parent, backend, "lfo.1.sync",        kX + 158, kY + 100, 152, 92);
                createParameterCard(parent, backend, "osc.1.wave",        kX + 316, kY + 100, 152, 92);
                createParameterCard(parent, backend, "osc.1.pitch_st",    kX + 474, kY + 100, 154, 92);

                createParameterCard(parent, backend, "osc.1.pan",         kX,       kY + 200, 152, 92);
                createParameterCard(parent, backend, "osc.1.unison",      kX + 158, kY + 200, 152, 92);
                createParameterCard(parent, backend, "osc.1.pulse_width", kX + 316, kY + 200, 152, 92);
                createParameterCard(parent, backend, "osc.1.detune_ct",   kX + 474, kY + 200, 154, 92);
                break;
        }

        refresh(backend);
        return true;
    }

    void onDestroy() override {
        confirm_dialog_.close(false);
    }

    void refresh(IUiBackend& backend) override {
        backend_ = &backend;
        const auto& patch = backend.model().activePatch();
        if (nav_ && patch.family != shown_family_) {
            nav_->requestNavigateTo(ScreenId::SoundEditor, mapSoundEditorModule(nav_->subTab(ScreenId::SoundEditor)));
            return;
        }
        if (banner_ && backend.hasSoundEditor()) {
            char text[96];
            std::snprintf(text, sizeof(text), "%03u %s (%s) | %s",
                          patch.id, backend.soundEditName(),
                          patch.family == SynthFamily::FM ? "FM" : "SUB",
                          backend.soundEditStatus());
            if (std::strcmp(lv_label_get_text(banner_), text) != 0) {
                lv_label_set_text(banner_, text);
            }
        }
        if (compare_btn_ && backend.hasSoundEditor()) {
            const bool cmp = std::strstr(backend.soundEditStatus(), "COMPARE") != nullptr;
            if (cmp) lv_obj_add_state(compare_btn_, LV_STATE_CHECKED);
            else lv_obj_remove_state(compare_btn_, LV_STATE_CHECKED);
        }
        if (s_active_preset_modal_) {
            lv_obj_send_event(s_active_preset_modal_, LV_EVENT_REFRESH, nullptr);
        }
    }

private:
    void openRevertConfirm() {
        if (!parent_ || !backend_) return;
        ConfirmDialogOptions opts{};
        opts.title = "CONFIRMAR REVERT";
        opts.body = "Descartar edicoes nao salvas do timbre atual?";
        opts.detail = "Os parametros serao restaurados para o estado original do patch.";
        opts.cancel_label = "CANCELAR";
        opts.confirm_label = "REVERTER";
        opts.border_color = palette::kWarning;
        opts.destructive = true;
        opts.user_data = backend_;
        opts.on_confirm = [](void* ud) {
            auto* b = static_cast<IUiBackend*>(ud);
            if (b) b->requestSoundAction(1);
        };
        confirm_dialog_.open(parent_, opts);
    }

    lv_obj_t* parent_ = nullptr;
    IUiBackend* backend_ = nullptr;
    UiNavigationManager* nav_ = nullptr;
    lv_obj_t* banner_ = nullptr;
    lv_obj_t* compare_btn_ = nullptr;
    SynthFamily shown_family_ = SynthFamily::Subtractive;
    ConfirmDialog confirm_dialog_{};
    static inline lv_obj_t* s_active_preset_modal_ = nullptr;
    static constexpr uint32_t kOverwriteConfirmTimeoutMs=10000;
    static bool presetPending(PresetOperationState state) {
        return state==PresetOperationState::Requested || state==PresetOperationState::WaitingForQuiet ||
               state==PresetOperationState::Writing || state==PresetOperationState::Verifying;
    }
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
        uint32_t overwrite_started_ms = 0;
        uint32_t overwrite_sequence = 0;
        uint16_t overwrite_origin = 0;
        uint32_t last_seen_revision = 0;
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
        const auto selected=lv_dropdown_get_selected(slot);
        lv_dropdown_set_options(slot,options);
        lv_dropdown_set_selected(slot,selected);
        lv_free(options);
    }
public:
    static void showPresets(lv_obj_t* button,IUiBackend& backend){
        lv_obj_t* screen = button ? lv_obj_get_screen(button) : lv_screen_active();
        if(!screen) screen = lv_screen_active();
        if(!screen) return;
        if(s_active_preset_modal_){
            lv_obj_move_foreground(s_active_preset_modal_);
            return;
        }
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
        auto* context=attachLvContext<PresetPanel>(root,PresetPanel{&backend,root,slot,name,info,save,recall,close},nullptr);
        if(!context){lv_obj_delete(root);return;}
        s_active_preset_modal_ = root;
        lv_obj_add_event_cb(root, [](lv_event_t* e){
            if(lv_event_get_target(e)==s_active_preset_modal_) s_active_preset_modal_=nullptr;
        }, LV_EVENT_DELETE, nullptr);
        lv_obj_add_event_cb(root, [](lv_event_t* e){
            auto* c = static_cast<PresetPanel*>(lv_event_get_user_data(e));
            if(!c || !c->backend) return;
            const auto op = c->backend->presetOperationStatus();
            const bool pending=presetPending(op.state);
            if(c->overwrite && (pending || op.revision!=c->last_seen_revision ||
               lv_tick_elaps(c->overwrite_started_ms)>=kOverwriteConfirmTimeoutMs)){
                c->overwrite=false;
                lv_label_set_text(c->info,"OVERWRITE CONFIRMATION EXPIRED; PRESS SAVE AS");
            }
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
                if(op.state != c->last_seen_op_state || op.revision!=c->last_seen_revision){
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
            c->last_seen_revision = op.revision;
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
            const auto op=c->backend->presetOperationStatus();
            if(presetPending(op.state)) return;
            char cur_name[25]{};uint16_t origin=0;uint32_t sequence=0;
            const bool occupied=c->backend->userPresetIdentity(slot,cur_name,sizeof(cur_name),origin,sequence);
            const bool matches=c->overwrite && c->overwrite_slot==slot &&
                op.revision==c->last_seen_revision &&
                lv_tick_elaps(c->overwrite_started_ms)<kOverwriteConfirmTimeoutMs &&
                c->overwrite_sequence==sequence && c->overwrite_origin==origin &&
                std::strcmp(c->overwrite_cur_name,cur_name)==0 &&
                std::strcmp(c->overwrite_new_name,new_name)==0;
            if(c->overwrite && !occupied){
                c->overwrite=false;
                lv_label_set_text(c->info,"SLOT CHANGED; PRESS SAVE AS AGAIN");
                return;
            }
            if(occupied && !matches){
                c->overwrite=true;
                c->overwrite_slot=slot;
                c->overwrite_started_ms=lv_tick_get();
                c->overwrite_sequence=sequence;
                c->overwrite_origin=origin;
                c->last_seen_revision=op.revision;
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
            if(presetPending(c->backend->presetOperationStatus().state)) return;
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
