#include "fake_nvs.h"
#include "user_preset_fx_nvs.h"
#include "ui_blueprint_performance_adapter.h"
#include "local_performance_backend.h"
#include "user_presets_v2.h"
#include "ui_demo_backend.h"
#include "ui_widgets.h"
#include "../ui-preview/preview_harness.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <vector>
using namespace smk::p4;
using namespace smk::p4::ui;
void testSmk25LegacyUi(amy::Runtime&, LocalAmyPerformanceBackend&, UiBlueprintPerformanceAdapter&, UiBlueprint&);

struct MemoryStore:edit::VersionedPresetStore {
    uint8_t bytes[16][2][160]{};bool present[16][2]{};bool fail=false;
    uint8_t v3[16][2][edit::kV3RecordBytes]{};bool has_v3[16][2]{};
    uint8_t v4[16][2][edit::kV4MaxBytes]{};
    unsigned v4_size[16][2]{};
    bool read(unsigned,unsigned,uint8_t*,unsigned&) override {return false;}
    bool write(unsigned,unsigned,const uint8_t*,unsigned) override {return false;}
    edit::BankRead readVersioned(unsigned version,unsigned s,unsigned b,uint8_t* p,unsigned& n) override {
        if(version==4){
            const unsigned length=v4_size[s][b];
            if(!length)return edit::BankRead::Missing;
            if(n<length)return edit::BankRead::Error;
            std::memcpy(p,v4[s][b],length);n=length;return edit::BankRead::Present;
        }
        if(version==3){
            if(!has_v3[s][b])return edit::BankRead::Missing;
            if(n<sizeof(v3[s][b]))return edit::BankRead::Error;
            n=sizeof(v3[s][b]);std::memcpy(p,v3[s][b],n);return edit::BankRead::Present;
        }
        if(version==1 || !present[s][b])return edit::BankRead::Missing;
        if(n<160)return edit::BankRead::Error;
        std::memcpy(p,bytes[s][b],160);n=160;return edit::BankRead::Present;
    }
    bool writeV2(unsigned s,unsigned b,const uint8_t* p,unsigned n) override {if(fail)return false;assert(n==160);std::memcpy(bytes[s][b],p,n);present[s][b]=true;return true;}
    bool writeV4(unsigned s,unsigned b,const uint8_t* p,unsigned n) override {
        if(fail || n>sizeof(v4[s][b]))return false;
        std::memcpy(v4[s][b],p,n);v4_size[s][b]=n;return true;
    }
    bool writeV3(unsigned s,unsigned b,const uint8_t* p,unsigned n) override {
        if(fail || n!=sizeof(v3[s][b]))return false;
        std::memcpy(v3[s][b],p,n);has_v3[s][b]=true;return true;
    }
};
struct ProfileStore:performance::ControllerProfileStore {
    uint8_t bytes[performance::kProfileBytes]{};
    bool load(performance::ControllerProfile& profile) override {
        return performance::decodeProfile(bytes,sizeof(bytes),profile);
    }
    bool save(const performance::ControllerProfile& profile) override {
        return performance::encodeProfile(profile,bytes);
    }
};
static lv_obj_t* button(lv_obj_t* root,const char* text){
    for(unsigned i=0;i<lv_obj_get_child_count(root);++i){auto* child=lv_obj_get_child(root,i);
        if(lv_obj_check_type(child,&lv_label_class) && std::strcmp(lv_label_get_text(child),text)==0)return lv_obj_get_parent(child);
        if(auto* found=button(child,text))return found;
    }return nullptr;
}
static lv_obj_t* widgetAt(lv_obj_t* root, const lv_obj_class_t* type, int x, int y) {
    if (lv_obj_check_type(root, type) && lv_obj_get_x(root) == x && lv_obj_get_y(root) == y) return root;
    for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i) {
        if (auto* match = widgetAt(lv_obj_get_child(root, i), type, x, y)) return match;
    }
    return nullptr;
}
static void select(lv_obj_t* dropdown, unsigned index) {
    assert(dropdown);
    lv_dropdown_set_selected(dropdown, index);
    lv_obj_send_event(dropdown, LV_EVENT_VALUE_CHANGED, nullptr);
}
static unsigned labels(lv_obj_t* root,const char* text){
    unsigned n=lv_obj_check_type(root,&lv_label_class) && std::strcmp(lv_label_get_text(root),text)==0?1:0;
    for(unsigned i=0;i<lv_obj_get_child_count(root);++i)n+=labels(lv_obj_get_child(root,i),text);
    return n;
}
static unsigned countObjects(lv_obj_t* root) {
    if (!root) return 0;
    unsigned n = 1;
    for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i) {
        n += countObjects(lv_obj_get_child(root, i));
    }
    return n;
}
static lv_obj_t* firstSlider(lv_obj_t* root) {
    if (!root) return nullptr;
    if (lv_obj_check_type(root, &lv_slider_class)) return root;
    for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i) {
        if (auto* s = firstSlider(lv_obj_get_child(root, i))) return s;
    }
    return nullptr;
}
template <typename ApplyFn>
static void auditInventory(UiBlueprintPerformanceAdapter& a, ApplyFn&& apply) {
    size_t count = 0;
    const ParameterDescriptor* all = ParameterRegistry::all(count);
    assert(count >= 76);
    for (size_t i = 0; i < count; ++i) {
        const auto& d = all[i];
        const FeatureStatus st = a.parameterStatus(d.id);
        assert(st != FeatureStatus::Demo); // Real adapter never reports Demo status
        if (st == FeatureStatus::Available) {
            if (std::strcmp(d.id,"fx.delay.division")==0 && a.model().getInt("fx.delay.mode")==0) {
                assert(!a.isParameterEditable(d.id));
                continue;
            }
            if (std::strcmp(d.id,"fx.delay.time_ms")==0 && a.model().getInt("fx.delay.mode")==1) {
                assert(!a.isParameterEditable(d.id));
                continue;
            }
            if (std::strcmp(d.id,"fx.chorus.depth")==0 && a.model().getInt("fx.chorus.mode")==0) {
                assert(!a.isParameterEditable(d.id));
                continue;
            }
            if (d.capabilities & kCapReadOnly) {
                assert(!a.isParameterEditable(d.id));
                continue;
            }
            assert(a.isParameterEditable(d.id));
            ParameterValue pv{};
            assert(a.model().getValue(d.id, pv));
            const UiCommandResult res = a.requestParameterValue(d.id, pv);
            assert(res.state == UiCommandState::Requested || res.state == UiCommandState::Confirmed);
            apply();
            assert(a.lastCommandState() == UiCommandState::Confirmed);
        } else {
            assert(!a.isParameterEditable(d.id));
            ParameterValue pv{};
            assert(a.model().getValue(d.id, pv));
            const UiCommandResult res = a.requestParameterValue(d.id, pv);
            assert(!res.accepted());
            if (st == FeatureStatus::Planned) {
                assert(res.state == UiCommandState::Unsupported || res.state == UiCommandState::Rejected);
            }
        }
    }
}
static amy::Runtime r;

namespace {

static const char* sliderCardStateText(lv_obj_t* card) {
    if (!card) return "";
    lv_obj_update_layout(card);
    const int32_t half_w = lv_obj_get_width(card) / 2;
    const uint32_t cnt = lv_obj_get_child_count(card);
    for (uint32_t i = 0; i < cnt; ++i) {
        lv_obj_t* ch = lv_obj_get_child(card, static_cast<int32_t>(i));
        if (lv_obj_check_type(ch, &lv_label_class) && lv_obj_get_x(ch) >= half_w) {
            const char* txt = lv_label_get_text(ch);
            return txt ? txt : "";
        }
    }
    return "";
}

static const char* sliderCardValueText(lv_obj_t* card) {
    if (!card) return "";
    lv_obj_update_layout(card);
    const int32_t half_w = lv_obj_get_width(card) / 2;
    const uint32_t cnt = lv_obj_get_child_count(card);
    for (uint32_t i = 0; i < cnt; ++i) {
        lv_obj_t* ch = lv_obj_get_child(card, static_cast<int32_t>(i));
        if (lv_obj_check_type(ch, &lv_label_class) && lv_obj_get_x(ch) < half_w && lv_obj_get_y(ch) >= 18) {
            const char* txt = lv_label_get_text(ch);
            return txt ? txt : "";
        }
    }
    return "";
}

class ProbeSliderBackend final : public IUiBackend {
public:
    UiModel& model() override { return inner_.model(); }
    const UiModel& model() const override { return inner_.model(); }
    bool isDemoMode() const override { return false; }
    bool isConnected() const override { return connected_; }
    bool isBusy() const override { return false; }
    const char* backendName() const override { return "ProbeSliderBackend"; }
    const char* modeBadgeText() const override { return "PROBE"; }
    const char* statusText() const override { return inner_.statusText(); }
    FeatureStatus parameterStatus(const char* id) const override { return inner_.parameterStatus(id); }
    bool isParameterEditable(const char* id) const override { return inner_.isParameterEditable(id); }
    bool isPatchSelectable(uint16_t patch_id) const override { return inner_.isPatchSelectable(patch_id); }
    UiCommandState lastCommandState() const override {
        return fake_cmd_id_ ? fake_cmd_state_ : inner_.lastCommandState();
    }
    const char* lastCommandParameterId() const override {
        return fake_cmd_id_ ? fake_cmd_id_ : inner_.lastCommandParameterId();
    }
    uint32_t lastCommandToken() const override {
        return fake_cmd_id_ ? fake_cmd_token_ : inner_.lastCommandToken();
    }
    UiCommandResult requestParameterValue(const char* id, const ParameterValue& val) override {
        if (val.type == ParameterValueType::Float) return requestParameterFloat(id, val.asFloat());
        if (val.type == ParameterValueType::Int) return requestParameterInt(id, val.asInt());
        if (val.type == ParameterValueType::Bool) return requestParameterBool(id, val.asBool());
        return inner_.requestParameterValue(id, val);
    }
    UiCommandResult requestParameterFloat(const char* id, float val) override {
        ++float_req_count_;
        last_sent_id_ = id;
        last_sent_val_ = val;
        if (!connected_ || reject_immediate_) {
            fake_cmd_state_ = UiCommandState::Rejected;
            fake_cmd_id_ = id;
            fake_cmd_token_ = 0;
            return {UiCommandState::Rejected, 0, "REJECTED"};
        }
        if (defer_mode_) {
            fake_cmd_state_ = UiCommandState::Requested;
            fake_cmd_id_ = id;
            fake_cmd_token_ = next_tok_++;
            return {UiCommandState::Requested, fake_cmd_token_, "WAIT"};
        }
        fake_cmd_state_ = UiCommandState::Confirmed;
        fake_cmd_id_ = id;
        fake_cmd_token_ = next_tok_++;
        inner_.model().setFloat(id, val);
        return {UiCommandState::Confirmed, fake_cmd_token_, "OK"};
    }
    UiCommandResult requestParameterInt(const char* id, int32_t val) override {
        fake_cmd_id_ = nullptr;
        return inner_.requestParameterInt(id, val);
    }
    UiCommandResult requestParameterBool(const char* id, bool val) override {
        fake_cmd_id_ = nullptr;
        return inner_.requestParameterBool(id, val);
    }
    UiCommandResult requestSelectPatch(uint16_t patch_id, uint8_t target_layer = 0) override {
        return inner_.requestSelectPatch(patch_id, target_layer);
    }
    UiCommandResult requestNoteOn(uint8_t note, uint8_t velocity) override {
        return inner_.requestNoteOn(note, velocity);
    }
    UiCommandResult requestNoteOff(uint8_t note, uint32_t token = 0) override {
        return inner_.requestNoteOff(note, token);
    }
    void releaseAllUiNotes() override { inner_.releaseAllUiNotes(); }
    UiCommandResult requestPanic() override { return inner_.requestPanic(); }
    void service(uint32_t now_ms) override { inner_.service(now_ms); }
    const UiSignalMeters& meters() const override { return inner_.meters(); }
    uint8_t activeSequencerStep() const override { return inner_.activeSequencerStep(); }
    bool isNoteHeld(uint8_t note) const override { return inner_.isNoteHeld(note); }

