// story_font.c —— 字体选择实现。见 story_font.h。
//
// 使用应用专用字库 app_font_16（由 tools/make_font.sh 从 simhei.ttf 生成，
// 精确覆盖 docs/font_chars.txt 里全部 489 个字符），避免依赖 LVGL 内置
// CJK 子集——后者只覆盖常用字，无法保证故事正文里的每个字都显示。
#include "story_font.h"

// app_font_16.c 导出的字体符号（在 assets/fonts/，由 CMake 加入编译）。
LV_FONT_DECLARE(app_font_16);

const lv_font_t *story_font_get(void) {
    return &app_font_16;
}

const lv_font_t *story_font_get_small(void) {
    // 当前只有一个字号；小字也用它。若要更精细，可再生成一个 app_font_14。
    return &app_font_16;
}
