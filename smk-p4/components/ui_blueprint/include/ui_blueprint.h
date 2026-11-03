#pragma once
#include "lvgl.h"
#include "ui_demo_backend.h"
#include "ui_navigation.h"
#include "ui_widgets.h"
#include <cstdint>

namespace smk::p4::ui {

// Top-level coordinator for the SMK-P4 800x480 LVGL UI Blueprint.
// Can be mounted on lv_screen_active() in host preview or isolated P4 target.
class UiBlueprint {
public:
    explicit UiBlueprint(UiDemoBackend& backend) : backend_(backend) {}
    ~UiBlueprint() { shutdown(); }

    bool begin(lv_obj_t* root_parent = nullptr);
    void shutdown();
    void service(uint32_t now_ms);

    UiDemoBackend& backend() { return backend_; }
    const UiDemoBackend& backend() const { return backend_; }

    UiNavigationManager& navigation() { return nav_; }
    const UiNavigationManager& navigation() const { return nav_; }

    lv_obj_t* rootObject() const { return root_; }
    lv_obj_t* contentObject() const { return content_; }

private:
    static void onNavigate(void* user_data, ScreenId new_screen);

    UiDemoBackend&      backend_;
    UiNavigationManager nav_{};
    StatusStrip         status_strip_{};
    BottomNavBar        nav_bar_{};
    lv_obj_t*           root_          = nullptr;
    lv_obj_t*           content_       = nullptr;
    uint32_t            shown_rev_     = 0;
    ScreenId            shown_screen_  = ScreenId::Performance;
};

} // namespace smk::p4::ui
