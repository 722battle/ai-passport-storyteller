# -*- coding: utf-8 -*-
"""从审核过的 CSV 生成 story_data.c（内容包）。

用途：需求方案要求内容可审核、可追溯、可更新。运营/内容责任人维护 CSV，
本脚本生成 C 源文件，保证每条故事都带 source / verified / confidence。

CSV 格式（UTF-8，首行为表头）：
    id,theme,title,body,source,verified,confidence,keywords,followups
字段说明：
    theme       主题下标（0-7，见 story_data.h 的 STORY_THEMES）
    body        正文（逗号请用中文逗号或用引号包裹）
    verified    true/false，是否经内容责任人审核
    confidence  confirmed/mixed/unknown
    keywords    英文分号分隔，如 "攻关;技术攻关;难题"
    followups   追问链，格式 "关键词组=>回答|关键词组=>回答"，
                关键词组内用英文分号分隔，如
                "为什么;为何=>因为...|出处;来源=>见官网..."

用法：
    python tools/gen_content.py content/stories.csv

⚠ 生成后必须重新跑字体覆盖检查与字库生成（新增了文案字符）：
    python tools/check_font_coverage.py && bash tools/make_font.sh
"""
from __future__ import annotations

import csv
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OUT = REPO / "main" / "app" / "story_data.c"

CONF_MAP = {"confirmed": "STORY_CONF_CONFIRMED",
            "mixed": "STORY_CONF_MIXED",
            "unknown": "STORY_CONF_UNKNOWN"}


