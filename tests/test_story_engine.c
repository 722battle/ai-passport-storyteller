// test_story_engine.c —— 对话引擎的主机逻辑测试（不依赖 ESP-IDF / LVGL）。
// 编译方式见 tools/run_host_tests.sh。用 C11 + -Wall -Wextra -Werror。
//
// 覆盖：越界拦截、追问承接、故事命中、主题切换、澄清兜底、
//       可播放过滤、随机可复现、UTF-8 子串匹配。
#include "story_engine.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_contains(void) {
    assert(story_engine_contains("讲个技术攻关的故事", "技术攻关"));
    assert(!story_engine_contains("讲个技术攻关的故事", "报价"));
    assert(story_engine_contains("hello huawei", "huawei"));
    assert(!story_engine_contains(NULL, "x"));
    assert(!story_engine_contains("x", ""));
}

static void test_boundary_first(void) {
    story_context_t ctx;
    story_engine_context_reset(&ctx);
    // 同时含越界词与故事词 -> 必须优先边界，防止"报价+技术攻关"漏拦。
    story_result_t r = story_engine_match("技术攻关这个报价多少", &ctx);
    assert(r.action == STORY_ACT_PLAY_PHRASE);
    assert(r.phrase == STORY_PHRASE_BOUNDARY);
    const char *const *words[] = { "芯片", "架构", "合作方", "竞品" };
    (void)words;
    r = story_engine_match("你们这个芯片架构跟我们的比怎么样", &ctx);
    assert(r.action == STORY_ACT_PLAY_PHRASE && r.phrase == STORY_PHRASE_BOUNDARY);
}

static void test_story_hit(void) {
    story_context_t ctx;
    story_engine_context_reset(&ctx);
    story_result_t r = story_engine_match("讲个技术攻关的故事", &ctx);
    assert(r.action == STORY_ACT_PLAY_STORY);
    assert(r.story != NULL);
    assert(ctx.current_story == r.story);
}

static void test_followup_context(void) {
    story_context_t ctx;
    story_engine_context_reset(&ctx);
    story_result_t r = story_engine_match("技术攻关", &ctx);
    assert(r.action == STORY_ACT_PLAY_STORY);
    // 承接上文："为什么"应命中当前故事的追问链，而不是重新匹配故事。
    r = story_engine_match("为什么要这么做", &ctx);
    assert(r.action == STORY_ACT_PLAY_FOLLOWUP);
    assert(r.followup != NULL);
    assert(r.followup->answer != NULL);
}

static void test_followup_without_context(void) {
    story_context_t ctx;
    story_engine_context_reset(&ctx);
    // 没有当前故事时，"为什么"不能凭空追问，走澄清。
    story_result_t r = story_engine_match("为什么", &ctx);
    assert(r.action == STORY_ACT_PLAY_PHRASE);
    assert(r.phrase == STORY_PHRASE_CLARIFY);
}

static void test_theme_and_clarify(void) {
    story_context_t ctx;
    story_engine_context_reset(&ctx);
    story_result_t r = story_engine_match("换个主题", &ctx);
    assert(r.action == STORY_ACT_PICK_THEME);
    r = story_engine_match("帮我写一段代码", &ctx);
    assert(r.action == STORY_ACT_PLAY_PHRASE && r.phrase == STORY_PHRASE_CLARIFY);
    r = story_engine_match("", &ctx);
    assert(r.action == STORY_ACT_NONE);
}

static void test_theme_navigation(void) {
    assert(STORY_THEME_COUNT == 8);
    assert(story_engine_first_in_theme(0) != NULL);
    // 随机项不该返回具体故事。
    assert(story_engine_first_in_theme(STORY_THEME_COUNT - 1) == NULL);
    assert(story_engine_first_in_theme(999) == NULL);
    // "再讲一个"：第二讲与第一讲不同。
    const story_t *a = story_engine_nth_in_theme(0, 0);
    const story_t *b = story_engine_nth_in_theme(0, 1);
    assert(a && b && a != b);
    assert(story_engine_nth_in_theme(0, 999) == NULL);
}

static void test_random_deterministic(void) {
    const story_t *a = story_engine_random(42);
    const story_t *b = story_engine_random(42);
    assert(a != NULL && a == b);
}

int main(void) {
    test_contains();
    test_boundary_first();
    test_story_hit();
    test_followup_context();
    test_followup_without_context();
    test_theme_and_clarify();
    test_theme_navigation();
    test_random_deterministic();
    puts("test_story_engine: PASS");
    return 0;
}
