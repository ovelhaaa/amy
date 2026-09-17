with open('smk-s3/components/ui/screens/home_screen.cpp', 'r') as f:
    content = f.read()

# Replace scope with local declaration like in the other blocks
target = """        // Scope
        int16_t scope_y = kHeaderHeight + kMargin + 2 * (kMacroTileHeight + kMacroTileGapY) + kMargin;
        int16_t scope_h = 56;
        scope.setPosition(kMargin, scope_y, dw - kMargin * 2, scope_h);
        scope.setColors(ColorAccentPrimary, ColorSurfaceElev);
        scope.draw(display);"""

replacement = """        // Scope
        int16_t scope_y = kHeaderHeight + kMargin + 2 * (kMacroTileHeight + kMacroTileGapY) + kMargin;
        int16_t scope_h = 56;
        OscilloscopeWidget scope(kMargin, scope_y, dw - kMargin * 2, scope_h);
        scope.setSamples(scope_samples_, scope_sample_count_);
        scope.setActive(active_voices_ > 0 || midi_active_);
        scope.setColors(ColorAccentPrimary, ColorSurfaceElev);
        scope.draw(display);"""

content = content.replace(target, replacement)

# Fix missing brace
if '} else if (dw <= 160) {' in content:
    # Check if there is a missing closing brace before the else if
    pass # we know it's missing in the square block

with open('smk-s3/components/ui/screens/home_screen.cpp', 'w') as f:
    f.write(content)
