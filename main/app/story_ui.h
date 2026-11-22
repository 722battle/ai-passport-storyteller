// story_ui.h —— 奋斗鹅讲述者的 LVGL 界面。
//
// 这是全新设计的 UI，不沿用基线 demo 测试菜单或 ui_pixel 外壳。
// 所有对象操作必须在 LVGL 任务内或持 bsp_lvgl_lock() 下进行，
// 由调用方（story_app）负责加锁；本模块的公开函数文档会注明上下文要求。
//
// 屏幕 240x320 RGB565，无 PSRAM：形象用 LVGL 基本图形绘制，不加载大位图，
// 只在需要时用一张预转换的 RGB565 小图替换（见 tools/make_mascot.py）。
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "lvgl.h"
#include "story_data.h"

// 奋斗鹅的 7 种表情状态，与需求方案 §1.1 对应。
typedef enum {
    STORY_FACE_WELCOME = 0,   // 欢迎：微笑 + 轻微前倾
    STORY_FACE_IDLE,          // 待机：平静
    STORY_FACE_LISTEN,        // 聆听：眼镜高亮 + 身体前倾
    STORY_FACE_TALK,          // 讲述：口型开合
    STORY_FACE_THINK,         // 思考：头微歪
    STORY_FACE_UNCERTAIN,     // 不确定：歪头 + 围巾下垂
    STORY_FACE_BOUNDARY,      // 拒绝：平静但侧身
} story_face_t;

// 页面类型。
typedef enum {
    STORY_SCREEN_WELCOME = 0, // 开场欢迎
    STORY_SCREEN_THEMES,      // 主题轮盘
    STORY_SCREEN_TELLING,     // 讲述页（故事/追问/话术共用）
    STORY_SCREEN_QUESTION,    // 提问列表（离网的结构化提问入口）
} story_screen_t;

// 初始化：创建根屏幕与奋斗鹅形象。必须在 LVGL 就绪后、持锁状态调用。
// 成功返回 true。失败（内存不足）返回 false，调用方应记录日志。
bool story_ui_init(void);

// 反初始化：删除所有对象并清空指针。持锁调用。
void story_ui_deinit(void);

// 切换到某个页面。持锁调用。
void story_ui_show(story_screen_t screen);

// 设置奋斗鹅表情。持锁调用。可在任意页面调用。
void story_ui_set_face(story_face_t face);

// 设置电量显示（0..100，-1 = 不可用）。持锁调用。
void story_ui_set_battery(int soc);

// ---- 主题轮盘 ----
// 设置主题列表的选中下标；UI 只高亮，不负责业务逻辑。持锁调用。
void story_ui_set_theme_selection(size_t index);
// 设置轮盘顶部的提示文字（可选，NULL 则用默认）。持锁调用。
void story_ui_set_theme_hint(const char *hint);

// 向主题轮盘追加一项。切到主题页后由 story_app 逐项调用（最多 8 项）。持锁调用。
void story_ui_add_theme(const char *label);

// ---- 讲述页 ----
// 显示一条故事的标题与正文。持锁调用。
// text 为 UTF-8；内部复制到 label，调用方可释放自己的缓冲。
void story_ui_set_story(const char *title, const char *text);

// 设置追问气泡（最多 3 个）。持锁调用。
// labels 为 UTF-8 字符串数组；count 超过 3 时截断。传 NULL/0 则隐藏。
void story_ui_set_followups(const char *const *labels, size_t count);

// 高亮当前选中的追问气泡（0..count-1，SIZE_MAX 表示不高亮）。持锁调用。
void story_ui_highlight_followup(size_t index);

// ---- 提问列表 ----
// 用问题文本填充提问列表；selection 为当前选中下标。持锁调用。
void story_ui_set_questions(const char *const *questions, size_t count, size_t selection);

// 返回当前页面（只读）。
story_screen_t story_ui_current_screen(void);
