# -*- coding: utf-8 -*-
"""把录音 WAV 转成 story_audio_data.c（预录音频清单）。

用途：需求方案要求所有语音提前录制、离网播放。本脚本把一批 16kHz/16bit/单声道
WAV 转成 C 数组，生成 main/app/story_audio_data.c。

用法：
    python tools/make_audio.py <wav目录>

约定：
  - 文件名即 audio_id（不含扩展名），必须与 story_data.c 的 audio_id 一致，
    例如 welcome.wav -> 欢迎话术；<story id>.wav -> 某条故事的讲解。
  - WAV 必须是 16kHz / 16bit / 单声道 PCM。脚本会重采样提示但不自动转换，
    请用 Audacity/ffmpeg 预先转好：
        ffmpeg -i in.mp3 -ar 16000 -ac 1 -c:a pcm_s16le welcome.wav
  - 8MB Flash 预算有限：建议总音频 < 3MB。每条故事控制在 15-25 秒。

输出会覆盖 main/app/story_audio_data.c，并在文件头记录生成时间。
"""
from __future__ import annotations

import struct
import sys
import wave
from datetime import datetime
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OUT = REPO / "main" / "app" / "story_audio_data.c"
TARGET_RATE = 16000
TARGET_WIDTH = 2   # 16bit
TARGET_CH = 1


def read_wav_mono(path: Path) -> list[int]:
    with wave.open(str(path), "rb") as w:
        rate, width, ch, nframes = w.getframerate(), w.getsampwidth(), w.getnchannels(), w.getnframes()
        if rate != TARGET_RATE or width != TARGET_WIDTH or ch != TARGET_CH:
            raise ValueError(
                f"{path.name}: 需 16kHz/16bit/单声道，实际 {rate}Hz/{width*8}bit/{ch}ch。"
                "请用 ffmpeg 先转换。"
            )
        raw = w.readframes(nframes)
    # 小端有符号 16bit
    return list(struct.unpack(f"<{nframes}h", raw))


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    wav_dir = Path(sys.argv[1])
    if not wav_dir.is_dir():
        print(f"目录不存在: {wav_dir}", file=sys.stderr)
        return 1

    clips: list[tuple[str, list[int]]] = []
    total_samples = 0
    for wav in sorted(wav_dir.glob("*.wav")):
        samples = read_wav_mono(wav)
        clips.append((wav.stem, samples))
        total_samples += len(samples)
        print(f"  {wav.stem}: {len(samples)} 采样 ({len(samples)/TARGET_RATE:.1f}s)")

    megabytes = total_samples * 2 / 1024 / 1024
    print(f"合计 {megabytes:.2f} MB PCM（未压缩，接入固件前请确认 Flash 预算）")

    lines: list[str] = [
        "// story_audio_data.c —— 由 tools/make_audio.py 自动生成，请勿手改。",
        f"// 生成时间: {datetime.now().isoformat(timespec='seconds')}",
        f"// 音频资源: {len(clips)} 条，共 {megabytes:.2f} MB（16kHz/16bit/mono）",
        '#include "story_audio.h"',
        "",
    ]
    # 每个 clip 的采样数组
    for audio_id, samples in clips:
        arr = "s_" + "".join(c if c.isalnum() else "_" for c in audio_id) + "_pcm"
        lines.append(f"static const int16_t {arr}[] = {{")
        # 每行 16 个采样，控制文件可读性
        for i in range(0, len(samples), 16):
            lines.append("    " + ", ".join(str(x) for x in samples[i:i+16]) + ",")
        lines.append("};")
        lines.append("")
    # clip 表
    lines.append("const story_audio_clip_t STORY_AUDIO_CLIPS[] = {")
    for audio_id, samples in clips:
        arr = "s_" + "".join(c if c.isalnum() else "_" for c in audio_id) + "_pcm"
        lines.append(f'    {{ "{audio_id}", {arr}, {len(samples)} }},')
    lines.append("};")
    lines.append("const size_t STORY_AUDIO_CLIP_COUNT =")
    lines.append("    sizeof(STORY_AUDIO_CLIPS) / sizeof(STORY_AUDIO_CLIPS[0]);")
    lines.append("")

    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"已生成: {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
