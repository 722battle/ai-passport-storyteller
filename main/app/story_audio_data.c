// story_audio_data.c —— 预录音频清单（占位实现）。
//
// ⚠ 当前状态：没有真实录音资源，本文件只提供空的音频表 + 一个 200ms 的
//    提示音样例（运行时生成，不占 Flash）。播放逻辑会：
//      - 找到 audio_id -> 播放真实音频；
//      - 找不到 -> story_app 降级为 chime + 屏幕文字。
//    因此即使没有录音，整个交互也能走通并验收；客户听到的是提示音，
//    屏幕显示完整文字。正式上线前请按 docs/DEPLOY.zh_CN.md 录制并打包音频。
//
// 生成方式：python tools/make_audio.py <wav 目录> > main/app/story_audio_data.c
#include "story_audio.h"

// 真实音频资源条目（当前为空）。
// 例：{ "welcome", s_welcome_pcm, WELCOME_LEN }, 数组由 make_audio.py 生成。
const story_audio_clip_t STORY_AUDIO_CLIPS[] = {
    /* 录音入库后在此生成条目 */
};
const size_t STORY_AUDIO_CLIP_COUNT =
    sizeof(STORY_AUDIO_CLIPS) / sizeof(STORY_AUDIO_CLIPS[0]);
