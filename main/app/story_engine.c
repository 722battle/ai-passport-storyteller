// story_engine.c —— 本地意图匹配实现。细节见 story_engine.h。
//
// 设计要点：
//   - 无动态内存、无全局可变状态：所有状态由调用方通过 story_context_t 持有，
//     便于多任务/重入场景下推理，也便于主机测试。
//   - 匹配顺序固定，先拦越界，再判追问，再判故事，最后澄清。
//   - 所有返回内容都指向 story_data 的静态 const 数据。
#include "story_engine.h"

#include <string.h>

// 越界词：命中即走边界话术，不讲任何相关内容。
// 这是接待侧红线的代码化：报价、合同、合作方、竞品、技术路线、客户比较。
static const char *const BOUNDARY_KEYWORDS[] = {
    "报价", "价格", "多少钱", "合同", "条款", "招标", "采购",
    "合作方", "伙伴", "供应商", "竞品", "对手", "对比", "比一比", "谁更强",
    "架构", "芯片", "参数", "技术路线", "选型", "你们的产品",
};
static const size_t BOUNDARY_KEYWORD_COUNT =
    sizeof(BOUNDARY_KEYWORDS) / sizeof(BOUNDARY_KEYWORDS[0]);

// 主题切换 / 控制类词：命中后让 UI 进入主题轮盘或直接播放固定话术。
static const char *const THEME_KEYWORDS[] = { "主题", "换一个", "别的", "挑一个", "菜单" };
static const size_t THEME_KEYWORD_COUNT = sizeof(THEME_KEYWORDS) / sizeof(THEME_KEYWORDS[0]);

static const char *const REVISIT_KEYWORDS[] = { "又见面", "上次", "接着讲" };
static const size_t REVISIT_KEYWORD_COUNT = sizeof(REVISIT_KEYWORDS) / sizeof(REVISIT_KEYWORDS[0]);

bool story_engine_contains(const char *haystack, const char *needle) {
    if (!haystack || !needle || needle[0] == '\0') return false;
    return strstr(haystack, needle) != NULL;
}

// 在词表中任一命中即返回 true。
static bool matches_any(const char *query, const char *const *words, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (story_engine_contains(query, words[i])) return true;
    }
    return false;
}

void story_engine_context_reset(story_context_t *ctx) {
    if (!ctx) return;
    ctx->current_story = NULL;
    ctx->turn_count = 0;
}

// 在一组词表中找到第一个命中项的数组下标；无命中返回 SIZE_MAX。
static size_t find_keyword_index(const char *query,
                                 const char *const *words, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (story_engine_contains(query, words[i])) return i;
    }
    return (size_t)-1;
}

// 在当前故事的追问链里找命中项。
static const story_followup_t *match_followup(const story_t *story, const char *query) {
    if (!story || !query) return NULL;
    for (size_t i = 0; i < story->followup_count; ++i) {
        const story_followup_t *fu = &story->followups[i];
        if (matches_any(query, fu->keywords, fu->keyword_count)) return fu;
    }
    return NULL;
}

// 判断一条故事是否命中 query 的关键词。
static bool story_matches_query(const story_t *story, const char *query) {
    if (!story_data_playable(story) || !query) return false;
    return matches_any(query, story->keywords, story->keyword_count);
}

story_result_t story_engine_match(const char *query, story_context_t *ctx) {
    story_result_t r = {0};
    r.action = STORY_ACT_NONE;

    if (!query || query[0] == '\0') return r;
    if (ctx) ctx->turn_count++;

    // 1) 越界优先：即使同时含"技术攻关"等词，也先拦下来。
    if (matches_any(query, BOUNDARY_KEYWORDS, BOUNDARY_KEYWORD_COUNT)) {
        r.action = STORY_ACT_PLAY_PHRASE;
        r.phrase = STORY_PHRASE_BOUNDARY;
        return r;
    }

    // 2) 追问：必须有当前故事，且命中当前故事的追问链。
    //    注意：这步放在故事匹配之前，保证"为什么这么做"不会误命中
    //    某条以"为什么"为关键词的故事。
    if (ctx && ctx->current_story) {
        const story_followup_t *fu = match_followup(ctx->current_story, query);
        if (fu) {
            r.action = STORY_ACT_PLAY_FOLLOWUP;
            r.story = ctx->current_story;
            r.followup = fu;
            return r;
        }
    }

    // 3) 故事关键词匹配：按内容包顺序取第一条命中且可播放的。
    for (size_t i = 0; i < story_data_count(); ++i) {
        const story_t *item = story_data_get(i);
        if (story_matches_query(item, query)) {
            r.action = STORY_ACT_PLAY_STORY;
            r.story = item;
            if (ctx) ctx->current_story = item;
            return r;
        }
    }

    // 4) 主题轮盘 / 复访话术。
    if (matches_any(query, THEME_KEYWORDS, THEME_KEYWORD_COUNT)) {
        r.action = STORY_ACT_PICK_THEME;
        return r;
    }
    if (matches_any(query, REVISIT_KEYWORDS, REVISIT_KEYWORD_COUNT)) {
        r.action = STORY_ACT_PLAY_PHRASE;
        r.phrase = STORY_PHRASE_REVISIT;
        return r;
    }

    // 5) 兜底澄清：绝不编造，引导用户到已知的可选范围。
    r.action = STORY_ACT_PLAY_PHRASE;
    r.phrase = STORY_PHRASE_CLARIFY;
    return r;
}

const story_t *story_engine_first_in_theme(size_t theme_index) {
    return story_engine_nth_in_theme(theme_index, 0);
}

const story_t *story_engine_nth_in_theme(size_t theme_index, size_t skip) {
    if (theme_index >= STORY_THEME_COUNT) return NULL;
    if (STORY_THEMES[theme_index].is_random) return NULL;
    size_t seen = 0;
    for (size_t i = 0; i < story_data_count(); ++i) {
        const story_t *item = story_data_get(i);
        if (!story_data_playable(item) || item->theme != theme_index) continue;
        if (seen == skip) return item;
        seen++;
    }
    return NULL;
}

const story_t *story_engine_random(uint32_t seed) {
    // 收集所有可播放故事的下标，再做一次简单取模。
    // 8MB/无 PSRAM 下不建数组，用两次遍历（water reservoir 的单样本特例）。
    size_t picked = (size_t)-1;
    size_t n = 0;
    for (size_t i = 0; i < story_data_count(); ++i) {
        const story_t *item = story_data_get(i);
        if (!story_data_playable(item)) continue;
        // 以 1/(n+1) 概率替换；用 seed+i 做确定性扰动，保证主机测试可复现。
        uint32_t r = seed + (uint32_t)i * 2654435761u + 1u;
        if ((r % (n + 1)) == 0) picked = i;
        n++;
    }
    if (picked == (size_t)-1) return NULL;
    return story_data_get(picked);
}
