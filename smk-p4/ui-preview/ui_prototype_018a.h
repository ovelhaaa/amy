#pragma once

#include "ui_backend.h"

#include <lvgl.h>

#include <array>
#include <cstdint>

namespace smk::p4::ui::proto018a {

enum class AreaId : uint8_t {
    Tocar = 0,
    Timbres = 1,
    Ritmo = 2,
    Fx = 3,
    Mais = 4,
    Count = 5
};

enum class HeaderVariant : uint8_t {
    CompactOverlay = 0,   // Variant A: 800x44 single row + Harmony/BPM overlays
    ExpandedTwoRow = 1    // Variant B: 800x68 dual row with always-visible Harmony context
};

enum class TocarMode : uint8_t {
    Macros1To4 = 0,
    Macros5To8 = 1,
    ScenesQuick = 2,
    VirtualKeys = 3,
    SceneChainProposal = 4,
    Count = 5
};

enum class TimbresModule : uint8_t {
    Biblioteca = 0,
    Sintese = 1,
    Camadas = 2,
    Modulacao = 3,
    Count = 4
};

enum class RitmoModule : uint8_t {
    Sequenciador = 0,
    Arpejador = 1,
    Padroes = 2,
    Bateria = 3,
    Count = 4
};

enum class SeqViewMode : uint8_t {
    Play = 0,
    Edit = 1
};

enum class FxModule : uint8_t {
    Delay = 0,
    Reverb = 1,
    Chorus = 2,
    Master = 3,
    Count = 4
};

enum class MaisModule : uint8_t {
    MidiSmk25 = 0,
    Preferencias = 1,
    Diagnosticos = 2,
    Count = 3
};

enum class SynthSubTab : uint8_t {
    Geral = 0,
    Motor = 1,
    FiltroEnv = 2,
    Routing = 3,
    Count = 4
};

enum class OverlayKind : uint8_t {
    None = 0,
    TempoBpm = 1,
    HarmonyContext = 2,
    SceneSelectiveModal = 3
};

struct LvglHeapStats {
    uint32_t total_size = 0U;
    uint32_t free_bytes = 0U;
    uint32_t free_biggest_size = 0U;
    uint32_t used_pct = 0U;
    uint32_t max_used = 0U;
    uint32_t frag_pct = 0U;
};

struct ShellMetrics {
    uint32_t shell_builds = 0U;
    uint32_t header_rebuilds = 0U;
    uint32_t sidebar_rebuilds = 0U;
    uint32_t module_mounts = 0U;
    uint32_t inplace_updates = 0U;
    uint32_t live_objects = 0U;
    uint32_t header_objects = 0U;
    uint32_t footer_objects = 0U;
    uint32_t ui_note_releases = 0U;
};

struct LayoutBox {
    int32_t x = 0;
    int32_t y = 0;
    int32_t w = 0;
    int32_t h = 0;
    bool visible = false;
};

class UiShell018A {
public:
    explicit UiShell018A(IUiBackend& backend);
    ~UiShell018A();

    UiShell018A(const UiShell018A&) = delete;
    UiShell018A& operator=(const UiShell018A&) = delete;

    bool begin(lv_obj_t* root_screen = nullptr);
    void teardown();
    void service(uint32_t now_ms);

    // Navigation API preserving module/sub-tab history per session.
    void selectArea(AreaId area);
    void selectTocarMode(TocarMode mode);
    void selectTimbresModule(TimbresModule mod);
    void selectRitmoModule(RitmoModule mod);
    void selectSeqViewMode(SeqViewMode mode);
    void selectFxModule(FxModule mod);
    void selectMaisModule(MaisModule mod);
    void selectSynthSubTab(SynthSubTab tab);
    void setHeaderVariant(HeaderVariant variant);

    void openOverlay(OverlayKind overlay, uint8_t scene_slot = 0U);
    void closeOverlay();

    // Contextual fine-adjustment & state helpers bound to IUiBackend.
    void selectParameter(const char* id);
    const char* selectedParameterId() const { return selected_param_id_; }
    void stepSelectedParameter(int32_t steps);
    void selectSceneSlot(uint8_t slot_index);
    uint8_t selectedSceneSlot() const { return selected_scene_slot_; }
    void selectSeqStep(uint8_t step_index);
    uint8_t selectedSeqStep() const { return selected_seq_step_; }

