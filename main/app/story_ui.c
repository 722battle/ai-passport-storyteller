// story_ui.c —— 奋斗鹅讲述者 UI 实现。接口语义见 story_ui.h。
//
// 内存与并发约束：
//   - 所有 lv_* 调用都在持 bsp_lvgl_lock() 的上下文中（由 story_app 保证）。
//   - 不创建额外 LVGL 任务；动画只用 LVGL 自带的 lv_anim，不引入线程。
//   - 形象用基本图形（矩形/圆/圆弧）拼出，避免解码大位图占用内部 RAM。
//   - 每次切页删除旧 screen，避免对象泄漏（C3 内存紧张）。
#include "story_ui.h"

#include "story_font.h"

#include <stdint.h>
#include <string.h>

// ---- 画布布局常量（240x320 竖屏）----
#define SCREEN_W 240
#define SCREEN_H 320
#define PAD 12
#define HEADER_H 26
#define MASCOT_BOX_W 96
#define MASCOT_BOX_H 120
#define MAX_FOLLOWUPS 3

// ---- 配色（浅色、清晰，适配客户接待场景）----
#define C_BG        0xF6F3EC  // 米白背景
#define C_PAPER     0xFFFFFF  // 卡片白
#define C_INK       0x1A1A1A  // 主文字
#define C_SUB       0x5A5A5A  // 次要文字
#define C_BLACK     0x141414  // 奋斗鹅身体
#define C_RED       0xE23B2E  // 喙/脚（华为红意象，仅形象点缀）
#define C_BLUE      0x2E86D1  // 蓝围巾
#define C_ACCENT    0x1F6FB2  // 高亮
#define C_BORDER    0xDDD8CE

// 模块级状态：当前页面与各页面根对象。全部只在持锁上下文中访问。
static story_screen_t s_screen = STORY_SCREEN_WELCOME;
static lv_obj_t *s_root = NULL;          // 当前 screen
static lv_obj_t *s_header_soc = NULL;    // 电量标签
static lv_obj_t *s_mascot = NULL;        // 奋斗鹅容器
static lv_obj_t *s_glasses_hl = NULL;    // 眼镜高亮层（聆听态显示）
static lv_obj_t *s_mouth = NULL;         // 口型（讲述态放大）
static lv_obj_t *s_scarf = NULL;         // 围巾（不确定态下垂）

// 各页面内容对象
static lv_obj_t *s_title = NULL;
static lv_obj_t *s_body = NULL;
static lv_obj_t *s_theme_items[MAX_FOLLOWUPS + 8]; // 主题最多 8 项，复用数组
static size_t s_theme_item_count = 0;
static lv_obj_t *s_followups[MAX_FOLLOWUPS];
static size_t s_followup_count = 0;
static lv_obj_t *s_questions[8];
static size_t s_question_count = 0;
static lv_obj_t *s_theme_hint = NULL;
static story_face_t s_face = STORY_FACE_IDLE;

// ---------------------------------------------------------------------------
// 内部工具：创建一个圆角卡片。
// ---------------------------------------------------------------------------
static lv_obj_t *create_card(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(C_PAPER), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    return card;
}