def cstr(s: str) -> str:
    """转义为 C 字符串字面量（保留 UTF-8 中文原文）。"""
    return (s.replace("\\", "\\\\").replace('"', '\\"')
             .replace("\n", "\\n").replace("\r", ""))


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    csv_path = Path(sys.argv[1])
    if not csv_path.is_file():
        print(f"CSV 不存在: {csv_path}", file=sys.stderr)
        return 1

    with csv_path.open(encoding="utf-8-sig", newline="") as f:
        rows = list(csv.DictReader(f))

    # 收集所有关键词组，去重后生成静态数组，故事表只引用。
    keyword_sets: list[list[str]] = []
    kw_index: dict[tuple[str, ...], int] = {}

    def intern(words: list[str]) -> int:
        key = tuple(words)
        if key not in kw_index:
            kw_index[key] = len(keyword_sets)
            keyword_sets.append(words)
        return kw_index[key]

    story_defs = []
    for row in rows:
        sid = row["id"].strip()
        theme = int(row["theme"])
        title = cstr(row["title"].strip())
        body = cstr(row["body"].strip())
        source = cstr(row["source"].strip())
        verified = "true" if row["verified"].strip().lower() == "true" else "false"
        conf = CONF_MAP.get(row["confidence"].strip().lower(), "STORY_CONF_MIXED")
        kws = [w.strip() for w in row["keywords"].split(";") if w.strip()]
        kw_i = intern(kws)

        # 解析追问链
        fus = []
        for part in row.get("followups", "").split("|"):
            if "=>" not in part:
                continue
            kpart, answer = part.split("=>", 1)
            fkws = [w.strip() for w in kpart.split(";") if w.strip()]
            if not fkws or not answer.strip():
                continue
            fus.append((intern(fkws), cstr(answer.strip()), sid + "_fu" + str(len(fus))))

        story_defs.append((sid, theme, title, body, source, verified, conf, kw_i, fus))

    lines: list[str] = [
        "// story_data.c —— 由 tools/gen_content.py 从审核 CSV 自动生成，请勿手改。",
        f"// 源文件: {csv_path.name}",
        f"// 故事数: {len(story_defs)}",
        '#include "story_data.h"',
        "#include <string.h>",
        "",
        "// 主题轮盘",
        "const story_theme_t STORY_THEMES[] = {",
    ]
    theme_labels = ["技术攻关", "研发文化", "失败与复盘", "协作方式",
                    "客户问题变需求", "松山湖的故事", "价值观溯源", "随机一个"]
    for i, label in enumerate(theme_labels):
        rand = "true" if i == len(theme_labels) - 1 else "false"
        lines.append(f'    {{ "{label}", {rand} }},')
    lines += [
        "};",
        "const size_t STORY_THEME_COUNT = sizeof(STORY_THEMES) / sizeof(STORY_THEMES[0]);",
        "",
        "// 去重关键词表",
    ]
    for i, words in enumerate(keyword_sets):
        arr = f"KW_{i}"
        lines.append(f"static const char *const {arr}[] = {{ " +
                     ", ".join('"' + cstr(w) + '"' for w in words) + " };")
    lines.append("")

    # 每条故事的追问数组：一条故事的所有追问必须在【同一个数组】里，
    # .followups 指向该数组首元素，followup_count 为总数。
    # 之前每个追问各生成一个数组导致 fu1 未被引用，-Werror=unused-const-variable 报错。
    for sid, theme, title, body, source, verified, conf, kw_i, fus in story_defs:
        if not fus:
            continue
        arr = "FU_" + "".join(c if c.isalnum() else "_" for c in sid)
        lines.append(f"static const story_followup_t {arr}[] = {{")
        for fkw_i, answer, audio_id in fus:
            lines.append(f'    {{ KW_{fkw_i}, {len(keyword_sets[fkw_i])}, "{answer}", "{audio_id}" }},')
        lines.append("};")

    lines += ["", "// 故事表", "static const story_t STORY_ITEMS_INTERNAL[] = {"]
    for idx, (sid, theme, title, body, source, verified, conf, kw_i, fus) in enumerate(story_defs):
        lines.append("    {")
        lines.append(f'        .id = "{sid}", .theme = {theme}, .title = "{title}",')
        lines.append(f'        .body = "{body}",')
        lines.append(f'        .source = "{source}",')
        lines.append(f"        .confidence = {conf}, .verified = {verified},")
        lines.append(f"        .keywords = KW_{kw_i}, .keyword_count = {len(keyword_sets[kw_i])},")
        if fus:
            arr = "FU_" + "".join(c if c.isalnum() else "_" for c in sid)
            lines.append(f"        .followups = {arr}, .followup_count = {len(fus)},")
        else:
            lines.append("        .followups = NULL, .followup_count = 0,")
        lines.append("    },")
    lines += [
        "};",
        "",
        "const story_t *story_data_get(size_t index) {",
        "    if (index >= sizeof(STORY_ITEMS_INTERNAL)/sizeof(STORY_ITEMS_INTERNAL[0])) return NULL;",
        "    return &STORY_ITEMS_INTERNAL[index];",
        "}",
        "size_t story_data_count(void) {",
        "    return sizeof(STORY_ITEMS_INTERNAL)/sizeof(STORY_ITEMS_INTERNAL[0]);",
        "}",
        "const story_t *story_data_find(const char *id) {",
        "    if (!id) return NULL;",
        "    for (size_t i = 0; i < story_data_count(); ++i) {",
        "        const story_t *it = story_data_get(i);",
        "        if (it->id && strcmp(it->id, id) == 0) return it;",
        "    }",
        "    return NULL;",
        "}",
        "bool story_data_playable(const story_t *item) {",
        "    if (!item) return false;",
        "    if (item->confidence == STORY_CONF_UNKNOWN) return false;",
        "#if STORY_ALLOW_UNVERIFIED",
        "    return true;",
        "#else",
        "    return item->verified;",
        "#endif",
        "}",
        "",
        "// 固定话术（欢迎/澄清/边界/离线/复访），与需求方案一致",
        "const char *const STORY_PHRASES[STORY_PHRASE_COUNT] = {",
        '    [STORY_PHRASE_WELCOME] = "欢迎来到华为松山湖研发中心！我是陪你走这一段的研发故事讲述者。按住这个键直接问我，或者用上下键挑个主题都行。",',
        '    [STORY_PHRASE_IDLE_PROMPT] = "想听什么？可以直接问我，或者用上下键挑个主题。",',
        '    [STORY_PHRASE_CLARIFY] = "这个我没太听明白。你可以说个主题，比如技术攻关、研发文化，或者用上下键挑一个。",',
        '    [STORY_PHRASE_BOUNDARY] = "技术细节和比较我讲不了，我只负责讲研发故事。这部分请找对接同事。",',
        '    [STORY_PHRASE_UNCERTAIN] = "这个细节我没有可靠来源，不乱讲。我给你讲有据可查的部分好吗？",',
        '    [STORY_PHRASE_OFFLINE] = "我现在是离网运行的，所有故事都在本机，随时可以讲。",',
        '    [STORY_PHRASE_REVISIT] = "又见面了。接着上次那个话题，还是换个？",',
        "};",
        "",
    ]

    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"已生成 {OUT}（{len(story_defs)} 条故事）")
    print("下一步：python tools/check_font_coverage.py && bash tools/make_font.sh")
    return 0


if __name__ == "__main__":
    sys.exit(main())