    // State getters.
    AreaId activeArea() const { return active_area_; }
    HeaderVariant headerVariant() const { return header_variant_; }
    TocarMode tocarMode() const { return tocar_mode_; }
    TimbresModule timbresModule() const { return timbres_module_; }
    RitmoModule ritmoModule() const { return ritmo_module_; }
    SeqViewMode seqViewMode() const { return seq_view_mode_; }
    FxModule fxModule() const { return fx_module_; }
    MaisModule maisModule() const { return mais_module_; }
    SynthSubTab synthSubTab() const { return synth_sub_tab_; }
    OverlayKind activeOverlay() const { return active_overlay_; }

    // Instrumentation & layout verification.
    const ShellMetrics& metrics() const { return metrics_; }
    uint32_t countAllObjects() const;
    static LvglHeapStats readLvglHeapStats();

    LayoutBox headerBox() const;
    LayoutBox sidebarBox() const;
    LayoutBox panelBox() const;
    LayoutBox footerBox() const;
    LayoutBox panicBox() const;

    bool headerTargetsDisjoint(int32_t* min_gap_out = nullptr) const;
    bool panicAccessibleInOverlay() const;

    // Interactive widget handles (real LVGL objects for tests & visual focus).
    lv_obj_t* root() const { return root_; }
    lv_obj_t* header() const { return header_; }
    lv_obj_t* sidebar() const { return sidebar_; }
    lv_obj_t* panel() const { return panel_; }
    lv_obj_t* footer() const { return footer_; }
    lv_obj_t* overlayBackdrop() const { return overlay_backdrop_; }
    lv_obj_t* overlayCard() const { return overlay_card_; }

    lv_obj_t* footerButton(AreaId area) const;
    lv_obj_t* sidebarButton(uint8_t index) const;
    lv_obj_t* panicButton() const { return hdr_panic_btn_; }
    lv_obj_t* qButton() const { return hdr_q_btn_; }
    lv_obj_t* chButton() const { return hdr_ch_btn_; }
    lv_obj_t* bpmButton() const { return hdr_bpm_btn_; }
    lv_obj_t* harmButton() const { return hdr_harm_btn_; }

private:
    enum class ControlTag : uint8_t {
        FooterArea = 1,
        SidebarModule = 2,
        TocarModeBtn = 3,
        HeaderBpmBtn = 4,
        HeaderHarmBtn = 5,
        HeaderQBtn = 6,
        HeaderChBtn = 7,
        HeaderPanicBtn = 8,
        HeaderVariantBtn = 9,
        CloseOverlayBtn = 10,
        ParamSlider = 11,
        ParamCardSelect = 12,
        ParamFineMinus = 13,
        ParamFinePlus = 14,
        ParamToggle = 15,
        ParamEnumPrev = 16,
        ParamEnumNext = 17,
        SeqViewModeBtn = 18,
        SeqTransportBtn = 19,
        SeqStepSelectBtn = 20,
        SeqStepToggleBtn = 21,
        SeqStepRecBtn = 22,
        SeqLiveTransposeBtn = 23,
        SeqScaleQuantBtn = 24,
        SeqDivisionBtn = 25,
        SeqStepFieldBtn = 26,
        ArpTransportBtn = 27,
        ArpLatchClearBtn = 28,
        PatternSlotBtn = 29,
        PatternLoadBtn = 30,
        PatternSaveBtn = 31,
        PatternDeleteBtn = 32,
        SceneSlotBtn = 33,
        SceneRecallBtn = 34,
        SceneSaveBtn = 35,
        SceneModalBtn = 36,
        SceneDomainBtn = 37,
        SceneClearBtn = 38,
        PerfLockBtn = 39,
        PatchBankSelectBtn = 40,
        PatchDiscardConfirmBtn = 41,
        SynthSubTabBtn = 42,
        BpmDeltaBtn = 43,
        BpmTapBtn = 44,
        BpmSaveBtn = 45,
        OverlayHarmApplyTouchBtn = 46,
        OverlayHarmPresetBtn = 47,
        OverlayHarmScopeCycleBtn = 48,
        MacroEditBankBtn = 49,
        MidiActionBtn = 50,
        OverlayQBtn = 51,
        OverlayChBtn = 52
    };