    void confirmDefer() {
        if (!last_sent_id_) return;
        fake_cmd_state_ = UiCommandState::Confirmed;
        fake_cmd_id_ = last_sent_id_;
        inner_.model().setFloat(last_sent_id_, last_sent_val_);
    }
    void rejectDefer() {
        fake_cmd_state_ = UiCommandState::Rejected;
        fake_cmd_id_ = last_sent_id_;
    }
    void pushExternalFloat(const char* id, float val) {
        inner_.model().setFloat(id, val);
    }

    UiDemoBackend inner_{};
    bool connected_ = true;
    bool reject_immediate_ = false;
    bool defer_mode_ = false;
    UiCommandState fake_cmd_state_ = UiCommandState::Confirmed;
    const char* fake_cmd_id_ = nullptr;
    const char* last_sent_id_ = nullptr;
    float last_sent_val_ = 0.0f;
    uint32_t fake_cmd_token_ = 0;
    uint32_t next_tok_ = 10;
    uint32_t float_req_count_ = 0;
};

static void runSliderRobustnessUnitTests() {
    ProbeSliderBackend probe;
    lv_obj_t* host = lv_obj_create(lv_screen_active());
    assert(host != nullptr);
    lv_obj_set_size(host, 640, 380);

    // Build cards for linear float, negative/discrete float, and log-scale float
    assert(probe.requestParameterFloat("fx.delay.mix", 20.0f).accepted());
    lv_obj_t* mix_card = createParameterSlider(host, probe, "fx.delay.mix", 0, 0, 220, 108,
                                               PresentationLevel::StageStacked);
    lv_obj_t* mix_s = firstSlider(mix_card);
    assert(mix_card && mix_s);

    // 3.1 Slow drag (>35 ms steps) + fast burst (<35 ms) + min/max + duplicate-commit prevention
    probe.float_req_count_ = 0;
    preview::g_sim_tick_ms += 50;
    lv_obj_add_state(mix_s, LV_STATE_PRESSED);
    lv_obj_send_event(mix_s, LV_EVENT_PRESSED, nullptr);
    lv_slider_set_value(mix_s, 30, LV_ANIM_OFF);
    lv_obj_send_event(mix_s, LV_EVENT_VALUE_CHANGED, nullptr);
    assert(probe.float_req_count_ == 1);
    preview::g_sim_tick_ms += 40; // >35ms slow drag step
    lv_slider_set_value(mix_s, 45, LV_ANIM_OFF);
    lv_obj_send_event(mix_s, LV_EVENT_VALUE_CHANGED, nullptr);
    assert(probe.float_req_count_ == 2);
    for (int i = 0; i < 6; ++i) {
        preview::g_sim_tick_ms += 4; // <35ms burst coalesced cleanly
        lv_slider_set_value(mix_s, 50 + i * 2, LV_ANIM_OFF);
        lv_obj_send_event(mix_s, LV_EVENT_VALUE_CHANGED, nullptr);
    }
    assert(probe.float_req_count_ == 2); // No extra flood during burst
    assert(std::strcmp(sliderCardStateText(mix_card), "EDIT") == 0);
    // Release on DIFFERENT value (60 vs last sent 45) -> commits once
    lv_obj_remove_state(mix_s, LV_STATE_PRESSED);
    lv_obj_send_event(mix_s, LV_EVENT_RELEASED, nullptr);
    assert(probe.float_req_count_ == 3 && std::fabs(probe.last_sent_val_ - 60.0f) < 0.1f);
    assert(std::strcmp(sliderCardStateText(mix_card), "") == 0);

    // Slow drag then release on the SAME value already sent -> 0 duplicate commit
    preview::g_sim_tick_ms += 50;
    lv_obj_add_state(mix_s, LV_STATE_PRESSED);
    lv_obj_send_event(mix_s, LV_EVENT_PRESSED, nullptr);
    lv_slider_set_value(mix_s, 72, LV_ANIM_OFF);
    lv_obj_send_event(mix_s, LV_EVENT_VALUE_CHANGED, nullptr);
    assert(probe.float_req_count_ == 4);
    preview::g_sim_tick_ms += 10;
    lv_obj_remove_state(mix_s, LV_STATE_PRESSED);
    lv_obj_send_event(mix_s, LV_EVENT_RELEASED, nullptr);
    assert(probe.float_req_count_ == 4 && std::fabs(probe.last_sent_val_ - 72.0f) < 0.1f);

    // Min (0) & Max (100) bounds + negative range / discrete step (eq.low_db & master_tone) + Log scale (cutoff)
    lv_obj_t* eq_card = createParameterSlider(host, probe, "fx.eq.low_db", 230, 0, 200, 108,
                                              PresentationLevel::StageStacked);
    lv_obj_t* eq_s = firstSlider(eq_card);
    lv_obj_t* tone_card = createParameterSlider(host, probe, "fx.master_tone", 0, 120, 200, 108,
                                                PresentationLevel::StageStacked);
    lv_obj_t* tone_s = firstSlider(tone_card);
    assert(probe.requestSelectPatch(4).accepted());
    assert(probe.requestParameterFloat("filter.cutoff_hz", 1000.0f).accepted());
    lv_obj_t* cut_card = createParameterSlider(host, probe, "filter.cutoff_hz", 230, 120, 200, 108,
                                               PresentationLevel::StageStacked);
    lv_obj_update_layout(host);
    lv_obj_t* cut_s = firstSlider(cut_card);
    assert(eq_s && tone_s && cut_s);

    auto dragTo = [](lv_obj_t* s, int32_t pct, bool release = true) {
        preview::g_sim_tick_ms += 50;
        lv_obj_add_state(s, LV_STATE_PRESSED);
        lv_obj_send_event(s, LV_EVENT_PRESSED, nullptr);
        lv_slider_set_value(s, pct, LV_ANIM_OFF);
        lv_obj_send_event(s, LV_EVENT_VALUE_CHANGED, nullptr);
        if (release) {
            lv_obj_remove_state(s, LV_STATE_PRESSED);
            lv_obj_send_event(s, LV_EVENT_RELEASED, nullptr);
        }
    };
    dragTo(mix_s, 0);
    assert(std::fabs(probe.model().getFloat("fx.delay.mix") - 0.0f) < 0.05f);
    dragTo(mix_s, 100);
    assert(std::fabs(probe.model().getFloat("fx.delay.mix") - 100.0f) < 0.05f);
    dragTo(eq_s, 0);
    assert(std::fabs(probe.model().getFloat("fx.eq.low_db") - (-12.0f)) < 0.1f);
    dragTo(eq_s, 50);
    assert(std::fabs(probe.model().getFloat("fx.eq.low_db") - 0.0f) < 0.1f);
    dragTo(eq_s, 100);
    assert(std::fabs(probe.model().getFloat("fx.eq.low_db") - 12.0f) < 0.1f);
    dragTo(tone_s, 53); // -100 + 2*53 = +6% (step 2.0)
    assert(std::fabs(probe.model().getFloat("fx.master_tone") - 6.0f) < 0.1f);
    dragTo(cut_s, 0);
    assert(std::fabs(probe.model().getFloat("filter.cutoff_hz") - 20.0f) < 0.5f);
    dragTo(cut_s, 50);
    const float mid_cut = probe.model().getFloat("filter.cutoff_hz");
    assert(std::fabs(mid_cut - 600.0f) < 5.0f); // Geometric mean sqrt(20*18000) = 600 Hz
    dragTo(cut_s, 100);
    assert(std::fabs(probe.model().getFloat("filter.cutoff_hz") - 18000.0f) < 5.0f);

    // 3.2 Cancel, immediate reject, Requested->Rejected, timeout, mid-drag teardown
    dragTo(mix_s, 40);
    assert(std::fabs(probe.model().getFloat("fx.delay.mix") - 40.0f) < 0.1f);

    // Immediate rejection reverts thumb and shows REJ
    probe.reject_immediate_ = true;
    const uint32_t req_before_rej = probe.float_req_count_;
    dragTo(mix_s, 85);
    assert(probe.float_req_count_ > req_before_rej);
    assert(lv_slider_get_value(mix_s) == 40);
    assert(std::strcmp(sliderCardStateText(mix_card), "REJ") == 0);
    probe.reject_immediate_ = false;

    // Requested -> WAIT -> asynchronous Rejected -> REJ & rollback
    probe.defer_mode_ = true;
    dragTo(mix_s, 75);
    assert(std::strcmp(sliderCardStateText(mix_card), "WAIT") == 0);
    assert(std::fabs(probe.model().getFloat("fx.delay.mix") - 40.0f) < 0.1f);
    probe.rejectDefer();
    lv_obj_send_event(mix_s, LV_EVENT_REFRESH, nullptr);
    assert(std::strcmp(sliderCardStateText(mix_card), "REJ") == 0);
    assert(lv_slider_get_value(mix_s) == 40);

    // Requested -> WAIT -> fallback timeout (>kFallbackPendingTimeoutMs 2500ms without ACK) -> REJ & rollback
    dragTo(mix_s, 82);
    assert(std::strcmp(sliderCardStateText(mix_card), "WAIT") == 0);
    preview::g_sim_tick_ms += 2050;
    lv_obj_send_event(mix_s, LV_EVENT_REFRESH, nullptr);
    assert(std::strcmp(sliderCardStateText(mix_card), "WAIT") == 0);
    preview::g_sim_tick_ms += 550;
    lv_obj_send_event(mix_s, LV_EVENT_REFRESH, nullptr);
    assert(std::strcmp(sliderCardStateText(mix_card), "REJ") == 0);
    assert(lv_slider_get_value(mix_s) == 40);

    // Requested -> WAIT -> asynchronous Confirmed -> clears WAIT
    dragTo(mix_s, 65);
    assert(std::strcmp(sliderCardStateText(mix_card), "WAIT") == 0);
    probe.confirmDefer();
    lv_obj_send_event(mix_s, LV_EVENT_REFRESH, nullptr);
    assert(std::strcmp(sliderCardStateText(mix_card), "") == 0);
    assert(lv_slider_get_value(mix_s) == 65);
    probe.defer_mode_ = false;

    // LV_EVENT_PRESS_LOST, LV_EVENT_CANCEL, LV_EVENT_INDEV_RESET (with LVGL trailing VALUE_CHANGED sequence)
    for (lv_event_code_t cancel_ev : {LV_EVENT_PRESS_LOST, LV_EVENT_CANCEL, LV_EVENT_INDEV_RESET}) {
        const uint32_t sent_before = probe.float_req_count_;
        preview::g_sim_tick_ms += 50;
        lv_obj_add_state(mix_s, LV_STATE_PRESSED);
        lv_obj_send_event(mix_s, LV_EVENT_PRESSED, nullptr);
        lv_slider_set_value(mix_s, 90, LV_ANIM_OFF);
        lv_obj_remove_state(mix_s, LV_STATE_PRESSED); // Matches LVGL base event clearing PRESSED before slider teardown
        lv_obj_send_event(mix_s, LV_EVENT_VALUE_CHANGED, nullptr); // Trailing teardown change must NOT emit!
        lv_obj_send_event(mix_s, cancel_ev, nullptr);
        assert(probe.float_req_count_ == sent_before);
        assert(lv_slider_get_value(mix_s) == 65);
    }

    // 3.3 External changes during drag, after release, and mid-drag Chorus OFF abort
    dragTo(mix_s, 80, false); // Hold drag without release at 80
    probe.pushExternalFloat("fx.delay.mix", 25.0f);
    lv_obj_send_event(mix_s, LV_EVENT_REFRESH, nullptr);
    assert(lv_slider_get_value(mix_s) == 80); // Must NOT jump under user finger while dragging!
    // Cancel after external change -> snaps cleanly to new external 25%
    lv_obj_remove_state(mix_s, LV_STATE_PRESSED);
    lv_obj_send_event(mix_s, LV_EVENT_PRESS_LOST, nullptr);
    assert(lv_slider_get_value(mix_s) == 25);
    // External change after release updates thumb immediately
    probe.pushExternalFloat("fx.delay.mix", 55.0f);
    lv_obj_send_event(mix_s, LV_EVENT_REFRESH, nullptr);
    assert(lv_slider_get_value(mix_s) == 55);

    // Mid-drag Chorus OFF external transition cleanly aborts drag, restores confirmed 42%, and disables Depth slider
    assert(probe.requestParameterInt("fx.chorus.mode", 2).accepted());
    assert(probe.requestParameterFloat("fx.chorus.depth", 42.0f).accepted());
    probe.defer_mode_ = true;
    lv_obj_t* cho_card = createParameterSlider(host, probe, "fx.chorus.depth", 0, 240, 220, 108,
                                               PresentationLevel::StageStacked);
    lv_obj_t* cho_s = firstSlider(cho_card);
    assert(cho_card && cho_s && !lv_obj_has_state(cho_s, LV_STATE_DISABLED));
    dragTo(cho_s, 75, false);
    assert(std::fabs(probe.model().getFloat("fx.chorus.depth") - 42.0f) < 0.1f);
    assert(probe.requestParameterInt("fx.chorus.mode", 0).accepted());
    lv_obj_send_event(cho_s, LV_EVENT_REFRESH, nullptr);
    assert(lv_obj_has_state(cho_s, LV_STATE_DISABLED));
    assert(std::strcmp(sliderCardStateText(cho_card), "OFF") == 0);
    assert(lv_slider_get_value(cho_s) == 42);
    probe.defer_mode_ = false;

    // Destroy widgets mid-drag: must not crash or corrupt LVGL heap
    dragTo(mix_s, 88, false);
    lv_obj_clean(host);
    lv_obj_delete(host);
    lv_timer_handler();
    assert(lv_mem_test() == LV_RESULT_OK);
    std::puts("PASS slider 0.18-B.1: coalescing, duplicates, min/max/neg/log, reject/timeout/cancel/mid-drag destroy & external updates");
}

static void runFxHardening018b1Suite(amy::Runtime& runtime, LocalAmyPerformanceBackend& backend,
                                    UiBlueprintPerformanceAdapter& adapter, UiBlueprint& ui) {
    auto apply = [&] {
        adapter.service(preview::g_sim_tick_ms += 25);
        runtime.apply(preview::g_sim_tick_ms);
        runtime.loadPendingPatch();
        assert(runtime.render());
        backend.publish(runtime.snapshot(), false, 0, 0);
        adapter.service(preview::g_sim_tick_ms += 25);
        preview::pumpFrame(ui);
    };

    // Normalize clean musical state for deterministic UI testing & screenshots
    assert(adapter.requestParameterInt("perf.bpm", 120).accepted());
    assert(adapter.requestParameterBool("fx.bypass_all", false).accepted());
    assert(adapter.requestParameterInt("fx.delay.mode", 0).accepted());
    assert(adapter.requestParameterFloat("fx.delay.time_ms", 350.0f).accepted());
    assert(adapter.requestParameterFloat("fx.delay.feedback", 40.0f).accepted());
    assert(adapter.requestParameterFloat("fx.delay.mix", 35.0f).accepted());
    assert(adapter.requestParameterFloat("fx.reverb.size", 70.0f).accepted());
    assert(adapter.requestParameterFloat("fx.reverb.mix", 25.0f).accepted());
    assert(adapter.requestParameterInt("fx.chorus.mode", 2).accepted());
    assert(adapter.requestParameterFloat("fx.chorus.depth", 68.0f).accepted());
    apply();
    // Switch Chorus back OFF preserving 68% depth
    assert(adapter.requestParameterInt("fx.chorus.mode", 0).accepted());
    apply();
    assert(!adapter.isParameterEditable("fx.chorus.depth"));
    assert(std::fabs(adapter.model().getFloat("fx.chorus.depth") - 68.0f) < 0.1f);

    // 1. Navigate Effects -> Delay / Reverb / Chorus OFF / Chorus ON / Master + 8 PPM screenshots
    assert(ui.navigation().navigateTo(ScreenId::Effects, 0));
    preview::pumpFrame(ui);
    assert(preview::saveFramebufferPpm("build/editor011/fx_01_delay.ppm"));

    lv_obj_send_event(button(ui.contentObject(), "REVERB"), LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(ui);
    assert(preview::saveFramebufferPpm("build/editor011/fx_02_reverb.ppm"));

    lv_obj_send_event(button(ui.contentObject(), "CHORUS"), LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(ui);
    assert(preview::saveFramebufferPpm("build/editor011/fx_03_chorus_off.ppm"));

    // Chorus OFF -> Depth must be visually disabled, ignore slider drag and Inspector +/- fine adjust
    {
        lv_obj_t* depth_card = button(ui.contentObject(), "CHORUS DEPTH");
        lv_obj_t* depth_s = firstSlider(depth_card);
        assert(depth_card && depth_s && lv_obj_has_state(depth_s, LV_STATE_DISABLED));
        assert(std::strcmp(sliderCardStateText(depth_card), "OFF") == 0);
        const uint32_t tok0 = backend.fxToken();
        lv_slider_set_value(depth_s, 15, LV_ANIM_OFF);
        lv_obj_send_event(depth_s, LV_EVENT_VALUE_CHANGED, nullptr);
        lv_obj_send_event(depth_s, LV_EVENT_RELEASED, nullptr);
        assert(backend.fxToken() == tok0);

        lv_obj_t* insp_bar = lv_obj_get_parent(button(ui.contentObject(), "AJUSTE FINO"));
        lv_obj_send_event(button(ui.contentObject(), "AJUSTE FINO"), LV_EVENT_CLICKED, nullptr);
        preview::pumpFrame(ui);
        lv_obj_send_event(button(ui.contentObject(), "PARAM >"), LV_EVENT_CLICKED, nullptr); // focus -> fx.chorus.depth
        preview::pumpFrame(ui);
        lv_obj_t* insp_inc = button(insp_bar, "+ FINO");
        lv_obj_t* insp_dec = button(insp_bar, "- FINO");
        assert(insp_inc && insp_dec);
        assert(lv_obj_has_state(insp_inc, LV_STATE_DISABLED));
        assert(lv_obj_has_state(insp_dec, LV_STATE_DISABLED));
        lv_obj_send_event(insp_inc, LV_EVENT_CLICKED, nullptr);
        assert(backend.fxToken() == tok0);
        lv_obj_send_event(button(ui.contentObject(), "FECHAR"), LV_EVENT_CLICKED, nullptr);
        preview::pumpFrame(ui);
    }

    // Pending OFF -> ON must keep Depth locked until owner confirms
    {
        lv_obj_t* mode_card = button(ui.contentObject(), "CHORUS MODE");
        lv_obj_t* plus_btn = button(mode_card, "+");
        assert(mode_card && plus_btn);
        lv_obj_send_event(plus_btn, LV_EVENT_CLICKED, nullptr);
        preview::pumpFrame(ui); // NOT applied yet!
        assert(adapter.lastCommandState() == UiCommandState::Requested);
        assert(!adapter.isParameterEditable("fx.chorus.depth"));
        assert(lv_obj_has_state(firstSlider(button(ui.contentObject(), "CHORUS DEPTH")), LV_STATE_DISABLED));
        apply(); // Owner confirms mode=1 (Classic)
        assert(!lv_obj_has_state(firstSlider(button(ui.contentObject(), "CHORUS DEPTH")), LV_STATE_DISABLED));
        assert(std::fabs(adapter.model().getFloat("fx.chorus.depth") - 68.0f) < 0.1f);
        // Switch to JUNO I (mode=2) for clean screenshot & Inspector + fine adjust -> 69%
        lv_obj_send_event(button(mode_card, "+"), LV_EVENT_CLICKED, nullptr);
        apply();
        assert(adapter.model().getInt("fx.chorus.mode") == 2);
        lv_obj_send_event(button(ui.contentObject(), "AJUSTE FINO"), LV_EVENT_CLICKED, nullptr);
        preview::pumpFrame(ui);
        lv_obj_t* insp_bar = lv_obj_get_parent(button(ui.contentObject(), "FECHAR"));
        assert(!lv_obj_has_state(button(insp_bar, "+ FINO"), LV_STATE_DISABLED));
        lv_obj_send_event(button(insp_bar, "+ FINO"), LV_EVENT_CLICKED, nullptr);
        apply();
        assert(std::fabs(adapter.model().getFloat("fx.chorus.depth") - 69.0f) < 0.1f);
        lv_obj_send_event(button(ui.contentObject(), "FECHAR"), LV_EVENT_CLICKED, nullptr);
        preview::pumpFrame(ui);
        assert(preview::saveFramebufferPpm("build/editor011/fx_04_chorus_on.ppm"));

        // External OFF transition re-locks Depth while preserving 69%
        assert(adapter.requestParameterInt("fx.chorus.mode", 0).accepted());
        apply();
        lv_obj_t* depth_card = button(ui.contentObject(), "CHORUS DEPTH");
        assert(lv_obj_has_state(firstSlider(depth_card), LV_STATE_DISABLED));
        assert(std::strcmp(sliderCardValueText(depth_card), "69 %") == 0);
    }

    lv_obj_send_event(button(ui.contentObject(), "MASTER"), LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(ui);
    assert(preview::saveFramebufferPpm("build/editor011/fx_05_master.ppm"));

    // Expanded Inspector screenshot on Delay
    lv_obj_send_event(button(ui.contentObject(), "DELAY"), LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(ui);
    lv_obj_send_event(button(ui.contentObject(), "AJUSTE FINO"), LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(ui);
    assert(button(ui.contentObject(), "FECHAR") && button(ui.contentObject(), "PARAM >"));
    assert(preview::saveFramebufferPpm("build/editor011/fx_06_inspector_expanded.ppm"));
    lv_obj_send_event(button(ui.contentObject(), "FECHAR"), LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(ui);

    // 2. Pending WAIT screenshot + module switch mid-pending + Rejection timeout screenshot
    {
        lv_obj_t* mix_card = button(ui.contentObject(), "DELAY MIX");
        lv_obj_t* mix_s = firstSlider(mix_card);
        assert(mix_card && mix_s);
        preview::g_sim_tick_ms += 50;
        lv_obj_add_state(mix_s, LV_STATE_PRESSED);
        lv_obj_send_event(mix_s, LV_EVENT_PRESSED, nullptr);
        lv_slider_set_value(mix_s, 55, LV_ANIM_OFF);
        lv_obj_send_event(mix_s, LV_EVENT_VALUE_CHANGED, nullptr);
        preview::g_sim_tick_ms += 5;
        lv_slider_set_value(mix_s, 80, LV_ANIM_OFF);
        lv_obj_send_event(mix_s, LV_EVENT_VALUE_CHANGED, nullptr);
        lv_obj_remove_state(mix_s, LV_STATE_PRESSED);
        lv_obj_send_event(mix_s, LV_EVENT_RELEASED, nullptr);
        preview::pumpFrame(ui); // Awaiting owner confirmation
        assert(std::strcmp(sliderCardStateText(mix_card), "WAIT") == 0);
        assert(preview::saveFramebufferPpm("build/editor011/fx_07_slider_pending.ppm"));

        // Switch module while command is pending, then return and confirm
        lv_obj_send_event(button(ui.contentObject(), "REVERB"), LV_EVENT_CLICKED, nullptr);
        preview::pumpFrame(ui);
        lv_obj_send_event(button(ui.contentObject(), "DELAY"), LV_EVENT_CLICKED, nullptr);
        apply();
        assert(std::fabs(runtime.snapshot().fx.config.delay_mix - 0.80f) < 0.02f);
        assert(std::strcmp(sliderCardStateText(button(ui.contentObject(), "DELAY MIX")), "") == 0);

        // Trigger timeout rejection on DELAY FDBK without owner apply to capture REJ rollback
        lv_obj_t* fdbk_card = button(ui.contentObject(), "DELAY FDBK");
        lv_obj_t* fdbk_s = firstSlider(fdbk_card);
        assert(fdbk_card && fdbk_s);
        const int32_t confirmed_pct = lv_slider_get_value(fdbk_s);
        preview::g_sim_tick_ms += 50;
        lv_obj_add_state(fdbk_s, LV_STATE_PRESSED);
        lv_obj_send_event(fdbk_s, LV_EVENT_PRESSED, nullptr);
        lv_slider_set_value(fdbk_s, 90, LV_ANIM_OFF);
        lv_obj_send_event(fdbk_s, LV_EVENT_VALUE_CHANGED, nullptr);
        lv_obj_remove_state(fdbk_s, LV_STATE_PRESSED);
        lv_obj_send_event(fdbk_s, LV_EVENT_RELEASED, nullptr);
        preview::g_sim_tick_ms += UiBlueprintPerformanceAdapter::kPatchConfirmationTimeoutMs + 50;
        adapter.service(preview::g_sim_tick_ms);
        preview::pumpFrame(ui);
        assert(adapter.lastCommandState() == UiCommandState::Rejected);
        assert(std::strcmp(sliderCardStateText(fdbk_card), "REJ") == 0);
        assert(lv_slider_get_value(fdbk_s) == confirmed_pct);
        assert(preview::saveFramebufferPpm("build/editor011/fx_08_slider_rejected.ppm"));

        // Drain queued command and restore clean Delay Mix/Feedback
        apply();
        assert(adapter.requestParameterFloat("fx.delay.feedback", 40.0f).accepted());
        assert(adapter.requestParameterFloat("fx.delay.mix", 35.0f).accepted());
        apply();
    }

    // 3. Musical context preservation across FX navigation & < TOCAR return
    // (Part A: Sequencer + USB sustained note via CC 64 + Performance Lock + Touch release safety;
    //  Part B: Active Arpeggiator + Sequencer preserved across FX navigation and < TOCAR)
    {
        // Part A: Sequencer + USB Sustain + Touch safety + Performance Lock
        seq::Request s_step{};
        s_step.action = seq::Action::SetStepEnabled;
        s_step.index = 0;
        s_step.value = 1;
        assert(adapter.requestSequencer(s_step).accepted());
        apply();
        seq::Request s_play{};
        s_play.action = seq::Action::Play;
        assert(adapter.requestSequencer(s_play).accepted());
        apply();
        assert(runtime.snapshot().sequencer.playing);

        setSessionLockMask(0x03);
        const uint8_t usb_on[]{0x09, 0x90, 60, 100};
        const uint8_t sus_on[]{0x0B, 0xB0, 64, 127};
        const uint8_t usb_off[]{0x08, 0x80, 60, 0};
        assert(runtime.usbInput().transfer(usb_on, sizeof(usb_on), preview::g_sim_tick_ms));
        assert(runtime.usbInput().transfer(sus_on, sizeof(sus_on), preview::g_sim_tick_ms));
        assert(runtime.usbInput().transfer(usb_off, sizeof(usb_off), preview::g_sim_tick_ms));
        apply();
        assert(runtime.snapshot().active_notes >= 1 && runtime.snapshot().sustain_active);

        assert(ui.navigation().navigateTo(ScreenId::Performance, 0));
        preview::pumpFrame(ui);
        const auto touch_perf = adapter.requestNoteOn(67, 100);
        assert(touch_perf.accepted());
        apply();
        assert(adapter.isNoteHeld(67));

        const uint16_t patch_before = runtime.snapshot().patch_id;
        assert(ui.navigation().navigateTo(ScreenId::Effects, 0));
        preview::pumpFrame(ui);
        apply();
        // Touch note from Performance safely released on screen change; USB sustained note + Seq + Lock stay alive!
        assert(!adapter.isNoteHeld(67));
        assert(runtime.snapshot().active_notes >= 1 && runtime.snapshot().sustain_active);
        assert(runtime.snapshot().sequencer.playing);
        assert(sessionLockMask() == 0x03);
        assert(runtime.snapshot().patch_id == patch_before);

        for (const char* mod : {"DELAY", "REVERB", "CHORUS", "MASTER", "CHORUS"}) {
            lv_obj_send_event(button(ui.contentObject(), mod), LV_EVENT_CLICKED, nullptr);
            preview::pumpFrame(ui);
            assert(runtime.snapshot().active_notes >= 1 && runtime.snapshot().sustain_active);
            assert(runtime.snapshot().sequencer.playing);
        }
        assert(ui.navigation().subTab(ScreenId::Effects) == 2); // CHORUS selected

        // Return via < TOCAR sidebar button: musical context preserved
        lv_obj_send_event(button(ui.contentObject(), "< TOCAR"), LV_EVENT_CLICKED, nullptr);
        preview::pumpFrame(ui);
        assert(ui.navigation().currentScreen() == ScreenId::Performance);
        assert(runtime.snapshot().active_notes >= 1 && runtime.snapshot().sustain_active);
        assert(runtime.snapshot().sequencer.playing);
        assert(sessionLockMask() == 0x03);
        assert(runtime.snapshot().patch_id == patch_before);

        // Returning to Effects restores last module (CHORUS)
        assert(ui.navigation().navigateTo(ScreenId::Effects));
        preview::pumpFrame(ui);
        assert(ui.navigation().subTab(ScreenId::Effects) == 2);
        assert(button(ui.contentObject(), "CHORUS DEPTH") != nullptr);

        // Release USB sustain, keep Sequencer playing, then enable & test Arpeggiator across FX -> < TOCAR
        const uint8_t sus_off[]{0x0B, 0xB0, 64, 0};
        assert(runtime.usbInput().transfer(sus_off, sizeof(sus_off), preview::g_sim_tick_ms));
        apply();

        assert(adapter.requestParameterBool("arp.enable", true).accepted());
        apply();
        assert(adapter.requestArpTransport(true).accepted());
        apply();
        assert(runtime.snapshot().arp.config.enabled && runtime.snapshot().arp.armed);
        assert(runtime.snapshot().sequencer.playing);

        for (const char* mod : {"MASTER", "DELAY"}) {
            lv_obj_send_event(button(ui.contentObject(), mod), LV_EVENT_CLICKED, nullptr);
            preview::pumpFrame(ui);
            assert(runtime.snapshot().arp.config.enabled && runtime.snapshot().arp.armed);
            assert(runtime.snapshot().sequencer.playing);
        }
        lv_obj_send_event(button(ui.contentObject(), "< TOCAR"), LV_EVENT_CLICKED, nullptr);
        preview::pumpFrame(ui);
        assert(ui.navigation().currentScreen() == ScreenId::Performance);
        assert(runtime.snapshot().arp.config.enabled && runtime.snapshot().arp.armed);
        assert(runtime.snapshot().sequencer.playing);
        assert(sessionLockMask() == 0x03);
        assert(runtime.snapshot().patch_id == patch_before);

        // Returning to Effects restores DELAY
        assert(ui.navigation().navigateTo(ScreenId::Effects));
        preview::pumpFrame(ui);
        assert(ui.navigation().subTab(ScreenId::Effects) == 0);

        // Clean up Arp, Sequencer, Panic, Lock
        seq::Request s_stop{};
        s_stop.action = seq::Action::Stop;
        assert(adapter.requestSequencer(s_stop).accepted());
        assert(adapter.requestArpTransport(false).accepted());
        apply();
        assert(adapter.requestParameterBool("arp.enable", false).accepted());
        assert(adapter.requestPanic().accepted());
        setSessionLockMask(0);
        apply();
    }

    // 4. Inspector 50-toggle & 200-idle-tick zero-revision/zero-leak benchmark
    {
        lv_obj_send_event(button(ui.contentObject(), "DELAY"), LV_EVENT_CLICKED, nullptr);
        preview::pumpFrame(ui);

        // 50 Inspector expand/collapse cycles -> zero heap drift, zero object drift
        const unsigned insp_objs_before = countObjects(ui.rootObject());
        lv_mem_monitor_t insp_mem_before{}, insp_mem_after{};
        lv_mem_monitor(&insp_mem_before);
        for (int i = 0; i < 25; ++i) {
            lv_obj_send_event(button(ui.contentObject(), "AJUSTE FINO"), LV_EVENT_CLICKED, nullptr);
            preview::pumpFrame(ui);
            lv_obj_send_event(button(ui.contentObject(), "FECHAR"), LV_EVENT_CLICKED, nullptr);
            preview::pumpFrame(ui);
        }
        const unsigned insp_objs_after = countObjects(ui.rootObject());
        lv_mem_monitor(&insp_mem_after);
        const ptrdiff_t insp_diff = static_cast<ptrdiff_t>(insp_mem_before.free_size) -
                                    static_cast<ptrdiff_t>(insp_mem_after.free_size);
        std::printf("INFO: [0.18-B.1 Inspector 50-toggle] objs_before=%u objs_after=%u free_before=%zuB free_after=%zuB diff=%tdB frag=%u%%\n",
                    insp_objs_before, insp_objs_after, insp_mem_before.free_size, insp_mem_after.free_size,
                    insp_diff, unsigned(insp_mem_after.frag_pct));
        assert(insp_objs_after == insp_objs_before && insp_diff == 0);

        // Settle snapshot & run 200 idle ticks -> 0 revision bumps & 0 heap drift
        backend.publish(runtime.snapshot(), false, 0, 0);
        preview::pumpFrame(ui);
        const uint32_t rev_before = adapter.model().revision();
        lv_mem_monitor_t idle_before{}, idle_after{};
        lv_mem_monitor(&idle_before);
        for (int i = 0; i < 200; ++i) {
            backend.publish(runtime.snapshot(), false, 0, 0);
            preview::pumpFrame(ui);
        }
        lv_mem_monitor(&idle_after);
        const uint32_t rev_delta = adapter.model().revision() - rev_before;
        const ptrdiff_t idle_diff = static_cast<ptrdiff_t>(idle_before.free_size) -
                                    static_cast<ptrdiff_t>(idle_after.free_size);
        std::printf("INFO: [0.18-B.1 Effects 200-idle-ticks] rev_delta=%u free_before=%zuB free_after=%zuB diff=%tdB frag=%u%%\n",
                    unsigned(rev_delta), idle_before.free_size, idle_after.free_size,
                    idle_diff, unsigned(idle_after.frag_pct));
        assert(rev_delta == 0 && idle_diff == 0);
    }

    assert(lv_mem_test() == LV_RESULT_OK);
    std::puts("PASS 0.18-B.1 FX Hardening: Chorus OFF/ON/OFF, 8x 800x480 captures, Navigation+Sustain+Arp+Seq+Lock, Inspector & Idle benchmarks");
}

} // namespace

int main(){
    preview::initHostLvgl();assert(r.begin());MemoryStore store;
    fx::NvsFxPresets fx_store;assert(fx_store.begin());
    LocalAmyPerformanceBackend b(r.commands());b.presetStore(&store);b.fxPresetStore(&fx_store);UiBlueprintPerformanceAdapter a(b);
    auto publish=[&]{b.publish(r.snapshot(),false,0,0);a.service(preview::g_sim_tick_ms+=25);};
    auto apply=[&]{a.service(preview::g_sim_tick_ms+=25);r.apply(preview::g_sim_tick_ms);r.loadPendingPatch();assert(r.render());publish();};
    publish();UiBlueprint app(a);assert(app.begin());

    // 1. Juno patch 4: Gate A, B, C, E parameters + Gate G0 inventory audit
    assert(a.requestSelectPatch(4).accepted());apply();
    auditInventory(a, apply);
    // Materialize historical offsets through the real owner mailbox.
    edit::Request legacy{};
    legacy.patch = 4;
    legacy.generation = r.commands().generation();
    legacy.request = r.snapshot().edit.request;
    legacy.macros[0] = 96;
    assert(r.commands().edits.push(legacy));
    r.apply(preview::g_sim_tick_ms += 25);
    publish();
    assert(app.navigation().navigateTo(ScreenId::Performance, 0));
    preview::pumpFrame(app);
    auto* open_legacy = button(app.contentObject(), "LEGACY OFFSETS");
    assert(open_legacy);
    lv_obj_send_event(open_legacy, LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(app);
    assert(labels(app.contentObject(), "LEGACY MACRO OFFSETS") == 1);
    assert(labels(app.contentObject(), "BRIGHTNESS   96 / 64   ACTIVE   Filter cutoff") == 1);
    assert(labels(app.contentObject(), "ATTACK   64 / 64   NEUTRAL   Amp envelope attack") == 1);
    assert(labels(app.contentObject(), "RELEASE   64 / 64   NEUTRAL   Amp envelope release") == 1);
    assert(preview::saveFramebufferPpm("build/editor011/legacy_offsets_0141.ppm"));
    auto* reset_legacy = button(app.contentObject(), "RESET LEGACY OFFSETS");
    assert(reset_legacy);
    const float with_legacy = r.snapshot().edit.applied[edit::Cutoff];
    assert(with_legacy > r.snapshot().edit.manual[edit::Cutoff]);
    lv_obj_send_event(reset_legacy, LV_EVENT_CLICKED, nullptr);
    assert(r.snapshot().edit.macros[0] == 96);
    lv_obj_send_event(reset_legacy, LV_EVENT_CLICKED, nullptr);
    assert(a.lastCommandState() == UiCommandState::Requested);
    assert(r.snapshot().edit.macros[0] == 96);
    apply();
    assert(a.lastCommandState() == UiCommandState::Confirmed);
    assert(r.snapshot().edit.macros[0] == 64);
    assert(r.snapshot().edit.applied[edit::Cutoff] == r.snapshot().edit.manual[edit::Cutoff]);

    assert(a.model().getFloat("filter.cutoff_hz")>3000);
    assert(a.parameterStatus("filter.cutoff_hz")==FeatureStatus::Available);
    assert(a.parameterStatus("fm.op.ratio")==FeatureStatus::Unavailable);
    assert(a.parameterStatus("fm.feedback")==FeatureStatus::Unavailable);
    assert(a.parameterStatus("fm.op.attack")==FeatureStatus::Unavailable);
    assert(a.parameterStatus("fm.op.decay")==FeatureStatus::Unavailable);
    assert(a.parameterStatus("fm.op.vel_sens")==FeatureStatus::Unavailable);
    assert(a.requestParameterFloat("filter.cutoff_hz",1000).state==UiCommandState::Requested);
    assert(a.model().getFloat("filter.cutoff_hz")!=1000);apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat("filter.cutoff_hz")==1000);
    for(const char* id:{"filter.env_amount","filter.keytrack"}){
        assert(a.parameterStatus(id)==FeatureStatus::Available);
        const float value=std::strcmp(id,"filter.env_amount")==0?3.0f:150.0f;
        assert(a.requestParameterFloat(id,value).state==UiCommandState::Requested);apply();
        assert(a.model().getFloat(id)==value && a.lastCommandState()==UiCommandState::Confirmed);
    }
    for(const char* id:{"env.amp.attack_ms","env.amp.decay_ms","env.amp.sustain","env.amp.release_ms"}){
        assert(a.parameterStatus(id)==FeatureStatus::Available);
    }
    assert(a.parameterStatus("env.amp.curve")==FeatureStatus::Planned);
    const char* component_ids[]={"osc.1.level","osc.sub_level","osc.noise_level","osc.1.pulse_width","osc.1.detune_ct","amp.level","amp.pan","amp.vel_sens"};
    const float component_values[]={35,15,15,30,35,50,-48,30};
    for(unsigned i=0;i<8;++i){
        if(a.parameterStatus(component_ids[i])!=FeatureStatus::Available){assert(!a.requestParameterFloat(component_ids[i],component_values[i]).accepted());continue;}
        assert(a.requestParameterFloat(component_ids[i],component_values[i]).state==UiCommandState::Requested);
        apply();assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat(component_ids[i])==component_values[i]);
    }
    // Gate E LFO & Mod Matrix Slot 1 on Juno
    for(const char* id:{"lfo.1.wave","lfo.1.rate_hz","lfo.1.depth","lfo.1.dest","mod.slot1.enable","mod.slot1.source","mod.slot1.dest","mod.slot1.amount","mod.slot1.polarity"}){
        assert(a.parameterStatus(id)==FeatureStatus::Available);
    }
    assert(a.parameterStatus("lfo.1.phase_deg")==FeatureStatus::Planned && a.parameterStatus("lfo.1.sync")==FeatureStatus::Planned);
    assert(a.isParameterValueSupported("lfo.1.dest",2) && !a.isParameterValueSupported("lfo.1.dest",4));
    assert(a.isParameterValueSupported("mod.slot1.dest",3) && !a.isParameterValueSupported("mod.slot1.dest",2));
    assert(a.requestParameterFloat("lfo.1.rate_hz",6.5f).state==UiCommandState::Requested);apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat("lfo.1.rate_hz")==6.5f);
    assert(a.requestParameterFloat("lfo.1.depth",45.0f).state==UiCommandState::Requested);apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat("lfo.1.depth")==45.0f);
    assert(a.requestParameterInt("lfo.1.wave",1).state==UiCommandState::Requested);apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getInt("lfo.1.wave")==1);
    assert(a.requestParameterInt("lfo.1.dest",2).state==UiCommandState::Requested);apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getInt("lfo.1.dest")==2);
    assert(!a.requestParameterInt("lfo.1.dest",4).accepted());
    assert(a.requestParameterInt("mod.slot1.dest",3).state==UiCommandState::Requested);apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getInt("mod.slot1.dest")==3 && a.model().modSlot(0).dest==3);
    assert(a.requestParameterFloat("mod.slot1.amount",60.0f).state==UiCommandState::Requested);apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat("mod.slot1.amount")==60.0f && a.model().modSlot(0).amount==60);
    assert(a.requestParameterInt("mod.slot1.polarity",0).state==UiCommandState::Requested);apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getInt("mod.slot1.polarity")==0 && a.model().modSlot(0).polarity==0);

    assert(!a.requestSelectPatch(135).accepted()); // dirty guard: no silent loss
    assert(a.requestParameterInt("perf.macro.2",32).accepted());
    assert(a.model().getInt("perf.macro.2")==64);
    apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getInt("perf.macro.2")==32);
    assert(a.model().getFloat("filter.cutoff_hz")==125);
    assert(a.requestParameterInt("perf.macro.2",64).accepted());apply();assert(a.model().getFloat("filter.cutoff_hz")==1000);
    assert(a.requestSoundAction(0).accepted());apply();assert(r.snapshot().edit.compare);
    assert(a.requestSoundAction(0).accepted());apply();assert(a.model().getFloat("filter.cutoff_hz")==1000);
    assert(!a.requestParameterFloat("filter.resonance",8).accepted());
    assert(a.requestSoundAction(2,0,"MY STRINGS 01").accepted());
    r.apply(preview::g_sim_tick_ms);assert(r.snapshot().storage_quiet);publish(); // writes in UI service
    r.apply(preview::g_sim_tick_ms);publish();assert(!b.busy());
    char name[25];uint16_t origin;assert(b.userPreset(0,name,sizeof(name),origin));assert(origin==4 && !std::strcmp(name,"MY STRINGS 01"));
    store.fail=true;assert(a.requestSoundAction(2,0,"MY STRINGS 01").accepted());
    r.apply(preview::g_sim_tick_ms);publish();r.apply(preview::g_sim_tick_ms);publish();
    assert(a.lastCommandState()==UiCommandState::Rejected);store.fail=false;

    // 2. Juno patch 31: contextual Pulse Width
    assert(a.requestSelectPatch(31).accepted());apply();
    assert(a.parameterStatus("osc.1.pulse_width")==FeatureStatus::Available);
    assert(a.requestParameterFloat("osc.1.pulse_width",30).state==UiCommandState::Requested);apply();
    assert(a.model().getFloat("osc.1.pulse_width")==30 && a.lastCommandState()==UiCommandState::Confirmed);
    assert(a.requestSoundAction(1).accepted());apply();

    // 3. DX7 patch 135: Gate D advanced FM + Gate E contextual LFO/Mod + Gate G0 inventory audit
    assert(a.requestSelectPatch(135).accepted());apply();
    auditInventory(a, apply);
    assert(a.requestParameterInt("fm.selected_op",1).accepted());
    assert(a.parameterStatus("filter.cutoff_hz")==FeatureStatus::Unavailable);
    assert(a.parameterStatus("filter.env_amount")==FeatureStatus::Unavailable && a.parameterStatus("filter.keytrack")==FeatureStatus::Unavailable);
    assert(a.parameterStatus("amp.level")==FeatureStatus::Available);
    assert(a.parameterStatus("amp.vel_sens")==FeatureStatus::Unavailable && a.parameterStatus("osc.sub_level")==FeatureStatus::Unavailable);
    for(const char* id:{"fm.depth","fm.algo","fm.feedback","fm.selected_op","fm.op.level","fm.op.ratio","fm.op.attack","fm.op.decay","fm.op.vel_sens"}){
        assert(a.parameterStatus(id)==FeatureStatus::Available);
    }
    assert(a.requestParameterInt("fm.algo",5).accepted());apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getInt("fm.algo")==5 && r.snapshot().edit.applied[edit::FmAlgorithm]==5.0f);
    assert(!a.isParameterValueSupported("lfo.1.dest",2) && a.isParameterValueSupported("lfo.1.dest",3));
    assert(!a.isParameterValueSupported("mod.slot1.dest",3) && a.isParameterValueSupported("mod.slot1.dest",1));
    assert(!a.requestParameterInt("lfo.1.dest",2).accepted());
    assert(!a.requestParameterInt("mod.slot1.dest",3).accepted());
    assert(a.requestParameterFloat("fm.depth",0.7f).accepted());apply();assert(r.snapshot().edit.applied[edit::Depth]==0.7f);
    assert(a.requestParameterFloat("fm.feedback",65.0f).accepted());apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat("fm.feedback")==65.0f);
    assert(a.requestParameterInt("fm.selected_op",1).accepted());
    assert(a.requestParameterFloat("fm.op.attack",120.0f).accepted());apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat("fm.op.attack")==120.0f && r.snapshot().edit.applied[edit::OpAttack0]==120.0f);
    assert(a.requestParameterFloat("fm.op.decay",850.0f).accepted());apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat("fm.op.decay")==850.0f && r.snapshot().edit.applied[edit::OpDecay0]==850.0f);
    assert(a.requestParameterFloat("fm.op.vel_sens",75.0f).accepted());apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat("fm.op.vel_sens")==75.0f && r.snapshot().edit.applied[edit::OpVel0]==75.0f);
    // Verify per-operator independence and contextual N/A when OP2 decay exceeds simplified bounds
    assert(a.requestParameterInt("fm.selected_op",2).accepted());
    assert(a.parameterStatus("fm.op.attack")==FeatureStatus::Available && a.parameterStatus("fm.op.vel_sens")==FeatureStatus::Available);
    assert(a.parameterStatus("fm.op.decay")==FeatureStatus::Unavailable && !a.requestParameterFloat("fm.op.decay",500.0f).accepted());
    assert(a.model().getFloat("fm.op.attack")!=120.0f); // OP2 attack was not overwritten by OP1 edit
    assert(a.requestSoundAction(1).accepted());apply();

    // 4. DX7 patch 153: fixed-frequency operator ratio contextual N/A vs ratio-mode AVAIL
    assert(a.requestSelectPatch(153).accepted());apply();
    unsigned na_ops=0, avail_ops=0;
    for(int op=1;op<=6;++op){
        assert(a.requestParameterInt("fm.selected_op",op).accepted());
        if(a.parameterStatus("fm.op.ratio")==FeatureStatus::Unavailable)++na_ops;
        else if(a.parameterStatus("fm.op.ratio")==FeatureStatus::Available)++avail_ops;
    }
    assert(na_ops>=1 && avail_ops>=1);
    assert(a.requestParameterInt("fm.selected_op",1).accepted());

    // 5. Recall slot 0 & reboot persistence across engine/backend lifecycle
    assert(a.requestSoundAction(3,0).accepted());apply();apply();assert(r.snapshot().patch_id==4 && r.snapshot().edit.applied[edit::Cutoff]==1000);
    app.shutdown();r.end();assert(r.begin());LocalAmyPerformanceBackend reboot(r.commands());reboot.presetStore(&store);reboot.fxPresetStore(&fx_store);
    reboot.publish(r.snapshot(),false,0,0);assert(reboot.editAction(3,0));r.apply(0);r.loadPendingPatch();reboot.publish(r.snapshot(),false,0,0);reboot.service(25);r.apply(1);reboot.publish(r.snapshot(),false,0,0);
    assert(r.snapshot().edit.applied[0]==1000 && r.snapshot().edit.user_slot==0);
    assert(r.snapshot().edit.applied[edit::FilterEnv]==3 && r.snapshot().edit.applied[edit::FilterKeytrack]==150);
    assert(r.snapshot().edit.applied[edit::OscLevel]==35 && r.snapshot().edit.applied[edit::SubLevel]==15 && r.snapshot().edit.applied[edit::NoiseLevel]==15);
    assert(r.snapshot().edit.applied[edit::PatchLevel]==50 && r.snapshot().edit.applied[edit::Pan]==-48 && r.snapshot().edit.applied[edit::Velocity]==30);
    assert(r.snapshot().edit.applied[edit::LfoRate]==6.5f && r.snapshot().edit.applied[edit::LfoDepth]==45.0f && r.snapshot().edit.applied[edit::LfoWave]==1.0f && r.snapshot().edit.applied[edit::LfoDest]==2.0f);
    assert(r.snapshot().edit.applied[edit::ModDest]==3.0f && r.snapshot().edit.applied[edit::ModAmount]==60.0f && r.snapshot().edit.applied[edit::ModPolarity]==0.0f);

    // 6. All 7 SoundEditor tabs UI verification & screenshot capture
    UiBlueprintPerformanceAdapter next(reboot);next.service(preview::g_sim_tick_ms+=25);UiBlueprint ui(next);assert(ui.begin());
    const char* tab_shots[7]={
        "build/editor011/osc.ppm","build/editor011/fm.ppm","build/editor011/filter.ppm",
        "build/editor011/amp.ppm","build/editor011/env.ppm","build/editor011/lfo.ppm","build/editor011/mod.ppm"
    };
    for(unsigned tab=0;tab<7;++tab){
        assert(ui.navigation().navigateTo(ScreenId::SoundEditor,uint8_t(tab)));
        preview::pumpFrame(ui);
        assert(preview::saveFramebufferPpm(tab_shots[tab]));
    }
    // Check OSC tab contextual badges
    assert(ui.navigation().navigateTo(ScreenId::SoundEditor,0));preview::pumpFrame(ui);
    assert(button(ui.contentObject(),"MAIN MIX"));
    auto* pulse_card=button(ui.contentObject(),"PULSE WIDTH");assert(pulse_card && labels(pulse_card,"N/A")>=2 && labels(pulse_card,"PLAN")==0);
    // Check FM tab shows both FM BRIGHTNESS and FM FEEDBACK and FM ALGORITHM
    assert(ui.navigation().navigateTo(ScreenId::SoundEditor,1));preview::pumpFrame(ui);
    assert(button(ui.contentObject(),"FM BRIGHTNESS") && button(ui.contentObject(),"FM FEEDBACK"));
    auto* algo_card=button(ui.contentObject(),"FM ALGORITHM");assert(algo_card);
    auto* op1_btn=button(ui.contentObject(),"OP 1");assert(op1_btn);
    auto* op3_btn=button(ui.contentObject(),"OP 3");assert(op3_btn);
    // Switch to DX7 FM patch 135 to test live interactive FM topology and operator switching
    auto next_apply=[&]{next.service(preview::g_sim_tick_ms+=25);r.apply(preview::g_sim_tick_ms);r.loadPendingPatch();assert(r.render());reboot.publish(r.snapshot(),false,0,0);next.service(preview::g_sim_tick_ms+=25);preview::pumpFrame(ui);};
    assert(next.requestSelectPatch(135).accepted());next_apply();
    assert(ui.navigation().navigateTo(ScreenId::SoundEditor,1));preview::pumpFrame(ui);
    auto* live_op3=button(ui.contentObject(),"OP 3");assert(live_op3 && !lv_obj_has_state(live_op3,LV_STATE_DISABLED));
    lv_obj_send_event(live_op3,LV_EVENT_CLICKED,nullptr);next_apply();
    assert(next.model().getInt("fm.selected_op",1)==3);
    // Check MOD tab Slot 1 interactive button click -> owner confirmation
    assert(ui.navigation().navigateTo(ScreenId::SoundEditor,6));preview::pumpFrame(ui);
    auto* m1_btn=button(ui.contentObject(),"M1 ON");assert(m1_btn && !lv_obj_has_state(m1_btn,LV_STATE_DISABLED));
    auto* m2_btn=button(ui.contentObject(),"M2 ON");assert(m2_btn && lv_obj_has_state(m2_btn,LV_STATE_DISABLED));
    lv_obj_send_event(m1_btn,LV_EVENT_CLICKED,nullptr);
    assert(next.lastCommandState()==UiCommandState::Requested && next.model().modSlot(0).enabled); // still ON until owner confirms
    next.service(preview::g_sim_tick_ms+=25);r.apply(preview::g_sim_tick_ms);reboot.publish(r.snapshot(),false,0,0);preview::pumpFrame(ui);
    assert(next.lastCommandState()==UiCommandState::Confirmed && !next.model().modSlot(0).enabled);
    assert(button(ui.contentObject(),"M1 OFF"));

    // Check User Presets modal
    assert(ui.navigation().navigateTo(ScreenId::SoundEditor,2));preview::pumpFrame(ui);
    auto* open=button(ui.contentObject(),"USER / SAVE AS");assert(open);lv_obj_send_event(open,LV_EVENT_CLICKED,nullptr);preview::pumpFrame(ui);
    assert(preview::saveFramebufferPpm("build/editor011/presets.ppm"));
    auto* close=button(lv_screen_active(),"CLOSE");assert(close);
    const auto screen_children=lv_obj_get_child_count(lv_screen_active());
    lv_obj_send_event(open,LV_EVENT_CLICKED,nullptr);
    assert(lv_obj_get_child_count(lv_screen_active())==screen_children);
    auto* panel=lv_obj_get_parent(close);
    auto* save_as=button(panel,"SAVE AS");assert(save_as);
    const auto before_overwrite=next.presetOperationStatus().revision;
    lv_obj_send_event(save_as,LV_EVENT_CLICKED,nullptr);
    preview::g_sim_tick_ms+=10000;
    preview::pumpFrame(ui);
    lv_obj_send_event(save_as,LV_EVENT_CLICKED,nullptr);
    assert(next.presetOperationStatus().revision==before_overwrite);

    edit::Preset replacement{};
    fx::FxConfig replacement_fx{};
    assert(reboot.transactionCoordinator().load(0,replacement,replacement_fx));
    assert(reboot.transactionCoordinator().save(0,replacement,replacement_fx).status==preset::SaveResult::Status::Success);
    lv_obj_send_event(save_as,LV_EVENT_CLICKED,nullptr);
    assert(next.presetOperationStatus().revision==before_overwrite);
    lv_obj_send_event(close,LV_EVENT_CLICKED,nullptr);preview::pumpFrame(ui);
    assert(ui.navigation().navigateTo(ScreenId::PatchBrowser));preview::pumpFrame(ui);assert(button(ui.contentObject(),"USER PRESETS / SAVE AS"));
    auto* browser_open=button(ui.contentObject(),"USER PRESETS / SAVE AS");
    assert(browser_open);
    lv_obj_send_event(browser_open,LV_EVENT_CLICKED,nullptr);
    preview::pumpFrame(ui);
    auto* browser_close=button(lv_screen_active(),"CLOSE");
    assert(browser_close);
    lv_obj_send_event(browser_close,LV_EVENT_CLICKED,nullptr);
    preview::pumpFrame(ui);
    // 7. Effects Screen 0.18-B Pilot verification, contextual switch, slider coalescing & screenshot capture
    assert(ui.navigation().navigateTo(ScreenId::Effects, 0));
    preview::pumpFrame(ui);
    assert(preview::saveFramebufferPpm("build/editor011/effects.ppm"));
    assert(button(ui.contentObject(), "DELAY") && button(ui.contentObject(), "REVERB"));
    assert(button(ui.contentObject(), "CHORUS") && button(ui.contentObject(), "MASTER"));
    assert(button(ui.contentObject(), "MANUAL TIME") && button(ui.contentObject(), "DELAY FDBK") && button(ui.contentObject(), "DELAY MIX"));
    assert(button(ui.contentObject(), "DELAY MODE") && button(ui.contentObject(), "DIVISION"));
    assert(!button(ui.contentObject(), "REVERB SIZE") && !button(ui.contentObject(), "CHORUS MODE"));

    // Coalesce continuous drag events (<35ms) and commit exact final position on RELEASED
    {
        lv_obj_t* fdbk_card = button(ui.contentObject(), "DELAY FDBK");
        lv_obj_t* fdbk_slider = firstSlider(fdbk_card);
        assert(fdbk_slider);
        const uint32_t tok0 = reboot.fxToken();
        lv_obj_add_state(fdbk_slider, LV_STATE_PRESSED);
        lv_slider_set_value(fdbk_slider, 60, LV_ANIM_OFF);
        lv_obj_send_event(fdbk_slider, LV_EVENT_VALUE_CHANGED, nullptr);
        assert(reboot.fxToken() == tok0 + 1);
        preview::g_sim_tick_ms += 5;
        lv_slider_set_value(fdbk_slider, 70, LV_ANIM_OFF);
        lv_obj_send_event(fdbk_slider, LV_EVENT_VALUE_CHANGED, nullptr);
        assert(reboot.fxToken() == tok0 + 1); // coalesced inside 35 ms window
        preview::g_sim_tick_ms += 5;
        lv_slider_set_value(fdbk_slider, 75, LV_ANIM_OFF);
        lv_obj_send_event(fdbk_slider, LV_EVENT_VALUE_CHANGED, nullptr);
        assert(reboot.fxToken() == tok0 + 1); // still coalesced
        lv_obj_remove_state(fdbk_slider, LV_STATE_PRESSED);
        lv_obj_send_event(fdbk_slider, LV_EVENT_RELEASED, nullptr);
        assert(reboot.fxToken() == tok0 + 2); // final release commit sent
        next_apply();
        assert(std::fabs(r.snapshot().fx.config.delay_feedback - 0.7125f) < 0.03f);
    }

    // Active Touch + USB notes remain intact across FX module switches
    const auto fx_touch = next.requestNoteOn(60, 100);
    assert(fx_touch.accepted());
    const uint8_t fx_usb_on[]{0x09, 0x90, 64, 100};
    assert(r.usbInput().transfer(fx_usb_on, sizeof(fx_usb_on), preview::g_sim_tick_ms));
    next_apply();
    assert(r.snapshot().active_notes == 2 && next.isNoteHeld(60));

    lv_obj_send_event(button(ui.contentObject(), "REVERB"), LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(ui);
    assert(preview::saveFramebufferPpm("build/editor011/effects_reverb.ppm"));
    assert(button(ui.contentObject(), "REVERB SIZE") && button(ui.contentObject(), "REVERB MIX"));
    assert(!button(ui.contentObject(), "DELAY MODE") && r.snapshot().active_notes == 2 && next.isNoteHeld(60));

    lv_obj_send_event(button(ui.contentObject(), "CHORUS"), LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(ui);
    assert(preview::saveFramebufferPpm("build/editor011/effects_chorus.ppm"));
    assert(button(ui.contentObject(), "CHORUS MODE") && button(ui.contentObject(), "CHORUS DEPTH"));
    assert(!button(ui.contentObject(), "REVERB SIZE") && r.snapshot().active_notes == 2 && next.isNoteHeld(60));

    lv_obj_send_event(button(ui.contentObject(), "MASTER"), LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(ui);
    assert(preview::saveFramebufferPpm("build/editor011/effects_master.ppm"));
    assert(button(ui.contentObject(), "FX BYPASS") && button(ui.contentObject(), "MASTER VOL"));
    assert(button(ui.contentObject(), "TILT TONE") && button(ui.contentObject(), "EQ LOW"));
    assert(button(ui.contentObject(), "EQ MID") && button(ui.contentObject(), "EQ HIGH"));
    assert(r.snapshot().active_notes == 2 && next.isNoteHeld(60));

    assert(next.requestNoteOff(60, fx_touch.token).accepted());
    const uint8_t fx_usb_off[]{0x08, 0x80, 64, 0};
    assert(r.usbInput().transfer(fx_usb_off, sizeof(fx_usb_off), preview::g_sim_tick_ms));
    next_apply();
    assert(r.snapshot().active_notes == 0 && !next.isNoteHeld(60));

    // Interactive FX edits with owner round-trip confirmation (including B.1 Mid/High EQ & Bypass All)
    assert(next.requestParameterInt("fx.chorus.mode", 2).accepted());
    next_apply();
    assert(next.lastCommandState() == UiCommandState::Confirmed && r.snapshot().fx.config.chorus_mode == 2);

    assert(next.requestParameterFloat("fx.delay.time_ms", 500.0f).accepted());
    next_apply();
    assert(next.lastCommandState() == UiCommandState::Confirmed && r.snapshot().fx.config.delay_time_ms == 500.0f);

    assert(next.requestParameterFloat("fx.reverb.mix", 50.0f).accepted());
    next_apply();
    assert(next.lastCommandState() == UiCommandState::Confirmed && std::fabs(r.snapshot().fx.config.reverb_mix - 0.5f) < 0.01f);

    assert(next.requestParameterFloat("fx.master_tone", 30.0f).accepted());
    next_apply();
    assert(next.lastCommandState() == UiCommandState::Confirmed && std::fabs(r.snapshot().fx.config.master_tone - 0.3f) < 0.01f);

    assert(next.requestParameterFloat("fx.eq.mid_db", -3.0f).accepted());
    next_apply();
    assert(next.lastCommandState() == UiCommandState::Confirmed && std::fabs(r.snapshot().fx.config.eq_mid_db - (-3.0f)) < 0.01f);

    assert(next.requestParameterFloat("fx.eq.high_db", 4.0f).accepted());
    next_apply();
    assert(next.lastCommandState() == UiCommandState::Confirmed && std::fabs(r.snapshot().fx.config.eq_high_db - 4.0f) < 0.01f);

    assert(next.requestParameterBool("fx.bypass_all", true).accepted());
    next_apply();
    assert(next.lastCommandState() == UiCommandState::Confirmed && r.snapshot().fx.config.bypass_all && r.snapshot().fx.applied.bypass_all);

    assert(next.requestParameterBool("fx.bypass_all", false).accepted());
    next_apply();
    assert(next.lastCommandState() == UiCommandState::Confirmed && !r.snapshot().fx.config.bypass_all && !r.snapshot().fx.applied.bypass_all);

    // SYNC lock on MANUAL TIME
    assert(next.requestParameterInt("fx.delay.mode", 1).accepted());
    next_apply();
    lv_obj_send_event(button(ui.contentObject(), "DELAY"), LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(ui);
    assert(lv_obj_has_state(firstSlider(button(ui.contentObject(), "MANUAL TIME")), LV_STATE_DISABLED));
    assert(next.requestParameterInt("fx.delay.mode", 0).accepted());
    next_apply();
    assert(!lv_obj_has_state(firstSlider(button(ui.contentObject(), "MANUAL TIME")), LV_STATE_DISABLED));

    // 500 in-place FX module switch stress benchmark (125 x 4 modules after 16 warmup switches)
    {
        const char* mods[4] = {"DELAY", "REVERB", "CHORUS", "MASTER"};
        for (unsigned w = 0; w < 16; ++w) {
            lv_obj_send_event(button(ui.contentObject(), mods[w % 4]), LV_EVENT_CLICKED, nullptr);
            preview::pumpFrame(ui);
        }
        lv_obj_send_event(button(ui.contentObject(), mods[0]), LV_EVENT_CLICKED, nullptr);
        preview::pumpFrame(ui);

        const unsigned objs_before = countObjects(ui.rootObject());
        lv_mem_monitor_t mem_before{}, mem_after{};
        lv_mem_monitor(&mem_before);
        std::vector<double> durations_us;
        durations_us.reserve(500);
        for (unsigned i = 1; i <= 500; ++i) {
            const auto t0 = std::chrono::steady_clock::now();
            lv_obj_send_event(button(ui.contentObject(), mods[i % 4]), LV_EVENT_CLICKED, nullptr);
            preview::pumpFrame(ui);
            const auto t1 = std::chrono::steady_clock::now();
            durations_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        const unsigned objs_after = countObjects(ui.rootObject());
        lv_mem_monitor(&mem_after);
        std::sort(durations_us.begin(), durations_us.end());
        const double median_us = durations_us[durations_us.size() / 2];
        const double p95_us = durations_us[(durations_us.size() * 95) / 100];
        const double max_us = durations_us.back();
        const ptrdiff_t diff_bytes = static_cast<ptrdiff_t>(mem_before.free_size) - static_cast<ptrdiff_t>(mem_after.free_size);
        std::printf("INFO: [0.18-B FX 500-switch host nav] objs_before=%u objs_after=%u | LVGL free_before=%zuB free_after=%zuB largest_before=%zuB largest_after=%zuB diff=%tdB peak_used=%zuB frag_before=%u%% frag_after=%u%% | HOST duration (NOT touch/DSI): median=%.1fus P95=%.1fus max=%.1fus\n",
                    objs_before, objs_after, mem_before.free_size, mem_after.free_size,
                    mem_before.free_biggest_size, mem_after.free_biggest_size, diff_bytes,
                    mem_after.max_used, unsigned(mem_before.frag_pct), unsigned(mem_after.frag_pct),
                    median_us, p95_us, max_us);
        assert(objs_after == objs_before);
        assert(diff_bytes == 0);
        assert(mem_after.frag_pct <= 5);
        assert(lv_mem_test() == LV_RESULT_OK);
    }

    // The real USB packet parser, UI learn actions and audio owner in one loop.
    using MidiAction=IUiBackend::MidiAction;
    r.usbInput().enableControls();
    next.setMidiInput(r.usbInput());
    ProfileStore profile_store;
    assert(profile_store.save(performance::ControllerProfile{}));
    next.setControllerProfileStore(profile_store);
    reboot.publishMidi(true,false,0);
    const auto service=[&]{
        next.service(preview::g_sim_tick_ms+=25);
        r.apply(preview::g_sim_tick_ms);
        r.loadPendingPatch();
        reboot.publish(r.snapshot(),false,0,0);
        next.service(preview::g_sim_tick_ms+=25);
    };
    service();
    assert(ui.navigation().navigateTo(ScreenId::MidiSettings,1));preview::pumpFrame(ui);
    assert(button(ui.contentObject(),"SAVE PROFILE"));
    next.midiPerformanceAction(MidiAction::Learn,6);
    const auto cc=[&](uint8_t value){
        const uint8_t packet[]{0x0b,0xb0,74,value};
        assert(r.usbInput().transfer(packet,sizeof(packet),preview::g_sim_tick_ms));
        service();
    };
    cc(20);
    assert(std::strstr(next.midiLearnStatus(),"CC74"));
    next.midiPerformanceAction(MidiAction::Confirm,0);
    const auto target=r.snapshot().edit.performance_macros[6];
    cc(10);cc(80);
    assert(r.snapshot().edit.performance_macros[6]==target);
    cc(81);service();
    assert(r.snapshot().edit.performance_macros[6]==81);
    next.midiPerformanceAction(MidiAction::SaveProfile,0);
    service();service();
    performance::ControllerProfile stored_profile;
    assert(profile_store.load(stored_profile) && stored_profile.knobs[6].controller==74);
    assert(std::strcmp(next.midiLearnStatus(),"PROFILE SAVED")==0);
    auto corrupt_profile=profile_store;
    corrupt_profile.bytes[10]^=1;
    assert(!corrupt_profile.load(stored_profile));
    assert(stored_profile.knobs[6].controller==74);
    const auto positions=r.snapshot().edit.performance_macros;
    assert(next.requestSoundAction(2,15,"PERFORMANCE V3").accepted());
    service();service();
    edit::Preset saved_performance;
    fx::FxConfig saved_manual_fx;
    assert(reboot.transactionCoordinator().load(15,saved_performance,saved_manual_fx));
    assert(saved_performance.performance_macros==positions);
    assert(next.requestSoundAction(3,15).accepted());
    for(unsigned i=0;i<5;++i)service();
    assert(r.snapshot().edit.performance_macros==positions);
    assert(next.presetOperationStatus().state==ui::PresetOperationState::Confirmed);
    next.midiPerformanceAction(MidiAction::Learn,0);
    cc(50);
    next.midiPerformanceAction(MidiAction::Confirm,0);
    assert(std::strstr(next.midiLearnStatus(),"IN USE BY K7"));
    next.midiPerformanceAction(MidiAction::Replace,0);
    assert(std::strstr(next.midiLearnStatus(),"K1 BOUND"));
    for(unsigned i=0;i<4;++i)next.midiPerformanceAction(MidiAction::NextBank,0);
    cc(20);cc(100);
    assert(r.snapshot().edit.performance_macros==positions);
    cc(101);service();
    assert(r.snapshot().edit.performance_macros[0]==101);
    assert(r.snapshot().edit.performance_macros[6]==positions[6]);
    assert(ui.navigation().navigateTo(ScreenId::Performance,0));preview::pumpFrame(ui);
    for(const char* macro:{"CHARACTER","BRIGHTNESS","MOTION","SHAPE","ATTACK","RELEASE","SPACE","EDGE"})
        assert(labels(ui.contentObject(),macro)==1);
    assert(preview::saveFramebufferPpm("build/editor011/performance_014.ppm"));

    const auto open_routes = [&] {
        auto* open = button(ui.contentObject(), "ROUTING EDITOR");
        assert(open);
        lv_obj_send_event(open, LV_EVENT_CLICKED, nullptr);
        preview::pumpFrame(ui);
        auto* content = ui.contentObject();
        assert(labels(content, "MACRO ROUTING EDITOR") == 1);
        select(widgetAt(content, &lv_dropdown_class, 12, 36), 6);
        select(widgetAt(content, &lv_dropdown_class, 267, 36), 5);
        return content;
    };
    const auto choose_reverb = [](lv_obj_t* content) {
        auto* destination = widgetAt(content, &lv_dropdown_class, 521, 36);
        assert(destination);
        const int option = lv_dropdown_get_option_index(destination, "FX / Reverb Mix");
        assert(option > 0);
        select(destination, unsigned(option));
        auto* depth = widgetAt(content, &lv_textarea_class, 372, 146);
        assert(depth);
        return depth;
    };
    auto* route_content = open_routes();
    auto* depth = choose_reverb(route_content);
    lv_textarea_set_text(depth, "0.55");
    lv_obj_send_event(button(route_content, "CANCEL / BACK"), LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(ui);
    assert(!r.snapshot().edit.performance_routes.custom);
    route_content = open_routes();
    depth = choose_reverb(route_content);
    const auto before_reverb = amy_global.bus[0]->reverb.level;
    lv_textarea_set_text(depth, "2");
    lv_obj_send_event(button(route_content, "APPLY ROUTES"), LV_EVENT_CLICKED, nullptr);
    assert(next.lastCommandState() == UiCommandState::Rejected);
    assert(amy_global.bus[0]->reverb.level == before_reverb);
    lv_textarea_set_text(depth, "0.55");
    select(widgetAt(route_content, &lv_dropdown_class, 12, 206), 3);
    lv_obj_send_event(button(route_content, "APPLY ROUTES"), LV_EVENT_CLICKED, nullptr);
    assert(next.routingResult().state == UiCommandState::Requested);
    assert(labels(route_content, "WAITING FOR OWNER") == 1);
    assert(amy_global.bus[0]->reverb.level == before_reverb);
    next_apply();
    assert(next.routingResult().state == UiCommandState::Confirmed);
    assert(labels(route_content, "ROUTES APPLIED. SAVE PRESET TO KEEP.") == 1);
    assert(amy_global.bus[0]->reverb.level > before_reverb);
    const auto& route = r.snapshot().edit.performance_routes.routes[6][5];
    assert(route.depth == 0.55f && route.curve == performance::Curve::SCurve);
    assert(preview::saveFramebufferPpm("build/editor011/routing_editor_0141.ppm"));
    IUiBackend::RoutingValues fx_values;
    assert(next.routingValues(performance::ReverbMix,fx_values));
    assert(fx_values.applied > fx_values.manual && (fx_values.sources & (1u<<6)));
    assert(next.requestSoundAction(2,14,"ROUTING V4").accepted());
    for(unsigned i=0;i<4;++i)service();
    assert(next.presetOperationStatus().state==ui::PresetOperationState::Confirmed);
    edit::Preset saved_routes;
    fx::FxConfig saved_routes_fx;
    preset::CommitMarker route_marker;
    assert(reboot.transactionCoordinator().load(14,saved_routes,saved_routes_fx,&route_marker));
    assert(route_marker.synth_version==4 && saved_routes.performance_routes==r.snapshot().edit.performance_routes);
    lv_obj_send_event(button(route_content, "CANCEL / BACK"), LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(ui);
    route_content = open_routes();
    lv_obj_send_event(button(route_content, "DELETE ROUTE"), LV_EVENT_CLICKED, nullptr);
    lv_obj_send_event(button(route_content, "APPLY ROUTES"), LV_EVENT_CLICKED, nullptr);
    assert(next.routingResult().state == UiCommandState::Requested);
    next_apply();
    assert(next.routingResult().state == UiCommandState::Confirmed);
    assert(!r.snapshot().edit.performance_routes.custom && amy_global.bus[0]->reverb.level == before_reverb);
    assert(reboot.unsaved());
    assert(next.requestSoundAction(1).accepted());
    next_apply();
    assert(r.snapshot().edit.performance_routes==saved_routes.performance_routes && !reboot.unsaved());
    assert(next.requestSoundAction(0).accepted());next_apply();
    assert(r.snapshot().edit.compare && amy_global.bus[0]->reverb.level==0);
    assert(next.requestSoundAction(0).accepted());next_apply();
    assert(!r.snapshot().edit.compare && amy_global.bus[0]->reverb.level>before_reverb);
    lv_obj_send_event(button(route_content, "CANCEL / BACK"), LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(ui);
    assert(next.requestSoundAction(3,14).accepted());
    for(unsigned i=0;i<6;++i)service();
    assert(next.presetOperationStatus().state==ui::PresetOperationState::Confirmed);
    assert(r.snapshot().edit.performance_routes==saved_routes.performance_routes);
    route_content = open_routes();
    next.requestPanic();
    next_apply();
    assert(lv_obj_has_state(button(route_content, "APPLY ROUTES"), LV_STATE_DISABLED));
    assert(labels(route_content, "PATCH OR ROUTES CHANGED. REOPEN EDITOR.") == 1);

    for(unsigned i=0;i<3;++i)next.midiPerformanceAction(MidiAction::NextBank,0);
    assert(ui.navigation().navigateTo(ScreenId::MidiSettings,1));preview::pumpFrame(ui);
    lv_obj_send_event(button(ui.contentObject(),"DESTINATION"),LV_EVENT_CLICKED,nullptr);
    preview::pumpFrame(ui);
    auto* midi_targets=widgetAt(ui.contentObject(),&lv_dropdown_class,12,110);
    assert(midi_targets);
    const int feedback=lv_dropdown_get_option_index(midi_targets,"DELAY FDBK");
    assert(feedback>0);
    lv_dropdown_set_selected(midi_targets,unsigned(feedback));
    lv_obj_send_event(button(ui.contentObject(),"APPLY DESTINATION"),LV_EVENT_CLICKED,nullptr);
    preview::pumpFrame(ui);
    assert(next.midiCustomTarget(0)!=0);
    assert(next.requestParameterFloat("fx.delay.feedback",40).accepted());
    service();service();
    cc(0);cc(120);cc(121);service();
    assert(r.snapshot().fx.config.delay_feedback>0.80f);
    next.midiPerformanceAction(MidiAction::SaveProfile,0);
    service();service();
    performance::ControllerProfile custom_profile;
    assert(profile_store.load(custom_profile));
    assert(custom_profile.destinations[3][0]==next.midiCustomTarget(0));
    assert(custom_profile.knobs[0].controller==74);
    uint8_t v1_profile[performance::kProfileV1Bytes]{};
    std::memcpy(v1_profile,profile_store.bytes,32);
    v1_profile[4]=1;
    edit::put32(v1_profile+32,edit::crc(v1_profile,32));
    performance::ControllerProfile legacy_profile;
    assert(performance::decodeProfile(v1_profile,sizeof(v1_profile),legacy_profile));
    assert(legacy_profile.destinations==performance::MidiDestinations{});
    assert(legacy_profile.knobs==custom_profile.knobs);
    assert(next.requestSoundAction(1).accepted());next_apply();
    assert(!reboot.unsaved());
    assert(ui.navigation().navigateTo(ScreenId::Performance,0));preview::pumpFrame(ui);
    route_content=open_routes();
    lv_obj_send_event(button(route_content,"RESET POSITION"),LV_EVENT_CLICKED,nullptr);
    assert(r.snapshot().edit.performance_macros[6]!=performance::kNeutral[6]);
    lv_obj_send_event(button(route_content,"APPLY ROUTES"),LV_EVENT_CLICKED,nullptr);
    next_apply();
    assert(r.snapshot().edit.performance_macros[6]==performance::kNeutral[6]);
    assert(r.snapshot().edit.performance_routes==saved_routes.performance_routes);
    assert(next.requestSoundAction(1).accepted());next_apply();
    assert(r.snapshot().edit.performance_macros[6]==saved_routes.performance_macros[6]);
    lv_obj_send_event(button(route_content,"RESET ROUTES"),LV_EVENT_CLICKED,nullptr);
    lv_obj_send_event(button(route_content,"APPLY ROUTES"),LV_EVENT_CLICKED,nullptr);
    next_apply();
    assert(!r.snapshot().edit.performance_routes.custom);
    assert(r.snapshot().edit.performance_macros[6]==saved_routes.performance_macros[6]);
    assert(next.requestSoundAction(1).accepted());next_apply();
    lv_obj_send_event(button(route_content,"CANCEL / BACK"),LV_EVENT_CLICKED,nullptr);
    preview::pumpFrame(ui);
    route_content=open_routes();
    lv_obj_send_event(button(route_content,"RESET BOTH"),LV_EVENT_CLICKED,nullptr);
    lv_obj_send_event(button(route_content,"APPLY ROUTES"),LV_EVENT_CLICKED,nullptr);
    next_apply();
    assert(!r.snapshot().edit.performance_routes.custom);
    assert(r.snapshot().edit.performance_macros[6]==performance::kNeutral[6]);
    assert(next.requestSoundAction(1).accepted());next_apply();
    assert(r.snapshot().edit.performance_routes==saved_routes.performance_routes);
    assert(next.requestSelectPatch(4).accepted());next_apply();
    assert(r.snapshot().edit.family==1);
    assert(lv_obj_has_state(button(route_content,"APPLY ROUTES"),LV_STATE_DISABLED));
    lv_obj_send_event(button(route_content,"CANCEL / BACK"),LV_EVENT_CLICKED,nullptr);
    preview::pumpFrame(ui);
    route_content=open_routes();
    auto* juno_catalog=widgetAt(route_content,&lv_dropdown_class,521,36);
    assert(juno_catalog && lv_dropdown_get_option_index(juno_catalog,"FILTER / Cutoff")>0);
    assert(lv_dropdown_get_option_index(juno_catalog,"FM / Feedback")<0);
    std::puts("PASS 0.14.1 routing UI, V4 Save/Recall, Compare/Revert, Manual/Applied and MIDI bank override");

    lv_obj_send_event(button(route_content,"CANCEL / BACK"),LV_EVENT_CLICKED,nullptr);
    preview::pumpFrame(ui);
    assert(next.requestParameterFloat("filter.cutoff_hz",2000).accepted());
    next_apply();
    const auto previous_patch=r.snapshot().patch_id;
    auto* advance=button(ui.contentObject(),"NEXT >");
    assert(advance);
    lv_obj_send_event(advance,LV_EVENT_CLICKED,nullptr);
    assert(button(ui.contentObject(),"DISCARD & LOAD"));
    assert(!reboot.busy() && r.snapshot().patch_id==previous_patch);
    lv_obj_send_event(button(ui.contentObject(),"CANCEL"),LV_EVENT_CLICKED,nullptr);
    assert(!button(ui.contentObject(),"DISCARD & LOAD"));
    assert(reboot.unsaved() && r.snapshot().edit.manual[edit::Cutoff]==2000);
    lv_obj_send_event(advance,LV_EVENT_CLICKED,nullptr);
    lv_obj_send_event(button(ui.contentObject(),"DISCARD & LOAD"),LV_EVENT_CLICKED,nullptr);
    assert(reboot.busy());
    next_apply();
    assert(r.snapshot().patch_id!=previous_patch && !reboot.unsaved());
    std::puts("PASS patch selector: cancel preserves edits, confirmed discard changes patch");

    auto* content = ui.contentObject();
    assert(!button(content, "DISCARD & LOAD"));
    const auto click = [](lv_obj_t* object) {
        assert(object);
        lv_obj_send_event(object, LV_EVENT_CLICKED, nullptr);
    };
    const auto open_patch = [&](uint16_t id) {
        selectPatchWithConfirmation(next, ui.navigation(), id, 0, true);
        assert(button(content, "LOAD PATCH"));
    };
    const auto same_patch = r.snapshot().patch_id;
    open_patch(same_patch);
    click(button(content, "LOAD PATCH"));
    const auto changes = r.snapshot().patch_changes;
    const auto before_request = r.snapshot();
    reboot.publish(before_request, false, 0, 0);
    preview::pumpFrame(ui);
    assert(labels(content, "LOADING PATCH...") == 1);
    auto* cancel = button(content, "CANCEL");
    assert(lv_obj_has_state(cancel, LV_STATE_DISABLED));
    click(cancel); // Synthetic click also hits the callback guard.
    assert(labels(content, "LOADING PATCH...") == 1);
    selectPatchWithConfirmation(next, ui.navigation(), 128, 0, true);
    click(button(content, "LOAD PATCH"));
    assert(!next.requestSelectPatch(128).accepted());
    next_apply();
    assert(r.snapshot().patch_id == same_patch && r.snapshot().patch_changes == changes + 1);
    assert(!button(content, "LOAD PATCH"));
    std::puts("PASS popup success, same-patch owner confirmation, CANCEL guard and rapid selection");

    open_patch(same_patch);
    click(button(content, "LOAD PATCH"));
    r.apply(preview::g_sim_tick_ms);
    assert(r.snapshot().patch_state == amy::PatchState::Requested);
    r.commands().panic(); // Owner reports failure without a UI Panic request.
    next_apply();
    assert(labels(content, "PATCH LOAD FAILED; RETRY SAFE PATCH") == 1);
    assert(!lv_obj_has_state(button(content, "CANCEL"), LV_STATE_DISABLED));
    click(button(content, "CANCEL"));
    open_patch(same_patch);
    click(button(content, "LOAD PATCH"));
    preview::pumpFrame(ui);
    assert(labels(content, "LOADING PATCH...") == 1); // Prior failure is stale.
    next_apply();
    assert(!button(content, "LOAD PATCH") && !reboot.patchError());
    std::puts("PASS popup owner failure, reopen and successful retry");

    open_patch(same_patch);
    click(button(content, "LOAD PATCH"));
    preview::pumpFrame(ui, UiBlueprintPerformanceAdapter::kPatchConfirmationTimeoutMs - 1);
    assert(labels(content, "LOADING PATCH...") == 1);
    preview::pumpFrame(ui, 1);
    assert(labels(content, "PATCH LOAD TIMEOUT; CHECK CURRENT PATCH") == 1);
    click(button(content, "CANCEL"));
    assert(!button(content, "LOAD PATCH"));
    next_apply();
    assert(!button(content, "LOAD PATCH")); // Late owner completion cannot resurrect the modal.
    std::puts("PASS popup timeout without model changes and late owner completion");

    open_patch(same_patch);
    click(button(content, "LOAD PATCH"));
    click(button(ui.rootObject(), "PANIC"));
    next_apply();
    assert(!r.snapshot().active_notes);
    assert(labels(content, "PATCH LOAD INTERRUPTED BY PANIC.") == 1);
    click(button(content, "CANCEL"));
    std::puts("PASS global PANIC with popup open");

    preview::pumpFrame(ui);
    lv_mem_monitor_t memory_before{}, memory_after{};
    lv_mem_monitor(&memory_before);
    for (unsigned i = 0; i < 32; ++i) {
        open_patch(same_patch);
        preview::pumpFrame(ui);
        click(button(content, "CANCEL"));
        preview::pumpFrame(ui);
        assert(!button(content, "LOAD PATCH"));
    }
    lv_mem_monitor(&memory_after);
    assert(memory_after.free_size == memory_before.free_size);
    assert(lv_mem_test() == LV_RESULT_OK);
    std::puts("PASS popup repeated open/close (32 cycles), LVGL heap stable");

    testSmk25LegacyUi(r,reboot,next,ui);
    std::puts("PASS 0.14.2 SMK25 native pickup, pad shortcuts, Save As safety and USB-only Stop");
    runSliderRobustnessUnitTests();
    runFxHardening018b1Suite(r, reboot, next, ui);
    assert(lv_mem_test()==LV_RESULT_OK);ui.shutdown();r.end();
    puts("PASS LVGL9.3: all 7 editor tabs, Effects screen, Gate G0 inventory audit, FM/LFO/Mod/FX owner confirmation, dirty guard, Save/Recall/reboot simulation, heap integrity");
}
