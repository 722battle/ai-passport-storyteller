// story_data.c —— 由 tools/gen_content.py 从审核 CSV 自动生成，请勿手改。
// 源文件: stories.csv
// 故事数: 8
#include "story_data.h"
#include <string.h>

// 主题轮盘
const story_theme_t STORY_THEMES[] = {
    { "技术攻关", false },
    { "研发文化", false },
    { "失败与复盘", false },
    { "协作方式", false },
    { "客户问题变需求", false },
    { "松山湖的故事", false },
    { "价值观溯源", false },
    { "随机一个", true },
};
const size_t STORY_THEME_COUNT = sizeof(STORY_THEMES) / sizeof(STORY_THEMES[0]);

// 去重关键词表
static const char *const KW_0[] = { "攻关", "技术攻关", "难题", "突破" };
static const char *const KW_1[] = { "为什么", "为何" };
static const char *const KW_2[] = { "出处", "来源" };
static const char *const KW_3[] = { "技术攻关", "平台", "算力" };
static const char *const KW_4[] = { "后来", "结果呢" };
static const char *const KW_5[] = { "失败", "复盘", "踩坑", "走了弯路" };
static const char *const KW_6[] = { "协作", "团队", "配合" };
static const char *const KW_7[] = { "客户需求", "客户问题", "需求怎么来" };
static const char *const KW_8[] = { "松山湖", "园区", "这栋楼" };
static const char *const KW_9[] = { "研发文化", "文化", "氛围" };
static const char *const KW_10[] = { "价值观", "为什么这么做", "理念" };

static const story_followup_t FU_story_metaerp[] = {
    { KW_1, 2, "因为关键系统受制于人时业务连续性会受影响把能力掌握在自己手里是给业务留一条自己的路", "story_metaerp_fu0" },
    { KW_2, 2, "对应华为公开材料里关于自主可控与数字化实践的表述", "story_metaerp_fu1" },
};
static const story_followup_t FU_story_compute[] = {
    { KW_1, 2, "算力不是目的是把研究更快变成可交付的东西工具链的积累往往比单次突破更长久", "story_compute_fu0" },
    { KW_4, 2, "后来这套思路被沉淀进日常研发流程新项目一开始就能站在已有的平台上", "story_compute_fu1" },
};
static const story_followup_t FU_story_team[] = {
    { KW_1, 2, "复杂问题靠单点英雄解决不了让听得见炮声的人参与决策是减少返工最直接的办法", "story_team_fu0" },
};