    struct ControlBinding {
        UiShell018A* self = nullptr;
        ControlTag tag = ControlTag::FooterArea;
        int32_t arg0 = 0;
        int32_t arg1 = 0;
        const char* param_id = nullptr;
    };

    struct TrackedParamWidget {
        const char* id = nullptr;
        lv_obj_t* card = nullptr;
        lv_obj_t* slider = nullptr;
        lv_obj_t* value_label = nullptr;
        lv_obj_t* status_label = nullptr;
        lv_obj_t* toggle_btn = nullptr;
        lv_obj_t* toggle_label = nullptr;
        bool sync_locked_visual = false;
    };

    static void onButtonClicked(lv_event_t* event);
    static void onSliderChanged(lv_event_t* event);
    static void onSliderReleased(lv_event_t* event);
    static void onControlDeleted(lv_event_t* event);
    static uint32_t countSubtree(const lv_obj_t* obj);

    ControlBinding* makeBinding(ControlTag tag, int32_t arg0 = 0, int32_t arg1 = 0, const char* param_id = nullptr);
    void attachClick(lv_obj_t* obj, ControlTag tag, int32_t arg0 = 0, int32_t arg1 = 0, const char* param_id = nullptr);
    void attachValueChanged(lv_obj_t* slider, const char* param_id);

    void buildShell();
    void rebuildHeader();
    void buildFooter();
    void updateFooterVisuals();
    void updateGeometryForArea();
    void rebuildSidebar();
    void rebuildActivePanel();
    void rebuildOverlay();
    void refreshDynamicLabels();
    void refreshHeaderDynamic();
    void refreshParamWidgets();

    // Panel builders (single active module in `panel_`)
    void buildTocarPanel();
    void buildTocarMacrosGrid(uint8_t base_macro, lv_obj_t* parent);
    void buildTocarScenesPanel(lv_obj_t* parent);
    void buildTocarVirtualKeysPanel(lv_obj_t* parent);
    void buildTocarChainProposalPanel(lv_obj_t* parent);

    void buildTimbresPanel();
    void buildTimbresBiblioteca();
    void buildTimbresSintese();
    void buildTimbresCamadas();
    void buildTimbresModulacao();

    void buildRitmoPanel();
    void buildRitmoSequencer();
    void buildRitmoArpeggiator();
    void buildRitmoPatterns();
    void buildRitmoDrums();

    void buildFxPanel();
    void buildFxDelay();
    void buildFxReverb();
    void buildFxChorus();
    void buildFxMaster();

    void buildMaisPanel();
    void buildMaisMidi();
    void buildMaisPreferences();
    void buildMaisDiagnostics();

    // Shared UI builders (no heap churn outside LVGL tree for active module)
    lv_obj_t* addSectionCard(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* title, const char* badge = nullptr, uint32_t badge_color = 0x334155U);
    void addParamSliderCard(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* id, const char* custom_title = nullptr, bool force_readonly = false, const char* readonly_reason = nullptr);
    void addParamSelectorCard(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* id, const char* custom_title = nullptr);
    void addParamToggleCard(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* id, const char* custom_title = nullptr);
    void addParamContextInspector(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h);
    void addUnsupportedCard(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* title, const char* badge, const char* detail);

    void handleBindingAction(const ControlBinding& binding);
    bool isPatchBankLockedOrWait() const;
    void maybeReleaseVirtualKeys(AreaId old_area, TocarMode old_mode);

    IUiBackend& backend_;
    bool initialized_ = false;
    bool owned_root_ = false;
    uint32_t last_seen_revision_ = 0U;
    uint32_t last_service_ms_ = 0U;

    AreaId active_area_ = AreaId::Tocar;
    HeaderVariant header_variant_ = HeaderVariant::CompactOverlay;
    TocarMode tocar_mode_ = TocarMode::Macros1To4;
    TimbresModule timbres_module_ = TimbresModule::Biblioteca;
    RitmoModule ritmo_module_ = RitmoModule::Sequenciador;
    SeqViewMode seq_view_mode_ = SeqViewMode::Play;
    FxModule fx_module_ = FxModule::Delay;
    MaisModule mais_module_ = MaisModule::MidiSmk25;
    SynthSubTab synth_sub_tab_ = SynthSubTab::Geral;
    OverlayKind active_overlay_ = OverlayKind::None;

