// story_engine.h —— 本地对话意图引擎（纯 C，无 ESP-IDF / LVGL 依赖）。
//
// 职责：把「用户输入的一段文本」解析成一个明确的动作，交给 UI 播放。
// 本引擎不做任何生成，只做匹配：命中预置内容 -> 返回内容 ID；未命中 -> 澄清；
// 越界 -> 边界话术。所有回答都来自 story_data.h 的预置数据。
//
// 为什么不用语音识别：ESP32-C3 无 PSRAM、8MB Flash，离网跑不了通用 ASR。
// 本引擎的输入是「已经转成文本」的query，来源可以是：
//   - 屏幕上的主题/问题列表（本期实现，靠三个按键选择，离线可用）
//   - 外部接入的关键词识别模块（预留接口，见 story_input.h 的说明）
// 两种来源都走同一个 engine_match()，保证行为一致、可测试。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "story_data.h"

// 引擎给出的动作类型。
typedef enum {
    STORY_ACT_NONE = 0,        // 无动作（空输入）
    STORY_ACT_PLAY_STORY,      // 播放某条故事
    STORY_ACT_PLAY_FOLLOWUP,   // 播放某条故事的某条追问回答
    STORY_ACT_PLAY_PHRASE,     // 播放固定话术（欢迎/澄清/边界等）
    STORY_ACT_PICK_THEME,      // 让 UI 进入主题轮盘
} story_action_t;

// 一次匹配的完整结果。所有指针都指向 story_data 的静态只读数据，
// 调用方不得修改，也无需释放。
typedef struct {
    story_action_t action;
    const story_t *story;          // PLAY_STORY / PLAY_FOLLOWUP 时有效
    const story_followup_t *followup;
    story_phrase_id_t phrase;      // PLAY_PHRASE 时有效
    size_t theme_index;            // PICK_THEME 时有效
} story_result_t;

// 会话上下文：记录当前故事，让"后来呢""为什么"能承接上文。
// 由调用方持有并在一次会话内传入；会话结束清零。
typedef struct {
    const story_t *current_story;  // 当前正在讲的故事，NULL 表示没有
    uint32_t turn_count;           // 会话轮次，仅统计用
} story_context_t;

// 初始化上下文（会话开始 / 设备重启后调用）。
void story_engine_context_reset(story_context_t *ctx);

// 核心匹配：给定文本 query 与上下文，返回动作结果。
// 参数：
//   query - UTF-8 文本，允许为 NULL（返回 NONE）；不修改其内容。
//   ctx   - 会话上下文，匹配到故事/追问后会更新 current_story。
// 返回：story_result_t，纯值语义，无堆分配。
// 副作用：可能更新 ctx->current_story 与 ctx->turn_count。
// 匹配优先级（重要，避免误判）：
//   1. 越界词 -> 边界话术（最高，防止"报价+技术攻关"混说时漏拦）
//   2. 追问触发词 + 有当前故事 -> 匹配追问链
//   3. 故事关键词 -> 播放故事
//   4. 主题/控制词 -> 进入主题轮盘或播放固定话术
//   5. 其他 -> 澄清话术
story_result_t story_engine_match(const char *query, story_context_t *ctx);

// 按主题取第一可播放故事；theme_index 越界或主题为"随机一个"时返回 NULL。
// 用于主题轮盘按下 OK 后选片。
const story_t *story_engine_first_in_theme(size_t theme_index);

// 取某主题下第 skip 个可播放故事（skip 从 0 起）。用于"再讲一个"。
const story_t *story_engine_nth_in_theme(size_t theme_index, size_t skip);

// 随机挑一条可播放故事。seed 由调用方给出（如 esp_timer 读值），
// 主机测试可传固定值以复现结果。
const story_t *story_engine_random(uint32_t seed);

// UTF-8 子串包含判断：haystack 中是否出现 needle。
// 逐字节比较即可正确匹配多字节 UTF-8（只要边界落在字符起点，
// 我们的关键词表都是完整词，满足该前提）。
bool story_engine_contains(const char *haystack, const char *needle);
