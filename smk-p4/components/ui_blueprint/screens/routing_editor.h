#pragma once

#include "ui_widgets.h"
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace smk::p4::ui {

class RoutingEditor {
public:
    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) {
        backend_ = &backend;
        nav_ = &nav;
        backend.routingSnapshot(original_);
        draft_ = original_.routes;

        createLabel(parent, "MACRO ROUTING EDITOR", 12, 8, 400, &lv_font_montserrat_14, palette::kAccentPrimary);
        position_ = createLabel(parent, "", 420, 8, 355);
        macro_ = dropdown(parent, 12, 36, 245,
            "M1 CHARACTER\nM2 BRIGHTNESS\nM3 MOTION\nM4 SHAPE\nM5 ATTACK\nM6 RELEASE\nM7 SPACE\nM8 EDGE");
        slot_ = dropdown(parent, 267, 36, 244, "");
        destination_ = dropdown(parent, 521, 36, 267, "");
        buildCatalog();
        domain_ = createLabel(parent, "", 12, 88, 770);
        values_ = createLabel(parent, "", 12, 109, 770, &lv_font_montserrat_14, palette::kAccentPrimary);
        constexpr const char* fields[]{"MINIMUM", "MAXIMUM", "DEPTH"};
        for (unsigned i = 0; i < 3; ++i) {
            const int x = 12 + 180 * int(i);
            createLabel(parent, fields[i], x, 130, 168);
            numeric_[i] = lv_textarea_create(parent);
            lv_obj_set_pos(numeric_[i], x, 146);
            lv_obj_set_size(numeric_[i], 166, 43);
            lv_textarea_set_one_line(numeric_[i], true);
            lv_textarea_set_max_length(numeric_[i], 18);
            lv_textarea_set_accepted_chars(numeric_[i], "0123456789.-+eE");
            lv_obj_add_event_cb(numeric_[i], dispatch, LV_EVENT_CLICKED, this);
        }
        curve_ = dropdown(parent, 12, 206, 184, "Linear\nExponential\nLogarithmic\nS-Curve");
        polarity_ = dropdown(parent, 205, 206, 172, "Bipolar\nPositive\nNegative");
        enabled_ = checkbox(parent, "Enabled", 400);
        inverted_ = checkbox(parent, "Invert", 572);

        button(parent, "DELETE ROUTE", 12, 266, 142, Action::Delete);
        button(parent, "RESTORE SLOT", 165, 266, 142, Action::RestoreSlot);
        button(parent, "RESET ROUTES", 318, 266, 142, Action::RestoreMacro);
        button(parent, "RESET POSITION", 471, 266, 145, Action::ResetPosition);
        button(parent, "RESET BOTH", 627, 266, 161, Action::ResetBoth);
        status_ = createLabel(parent, "SELECT A DESTINATION TO ADD OR REPLACE A ROUTE.", 12, 326, 764);
        button(parent, "CANCEL / BACK", 12, 350, 190, Action::Close);
        apply_ = button(parent, "APPLY ROUTES", 606, 350, 182, Action::Apply);

        keyboard_ = lv_keyboard_create(parent);
        lv_obj_set_align(keyboard_, LV_ALIGN_TOP_LEFT);
        lv_obj_set_pos(keyboard_, 12, 193);
        lv_obj_set_size(keyboard_, 552, 154);
        lv_keyboard_set_mode(keyboard_, LV_KEYBOARD_MODE_NUMBER);
        lv_obj_add_event_cb(keyboard_, dispatch, LV_EVENT_READY, this);
        lv_obj_add_event_cb(keyboard_, dispatch, LV_EVENT_CANCEL, this);
        lv_obj_add_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
        loadSlot();
        refresh();
        return true;
    }

    void refresh() {
        showValues();
        RoutingSnapshot current;
        const bool readable = backend_->routingSnapshot(current);
        if (pending_) {
            const auto ack = backend_->routingResult();
            if (ack.token == pending_ && ack.state != UiCommandState::Requested) {
                pending_ = 0;
                if (ack.state == UiCommandState::Confirmed && readable) {
                    original_ = current;
                    draft_ = current.routes;
                    reset_positions_ = 0;
                    loadSlot();
                    setStatus("ROUTES APPLIED. SAVE PRESET TO KEEP.");
                } else {
                    setStatus("ROUTES NOT CONFIRMED.");
                }
            }
        }
        const bool stale = !readable || current.patch != original_.patch ||
            current.generation != original_.generation || current.epoch != original_.epoch ||
            current.routes != original_.routes;
        if (stale && !pending_) setStatus("PATCH OR ROUTES CHANGED. REOPEN EDITOR.");
        else if (current.compare && !pending_) setStatus("EXIT COMPARE TO APPLY ROUTES.");
        else if (!pending_ && std::strcmp(lv_label_get_text(status_), "EXIT COMPARE TO APPLY ROUTES.") == 0)
            setStatus("SELECT A DESTINATION TO ADD OR REPLACE A ROUTE.");
        if (stale || current.compare || pending_) lv_obj_add_state(apply_, LV_STATE_DISABLED);
        else lv_obj_remove_state(apply_, LV_STATE_DISABLED);
        if (readable && shown_position_ != current.positions[macro_index_]) {
            shown_position_ = current.positions[macro_index_];
            char text[40];
            std::snprintf(text, sizeof(text), "POSITION %d / 127", shown_position_);
            lv_label_set_text(position_, text);
        }
    }

private:
    using RoutingSnapshot = IUiBackend::RoutingSnapshot;
    enum class Action { Delete, RestoreSlot, RestoreMacro, ResetPosition, ResetBoth, Close, Apply };
    struct ButtonContext { RoutingEditor* editor; Action action; };

    static void dispatch(lv_event_t* event) {
        auto* self = static_cast<RoutingEditor*>(lv_event_get_user_data(event));
        auto* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
        self->handle(target);
    }

    lv_obj_t* dropdown(lv_obj_t* parent, int x, int y, int width, const char* options) {
        auto* widget = lv_dropdown_create(parent);
        lv_obj_set_pos(widget, x, y);
        lv_obj_set_size(widget, width, 43);
        lv_dropdown_set_options(widget, options);
        lv_obj_set_height(lv_dropdown_get_list(widget), LV_SIZE_CONTENT);
        lv_obj_set_style_max_height(lv_dropdown_get_list(widget), 246, 0);
        lv_obj_add_event_cb(widget, dispatch, LV_EVENT_VALUE_CHANGED, this);
        return widget;
    }

    lv_obj_t* checkbox(lv_obj_t* parent, const char* text, int x) {
        auto* widget = lv_checkbox_create(parent);
        lv_checkbox_set_text(widget, text);
        lv_obj_set_pos(widget, x, 208);
        lv_obj_set_size(widget, 155, 42);
        lv_obj_set_style_bg_color(widget, lv_color_hex(palette::kAccentPrimary), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(widget, lv_color_hex(palette::kBackground), LV_PART_INDICATOR | LV_STATE_CHECKED);
        return widget;
    }

    lv_obj_t* button(lv_obj_t* parent, const char* text, int x, int y, int width, Action action) {
        auto* widget = createActionButton(parent, text, x, y, width, 42, nullptr, nullptr);
        attachLvContext<ButtonContext>(widget, {this, action}, [](lv_event_t* event) {
            auto* context = static_cast<ButtonContext*>(lv_event_get_user_data(event));
            context->editor->act(context->action);
        }, LV_EVENT_CLICKED);
        return widget;
    }

    void buildCatalog() {
        char options[2400] = "SELECT DESTINATION";
        size_t used = std::strlen(options);
        constexpr const char* groups[]{"OSC", "FILTER", "AMP", "ENV", "FM", "LFO", "MOD", "FX"};
        for (const auto* group : groups) {
            const auto length = std::strlen(group);
            for (unsigned id = 0; id < destinations_.size(); ++id) {
                const auto entry = backend_->routingDestination(id);
                if (!entry.available || !entry.name || std::strncmp(entry.name, group, length) != 0 ||
                    entry.name[length] != ' ') continue;
                const size_t name_length = std::strlen(entry.name);
                if (used + name_length + 2 > sizeof(options)) continue;
                options[used++] = '\n';
                std::memcpy(options + used, entry.name, name_length + 1);
                used += name_length;
                destinations_[destination_count_++] = uint16_t(id);
            }
        }
        lv_dropdown_set_options(destination_, options);
    }

    void setStatus(const char* text) {
        if (std::strcmp(lv_label_get_text(status_), text) != 0) lv_label_set_text(status_, text);
    }

    performance::Route& route() { return draft_[macro_index_][slot_index_]; }

    void updateSlotNames() {
        char options[320]{};
        size_t used = 0;
        for (unsigned i = 0; i < performance::kRoutesPerMacro; ++i) {
            const auto& entry = draft_[macro_index_][i];
            const auto meta = backend_->routingDestination(entry.destination);
            const char* name = entry == performance::Route{} ? "EMPTY" : meta.name;
            used += size_t(std::snprintf(options + used, sizeof(options) - used, "%s%u  %s",
                i ? "\n" : "", i + 1, name ? name : "UNAVAILABLE"));
        }
        lv_dropdown_set_options(slot_, options);
        lv_dropdown_set_selected(slot_, slot_index_);
    }

    void loadSlot() {
        updateSlotNames();
        const auto& entry = route();
        const bool empty = entry == performance::Route{};
        const auto meta = backend_->routingDestination(entry.destination);
        unsigned selection = 0;
        if (!empty) {
            for (unsigned i = 0; i < destination_count_; ++i)
                if (destinations_[i] == entry.destination) selection = i + 1;
        }
        lv_dropdown_set_selected(destination_, selection);
        const char* family = entry.families == performance::Both ? "JUNO / DX7" :
            entry.families == performance::Juno ? "JUNO" : "DX7";
        char text[140];
        if (empty) std::snprintf(text, sizeof(text), "EMPTY SLOT");
        else std::snprintf(text, sizeof(text), "%s   %g to %g %s   |   %s%s",
            meta.name ? meta.name : "UNAVAILABLE", double(meta.minimum), double(meta.maximum),
            meta.unit ? meta.unit : "", family, selection ? "" : "   |   UNAVAILABLE FOR THIS PATCH");
        lv_label_set_text(domain_, text);
        const float values[]{entry.minimum, entry.maximum, entry.depth};
        for (unsigned i = 0; i < 3; ++i) {
            std::snprintf(text, sizeof(text), "%.7g", double(values[i]));
            lv_textarea_set_text(numeric_[i], empty ? "" : text);
        }
        lv_dropdown_set_selected(curve_, unsigned(entry.curve));
        lv_dropdown_set_selected(polarity_, unsigned(entry.polarity));
        lv_obj_set_state(enabled_, LV_STATE_CHECKED, entry.enabled);
        lv_obj_set_state(inverted_, LV_STATE_CHECKED, entry.inverted);
        for (auto* widget : {numeric_[0], numeric_[1], numeric_[2], curve_, polarity_, enabled_, inverted_})
            lv_obj_set_state(widget, LV_STATE_DISABLED, !selection);
        showValues();
    }

    void showValues() {
        IUiBackend::RoutingValues values;
        char text[180]{};
        if (!(route() == performance::Route{}) && backend_->routingValues(route().destination, values)) {
            char sources[40]{};
            size_t used = 0;
            for (unsigned m = 0; m < performance::kMacroCount; ++m)
                if (values.sources & (1u << m))
                    used += size_t(std::snprintf(sources + used, sizeof(sources) - used, " M%u", m + 1));
            std::snprintf(text, sizeof(text), "MANUAL %g   APPLIED %g%s%s%s%s",
                double(values.manual), double(values.applied), values.legacy ? "   LEGACY" : "",
                sources, values.compare ? "   COMPARE" : "", values.bypass ? "   FX BYPASS" : "");
        }
        if (std::strcmp(lv_label_get_text(values_), text) != 0) lv_label_set_text(values_, text);
    }

    bool storeSlot() {
        if (route() == performance::Route{}) return true;
        if (!backend_->routingDestination(route().destination).available) return true;
        auto value = route();
        float* fields[]{&value.minimum, &value.maximum, &value.depth};
        for (unsigned i = 0; i < 3; ++i) {
            const char* text = lv_textarea_get_text(numeric_[i]);
            char original[24];
            std::snprintf(original, sizeof(original), "%.7g", double(*fields[i]));
            if (std::strcmp(text, original) == 0) continue;
            char* end = nullptr;
            errno = 0;
            const float parsed = std::strtof(text, &end);
            if (end == text || *end || errno == ERANGE || !std::isfinite(parsed)) {
                setStatus("ENTER FINITE NUMBERS FOR MINIMUM, MAXIMUM AND DEPTH.");
                return false;
            }
            *fields[i] = parsed;
        }
        value.curve = performance::Curve(lv_dropdown_get_selected(curve_));
        value.polarity = performance::Polarity(lv_dropdown_get_selected(polarity_));
        value.enabled = lv_obj_has_state(enabled_, LV_STATE_CHECKED);
        value.inverted = lv_obj_has_state(inverted_, LV_STATE_CHECKED);
        route() = value;
        return true;
    }

    void handle(lv_obj_t* target) {
        if (target == keyboard_) {
            lv_keyboard_set_textarea(keyboard_, nullptr);
            lv_obj_add_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        if (pending_) return;
        for (auto* input : numeric_) {
            if (target == input) {
                lv_keyboard_set_textarea(keyboard_, input);
                lv_obj_remove_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
                return;
            }
        }
        if (target == destination_) {
            const unsigned selected = lv_dropdown_get_selected(destination_);
            if (selected && selected <= destination_count_) {
                const auto id = destinations_[selected - 1];
                if (route() == performance::Route{} || route().destination != id) {
                    const auto meta = backend_->routingDestination(id);
                    if (meta.available) route() = {id, meta.minimum, meta.maximum, 0,
                        performance::Curve::Linear, performance::Polarity::Bipolar, meta.families, true, false};
                }
            }
            loadSlot();
            setStatus("DRAFT UPDATED. APPLY TO SEND TO OWNER.");
            return;
        }
        if (target != macro_ && target != slot_) return;
        if (!storeSlot()) {
            lv_dropdown_set_selected(macro_, macro_index_);
            lv_dropdown_set_selected(slot_, slot_index_);
            return;
        }
        if (target == macro_) {
            macro_index_ = lv_dropdown_get_selected(macro_);
            slot_index_ = 0;
        } else {
            slot_index_ = lv_dropdown_get_selected(slot_);
        }
        loadSlot();
        refresh();
    }

    void act(Action action) {
        if (action == Action::Close) {
            nav_->navigateTo(ScreenId::Performance, 0);
            return;
        }
        if (pending_) return;
        if (action == Action::Apply) {
            if (!storeSlot()) return;
            const auto response = backend_->requestRouting(original_, draft_, reset_positions_);
            pending_ = response.state == UiCommandState::Requested ? response.token : 0;
            setStatus(response.notice ? response.notice : "ROUTES REJECTED.");
            refresh();
            return;
        }
        if (action == Action::ResetPosition) {
            if (!storeSlot()) return;
            reset_positions_ |= uint8_t(1u << macro_index_);
            setStatus("POSITION RESET QUEUED IN DRAFT. APPLY TO CONFIRM.");
            return;
        }
        if (action == Action::ResetBoth) reset_positions_ |= uint8_t(1u << macro_index_);
        if (action == Action::Delete) route() = {};
        else if (action == Action::RestoreSlot) route() = backend_->factoryRoute(macro_index_, slot_index_);
        else for (unsigned i = 0; i < performance::kRoutesPerMacro; ++i)
            draft_[macro_index_][i] = backend_->factoryRoute(macro_index_, i);
        loadSlot();
        setStatus("DRAFT UPDATED. APPLY TO SEND TO OWNER.");
    }

    IUiBackend* backend_ = nullptr;
    UiNavigationManager* nav_ = nullptr;
    RoutingSnapshot original_{};
    performance::Routes draft_{};
    std::array<uint16_t, 128> destinations_{};
    unsigned destination_count_ = 0, macro_index_ = 0, slot_index_ = 0;
    int shown_position_ = -1;
    uint32_t pending_ = 0;
    uint8_t reset_positions_ = 0;
    lv_obj_t *macro_ = nullptr, *slot_ = nullptr, *destination_ = nullptr, *position_ = nullptr;
    lv_obj_t *values_ = nullptr, *domain_ = nullptr, *curve_ = nullptr, *polarity_ = nullptr, *enabled_ = nullptr;
    lv_obj_t *inverted_ = nullptr, *status_ = nullptr, *keyboard_ = nullptr, *apply_ = nullptr;
    lv_obj_t* numeric_[3]{};
};

}
