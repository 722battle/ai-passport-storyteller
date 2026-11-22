# -*- coding: utf-8 -*-
"""story_engine 的 Python 镜像测试（无需 C 编译器 / ESP-IDF）。

用途：在没有 C 工具链的环境里，验证「意图匹配逻辑」的正确性：
越界优先、追问承接、故事命中、主题切换、澄清兜底、可播放过滤、随机可复现。
逻辑与 main/app/story_engine.c 一一对应；真机前仍需用 C 版测试复核。

运行：python tools/test_story_engine.py
"""
from __future__ import annotations

import sys
from dataclasses import dataclass, field
from typing import Optional

# ---------------------------------------------------------------------------
# 镜像数据：与 story_data.c 的关键字段保持一致（关键词 + 可播放状态）。
# 正式部署以 C 表为准；这里只镜像「会影响匹配结果」的字段。
# ---------------------------------------------------------------------------
BOUNDARY = ["报价", "价格", "多少钱", "合同", "条款", "招标", "采购",
            "合作方", "伙伴", "供应商", "竞品", "对手", "对比", "比一比", "谁更强",
            "架构", "芯片", "参数", "技术路线", "选型", "你们的产品"]
THEME_WORDS = ["主题", "换一个", "别的", "挑一个", "菜单"]
REVISIT_WORDS = ["又见面", "上次", "接着讲"]

STORY_ALLOW_UNVERIFIED = True  # 对应 STORY_ALLOW_UNVERIFIED=1


@dataclass
class Followup:
    keywords: list[str]
    answer: str


@dataclass
class Story:
    id: str
    theme: int
    keywords: list[str]
    verified: bool = False
    followups: list[Followup] = field(default_factory=list)


STORIES = [
    Story("metaerp_why_selfbuild", 0, ["攻关", "技术攻关", "难题", "突破"],
          followups=[Followup(["为什么", "为何", "为啥"], "自主可控理由"),
                     Followup(["出处", "来源", "有依据吗", "真的吗"], "来源说明")]),
    Story("compute_platform_accumulation", 0, ["攻关", "技术攻关", "难题", "突破"],
          followups=[Followup(["为什么", "为何", "为啥"], "平台理由"),
                     Followup(["后来", "之后", "结果呢", "怎么样了"], "后续说明")]),
    Story("review_culture", 2, ["失败", "复盘", "踩坑", "走了弯路"]),
    Story("frontline_decision", 3, ["协作", "团队", "配合"],
          followups=[Followup(["为什么", "为何", "为啥"], "一线决策理由")]),
    Story("customer_problem_to_requirement", 4, ["客户需求", "客户问题", "需求怎么来"]),
    Story("songshan_lab_culture", 5, ["松山湖", "园区", "这栋楼"]),
    Story("culture_facts_first", 1, ["研发文化", "文化", "氛围"]),
    Story("value_long_term", 6, ["价值观", "为什么这么做", "理念"]),
]
THEMES = ["技术攻关", "研发文化", "失败与复盘", "协作方式",
          "客户问题变需求", "松山湖的故事", "价值观溯源", "随机一个"]


def playable(s: Story) -> bool:
    # 对应 story_data_playable：UNKNOWN 不入库；本镜像无 UNKNOWN 条目。
    return STORY_ALLOW_UNVERIFIED or s.verified


def contains(haystack: str | None, needle: str) -> bool:
    return bool(haystack and needle and needle in haystack)


def matches_any(query: str, words: list[str]) -> bool:
    return any(contains(query, w) for w in words)


@dataclass
class Context:
    current_story: Optional[Story] = None
    turn_count: int = 0


@dataclass
class Result:
    action: str
    story: Optional[Story] = None
    followup: Optional[Followup] = None
    phrase: Optional[str] = None


def match(query: str | None, ctx: Context) -> Result:
    if not query:
        return Result("NONE")
    ctx.turn_count += 1

    # 1) 越界优先
    if matches_any(query, BOUNDARY):
        return Result("PHRASE", phrase="BOUNDARY")

    # 2) 追问（须有当前故事）
    if ctx.current_story:
        for fu in ctx.current_story.followups:
            if matches_any(query, fu.keywords):
                return Result("FOLLOWUP", story=ctx.current_story, followup=fu)

    # 3) 故事关键词
    for s in STORIES:
        if playable(s) and matches_any(query, s.keywords):
            ctx.current_story = s
            return Result("STORY", story=s)

    # 4) 主题 / 复访
    if matches_any(query, THEME_WORDS):
        return Result("PICK_THEME")
    if matches_any(query, REVISIT_WORDS):
        return Result("PHRASE", phrase="REVISIT")

    # 5) 澄清兜底
    return Result("PHRASE", phrase="CLARIFY")


def nth_in_theme(theme: int, skip: int) -> Optional[Story]:
    seen = 0
    for s in STORIES:
        if not playable(s) or s.theme != theme:
            continue
        if seen == skip:
            return s
        seen += 1
    return None


def random_story(seed: int) -> Optional[Story]:
    picked = None
    n = 0
    for i, s in enumerate(STORIES):
        if not playable(s):
            continue
        r = (seed + i * 2654435761 + 1) & 0xFFFFFFFF
        if r % (n + 1) == 0:
            picked = s
        n += 1
    return picked


# ---------------------------------------------------------------------------
# 测试
# ---------------------------------------------------------------------------
def check(cond: bool, msg: str) -> None:
    if not cond:
        raise AssertionError(msg)


def main() -> int:
    c = Context()
    check(contains("讲个技术攻关的故事", "技术攻关"), "contains 命中失败")
    check(not contains("讲个技术攻关的故事", "报价"), "contains 误命中")

    check(match("技术攻关这个报价多少", c).phrase == "BOUNDARY", "越界优先失败")
    check(match("你们这个芯片架构跟我们的比怎么样", c).phrase == "BOUNDARY", "越界词失败")

    c = Context()
    r = match("讲个技术攻关的故事", c)
    check(r.action == "STORY" and r.story is not None, "故事命中失败")
    check(c.current_story is r.story, "上下文未更新")

    r = match("为什么要这么做", c)
    check(r.action == "FOLLOWUP" and r.followup is not None, "追问承接失败")

    c = Context()
    check(match("为什么", c).phrase == "CLARIFY", "无上下文追问应澄清")

    c = Context()
    check(match("换个主题", c).action == "PICK_THEME", "主题切换失败")
    check(match("帮我写一段代码", c).phrase == "CLARIFY", "澄清兜底失败")
    check(match("", c).action == "NONE", "空输入处理失败")

    check(len(THEMES) == 8, "主题数错误")
    check(nth_in_theme(0, 0) is not None, "主题首个故事为空")
    check(nth_in_theme(7, 0) is None, "随机项不应返回具体故事")
    check(nth_in_theme(0, 0) is not nth_in_theme(0, 1), "再讲一个未切换")
    check(nth_in_theme(0, 999) is None, "越界 skip 应返回 None")

    check(random_story(42) is random_story(42), "随机结果不可复现")

    print("test_story_engine.py: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
