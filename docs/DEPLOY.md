**English** · [简体中文](DEPLOY.zh_CN.md)

# Deployment guide — "FenDou Goose" Huawei R&D Storyteller

This guide takes the project from source to an AI Passport device. The app runs **fully offline** with no cloud dependency.

---

## 0. Current status and prerequisites

| Item | Status |
| --- | --- |
| Source project | ✅ Ready on branch `feature/huawei-storyteller` |
| Dialogue engine | ✅ Verified via Python mirror test (`test_story_engine.py: PASS`) |
| Chinese font | ✅ Generated (`assets/fonts/app_font_16.c`, covers 477 chars) |
| Prerecorded voice | ⚠️ None; currently falls back to "chime + on-screen text", interaction still complete |
| Story content | ⚠️ Sample copy, `verified=false`; **must be reviewed before showing customers** |
| ESP-IDF on this PC | ❌ Not installed; see step 1 |
| Device flashing | ⚠️ Requires a data cable and your explicit authorization |

**Two red lines before production:**
1. **Content review**: every story in `content/stories.csv` has a "TODO URL" source and `verified=false`. The content owner must verify facts, add sources, set `true`, then regenerate the pack and font.
2. **Font/mascot licensing**: current font is Windows SimHei (dev only); mascot is AI-generated. See `docs/ASSETS.md`.

---

## 1. Install ESP-IDF 5.5.3

**Option A: Windows installer (recommended)**
Download the ESP-IDF v5.5.3 offline installer from Espressif, select ESP32-C3 target and USB Serial/JTAG driver, then open the provided "ESP-IDF 5.5 PowerShell" (it injects the environment).

**Option B: command line**
```bash
mkdir -p ~/esp && cd ~/esp
git clone -b v5.5.3 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32c3
. ./export.sh
```
Verify: `idf.py --version` should show v5.5.3.

---

## 2. Run logic tests (no ESP-IDF needed)

```bash
cd <project>/huawei-storyteller
python tools/test_story_engine.py       # expect: PASS
python tools/check_font_coverage.py
```
With a C compiler available: `bash tools/run_host_tests.sh`.

---

## 3. (Optional) Record prerecorded audio

Record one WAV per `id` in `content/stories.csv`, plus `welcome.wav`. Convert all to 16 kHz / 16-bit / mono:
```bash
ffmpeg -i in.mp3 -ar 16000 -ac 1 -c:a pcm_s16le welcome.wav
python tools/make_audio.py <wav_dir>
```
Keep total audio < 3 MB (8 MB Flash budget). Without recordings the app still works with chime + text.

---

## 4. (If content changed) Regenerate pack and font

```bash
python tools/gen_content.py content/stories.csv
python tools/check_font_coverage.py
bash tools/make_font.sh
bash tools/run_host_tests.sh
```
Any new Chinese character requires regenerating the font, otherwise it renders as a box.

---

## 5. Set target and build

```bash
idf.py set-target esp32c3
idf.py build
idf.py merge-bin -o full.bin
```
Outputs: `build/full.bin` (flash from 0x0) and matching ELF/MAP.

---

## 6. Connect the device

Use a **data-capable** USB Type-C cable, power on (~0.5 s power-key hold), find the port in Device Manager (COMx) or via `idf.py -p COM?? monitor`.

---

## 7. Flash (requires your explicit authorization)

⚠️ Flashing overwrites existing firmware and may reset stored data (NVS). Proceed only after you confirm.
```bash
idf.py -p COM?? flash
# or: esptool.py --chip esp32c3 -p COM?? write_flash 0x0 build/full.bin
```
Expected serial log:
```
I (xxx) main: FenDou Goose storyteller starting
I (xxx) main: Ready: Button=1 Battery=1 (offline, no network dependency)
```

---

## 8. Device acceptance checklist

| Item | Method | Expected |
| --- | --- | --- |
| Welcome | Power on | Mascot + "Welcome to Huawei Songshan Lake R&D Center" + chime/voice |
| Theme wheel | UP/DOWN | 8 themes scroll; OK enters |
| Story | OK on theme page | Title + body, no tofu/blank |
| Follow-up | UP/DOWN select bubble + OK | Plays matching prerecorded answer |
| Structured Q | Long-press OK on welcome | Question list; OK asks |
| Boundary | Select "price" question | Boundary phrase, no related content |
| Offline | Stay off Wi-Fi throughout | All flows work, no stalls |
| Battery | Top-right | Shows %; empty if meter unavailable |
| Font | Compare with `docs/font_chars.txt` | No tofu/blank/clipping |
| Mascot | All screens | 7 expressions switch correctly, no watermark |

---

## 9. Delivery report template

```
Build: PASS / FAIL / NOT RUN
Host tests: PASS / FAIL / NOT RUN
Device tests: PASS / FAIL / NOT RUN
Unverified:
  - Chinese glyphs (on-device check pending)
  - Prerecorded voice (chime fallback until audio added)
  - Content facts (sample copy pending owner review)
```

---

## 10. Known boundaries (no overclaiming)

- **Cannot understand arbitrary speech**: ESP32-C3 has no PSRAM and 8 MB Flash; general offline ASR is not feasible. "Ask a question" is implemented via an on-screen question list (long-press OK), fully offline. Hold-OK-and-speak free-form input is a future KWS extension.
- **Answers come from prerecorded content**: no online generation; unmatched input falls back to a clarification phrase, never fabricated.
- **Voice is a fallback chime** until recordings are added.
- **Content is sample copy**: review is mandatory before customer use.
