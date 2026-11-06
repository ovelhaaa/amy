#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
/* Host assertions must terminate tests instead of spinning indefinitely. */
#define LV_ASSERT_HANDLER do { fprintf(stderr,"LVGL assertion %s:%d\n",__FILE__,__LINE__); abort(); } while(0);

/* Color depth: 16 (RGB565) matching Guition JC-ESP32P4-M3 ST7701S configuration */
#define LV_COLOR_DEPTH 16

/* Use LVGL built-in TLSF allocator with 128 KB pool matching sdkconfig.defaults */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB
#ifndef SMKP4_LVGL_POOL_KB
#define SMKP4_LVGL_POOL_KB 128U
#endif
#define LV_MEM_SIZE             (SMKP4_LVGL_POOL_KB * 1024U)

/* HAL settings */
#define LV_DEF_REFR_PERIOD      16
#define LV_DPI_DEF              130

/* Operating system */
#define LV_USE_OS               LV_OS_NONE

/* Rendering configuration */
#define LV_DRAW_BUF_STRIDE_ALIGN    1
#define LV_DRAW_BUF_ALIGN           4
#define LV_USE_DRAW_SW              1

/* Feature configuration */
#define LV_USE_LOG              0
#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1
#define LV_USE_SYSMON           0

/* Fonts matching smk-p4/sdkconfig.defaults */
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_28   1
#define LV_FONT_DEFAULT         &lv_font_montserrat_14

/* Widgets used in UI Blueprint */
#define LV_USE_ARC              1
#define LV_USE_BAR              1
#define LV_USE_BUTTON           1
#define LV_USE_LABEL            1
#define LV_USE_SLIDER           1
#define LV_USE_LINE             1

/* Layouts */
#define LV_USE_FLEX             1
#define LV_USE_GRID             1

/* Disable unused heavy extras */
#define LV_BUILD_EXAMPLES       0
#define LV_USE_THORVG_INTERNAL  0
#define LV_USE_LOTTIE           0

#endif /* LV_CONF_H */
