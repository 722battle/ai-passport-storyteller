// story_app.c —— 应用主控实现。接口语义见 story_app.h。
//
// 交互契约（与需求方案 §4.1 对应）：
//   待机/欢迎页：UP/DOWN 进入主题轮盘；OK 短按进入主题；OK 长按进入提问列表
//   主题轮盘  ：UP/DOWN 移动选择；OK 短按播放该主题故事；OK 长按返回欢迎
//   讲述页    ：UP/DOWN 移动追问气泡高亮；OK 短按播放选中追问；
//               OK 再短按一次（无追问时）"再讲一个"；OK 长按返回主题轮盘
//   提问列表  ：UP/DOWN 移动；OK 短按把问题文本送引擎；OK 长按返回欢迎
//
// 所有回答都来自 story_data 的预置内容；没有真词识别时，用"提问列表"提供
// 结构化提问入口，离线可用。用户按住 OK 说中文的场景属于后续 KWS 扩展。
#include "story_app.h"

#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_log.h"
#include "esp_timer.h"   // esp_timer_get_time：随机选片的种子
#include "story_audio.h"
#include "story_data.h"
#include "story_engine.h"
#include "story_ui.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "story_app";

// 提问列表：离网的结构化提问入口（都是引擎能命中的高频问法）。
static const char *const QUESTIONS[] = {
    "讲个技术攻关的故事",
    "研发文化是什么样的",
    "失败之后你们怎么复盘",
    "团队怎么协作",
    "松山湖有什么故事",
    "为什么这么做",
    "换个主题",
    "报价多少",   // 故意放入越界问法，演示边界话术
};
#define QUESTION_COUNT ((int)(sizeof(QUESTIONS) / sizeof(QUESTIONS[0])))

// 状态
static story_context_t s_ctx;
// 讲述页当前显示的内容类型：true=一条故事（有追问气泡），false=固定话术/追问回答。
// 用于讲述页 OK 短按时决定"播放追问"还是"再讲一个"，避免对固定话术误用旧故事的追问。
static bool s_telling_is_story = false;
static size_t s_theme_sel = 0;
static int s_question_sel = 0;
static size_t s_followup_sel = 0;
static size_t s_theme_story_skip = 0;   // 当前主题已讲过几条，支持"再讲一个"
static bool s_ready = false;

// ---------------------------------------------------------------------------
// 播放一段内容：同步更新 UI + 尝试播放音频（缺音频则提示音降级）。
// 参数 title/text 为预置内容（静态字符串），audio_id 可为 NULL。
// 本函数在持锁上下文中调用；音频播放是异步的，不阻塞 UI。
// ---------------------------------------------------------------------------
static void present(const char *title, const char *text,
                    const char *audio_id, story_face_t face) {
    story_ui_show(STORY_SCREEN_TELLING);
    story_ui_set_face(face);
    story_ui_set_story(title ? title : "", text ? text : "");
    story_ui_set_followups(NULL, 0);
    s_telling_is_story = false;   // 固定话术与追问回答都没有可点追问气泡

    esp_err_t err = ESP_ERR_NOT_FOUND;
    if (audio_id) err = story_audio_play(audio_id, NULL, NULL);
    if (err != ESP_OK) {
        // 没有对应录音：用短提示音反馈，同时屏幕文字完整呈现内容。
        ESP_LOGW(TAG, "audio_id=%s 无录音(%s)，降级为提示音+文字",
                 audio_id ? audio_id : "(null)", esp_err_to_name(err));
        (void)story_audio_play_chime(180);
    }
}

// ---------------------------------------------------------------------------
// 播放固定话术。
// ---------------------------------------------------------------------------
static void present_phrase(story_phrase_id_t pid) {
    const char *text = STORY_PHRASES[pid];
    story_face_t face = (pid == STORY_PHRASE_BOUNDARY) ? STORY_FACE_BOUNDARY
                      : (pid == STORY_PHRASE_UNCERTAIN) ? STORY_FACE_UNCERTAIN
                      : STORY_FACE_IDLE;
    present("奋斗鹅", text, NULL, face);
}

// ---------------------------------------------------------------------------
// 播放一条故事，并展示它的追问气泡。
// ---------------------------------------------------------------------------
static void present_story(const story_t *item) {
    if (!item) {
        present_phrase(STORY_PHRASE_CLARIFY);
        return;
    }
    s_ctx.current_story = item;
    s_telling_is_story = true;
    story_ui_show(STORY_SCREEN_TELLING);
    story_ui_set_face(STORY_FACE_TALK);
    story_ui_set_story(item->title, item->body);
    story_ui_set_followups(NULL, 0);

    // 把追问链的关键词首项作为气泡文案（简洁、可点）。
    // 最多 3 个，数组在栈上仅存指针，生命周期覆盖本次调用即可。
    const char *labels[3] = { NULL, NULL, NULL };
    size_t n = item->followup_count < 3 ? item->followup_count : 3;
    for (size_t i = 0; i < n; ++i) {
        const story_followup_t *fu = &item->followups[i];
        labels[i] = (fu->keyword_count > 0) ? fu->keywords[0] : "继续";
    }
    story_ui_set_followups(labels, n);
    s_followup_sel = 0;
    story_ui_highlight_followup(n > 0 ? 0 : (size_t)-1);

    esp_err_t err = story_audio_play(item->id, NULL, NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "故事 %s 无录音，降级为提示音+文字", item->id);
        (void)story_audio_play_chime(200);
    }
}

