// story_audio.h —— 预录音频播放（离网）。
//
// 所有语音都是提前录制、打包进固件的音频，不在设备上实时合成（C3 做不到）。
// 本模块负责：按 audio_id 找到音频数据 -> 分块送入 bsp_audio_write 播放。
//
// 音频资源来源有两种，兼容不同阶段：
//   A. 头文件数组（stage 1，便于小样验证）：由 tools/make_audio.py 把 WAV
//      转成 const int16_t 数组，编译进固件。适合少量短音频。
//   B. 分区二进制（stage 2，正式方案）：把音频打包成一个只读分区，按索引读取，
//      不占 app 固件体积。本工程先实现 A 的接口，B 留 TODO 并在文档说明。
//
// 重要：bsp_audio_write 是阻塞调用，播放必须在独立任务中进行，
//       不能放在按键回调或 LVGL 任务里。本模块内部创建并管理播放任务。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

// 一条预录音频：16kHz / 16bit / 单声道 PCM。
typedef struct {
    const char *id;                 // 与 story_data 的 audio_id 对应
    const int16_t *samples;         // PCM 采样数据（Flash 中）
    size_t sample_count;            // 采样数
} story_audio_clip_t;

// 初始化音频子系统（内部调用 bsp_audio_init）。可重复调用。
esp_err_t story_audio_init(void);

// 播放指定 ID 的音频（异步：立即返回，播放任务后台执行）。
// 参数：
//   audio_id - story_audio_clip_t 的 id；找不到时返回 ESP_ERR_NOT_FOUND。
//   on_done  - 播放完成回调（可为 NULL），在播放任务上下文调用，
//              只应做轻量事件投递，不得操作 LVGL 或阻塞。
//   user     - 透传给 on_done 的用户数据。
// 并发：新播放会取消上一条未播完的音频。
esp_err_t story_audio_play(const char *audio_id, void (*on_done)(void *), void *user);

// 停止当前播放。幂等。
void story_audio_stop(void);

// 是否有音频正在播放。
bool story_audio_is_playing(void);

// 用 1kHz 短提示音代替语音（当某条 audio_id 缺音频资源时的降级反馈）。
// duration_ms 建议 <= 300，避免打扰。异步播放。
esp_err_t story_audio_play_chime(uint32_t duration_ms);

// 反初始化：停止任务并释放资源。
void story_audio_deinit(void);

// ---- 音频清单（由 tools/make_audio.py 生成）----
extern const story_audio_clip_t STORY_AUDIO_CLIPS[];
extern const size_t STORY_AUDIO_CLIP_COUNT;