    uint8_t selected_scene_slot_ = 0U;
    uint8_t selected_seq_step_ = 0U;
    uint8_t selected_pattern_slot_ = 0U;
    uint8_t patch_bank_page_ = 0U;
    uint16_t pending_discard_patch_ = 0xFFFFU;
    uint8_t selective_capture_mask_ = 0x1FU;
    uint8_t pending_macro_slot_ = 0U;
    const char* selected_param_id_ = "perf.macro.1";
    const char* status_flash_text_ = nullptr;
    uint32_t last_slider_drag_send_ms_ = 0U;
    bool dragging_slider_ = false;

    ShellMetrics metrics_{};

    // Shell containers (built once on begin(), reused across navigation)
    lv_obj_t* root_ = nullptr;
    lv_obj_t* header_ = nullptr;
    lv_obj_t* sidebar_ = nullptr;
    lv_obj_t* panel_ = nullptr;
    lv_obj_t* footer_ = nullptr;
    lv_obj_t* overlay_backdrop_ = nullptr;
    lv_obj_t* overlay_card_ = nullptr;

    // Header interactive targets & live labels
    lv_obj_t* hdr_mode_badge_ = nullptr;
    lv_obj_t* hdr_mode_label_ = nullptr;
    lv_obj_t* hdr_patch_label_ = nullptr;
    lv_obj_t* hdr_scene_label_ = nullptr;
    lv_obj_t* hdr_bpm_btn_ = nullptr;
    lv_obj_t* hdr_bpm_label_ = nullptr;
    lv_obj_t* hdr_harm_btn_ = nullptr;
    lv_obj_t* hdr_harm_label_ = nullptr;
    lv_obj_t* hdr_q_btn_ = nullptr;
    lv_obj_t* hdr_q_label_ = nullptr;
    lv_obj_t* hdr_ch_btn_ = nullptr;
    lv_obj_t* hdr_ch_label_ = nullptr;
    lv_obj_t* hdr_panic_btn_ = nullptr;
    lv_obj_t* hdr_variant_btn_ = nullptr;
    lv_obj_t* hdr_row2_info_label_ = nullptr;

    // Footer & sidebar fixed button arrays
    std::array<lv_obj_t*, 5> footer_btns_{};
    std::array<lv_obj_t*, 4> sidebar_btns_{};

    // Dynamic widgets rebuilt per active module (small fixed capacity)
    static constexpr uint8_t kMaxTrackedParams = 12U;
    std::array<TrackedParamWidget, kMaxTrackedParams> tracked_params_{};
    uint8_t tracked_param_count_ = 0U;

    // Context inspector labels (optional on modules that expose fine edit card)
    lv_obj_t* inspector_title_ = nullptr;
    lv_obj_t* inspector_values_ = nullptr;
    lv_obj_t* inspector_ack_ = nullptr;
    lv_obj_t* inspector_minus_btn_ = nullptr;
    lv_obj_t* inspector_plus_btn_ = nullptr;

    // In-place dynamic labels on Tocar / Status / Overlay
    lv_obj_t* tocar_arp_pill_ = nullptr;
    lv_obj_t* tocar_seq_pill_ = nullptr;
    lv_obj_t* tocar_fx_pill_ = nullptr;
    lv_obj_t* tocar_harm_pill_ = nullptr;
    lv_obj_t* tocar_vkbd_ = nullptr;
    lv_obj_t* tocar_vkbd_info_ = nullptr;
    lv_obj_t* overlay_status_label_ = nullptr;
    lv_obj_t* overlay_q_btn_ = nullptr;
    lv_obj_t* overlay_ch_btn_ = nullptr;
    lv_obj_t* dynamic_feedback_label_ = nullptr;
    lv_obj_t* delay_status_label_ = nullptr;
    lv_obj_t* diag_live_label_ = nullptr;

    // Static backing buffers for persistent header labels (eliminates lv_label heap churn)
    char hdr_patch_buf_[48]{};
    char hdr_scene_buf_[40]{};
    char hdr_bpm_buf_[24]{};
    char hdr_harm_buf_[32]{};
    char hdr_row2_buf_[72]{};
};

}  // namespace smk::p4::ui::proto018a
