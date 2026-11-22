**English** · [简体中文](ASSETS.zh_CN.md)

# Asset inventory and licensing

All assets used by this app, with source and licensing status. Confirm licensing before productization.

| Asset | Path | Source | Status | Notes |
| --- | --- | --- | --- | --- |
| Mascot reference image | `assets/` (filename in Chinese) | User-provided, AI-generated watermark | ⚠️ Unconfirmed | Design reference only. The on-device UI is drawn with LVGL primitives; this bitmap is **not embedded**, so no watermark appears. Confirm generation-platform license before embedding. |
| Chinese font | `assets/fonts/app_font_16.c` | Generated from `C:/Windows/Fonts/simhei.ttf` | ⚠️ Dev only | Windows SimHei is a system font; replace with a licensed font (e.g. Source Han Sans, SIL OFL) for production, then rerun `tools/make_font.sh`. |
| Prerecorded audio | `main/app/story_audio_data.c` | Empty (chime fallback) | — | Record via `tools/make_audio.py` before production; voice talent must be licensed. |
| Story copy | `content/stories.csv` | Engineering sample copy | ⚠️ Pending review | Every entry has `verified=false`; content owner must verify facts, add source URLs, set `true`, then regenerate. |

## Font generation record

- Tool: `lv_font_conv@1.5.3` (via npx)
- Source font: `C:/Windows/Fonts/simhei.ttf`
- Options: `--size 16 --bpp 4 --format lvgl --no-compress --lv-font-name app_font_16`
- Character set: `docs/font_chars.txt` (extracted from source by `tools/check_font_coverage.py`)
- Coverage: verify on device character by character; no tofu/blank glyphs allowed.
