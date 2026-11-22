[English](ASSETS.md) · **简体中文**

# 素材清单与授权说明

本应用用到的素材及其来源、授权状态。产品化前必须逐项确认授权。

| 素材 | 路径 | 来源 | 授权状态 | 说明 |
| --- | --- | --- | --- | --- |
| 奋斗鹅形象参考图 | `assets/奋斗鹅-形象参考.jpg` | 用户提供，图角标"豆包 AI 生成" | ⚠ 待确认 | 仅作形象设计参考；设备 UI 用 LVGL 基本图形绘制，**不内嵌该位图**，正式界面不含水印。若要内嵌，须确认生成平台商用授权并去水印。 |
| 中文字库 | `assets/fonts/app_font_16.c` | 从 `C:/Windows/Fonts/simhei.ttf`（Windows 黑体）生成 | ⚠ 开发验证用 | 黑体为 Windows 系统字体，**商用需替换为有授权字体**（如思源黑体 SIL OFL）。替换后重跑 `tools/make_font.sh`。 |
| 预录音频 | `main/app/story_audio_data.c` | 当前为空（提示音降级） | — | 正式上线前按 `tools/make_audio.py` 录制并入库；录音须为授权配音。 |
| 故事文案 | `content/stories.csv` | 工程示例文案（概括性表述） | ⚠ 待内容审核 | 每条 `verified=false`，须内容责任人逐条核验事实、补 URL、改 `verified=true` 后再生成内容包。 |

## 字库生成记录

- 转换工具：`lv_font_conv@1.5.3`（经 npx 拉取）
- 源字体：`C:/Windows/Fonts/simhei.ttf`
- 参数：`--size 16 --bpp 4 --format lvgl --no-compress --lv-font-name app_font_16`
- 字符范围：`docs/font_chars.txt`（由 `tools/check_font_coverage.py` 从源码提取）
- 覆盖校验：真机按字符清单逐字检查，禁止方框/空白。
