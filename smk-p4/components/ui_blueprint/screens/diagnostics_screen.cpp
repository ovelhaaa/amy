#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

namespace {

void setDiagLine(lv_obj_t* lbl, const char* text) {
    if (!lbl || !text) return;
    const char* cur = lv_label_get_text(lbl);
    if (!cur || std::strcmp(cur, text) != 0) lv_label_set_text(lbl, text);
}

class DiagnosticsScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Diagnostics; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        mod_ = nav.subTab(ScreenId::Diagnostics) % 4;
        for (auto& l : lines_) l = nullptr;

        // 1. Left Contextual Sidebar (x = 4..160, y = 4..384)
        lv_obj_t* sidebar = lv_obj_create(parent);
        applyPanelStyle(sidebar, false);
        lv_obj_set_pos(sidebar, 4, 4);
        lv_obj_set_size(sidebar, 156, 380);
        lv_obj_set_style_pad_all(sidebar, 4, 0);
        lv_obj_remove_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);

        createLabel(sidebar, "DIAGNOSTICOS", 4, 2, 140, &lv_font_montserrat_14, palette::kAccentPrimary);

        static constexpr const char* kGroups[4] = {"AUDIO", "MEMORIA", "USB / MIDI", "SISTEMA"};
        struct ModCtx { UiNavigationManager* nav; uint8_t mod; };
        for (uint8_t i = 0; i < 4; ++i) {
            lv_obj_t* b = createActionButton(sidebar, kGroups[i], 2, 28 + i * 48, 144, 42, nullptr, nullptr, mod_ == i, false);
            if (b) {
                attachLvContext<ModCtx>(b, {&nav, i}, [](lv_event_t* e) {
                    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                    auto* c = static_cast<ModCtx*>(lv_event_get_user_data(e));
                    if (c && c->nav) c->nav->requestNavigateTo(ScreenId::Diagnostics, c->mod);
                }, LV_EVENT_CLICKED);
            }
        }

        lv_obj_t* pf_btn = createActionButton(sidebar, "TOCAR >", 2, 336, 144, 34, nullptr, nullptr);
        if (pf_btn) {
            attachLvContext<UiNavigationManager*>(pf_btn, &nav, [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* n = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
                if (n) n->requestReturnToPerformance();
            }, LV_EVENT_CLICKED);
        }

        // 2. Right Contextual Telemetry Panel (x = 166..794, y = 4..384)
        lv_obj_t* card = lv_obj_create(parent);
        applyPanelStyle(card, true);
        lv_obj_set_pos(card, 166, 4);
        lv_obj_set_size(card, 628, 380);
        lv_obj_set_style_pad_all(card, 8, 0);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        static constexpr const char* kTitles[4] = {
            "TELEMETRIA DE AUDIO & AMY DSP",
            "MEMORIA LVGL, HEAP INTERNO & PSRAM",
            "USB MIDI HOST & COMUNICACAO",
            "SISTEMA, VERSAO & QUALIFICACAO"
        };
        createLabel(card, kTitles[mod_], 6, 4, 460, &lv_font_montserrat_14, palette::kAccentPrimary);
        createStatusBadge(card, backend.isDemoMode() ? FeatureStatus::Demo : FeatureStatus::Available, 536, 4);

        for (unsigned i = 0; i < 8; ++i) {
            lines_[i] = createLabel(
                card, "", 6, 40 + int32_t(i) * 38, 608,
                &lv_font_montserrat_14, i == 7 ? palette::kAccentPrimary : palette::kTextPrimary);
        }

        refresh(backend);
        return true;
    }

    void refresh(IUiBackend& backend) override {
        if (!lines_[0]) return;
        const auto& d = backend.model().diagnostics();
        const bool demo = backend.isDemoMode();
        char b[8][96]{};

        switch (mod_) {
            case 0: // AUDIO
                std::snprintf(b[0], sizeof(b[0]), "AMY STATUS: %s", d.amy_status);
                std::snprintf(b[1], sizeof(b[1]), "I2S DAC: %s", d.i2s_status);
                std::snprintf(b[2], sizeof(b[2]), "VOZES ATIVAS: %u / %u", d.active_voices, d.max_voices);
                if (demo) {
                    std::snprintf(b[3], sizeof(b[3]), "RENDER AVG / MAX: N/A (DEMO) | BUDGET: %lu us",
                                  static_cast<unsigned long>(d.budget_us));
                    std::snprintf(b[4], sizeof(b[4]), "DEADLINE MISSES: N/A (DEMO)");
                } else {
                    std::snprintf(b[3], sizeof(b[3]), "RENDER AVG / MAX: %lu / %lu us (BUDGET: %lu us)",
                                  static_cast<unsigned long>(d.render_avg_us),
                                  static_cast<unsigned long>(d.render_max_us),
                                  static_cast<unsigned long>(d.budget_us));
                    std::snprintf(b[4], sizeof(b[4]), "DEADLINE MISSES: %lu",
                                  static_cast<unsigned long>(d.deadline_misses));
                }
                std::snprintf(b[5], sizeof(b[5]), "DMA UNDERRUNS: N/A (MEDICAO DE BANCADA PENDENTE)");
                std::snprintf(b[6], sizeof(b[6]), "AUDIO LIVE: %s", d.audio_live ? "SIM (RUN)" : "NAO (OFF/SIM)");
                std::snprintf(b[7], sizeof(b[7]), "PATCH ATIVO: #%03u %s",
                              unsigned(backend.model().activePatch().id),
                              backend.model().activePatch().name);
                break;

            case 1: // MEMORIA
                if (!demo && !d.memory_valid) {
                    std::snprintf(b[0], sizeof(b[0]), "LVGL POOL FREE: N/A (AGUARDANDO AMOSTRA)");
                    std::snprintf(b[1], sizeof(b[1]), "LVGL LARGEST BLOCK: N/A");
                    std::snprintf(b[2], sizeof(b[2]), "LVGL PEAK USED / FRAG: N/A");
                    std::snprintf(b[3], sizeof(b[3]), "HEAP INTERNO FREE: N/A");
                    std::snprintf(b[4], sizeof(b[4]), "PSRAM FREE: N/A");
                } else {
                    std::snprintf(b[0], sizeof(b[0]), "LVGL POOL FREE: %lu bytes",
                                  static_cast<unsigned long>(d.lvgl_free_bytes));
                    std::snprintf(b[1], sizeof(b[1]), "LVGL LARGEST BLOCK: %lu bytes",
                                  static_cast<unsigned long>(d.lvgl_largest_block));
                    std::snprintf(b[2], sizeof(b[2]), "LVGL PEAK USED: %lu bytes | FRAGMENTACAO: %u%%",
                                  static_cast<unsigned long>(d.lvgl_peak_used), unsigned(d.lvgl_frag_pct));
                    if (demo) {
                        std::snprintf(b[3], sizeof(b[3]), "HEAP INTERNO FREE: N/A (HOST DEMO)");
                        std::snprintf(b[4], sizeof(b[4]), "PSRAM FREE: N/A (HOST DEMO)");
                    } else {
                        std::snprintf(b[3], sizeof(b[3]), "HEAP INTERNO FREE: %lu bytes",
                                      static_cast<unsigned long>(d.free_internal_ram));
                        std::snprintf(b[4], sizeof(b[4]), "PSRAM FREE: %lu bytes",
                                      static_cast<unsigned long>(d.free_psram));
                    }
                }
                std::snprintf(b[5], sizeof(b[5]), "STACK HIGH-WATER: N/A (CONSULTAR SERIAL 5s)");
                std::snprintf(b[6], sizeof(b[6]), "ARQUITETURA: PAINEL UNICO ATIVO POR VEZ");
                std::snprintf(b[7], sizeof(b[7]), "ORCAMENTO LVGL: ~128 KiB TLSF");
                break;

            case 2: // USB / MIDI
                std::snprintf(b[0], sizeof(b[0]), "USB MIDI HOST: %s", d.usb_status);
                std::snprintf(b[1], sizeof(b[1]), "DISPOSITIVO CONECTADO: %s", d.midi_connected ? "SIM" : "NAO");
                std::snprintf(b[2], sizeof(b[2]), "ERRO MIDI / OVERFLOW: %s", backend.hasMidiError() ? "SIM (ERRO)" : "NENHUM");
                std::snprintf(b[3], sizeof(b[3]), "PERFIL ATIVO: %s",
                              backend.hasMidiPerformance() ? backend.midiProfileName() : "N/A (DEMO)");
                std::snprintf(b[4], sizeof(b[4]), "PARSE ERRORS / DROPS: N/A (CONTADOR NO SERIAL 5s)");
                std::snprintf(b[5], sizeof(b[5]), "HOT-PLUG PANIC: ATIVO NO FIRMWARE");
                std::snprintf(b[6], sizeof(b[6]), "ULTIMO CMD: %s (%s)",
                              commandStateLabel(backend.lastCommandState()),
                              backend.lastCommandParameterId() ? backend.lastCommandParameterId() : "--");
                std::snprintf(b[7], sizeof(b[7]), "NOTICE: %s", backend.model().lastNotice());
                break;

            case 3: // SISTEMA / ABOUT P4my
                std::snprintf(b[0], sizeof(b[0]), "P4my - Digital Performance Synth | Powered by AMY");
                std::snprintf(b[1], sizeof(b[1]), "VERSAO FW: %s | ESP32-P4 (800x480)", d.firmware_version);
                std::snprintf(b[2], sizeof(b[2]), "UPTIME: %lu ms | TROCAS DE TELA: %lu",
                              static_cast<unsigned long>(d.uptime_ms),
                              static_cast<unsigned long>(d.screen_switches));
                std::snprintf(b[3], sizeof(b[3]), "PARAMETROS: %u | VOZES MAX: %u",
                              unsigned(ParameterRegistry::count()), unsigned(d.max_voices));
                std::snprintf(b[4], sizeof(b[4]), "CREDITOS: AMY Synthesizer (shorepine) | LVGL 9.3.0");
                std::snprintf(b[5], sizeof(b[5]), "BUILD: ESP-IDF 5.5.2 | GUITION JC-ESP32P4-M3");
                std::snprintf(b[6], sizeof(b[6]), "BACKEND: %s | CONECTADO: %s",
                              backend.backendName(), backend.isConnected() ? "SIM" : "NAO");
                std::snprintf(b[7], sizeof(b[7]), "QUALIFICACAO FISICA: PENDENTE (HOST VALIDADO)");
                break;
        }

        for (unsigned i = 0; i < 8; ++i) {
            setDiagLine(lines_[i], b[i]);
        }
    }

private:
    uint8_t mod_ = 0;
    lv_obj_t* lines_[8]{};
};

} // namespace

ScreenView* createDiagnosticsScreen() {
    return new DiagnosticsScreen();
}

} // namespace smk::p4::ui
