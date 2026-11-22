// story_data.h —— 离网内容包的数据结构与只读数据声明。
//
// 职责：定义主题、故事、追问链、话术的数据结构，并声明由 story_data.c 提供的
// 只读内容表。本头文件不依赖 ESP-IDF / LVGL，可被主机测试直接包含。
//
// 内容边界（重要）：
//   1. 本表是「工程示例内容」，每条 story 都带 source 与 verified 字段。
//      verified=0 的条目在正式固件中不得对客户播放；调试时由
//      STORY_ALLOW_UNVERIFIED 控制是否放行（见下）。
//   2. 证据不足的细节不要写进 body；宁可少讲，不可编造。
//   3. 真实部署前必须用 tools/gen_content.py 从审核过的 CSV 重新生成本文件。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 生产环境必须设为 0：只播放 verified=true 且 confidence 达标的内容。
// 设为 1 仅供开发调试，让未审核的示例文案也能播放，便于走通全流程。
#ifndef STORY_ALLOW_UNVERIFIED
#define STORY_ALLOW_UNVERIFIED 1
#endif

// 置信度：与需求方案 §8.3 的三档表达对应。
typedef enum {
    STORY_CONF_CONFIRMED = 0,   // 有据可查，可正常讲
    STORY_CONF_MIXED,           // 公开资料表述不一，需加限定语
    STORY_CONF_UNKNOWN,         // 暂无可靠来源，禁止进入内容包
} story_confidence_t;

// 一条追问链：用户问法关键词 -> 回答文本。
// 每条回答必须来自已审核条目，运行时只做匹配与播放，不生成新内容。
typedef struct {
    const char *const *keywords;   // 触发词数组（UTF-8 子串匹配，任一命中即可）
    size_t keyword_count;
    const char *answer;            // 预置回答全文
    const char *audio_id;          // 对应预录音频资源 ID（见 story_audio.h）
} story_followup_t;

// 一条研发故事。
typedef struct {
    const char *id;                // 稳定 ID，日志与反馈按此追溯
    uint8_t theme;                 // 所属主题下标，对应 STORY_THEMES
    const char *title;             // 标题（屏幕显示）
    const char *body;              // 讲解正文（预录音频对应同一份文本）
    const char *source;            // 来源，如 "华为官网/年报/公开讲话 <URL或页码>"
    story_confidence_t confidence;
    bool verified;                 // 是否已经内容责任人审核
    const char *const *keywords;   // 本地意图匹配关键词
    size_t keyword_count;
    const story_followup_t *followups;
    size_t followup_count;
} story_t;

// 主题轮盘的一项。"随机一个"是特殊项，不对应具体故事。
typedef struct {
    const char *label;
    bool is_random;
} story_theme_t;

// 全局只读表（主题轮盘）。
extern const story_theme_t STORY_THEMES[];
extern const size_t STORY_THEME_COUNT;

// 故事表的访问接口。用访问函数而非直接暴露数组，便于 gen_content.py
// 生成时替换底层数据而调用方不变；同时让主机测试只依赖本头文件。
size_t story_data_count(void);
const story_t *story_data_get(size_t index);

// 固定话术（欢迎 / 澄清 / 边界拒绝 / 离线提示 / 未命中）。
// 全部为预置文本，播放时配预录音频，不允许运行时拼装新事实。
typedef enum {
    STORY_PHRASE_WELCOME = 0,      // 上电欢迎，点出松山湖研发中心 + 身份 + 邀请提问
    STORY_PHRASE_IDLE_PROMPT,      // 待机提示
    STORY_PHRASE_CLARIFY,          // 未命中后的澄清
    STORY_PHRASE_BOUNDARY,         // 越界拒绝（报价/架构比较/合作等）
    STORY_PHRASE_UNCERTAIN,        // 无可靠来源
    STORY_PHRASE_OFFLINE,          // 离网状态说明
    STORY_PHRASE_REVISIT,          // 复访短问候
    STORY_PHRASE_COUNT,
} story_phrase_id_t;

extern const char *const STORY_PHRASES[STORY_PHRASE_COUNT];

// 按 ID 查找故事；找不到返回 NULL。
const story_t *story_data_find(const char *id);

// 判断某条故事是否允许播放（生产环境过滤未审核/低置信度条目）。
bool story_data_playable(const story_t *item);
