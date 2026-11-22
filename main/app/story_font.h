// story_font.h —— 应用统一字体选择与中文覆盖检查的入口。
//
// 本工程采用仓库 LVGL 内置的 Source Han Sans SC 16 CJK 子集做快速验证
// （CONFIG_LV_FONT_SOURCE_HAN_SANS_SC_16_CJK=y）。它覆盖本应用全部 UI 文案
// 吗？必须在真机按 tools/check_font_coverage.py 生成的码点清单验证，
// 不能只凭"能显示中文"就认定齐全。
//
// 若后续改用自定义子集（assets/fonts/app_font_16.c），只需修改
// story_font_get() 的返回值，UI 代码不必改。
#pragma once

#include "lvgl.h"

// 返回应用正文/标题统一使用的字体指针。必须在 LVGL 初始化之后调用。
// 注意：字体描述符在 Flash 中，生命周期与固件相同，不可释放。
const lv_font_t *story_font_get(void);

// 返回稍小一号的字体（用于次要信息/电量），当前与正文共用同一字体。
const lv_font_t *story_font_get_small(void);
