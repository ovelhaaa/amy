def rgb565(hex_str):
    h = hex_str.lstrip('#')
    r, g, b = tuple(int(h[i:i+2], 16) for i in (0, 2, 4))
    r5 = (r * 31 + 127) // 255
    g6 = (g * 63 + 127) // 255
    b5 = (b * 31 + 127) // 255
    val = (r5 << 11) | (g6 << 5) | b5
    print(f"{hex_str}: 0x{val:04X}")

colors = ["#0B0E12", "#12171D", "#181E25", "#29313A", "#F3F5F7", "#98A2AD", "#596571", "#37D7C4", "#F2B84B"]
for c in colors:
    rgb565(c)
