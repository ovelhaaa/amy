#pragma once
#include "sound_edit.h"

namespace smk::p4::edit {

// Availability captured from the resident AMY topology after each qualified
// factory patch is loaded. Zero marks a patch excluded from the library.
// The real-AMY scene tests check this table against Runtime::captureEdit().
inline constexpr Mask kFactoryPatchCapabilities[] = {
    0x01ff00001efe000fULL, 0x01ff00001ef8000fULL, 0x01ff00001efe000fULL, 0x01ff00001efe000fULL, // 0
    0x01ff00001efe000fULL, 0x01ff00001efe000fULL, 0, 0x01ff00001ffe000fULL, // 4
    0x01ff00001ef8000fULL, 0x01ff00001ef8000fULL, 0, 0x01ff00001ff8000fULL, // 8
    0x01ff00001ff8000fULL, 0x01ff00001ffe000fULL, 0, 0, // 12
    0x01ff00001cde000fULL, 0, 0x01ff00001ffe000fULL, 0x01ff00001efe000fULL, // 16
    0x01ff00001ef8000fULL, 0x01ff00001efe000fULL, 0x01ff00001efe000fULL, 0x01ff00001efe000fULL, // 20
    0x01ff00001ffe000fULL, 0x01ff00001ffe000fULL, 0x01ff00001ef8000fULL, 0x01ff00001ffe000fULL, // 24
    0x01ff00001ffe000fULL, 0x01ff00001ffe000fULL, 0x01ff00001ff8000fULL, 0x01ff00001ffe000fULL, // 28
    0, 0x01ff00001ef8000fULL, 0x01ff00001ef8000fULL, 0x01ff00001ff8000fULL, // 32
    0x01ff00001ef8000fULL, 0x01ff00001ff8000fULL, 0x01ff00001ef8000fULL, 0x01ff00001cd8000fULL, // 36
    0x01ff00001efe000fULL, 0x01ff00001ff8000fULL, 0x01ff00001ef8000fULL, 0x01ff00001cde000fULL, // 40
    0x01ff00001cde000fULL, 0x01ff00001cde000fULL, 0x01ff00001cde000fULL, 0, // 44
    0x01ff00001efe000fULL, 0x01ff00001efe000fULL, 0x01ff00001ff8000fULL, 0, // 48
    0x01ff00001efe000fULL, 0x01f000001ffe000fULL, 0, 0x01ff00001ffe000fULL, // 52
    0x01f000001ffe000fULL, 0x01ff00001ffe000fULL, 0x01ff00001cde000fULL, 0, // 56
    0x01ff00001cde000fULL, 0, 0x01f000001efe000fULL, 0x01ff00001cde000fULL, // 60
    0x01ff00001efe000fULL, 0x01ff00001efe000fULL, 0x01ff00001ffe000fULL, 0x01ff00001ff8000fULL, // 64
    0x01ff00001efe000fULL, 0, 0x01ff00001ffe000fULL, 0, // 68
    0, 0x01ff00001ffe000fULL, 0x01ff00001efe000fULL, 0x01ff00001ffe000fULL, // 72
    0x01ff00001ff8000fULL, 0x01ff00001ffe000fULL, 0x01ff00001ef8000fULL, 0x01ff00001ef8000fULL, // 76
    0x01ff00001ef8000fULL, 0, 0x01ff00001ffe000fULL, 0x01ff00001ff8000fULL, // 80
    0x01ff00001ef8000fULL, 0x01f000001ff8000fULL, 0x01ff00001ef8000fULL, 0x01ff00001efe000fULL, // 84
    0x01ff00001ef8000fULL, 0x01ff00001ffe000fULL, 0x01ff00001ff8000fULL, 0x01ff00001ff8000fULL, // 88
    0x01f000001cde000fULL, 0x01ff00001ffe000fULL, 0x01ff00001efe000fULL, 0x01ff00001efe000fULL, // 92
    0x01ff00001ffe000fULL, 0x01ff00001ef8000fULL, 0, 0, // 96
    0x01ff00001efe000fULL, 0x01ff00001efe000fULL, 0x01ff00001ff8000fULL, 0, // 100
    0x01ff00001efe000fULL, 0, 0, 0x01ff00001ef8000fULL, // 104
    0x01ff00001efe000fULL, 0x01ff00001ffe000fULL, 0x01ff00001cde000fULL, 0x01ff00001ff8000fULL, // 108
    0, 0x01ff00001cde000fULL, 0x01ff0000041e000fULL, 0x01ff00001efe000fULL, // 112
    0x01ff0000041e000fULL, 0x01ff00001efe000fULL, 0, 0x01ff00001efe000fULL, // 116
    0x01ff00001efe000fULL, 0x01ff00001ef8000fULL, 0x01ff00001efe000fULL, 0x01ff00001efe000fULL, // 120
    0x01ff00001ef8000fULL, 0x01ff00001ffe000fULL, 0x01ff00001ff8000fULL, 0x01ff00001ffe000fULL, // 124
    0x03ffffffe401fff0ULL, 0x03ffffffe401cff0ULL, 0x03ffffffe401fff0ULL, 0x03ffffffe401fff0ULL, // 128
    0x03ffff6fe401fff0ULL, 0x03ffffffe401fff0ULL, 0x03ffff6fe401fff0ULL, 0x03fffd9fe401fff0ULL, // 132
    0x03ffffcfe401fff0ULL, 0x03ffffffe401fff0ULL, 0x03ffffffe401fff0ULL, 0x03ffffffe401fff0ULL, // 136
    0x03fffe8f6401fff0ULL, 0x03ffffffe4017ff0ULL, 0x03fffedfe401fff0ULL, 0x03ffffffe401fff0ULL, // 140
    0, 0x03fffdffe401fff0ULL, 0x03ffffffe401fff0ULL, 0x03ffffffe401b7f0ULL, // 144
    0x03ffffefe401fff0ULL, 0x03fffff7e401fff0ULL, 0x03ffffffe401fff0ULL, 0x03ffffffe401fff0ULL, // 148
    0x03fffccfe401fff0ULL, 0x03ffff0fe401bff0ULL, 0x03ffffffe4017ff0ULL, 0x03fffdffe401fff0ULL, // 152
    0x03f0fe8fe40103f0ULL, 0x03fffdff6401fff0ULL, 0, 0x03fffffda401fff0ULL, // 156
    0x03ffffcfe401fff0ULL, 0x03ffffb7e401fff0ULL, 0x03fffdffe401fff0ULL, 0x03ffffdfe401fff0ULL, // 160
    0x03ffffff6401fff0ULL, 0x03ffffffe401fff0ULL, 0x03ffffffe401fff0ULL, 0x03fffe7fe401fff0ULL, // 164
    0x03ffffd6e401fff0ULL, 0x03fffed6e401fff0ULL, 0x03ffffffe401b7f0ULL, 0x03fffd7fe401fff0ULL, // 168
    0x03fffd0fe401fff0ULL, 0x03fffe0fe401fff0ULL, 0x03ffffefe401dbf0ULL, 0x03fffc0fe401fff0ULL, // 172
    0x03ffffffe4013ff0ULL, 0x03fffdbfe401fff0ULL, 0x03fffc7fe401fff0ULL, 0x03ffffffe401fff0ULL, // 176
    0x03ffffffe401fff0ULL, 0x03ffffffe401fbf0ULL, 0x03fffc0fe401fff0ULL, 0x03fffd5fe401fff0ULL, // 180
    0x03ffff6fe401fff0ULL, 0x03ffffffe401fff0ULL, 0x03fffeffe401fff0ULL, 0x03ffffdfe401fff0ULL, // 184
    0x03ffffffe401fff0ULL, 0x03ffffffe401fff0ULL, 0x03fffeffe401fff0ULL, 0x03fffeffe401fff0ULL, // 188
    0x03ffffffe401fff0ULL, 0x03ffffefe4017ff0ULL, 0x03fffcdfe401fff0ULL, 0x03ffffefe401fff0ULL, // 192
    0x03fffd4fe401fff0ULL, 0x03ffff5fe401fff0ULL, 0x03ffff6fe401fff0ULL, 0x03ffffffe401fff0ULL, // 196
    0x03fffd6fe401fff0ULL, 0x03fffd5fe401fff0ULL, 0x03ffffffe401fff0ULL, 0x03ffffffe4010ff0ULL, // 200
    0x03fffc7fe401fff0ULL, 0x03ffffffe401fff0ULL, 0x03ffffffe401fff0ULL, 0x03ffffffe401fff0ULL, // 204
    0x03fffeafe401fff0ULL, 0x03ffff5fe401fbf0ULL, 0x03ffff5fe401fbf0ULL, 0x03fffdff6401fff0ULL, // 208
    0x03ffffff6401fff0ULL, 0x03ffffffe401fff0ULL, 0x03ffffffe401fff0ULL, 0x03fffff7e401fff0ULL, // 212
    0x03fffffae401fff0ULL, 0x03ffffb8e401fff0ULL, 0x03ffffffe401fff0ULL, 0x03fffc0fe401fff0ULL, // 216
    0x03ffffffe401eff0ULL, 0x03ffffffe4017ff0ULL, 0x03ffffffe4013ff0ULL, 0x03ffffbfe401fff0ULL, // 220
    0x03ffffffe401fff0ULL, 0x03ffffffe401fff0ULL, 0x03ffffffe401fff0ULL, 0x03fffc3fe401fff0ULL, // 224
    0x03ffffffe4017ff0ULL, 0x03ffffffe401eff0ULL, 0x03fffddfe401fff0ULL, 0x03f0ffdf64017ff0ULL, // 228
    0, 0x03ffffffe401fff0ULL, 0x03fffe3fe4017ff0ULL, 0x03fffc9c2401fff0ULL, // 232
    0x03fffcffe4017ff0ULL, 0x03fffcffe401fff0ULL, 0x03ffffffe401fff0ULL, 0x03ffffffe401dff0ULL, // 236
    0x03ffffffe401fff0ULL, 0x03fffd7ae401fff0ULL, 0x03fffedfe401fff0ULL, 0x03fffc07e401fff0ULL, // 240
    0x03fffccde4017ff0ULL, 0x03fffdffe40143f0ULL, 0x03ffffffe401fff0ULL, 0x03f0fffc2401bff0ULL, // 244
    0, 0x03f0ffff6401fff0ULL, 0x03fffdffe401e7f0ULL, 0x03fffc016401fff0ULL, // 248
    0x03ffff0fe401fff0ULL, 0x03f0ffefe401bff0ULL, 0, 0x03f0ffefe401f3f0ULL, // 252
};
static_assert(sizeof(kFactoryPatchCapabilities)/sizeof(Mask)==256);

constexpr Mask factoryPatchCapabilities(uint16_t patch) {
    return patch<256?kFactoryPatchCapabilities[patch]:0;
}

}