// ---------------------------------------------------------------------------
// 绘制奋斗鹅：黑颈红喙、圆框眼镜、白大褂、蓝围巾。
// 返回容器对象；所有子图形挂在容器上，切页时随 screen 一起销毁。
// ---------------------------------------------------------------------------
static lv_obj_t *create_mascot(lv_obj_t *parent, int x, int y) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, MASCOT_BOX_W, MASCOT_BOX_H);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);

    // 1) 白大褂（身体）：白色圆角矩形，稍窄，居中偏下。
    lv_obj_t *coat = lv_obj_create(box);
    lv_obj_set_pos(coat, 20, 62);
    lv_obj_set_size(coat, 56, 48);
    lv_obj_set_style_radius(coat, 12, 0);
    lv_obj_set_style_bg_color(coat, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(coat, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_border_width(coat, 1, 0);

    // 白大褂口袋上的简约标识（一个小小的红色方块，避免直接用受保护的 logo）。
    lv_obj_t *badge = lv_obj_create(box);
    lv_obj_set_pos(badge, 26, 88);
    lv_obj_set_size(badge, 10, 8);
    lv_obj_set_style_radius(badge, 2, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(C_RED), 0);
    lv_obj_set_style_border_width(badge, 0, 0);

    // 2) 黑色长颈 + 头：黑色圆角矩形，从大褂领口向上延伸。
    lv_obj_t *neck = lv_obj_create(box);
    lv_obj_set_pos(neck, 30, 14);
    lv_obj_set_size(neck, 36, 58);
    lv_obj_set_style_radius(neck, 12, 0);
    lv_obj_set_style_bg_color(neck, lv_color_hex(C_BLACK), 0);
    lv_obj_set_style_border_width(neck, 0, 0);

    // 3) 蓝围巾：横过颈部的一条蓝色圆角矩形。
    s_scarf = lv_obj_create(box);
    lv_obj_set_pos(s_scarf, 22, 60);
    lv_obj_set_size(s_scarf, 52, 10);
    lv_obj_set_style_radius(s_scarf, 5, 0);
    lv_obj_set_style_bg_color(s_scarf, lv_color_hex(C_BLUE), 0);
    lv_obj_set_style_border_width(s_scarf, 0, 0);

    // 4) 红色喙：小三角近似为圆角矩形 + 下唇。
    lv_obj_t *beak = lv_obj_create(box);
    lv_obj_set_pos(beak, 40, 34);
    lv_obj_set_size(beak, 16, 9);
    lv_obj_set_style_radius(beak, 4, 0);
    lv_obj_set_style_bg_color(beak, lv_color_hex(C_RED), 0);
    lv_obj_set_style_border_width(beak, 0, 0);
    s_mouth = beak; // 讲述态用同一个对象放大表示口型开合

    // 5) 圆框眼镜：两个空心圆（border 粗，bg 透明）。
    //    左镜片
    lv_obj_t *glass_l = lv_obj_create(box);
    lv_obj_set_pos(glass_l, 24, 18);
    lv_obj_set_size(glass_l, 18, 18);
    lv_obj_set_style_radius(glass_l, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(glass_l, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(glass_l, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(glass_l, 2, 0);
    // 右镜片
    lv_obj_t *glass_r = lv_obj_create(box);
    lv_obj_set_pos(glass_r, 54, 18);
    lv_obj_set_size(glass_r, 18, 18);
    lv_obj_set_style_radius(glass_r, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(glass_r, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(glass_r, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(glass_r, 2, 0);
    // 镜片里的眼珠（小白点），让它显得精神
    lv_obj_t *eye_l = lv_obj_create(box);
    lv_obj_set_pos(eye_l, 31, 24);
    lv_obj_set_size(eye_l, 6, 6);
    lv_obj_set_style_radius(eye_l, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(eye_l, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(eye_l, 0, 0);
    lv_obj_t *eye_r = lv_obj_create(box);
    lv_obj_set_pos(eye_r, 61, 24);
    lv_obj_set_size(eye_r, 6, 6);
    lv_obj_set_style_radius(eye_r, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(eye_r, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(eye_r, 0, 0);

    // 6) 眼镜高亮层：聆听态显示一圈浅蓝描边，默认隐藏。
    s_glasses_hl = lv_obj_create(box);
    lv_obj_set_pos(s_glasses_hl, 21, 15);
    lv_obj_set_size(s_glasses_hl, 54, 24);
    lv_obj_set_style_radius(s_glasses_hl, 12, 0);
    lv_obj_set_style_bg_opa(s_glasses_hl, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(s_glasses_hl, lv_color_hex(C_ACCENT), 0);
    lv_obj_set_style_border_width(s_glasses_hl, 2, 0);
    lv_obj_add_flag(s_glasses_hl, LV_OBJ_FLAG_HIDDEN);

    // 7) 红色脚（两个小椭圆）。
    lv_obj_t *foot_l = lv_obj_create(box);
    lv_obj_set_pos(foot_l, 24, 108);
    lv_obj_set_size(foot_l, 20, 8);
    lv_obj_set_style_radius(foot_l, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(foot_l, lv_color_hex(C_RED), 0);
    lv_obj_set_style_border_width(foot_l, 0, 0);
    lv_obj_t *foot_r = lv_obj_create(box);
    lv_obj_set_pos(foot_r, 52, 108);
    lv_obj_set_size(foot_r, 20, 8);
    lv_obj_set_style_radius(foot_r, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(foot_r, lv_color_hex(C_RED), 0);
    lv_obj_set_style_border_width(foot_r, 0, 0);

    return box;
}

// ---------------------------------------------------------------------------
// 创建统一的顶部栏：标题 + 右上角电量。
// ---------------------------------------------------------------------------
static lv_obj_t *create_header(lv_obj_t *parent, const char *title) {
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_size(bar, SCREEN_W, HEADER_H);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(C_PAPER), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);

    lv_obj_t *t = lv_label_create(bar);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_font(t, story_font_get(), 0);
    lv_obj_set_style_text_color(t, lv_color_hex(C_INK), 0);
    lv_obj_set_pos(t, PAD, 4);

    s_header_soc = lv_label_create(bar);
    lv_label_set_text(s_header_soc, "");
    lv_obj_set_style_text_font(s_header_soc, story_font_get_small(), 0);
    lv_obj_set_style_text_color(s_header_soc, lv_color_hex(C_SUB), 0);
    lv_obj_align(s_header_soc, LV_ALIGN_RIGHT_MID, -PAD, 0);
    return bar;
}

// ---------------------------------------------------------------------------
// 销毁当前 screen：先删除整个 screen（会连带删除所有子对象），再清空指针。
// 注意：删除 screen 前调用方必须已停止所有可能访问这些对象的任务/定时器。
// ---------------------------------------------------------------------------
static void destroy_screen(void) {
    s_title = NULL;
    s_body = NULL;
    s_mascot = NULL;
    s_glasses_hl = NULL;
    s_mouth = NULL;
    s_scarf = NULL;
    s_theme_hint = NULL;
    s_header_soc = NULL;
    s_theme_item_count = 0;
    s_followup_count = 0;
    s_question_count = 0;
    // 提问列表对象在页面重建时必须清空，否则 set_questions 会复用已删除对象。
    for (size_t i = 0; i < sizeof(s_questions) / sizeof(s_questions[0]); ++i) {
        s_questions[i] = NULL;
    }
    for (size_t i = 0; i < MAX_FOLLOWUPS; ++i) {
        s_followups[i] = NULL;
    }
    if (s_root) {
        lv_obj_delete(s_root);
        s_root = NULL;
    }
}

// ---------------------------------------------------------------------------
// 新建 screen：设置背景色并加载。
// ---------------------------------------------------------------------------
static lv_obj_t *new_screen(void) {
    destroy_screen();
    s_root = lv_obj_create(NULL);
    lv_obj_set_size(s_root, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(C_BG), 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);
    lv_screen_load(s_root);
    return s_root;
}

// ---------------------------------------------------------------------------
// 各页面构建
// ---------------------------------------------------------------------------
static void build_welcome(void) {
    lv_obj_t *root = new_screen();
    create_header(root, "华为研发故事");
    lv_obj_t *m = create_mascot(root, (SCREEN_W - MASCOT_BOX_W) / 2, 60);
    s_mascot = m;

    lv_obj_t *card = create_card(root, PAD, 196, SCREEN_W - 2 * PAD, 108);
    s_title = lv_label_create(card);
    lv_label_set_text(s_title, "欢迎来到松山湖研发中心");
    lv_obj_set_style_text_font(s_title, story_font_get(), 0);
    lv_obj_set_style_text_color(s_title, lv_color_hex(C_INK), 0);
    lv_obj_set_width(s_title, SCREEN_W - 2 * PAD - 16);
    lv_obj_set_pos(s_title, 0, 0);

    s_body = lv_label_create(card);
    lv_label_set_text(s_body,
        "我是奋斗鹅，陪你走这一段的研发故事讲述者。\n"
        "按住 OK 直接问我，或用上下键挑个主题。");
    lv_obj_set_style_text_font(s_body, story_font_get(), 0);
    lv_obj_set_style_text_color(s_body, lv_color_hex(C_SUB), 0);
    lv_obj_set_width(s_body, SCREEN_W - 2 * PAD - 16);
    lv_obj_set_pos(s_body, 0, 30);
    lv_obj_set_style_text_line_space(s_body, 6, 0);
    story_ui_set_face(STORY_FACE_WELCOME);
}

static void build_themes(void) {
    lv_obj_t *root = new_screen();
    create_header(root, "挑个主题");
    s_theme_hint = lv_label_create(root);
    lv_label_set_text(s_theme_hint, "上下键选择 · OK 进入");
    lv_obj_set_style_text_font(s_theme_hint, story_font_get_small(), 0);
    lv_obj_set_style_text_color(s_theme_hint, lv_color_hex(C_SUB), 0);
    lv_obj_set_pos(s_theme_hint, PAD, 34);
    s_theme_item_count = 0;
}

static void build_telling(void) {
    lv_obj_t *root = new_screen();
    create_header(root, "研发故事");
    s_mascot = create_mascot(root, 8, 40);

    lv_obj_t *card = create_card(root, PAD + MASCOT_BOX_W + 4, 40,
                                 SCREEN_W - 2 * PAD - MASCOT_BOX_W - 4, 150);
    s_title = lv_label_create(card);
    lv_label_set_text(s_title, "");
    lv_obj_set_style_text_font(s_title, story_font_get(), 0);
    lv_obj_set_style_text_color(s_title, lv_color_hex(C_INK), 0);
    lv_obj_set_width(s_title, SCREEN_W - 2 * PAD - MASCOT_BOX_W - 20);
    lv_obj_set_pos(s_title, 0, 0);

    s_body = lv_label_create(card);
    lv_label_set_text(s_body, "");
    lv_obj_set_style_text_font(s_body, story_font_get(), 0);
    lv_obj_set_style_text_color(s_body, lv_color_hex(C_SUB), 0);
    lv_obj_set_width(s_body, SCREEN_W - 2 * PAD - MASCOT_BOX_W - 20);
    lv_obj_set_pos(s_body, 0, 26);
    lv_obj_set_style_text_line_space(s_body, 4, 0);
    lv_label_set_long_mode(s_body, LV_LABEL_LONG_SCROLL_CIRCULAR);

    // 追问气泡区
    for (size_t i = 0; i < MAX_FOLLOWUPS; ++i) {
        s_followups[i] = lv_label_create(root);
        lv_obj_set_style_text_font(s_followups[i], story_font_get_small(), 0);
        lv_obj_set_style_text_color(s_followups[i], lv_color_hex(C_ACCENT), 0);
        lv_obj_add_flag(s_followups[i], LV_OBJ_FLAG_HIDDEN);
    }
    story_ui_set_face(STORY_FACE_TALK);
}

static void build_question(void) {
    lv_obj_t *root = new_screen();
    create_header(root, "想问什么？");
    s_question_count = 0;
}

// ---------------------------------------------------------------------------
// 公开接口实现
// ---------------------------------------------------------------------------
bool story_ui_init(void) {
    s_screen = STORY_SCREEN_WELCOME;
    build_welcome();
    return s_root != NULL;
}

void story_ui_deinit(void) {
    destroy_screen();
}

void story_ui_show(story_screen_t screen) {
    s_screen = screen;
    switch (screen) {
        case STORY_SCREEN_WELCOME: build_welcome(); break;
        case STORY_SCREEN_THEMES:  build_themes();  break;
        case STORY_SCREEN_TELLING: build_telling(); break;
        case STORY_SCREEN_QUESTION: build_question(); break;
        default: break;
    }
}

void story_ui_set_face(story_face_t face) {
    s_face = face;
    if (!s_glasses_hl || !s_mouth || !s_scarf || !s_mascot) return;

    // 眼镜高亮：仅聆听态显示。
    if (face == STORY_FACE_LISTEN) {
        lv_obj_clear_flag(s_glasses_hl, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_glasses_hl, LV_OBJ_FLAG_HIDDEN);
    }

    // 口型：讲述态放大喙，其他态恢复。
    if (face == STORY_FACE_TALK) {
        lv_obj_set_size(s_mouth, 18, 12);
    } else {
        lv_obj_set_size(s_mouth, 16, 9);
    }

    // 围巾：不确定态"下垂"（下移并变窄），思考/拒绝态轻微偏移。
    switch (face) {
        case STORY_FACE_UNCERTAIN:
            lv_obj_set_pos(s_scarf, 24, 64);
            lv_obj_set_size(s_scarf, 44, 8);
            break;
        case STORY_FACE_THINK:
            lv_obj_set_pos(s_scarf, 24, 60);
            break;
        default:
            lv_obj_set_pos(s_scarf, 22, 60);
            lv_obj_set_size(s_scarf, 52, 10);
            break;
    }
    lv_obj_set_style_transform_rotation(s_mascot,
        (face == STORY_FACE_THINK || face == STORY_FACE_UNCERTAIN) ? 400 : 0, 0);
}

void story_ui_set_battery(int soc) {
    if (!s_header_soc) return;
    if (soc < 0) {
        lv_label_set_text(s_header_soc, "");
    } else {
        lv_label_set_text_fmt(s_header_soc, "%d%%", soc);
    }
}

void story_ui_set_theme_selection(size_t index) {
    for (size_t i = 0; i < s_theme_item_count; ++i) {
        lv_obj_set_style_bg_color(s_theme_items[i],
            lv_color_hex(i == index ? C_ACCENT : C_PAPER), 0);
        lv_obj_set_style_text_color(s_theme_items[i],
            lv_color_hex(i == index ? 0xFFFFFF : C_INK), 0);
    }
}

void story_ui_set_theme_hint(const char *hint) {
    if (!s_theme_hint) return;
    lv_label_set_text(s_theme_hint, hint ? hint : "上下键选择 · OK 进入");
}

// 主题列表由 story_app 在切到主题页后逐项设置；这里提供追加函数。
// 说明：头文件未暴露 append，主题项在 story_app 调用 story_ui_add_theme() 追加。
// 为实现内聚，这里直接定义追加接口（在头文件声明见下）。
void story_ui_add_theme(const char *label) {
    if (!s_root || s_theme_item_count >= sizeof(s_theme_items) / sizeof(s_theme_items[0])) return;
    lv_obj_t *item = lv_obj_create(s_root);
    size_t i = s_theme_item_count;
    lv_obj_set_pos(item, PAD, 66 + (int)i * 30);
    lv_obj_set_size(item, SCREEN_W - 2 * PAD, 26);
    lv_obj_set_style_radius(item, 6, 0);
    lv_obj_set_style_bg_color(item, lv_color_hex(C_PAPER), 0);
    lv_obj_set_style_border_width(item, 0, 0);
    lv_obj_set_style_pad_all(item, 4, 0);

    lv_obj_t *label_obj = lv_label_create(item);
    lv_label_set_text(label_obj, label);
    lv_obj_set_style_text_font(label_obj, story_font_get(), 0);
    lv_obj_set_style_text_color(label_obj, lv_color_hex(C_INK), 0);
    lv_obj_center(label_obj);
    s_theme_items[i] = item;
    s_theme_item_count++;
}

void story_ui_set_story(const char *title, const char *text) {
    if (s_title) lv_label_set_text(s_title, title ? title : "");
    if (s_body)  lv_label_set_text(s_body, text ? text : "");
}

void story_ui_set_followups(const char *const *labels, size_t count) {
    if (count > MAX_FOLLOWUPS) count = MAX_FOLLOWUPS;
    s_followup_count = count;
    for (size_t i = 0; i < MAX_FOLLOWUPS; ++i) {
        if (!s_followups[i]) continue;
        if (i < count && labels && labels[i]) {
            lv_label_set_text_fmt(s_followups[i], "OK[%d] %s", (int)(i + 1), labels[i]);
            lv_obj_set_pos(s_followups[i], PAD, 202 + (int)i * 26);
            lv_obj_clear_flag(s_followups[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_followups[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void story_ui_highlight_followup(size_t index) {
    for (size_t i = 0; i < s_followup_count; ++i) {
        lv_obj_set_style_text_color(s_followups[i],
            lv_color_hex(i == index ? C_ACCENT : C_SUB), 0);
    }
}

void story_ui_set_questions(const char *const *questions, size_t count, size_t selection) {
    if (count > sizeof(s_questions) / sizeof(s_questions[0])) {
        count = sizeof(s_questions) / sizeof(s_questions[0]);
    }
    s_question_count = count;
    for (size_t i = 0; i < count; ++i) {
        if (!s_questions[i]) {
            s_questions[i] = lv_obj_create(s_root);
            lv_obj_set_size(s_questions[i], SCREEN_W - 2 * PAD, 26);
            lv_obj_set_style_radius(s_questions[i], 6, 0);
            lv_obj_set_style_border_width(s_questions[i], 0, 0);
            lv_obj_set_style_pad_all(s_questions[i], 4, 0);
            lv_obj_t *lab = lv_label_create(s_questions[i]);
            lv_obj_set_style_text_font(lab, story_font_get(), 0);
            lv_obj_center(lab);
        }
        lv_obj_set_pos(s_questions[i], PAD, 40 + (int)i * 30);
        lv_obj_set_style_bg_color(s_questions[i],
            lv_color_hex(i == selection ? C_ACCENT : C_PAPER), 0);
        // 第 0 个子对象是 label；设置其颜色
        lv_obj_t *lab = lv_obj_get_child(s_questions[i], 0);
        if (lab) {
            lv_label_set_text(lab, questions[i]);
            lv_obj_set_style_text_color(lab,
                lv_color_hex(i == selection ? 0xFFFFFF : C_INK), 0);
        }
    }
}

story_screen_t story_ui_current_screen(void) {
    return s_screen;
}