// 故事表
static const story_t STORY_ITEMS_INTERNAL[] = {
    {
        .id = "story_metaerp", .theme = 0, .title = "为什么要自己做一套企业系统",
        .body = "企业核心系统一旦受制于人业务连续性就会打折扣。当时面对的具体问题是关键系统必须可控又要支撑全球业务运转。团队没有一上来就写代码而是先把业务流程一条条梳理清楚再决定系统怎么建。走通之后沉淀下来的不只是一套软件而是把复杂业务拆成可管理模块的工程方法。",
        .source = "华为公开材料 数字化转型实践（待补URL）",
        .confidence = STORY_CONF_MIXED, .verified = false,
        .keywords = KW_0, .keyword_count = 4,
        .followups = FU_story_metaerp, .followup_count = 2,
    },
    {
        .id = "story_compute", .theme = 0, .title = "算力背后的平台思路",
        .body = "研发里常见的处境是每个项目都从零搭环境时间都花在重复的事情上。团队的做法是先把共性能力做成平台让上层项目直接复用。这样一次投入后面很多项目都省了下来。这件事留下的经验是把不确定性挡在平台层让业务开发跑在稳定的地基上。",
        .source = "华为官网 算力与计算生态公开介绍（待补URL）",
        .confidence = STORY_CONF_MIXED, .verified = false,
        .keywords = KW_3, .keyword_count = 3,
        .followups = FU_story_compute, .followup_count = 2,
    },
    {
        .id = "story_review", .theme = 2, .title = "把复盘写成习惯",
        .body = "项目走弯路不可怕可怕的是同一个坑掉两次。研发团队的做法是阶段结束后不只看结果也回头看当时为什么那么判断哪里信息不够。把这些写下来下一个项目就能少走一段。复盘不是追责是把个人经验变成团队共有的判断力。",
        .source = "华为公开材料 自我批判持续改进（待补URL）",
        .confidence = STORY_CONF_MIXED, .verified = false,
        .keywords = KW_5, .keyword_count = 4,
        .followups = NULL, .followup_count = 0,
    },
    {
        .id = "story_team", .theme = 3, .title = "让听得见炮声的人参与决策",
        .body = "一线最清楚客户真正卡在哪里但一线往往离资源最远。管理的难点就是让这两端连起来。做法是把决策往前放让了解现场的人有话语权后方团队负责提供炮火支援。这样方案从一开始就更贴近真实问题返工也少。",
        .source = "华为公开材料 以客户为中心 一线呼唤炮火（待补URL）",
        .confidence = STORY_CONF_MIXED, .verified = false,
        .keywords = KW_6, .keyword_count = 3,
        .followups = FU_story_team, .followup_count = 1,
    },
    {
        .id = "story_customer", .theme = 4, .title = "客户问题怎么变成研发需求",
        .body = "客户说出来的常常是现象不是需求本身。研发要做的第一步是把现象翻译成可验证的问题在什么场景什么条件下希望达到什么结果。翻译清楚了后面的设计才有靶子。这一步做扎实比多写几版方案更值钱。",
        .source = "华为官网 以客户为中心（待补URL）",
        .confidence = STORY_CONF_MIXED, .verified = false,
        .keywords = KW_7, .keyword_count = 3,
        .followups = NULL, .followup_count = 0,
    },
    {
        .id = "story_songshan", .theme = 5, .title = "松山湖的研发日常",
        .body = "你现在所在的松山湖园区研发日常里有一件很朴素的事遇到问题先到现场看再回到图纸前讨论。很多看起来是技术上的分歧到了现场常常变成对同一现象的不同描述。把大家拉到同一个事实面前讨论就快很多。",
        .source = "园区公开介绍 现场讲解口径（待核准）",
        .confidence = STORY_CONF_MIXED, .verified = false,
        .keywords = KW_8, .keyword_count = 3,
        .followups = NULL, .followup_count = 0,
    },
    {
        .id = "story_culture", .theme = 1, .title = "研发文化里的事实优先",
        .body = "研发讨论最容易跑偏的地方是把观点当成事实。一个比较稳的习惯是先问证据是什么边界条件是什么。把这两点说清楚争论就有了共同的地基。这看起来是方法问题其实也是文化问题允许不同意见但都要落在事实上。",
        .source = "华为公开材料 以客户为中心 自我批判（待补URL）",
        .confidence = STORY_CONF_MIXED, .verified = false,
        .keywords = KW_9, .keyword_count = 3,
        .followups = NULL, .followup_count = 0,
    },
    {
        .id = "story_value", .theme = 6, .title = "为什么愿意做长期的事",
        .body = "有些投入短期内看不到回报比如基础研究工具平台人才培养。判断要不要做靠的是把时间轴拉长今天省下的投入会不会变成明天的瓶颈。价值观在这里不是口号而是做取舍时的那把尺子。",
        .source = "华为官网 愿景与使命 研发投入公开数据（待补URL）",
        .confidence = STORY_CONF_MIXED, .verified = false,
        .keywords = KW_10, .keyword_count = 3,
        .followups = NULL, .followup_count = 0,
    },
};

const story_t *story_data_get(size_t index) {
    if (index >= sizeof(STORY_ITEMS_INTERNAL)/sizeof(STORY_ITEMS_INTERNAL[0])) return NULL;
    return &STORY_ITEMS_INTERNAL[index];
}
size_t story_data_count(void) {
    return sizeof(STORY_ITEMS_INTERNAL)/sizeof(STORY_ITEMS_INTERNAL[0]);
}
const story_t *story_data_find(const char *id) {
    if (!id) return NULL;
    for (size_t i = 0; i < story_data_count(); ++i) {
        const story_t *it = story_data_get(i);
        if (it->id && strcmp(it->id, id) == 0) return it;
    }
    return NULL;
}
bool story_data_playable(const story_t *item) {
    if (!item) return false;
    if (item->confidence == STORY_CONF_UNKNOWN) return false;
#if STORY_ALLOW_UNVERIFIED
    return true;
#else
    return item->verified;
#endif
}

// 固定话术（欢迎/澄清/边界/离线/复访），与需求方案一致
const char *const STORY_PHRASES[STORY_PHRASE_COUNT] = {
    [STORY_PHRASE_WELCOME] = "欢迎来到华为松山湖研发中心！我是陪你走这一段的研发故事讲述者。按住这个键直接问我，或者用上下键挑个主题都行。",
    [STORY_PHRASE_IDLE_PROMPT] = "想听什么？可以直接问我，或者用上下键挑个主题。",
    [STORY_PHRASE_CLARIFY] = "这个我没太听明白。你可以说个主题，比如技术攻关、研发文化，或者用上下键挑一个。",
    [STORY_PHRASE_BOUNDARY] = "技术细节和比较我讲不了，我只负责讲研发故事。这部分请找对接同事。",
    [STORY_PHRASE_UNCERTAIN] = "这个细节我没有可靠来源，不乱讲。我给你讲有据可查的部分好吗？",
    [STORY_PHRASE_OFFLINE] = "我现在是离网运行的，所有故事都在本机，随时可以讲。",
    [STORY_PHRASE_REVISIT] = "又见面了。接着上次那个话题，还是换个？",
};