// ---------------------------------------------------------------------------
// 把引擎结果落到 UI/音频。
// ---------------------------------------------------------------------------
static void apply_result(const story_result_t *r) {
    switch (r->action) {
        case STORY_ACT_PLAY_STORY:
            present_story(r->story);
            break;
        case STORY_ACT_PLAY_FOLLOWUP:
            present(r->story ? r->story->title : "接着说",
                    r->followup ? r->followup->answer : "",
                    r->followup ? r->followup->audio_id : NULL,
                    STORY_FACE_TALK);
            // 回答后恢复原故事的追问气泡，支持连续追问。
            if (r->story) {
                const char *labels[3] = { NULL, NULL, NULL };
                size_t n = r->story->followup_count < 3 ? r->story->followup_count : 3;
                for (size_t i = 0; i < n; ++i) {
                    labels[i] = r->story->followups[i].keyword_count > 0
                              ? r->story->followups[i].keywords[0] : "继续";
                }
                story_ui_set_followups(labels, n);
                s_followup_sel = 0;
                story_ui_highlight_followup(n > 0 ? 0 : (size_t)-1);
                // 追问回答仍是"围绕故事"的上下文，保留故事态以支持连续追问。
                s_telling_is_story = true;
            }
            break;
        case STORY_ACT_PLAY_PHRASE:
            present_phrase(r->phrase);
            break;
        case STORY_ACT_PICK_THEME:
            s_theme_sel = 0;
            s_theme_story_skip = 0;
            story_ui_show(STORY_SCREEN_THEMES);
            for (size_t i = 0; i < STORY_THEME_COUNT; ++i) {
                story_ui_add_theme(STORY_THEMES[i].label);
            }
            story_ui_set_theme_selection(s_theme_sel);
            break;
        case STORY_ACT_NONE:
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// 按键处理：先在持锁状态下做 UI 与逻辑，音频调用在此之后（异步，不持锁也行）。
// ---------------------------------------------------------------------------
void story_app_on_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (!s_ready) return;
    // 只处理 CLICK 与 LONG，忽略 PRESS/DOUBLE/RELEASE，避免重复动作。
    if (ev != BSP_BTN_CLICK && ev != BSP_BTN_LONG) return;

    if (!bsp_lvgl_lock(500)) {
        ESP_LOGW(TAG, "LVGL 加锁超时，丢弃按键");
        return;
    }

    const story_screen_t screen = story_ui_current_screen();
    const bool is_long = (ev == BSP_BTN_LONG);

    if (screen == STORY_SCREEN_WELCOME) {
        if (is_long && btn == BSP_BTN_OK) {
            // 长按 OK -> 结构化提问列表
            s_question_sel = 0;
            story_ui_show(STORY_SCREEN_QUESTION);
            story_ui_set_questions(QUESTIONS, QUESTION_COUNT, s_question_sel);
        } else if (!is_long && (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN)) {
            story_result_t r = {0};
            r.action = STORY_ACT_PICK_THEME;
            apply_result(&r);
        } else if (!is_long && btn == BSP_BTN_OK) {
            // 欢迎页 OK -> 直接随机讲一个
            const story_t *item = story_engine_random((uint32_t)esp_timer_get_time());
            story_result_t r = {0};
            r.action = STORY_ACT_PLAY_STORY;
            r.story = item;
            apply_result(&r);
        }
    } else if (screen == STORY_SCREEN_THEMES) {
        if (is_long && btn == BSP_BTN_OK) {
            story_ui_show(STORY_SCREEN_WELCOME);
        } else if (!is_long && btn == BSP_BTN_UP) {
            s_theme_sel = (s_theme_sel + STORY_THEME_COUNT - 1) % STORY_THEME_COUNT;
            story_ui_set_theme_selection(s_theme_sel);
        } else if (!is_long && btn == BSP_BTN_DOWN) {
            s_theme_sel = (s_theme_sel + 1) % STORY_THEME_COUNT;
            story_ui_set_theme_selection(s_theme_sel);
        } else if (!is_long && btn == BSP_BTN_OK) {
            if (STORY_THEMES[s_theme_sel].is_random) {
                const story_t *item = story_engine_random((uint32_t)esp_timer_get_time());
                present_story(item);
            } else {
                const story_t *item = story_engine_nth_in_theme(s_theme_sel, 0);
                s_theme_story_skip = 1;
                present_story(item);
            }
        }
    } else if (screen == STORY_SCREEN_TELLING) {
        if (is_long && btn == BSP_BTN_OK) {
            story_result_t r = {0};
            r.action = STORY_ACT_PICK_THEME;
            apply_result(&r);
        } else if (!is_long && btn == BSP_BTN_UP) {
            if (s_telling_is_story && s_ctx.current_story && s_ctx.current_story->followup_count > 0) {
                size_t n = s_ctx.current_story->followup_count < 3
                         ? s_ctx.current_story->followup_count : 3;
                s_followup_sel = (s_followup_sel + n - 1) % n;
                story_ui_highlight_followup(s_followup_sel);
            }
        } else if (!is_long && btn == BSP_BTN_DOWN) {
            if (s_telling_is_story && s_ctx.current_story && s_ctx.current_story->followup_count > 0) {
                size_t n = s_ctx.current_story->followup_count < 3
                         ? s_ctx.current_story->followup_count : 3;
                s_followup_sel = (s_followup_sel + 1) % n;
                story_ui_highlight_followup(s_followup_sel);
            }
        } else if (!is_long && btn == BSP_BTN_OK) {
            // 只有当前是"故事"且有追问气泡时，OK 才播放选中追问；
            // 固定话术/追问回答页 OK 走"再讲一个"，避免误用旧故事上下文。
            if (s_telling_is_story && s_ctx.current_story &&
                s_followup_sel < s_ctx.current_story->followup_count) {
                const story_followup_t *fu = &s_ctx.current_story->followups[s_followup_sel];
                present(s_ctx.current_story->title, fu->answer,
                        fu->audio_id, STORY_FACE_TALK);
            } else {
                const story_t *next = story_engine_nth_in_theme(
                    s_ctx.current_story ? s_ctx.current_story->theme : 0,
                    s_theme_story_skip++);
                present_story(next);
            }
        }
    } else if (screen == STORY_SCREEN_QUESTION) {
        if (is_long && btn == BSP_BTN_OK) {
            story_ui_show(STORY_SCREEN_WELCOME);
        } else if (!is_long && btn == BSP_BTN_UP) {
            s_question_sel = (s_question_sel + QUESTION_COUNT - 1) % QUESTION_COUNT;
            story_ui_set_questions(QUESTIONS, QUESTION_COUNT, s_question_sel);
        } else if (!is_long && btn == BSP_BTN_DOWN) {
            s_question_sel = (s_question_sel + 1) % QUESTION_COUNT;
            story_ui_set_questions(QUESTIONS, QUESTION_COUNT, s_question_sel);
        } else if (!is_long && btn == BSP_BTN_OK) {
            story_ui_set_face(STORY_FACE_LISTEN);
            story_result_t r = story_engine_match(QUESTIONS[s_question_sel], &s_ctx);
            apply_result(&r);
        }
    }

    bsp_lvgl_unlock();
}

void story_app_tick(void) {
    if (!s_ready) return;
    if (!bsp_lvgl_lock(200)) return;
    story_ui_set_battery(bsp_battery_soc());
    bsp_lvgl_unlock();
}

esp_err_t story_app_init(void) {
    story_engine_context_reset(&s_ctx);
    s_theme_sel = 0;
    s_question_sel = 0;
    s_followup_sel = 0;
    s_theme_story_skip = 0;

    esp_err_t err = story_audio_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "音频初始化失败，将以纯文字模式运行: %s", esp_err_to_name(err));
        // 音频失败不阻塞 UI：屏幕文字仍可完整呈现故事。
    }

    if (!bsp_lvgl_lock(1000)) return ESP_ERR_TIMEOUT;
    bool ok = story_ui_init();
    if (ok) {
        story_ui_set_battery(bsp_battery_soc());
        // 上电欢迎：热情欢迎 + 松山湖 + 身份 + 邀请提问（文案见 STORY_PHRASES）
        story_ui_set_face(STORY_FACE_WELCOME);
    }
    bsp_lvgl_unlock();

    s_ready = ok;
    if (!ok) {
        ESP_LOGE(TAG, "UI 初始化失败（内存不足？）");
        return ESP_ERR_NO_MEM;
    }
    // 欢迎音频：若已录 welcome 则播放，否则提示音
    esp_err_t aerr = story_audio_play("welcome", NULL, NULL);
    if (aerr != ESP_OK) (void)story_audio_play_chime(220);
    return ESP_OK;
}

void story_app_deinit(void) {
    s_ready = false;
    story_audio_deinit();
    if (bsp_lvgl_lock(500)) {
        story_ui_deinit();
        bsp_lvgl_unlock();
    }
}
