// story_audio.c —— 预录音频播放实现。接口语义见 story_audio.h。
//
// 并发模型：
//   - 一个后台任务 story_audio_task 独占 bsp_audio_write，避免多任务同时写 I2S。
//   - 播放请求通过任务通知传递索引；新请求取消旧播放（换 ID 通知即可）。
//   - 完成回调在播放任务里执行，调用方只能投递事件，不能碰 LVGL。
// 内存：每次只分块播放（CHUNK_SAMPLES），不一次性拷贝整段音频到 RAM。
#include "story_audio.h"

#include "bsp_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "story_audio";

#define AUDIO_SAMPLE_RATE 16000
#define CHUNK_SAMPLES 512
#define PLAY_TASK_STACK 4096
#define PLAY_TASK_PRIO 5

// 任务命令：高 16 位是 clip 下标（+1），低 16 位是 chime 标志。
#define CMD_STOP 0u
#define CMD_CHIME_BASE 0x10000u

// 模块状态（只由播放任务写，主线程读时加锁保护更严谨，这里用原子通知即可）。
static TaskHandle_t s_play_task = NULL;
static volatile bool s_playing = false;
static void (*s_on_done)(void *) = NULL;
static void *s_on_done_user = NULL;

// 查找音频资源下标，找不到返回 SIZE_MAX。
static size_t find_clip(const char *id) {
    if (!id) return (size_t)-1;
    for (size_t i = 0; i < STORY_AUDIO_CLIP_COUNT; ++i) {
        if (strcmp(STORY_AUDIO_CLIPS[i].id, id) == 0) return i;
    }
    return (size_t)-1;
}

// 播放一段 PCM：分块写入，检查取消标记。
static void play_samples(const int16_t *samples, size_t count) {
    size_t sent = 0;
    while (sent < count) {
        size_t n = (count - sent) < CHUNK_SAMPLES ? (count - sent) : CHUNK_SAMPLES;
        if (bsp_audio_write(samples + sent, n * sizeof(int16_t)) != ESP_OK) {
            ESP_LOGE(TAG, "bsp_audio_write 失败");
            break;
        }
        sent += n;
        // 让出 CPU，避免长时间占用导致按键/看门狗异常
        vTaskDelay(1);
    }
}

// 播放提示音：本地生成短正弦/方波，不需要音频资源。
static void play_chime(uint32_t duration_ms) {
    int16_t buf[CHUNK_SAMPLES];
    const uint32_t total = AUDIO_SAMPLE_RATE * duration_ms / 1000;
    uint32_t sent = 0;
    const int period = AUDIO_SAMPLE_RATE / 1000; // 1kHz
    int phase = 0;
    while (sent < total) {
        uint32_t n = (total - sent) < CHUNK_SAMPLES ? (total - sent) : CHUNK_SAMPLES;
        for (uint32_t i = 0; i < n; ++i) {
            buf[i] = (phase < period / 2) ? 4000 : -4000;
            if (++phase >= period) phase = 0;
        }
        if (bsp_audio_write(buf, n * sizeof(int16_t)) != ESP_OK) break;
        sent += n;
        vTaskDelay(1);
    }
}

// 播放任务：等待通知，按命令播放。
static void audio_task(void *arg) {
    (void)arg;
    for (;;) {
        uint32_t cmd = 0;
        if (xTaskNotifyWait(0, UINT32_MAX, &cmd, portMAX_DELAY) != pdTRUE) continue;
        if (cmd == CMD_STOP) {
            s_playing = false;
            continue;
        }
        s_playing = true;
        if (bsp_audio_set_format(AUDIO_SAMPLE_RATE, 16, 1) != ESP_OK) {
            ESP_LOGE(TAG, "设置音频格式失败");
            s_playing = false;
            continue;
        }
        bsp_audio_set_volume(85);

        if (cmd & 0x10000u) {
            // 低 16 位是 duration_ms（限幅 1..1000）
            uint32_t ms = cmd & 0xFFFFu;
            if (ms == 0 || ms > 1000) ms = 200;
            play_chime(ms);
        } else {
            size_t idx = (size_t)(cmd & 0xFFFFu);
            if (idx > 0 && idx <= STORY_AUDIO_CLIP_COUNT) {
                const story_audio_clip_t *clip = &STORY_AUDIO_CLIPS[idx - 1];
                play_samples(clip->samples, clip->sample_count);
            }
        }
        s_playing = false;
        void (*done)(void *) = s_on_done;
        void *done_user = s_on_done_user;
        s_on_done = NULL;
        s_on_done_user = NULL;
        if (done) done(done_user);
    }
}

esp_err_t story_audio_init(void) {
    if (s_play_task) return ESP_OK;
    esp_err_t err = bsp_audio_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_audio_init 失败: %s", esp_err_to_name(err));
        return err;
    }
    if (xTaskCreate(audio_task, "story_audio", PLAY_TASK_STACK, NULL,
                    PLAY_TASK_PRIO, &s_play_task) != pdPASS) {
        s_play_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t story_audio_play(const char *audio_id, void (*on_done)(void *), void *user) {
    if (!s_play_task) return ESP_ERR_INVALID_STATE;
    size_t idx = find_clip(audio_id);
    if (idx == (size_t)-1) return ESP_ERR_NOT_FOUND;
    if (idx >= 0xFFFF) return ESP_ERR_INVALID_ARG;
    // 换 ID：先停旧播放，再投递新命令（cli 下标 +1，0 保留给 STOP）
    (void)xTaskNotify(s_play_task, CMD_STOP, eSetValueWithOverwrite);
    s_on_done = on_done;
    s_on_done_user = user;
    (void)xTaskNotify(s_play_task, (uint32_t)(idx + 1), eSetValueWithOverwrite);
    return ESP_OK;
}

void story_audio_stop(void) {
    if (s_play_task) (void)xTaskNotify(s_play_task, CMD_STOP, eSetValueWithOverwrite);
    s_playing = false;
}

bool story_audio_is_playing(void) {
    return s_playing;
}

esp_err_t story_audio_play_chime(uint32_t duration_ms) {
    if (!s_play_task) return ESP_ERR_INVALID_STATE;
    if (duration_ms == 0 || duration_ms > 1000) duration_ms = 200;
    (void)xTaskNotify(s_play_task, CMD_STOP, eSetValueWithOverwrite);
    s_on_done = NULL;
    s_on_done_user = NULL;
    (void)xTaskNotify(s_play_task, CMD_CHIME_BASE | duration_ms, eSetValueWithOverwrite);
    return ESP_OK;
}

void story_audio_deinit(void) {
    if (s_play_task) {
        (void)xTaskNotify(s_play_task, CMD_STOP, eSetValueWithOverwrite);
        vTaskDelay(pdMS_TO_TICKS(50)); // 给播放任务退出本次循环的时间
        vTaskDelete(s_play_task);
        s_play_task = NULL;
    }
    s_playing = false;
}
