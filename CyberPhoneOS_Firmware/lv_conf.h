/**
 * CyberPhoneOS - lv_conf.h
 * Target: LVGL v8.3.x
 *
 * WHERE THIS FILE GOES:
 *   <Arduino sketchbook>/libraries/lv_conf.h
 *   (i.e. NEXT TO the "lvgl" library folder, NOT inside it)
 *
 * This is a trimmed config: any setting not listed here falls back to
 * LVGL's own default (lv_conf_internal.h fills gaps automatically), so it's
 * safe to keep this file short and only override what CyberPhoneOS needs.
 */

#if 1 /* <-- must stay 1, this enables the whole file */

#ifndef LV_CONF_H
#define LV_CONF_H

/*------------- Color settings -------------*/
#define LV_COLOR_DEPTH     16
#define LV_COLOR_16_SWAP    0   // set to 1 only if colors look byte-swapped on your panel

/*------------- Memory -------------*/
#define LV_MEM_CUSTOM       0
#define LV_MEM_SIZE     (48U * 1024U)   // 48kB for LVGL's internal heap

/*------------- HAL / tick -------------*/
#define LV_TICK_CUSTOM      1
#define LV_TICK_CUSTOM_INCLUDE  "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/*------------- Misc feature toggles CyberPhoneOS relies on -------------*/
#define LV_USE_LOG           0
#define LV_USE_ASSERT_NULL   1
#define LV_USE_PERF_MONITOR  0

#define LV_USE_ARC           1
#define LV_USE_BAR           1
#define LV_USE_BTN           1
#define LV_USE_BTNMATRIX     1
#define LV_USE_CANVAS        1
#define LV_USE_CHART         1
#define LV_USE_CHECKBOX      1
#define LV_USE_DROPDOWN      1
#define LV_USE_IMG           1
#define LV_USE_KEYBOARD      1
#define LV_USE_LABEL         1
#define LV_USE_LIST          1
#define LV_USE_METER         1
#define LV_USE_SLIDER        1
#define LV_USE_SPINNER       1
#define LV_USE_SWITCH        1
#define LV_USE_TABVIEW       1
#define LV_USE_TEXTAREA      1
#define LV_USE_TABLE         1

/*------------- Fonts -------------*/
#define LV_FONT_MONTSERRAT_12    1
#define LV_FONT_MONTSERRAT_14    1
#define LV_FONT_MONTSERRAT_16    1
#define LV_FONT_MONTSERRAT_20    1
#define LV_FONT_MONTSERRAT_24    1
#define LV_FONT_DEFAULT   &lv_font_montserrat_14

/*------------- Widget behaviour -------------*/
#define LV_USE_ANIMATION  1
#define LV_USE_SHADOW     1
#define LV_USE_BLEND_MODES 1

/*------------- Compiler / OS -------------*/
#define LV_USE_OS         LV_OS_NONE

/*------------- Demos / examples: keep off, they bloat flash -------------*/
#define LV_BUILD_EXAMPLES 0
#define LV_USE_DEMO_WIDGETS 0
#define LV_USE_DEMO_BENCHMARK 0

#endif /*LV_CONF_H*/
#endif /*end of "#if 1"*/
