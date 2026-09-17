#include "ui_manager.h"
#include "dummy_display_driver.h"
#include "ui_theme.h"

int main() {
    smk::DummyDisplayDriver driver(240, 240);
    return 0;
}
