// main.c —— AI Passport「奋斗鹅 · 华为研发故事讲述者」应用入口。
//
// 这是二次开发应用，不是基线 BSP demo：
//   - 不注册 demo 测试菜单，不加载 ui_pixel 测试外壳（仓库强制要求）。
//   - 直接使用 bsp_* 初始化硬件，然后把按键事件投递给 story_app。
//   - 保留基线的两个成熟机制：按键事件队列（回调不阻塞）、深睡唤醒按键守卫。
//
// 启动流程：
//   bsp_i2c -> display/LVGL -> 输入队列 -> button -> audio(由 story_app) -> battery
//   -> story_app_init()（创建全新 UI + 播放欢迎）
//
// 按键语义见 story_app.c：三键在欢迎/主题/讲述/提问四个页面间导航，
// OK 短按确认、OK 长按返回（提问页长按返回欢迎）。
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_pins.h"
#include "key_guard.h"

#include "story_app.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <stdbool.h>

static const char *TAG = "main";

#define INPUT_QUEUE_DEPTH 8
#define KEY_GUARD_MAX_US (10LL * 1000 * 1000)  // 兜底：按键卡住时最多守卫 10s

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t event;
} input_event_t;

static QueueHandle_t s_input_queue;
static TaskHandle_t s_input_task;
static volatile bool s_input_ready;
static key_guard_t s_key_guard;

// ---------------------------------------------------------------------------
// 输入任务：从队列取事件，交给 story_app 处理。story_app 内部自行加 LVGL 锁。
// ---------------------------------------------------------------------------
static void input_task(void *arg) {
    (void)arg;
    input_event_t input;
    for (;;) {
        if (xQueueReceive(s_input_queue, &input, portMAX_DELAY) == pdTRUE) {
            story_app_on_key(input.btn, input.event);
        }
    }
}

static esp_err_t input_dispatch_init(void) {
    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    if (!s_input_queue) return ESP_ERR_NO_MEM;
    if (xTaskCreate(input_task, "app_input", 4096, NULL, 5, &s_input_task) != pdPASS) {
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// 按键回调：运行在 button 组件的共享 esp_timer 任务，只做守卫判断 + 入队。
// ---------------------------------------------------------------------------
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    const bool was_armed = s_key_guard.armed;
    if (key_guard_consume(&s_key_guard, ev == BSP_BTN_RELEASE, esp_timer_get_time())) {
        if (was_armed && !s_key_guard.armed) {
            ESP_LOGI(TAG, "唤醒键已松手，按键恢复响应");
        }
        return;
    }
    if (!s_input_ready || !s_input_queue) return;
    const input_event_t input = { .btn = btn, .event = ev };
    (void)xQueueSend(s_input_queue, &input, 0);
}

// ---------------------------------------------------------------------------
// 低频 tick 任务：刷新电量（右上角），避免频繁 I2C 读取。
// ---------------------------------------------------------------------------
static void tick_task(void *arg) {
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        story_app_tick();
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "奋斗鹅 · 华为研发故事讲述者启动");

    const uint32_t wake_causes = esp_sleep_get_wakeup_causes();
    if (wake_causes & (1u << ESP_SLEEP_WAKEUP_GPIO)) {
        key_guard_arm(&s_key_guard, esp_timer_get_time(), KEY_GUARD_MAX_US);
        ESP_LOGI(TAG, "按键唤醒：先屏蔽按键动作，直到松手");
    }

    bsp_i2c_init();

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "显示/LVGL 初始化失败，应用无法继续。"
                      "检查 SPI 接线(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    esp_err_t input_err = input_dispatch_init();
    esp_err_t button_err = input_err == ESP_OK ? bsp_button_init(on_key, NULL)
                                               : ESP_ERR_INVALID_STATE;
    if (input_err != ESP_OK) {
        ESP_LOGE(TAG, "输入队列创建失败: %s", esp_err_to_name(input_err));
    } else if (button_err != ESP_OK) {
        ESP_LOGE(TAG, "按键初始化失败: %s", esp_err_to_name(button_err));
        vTaskDelete(s_input_task);
        vQueueDelete(s_input_queue);
        s_input_task = NULL;
        s_input_queue = NULL;
        input_err = ESP_ERR_INVALID_STATE;
    } else if (s_key_guard.armed) {
        const bool released = bsp_button_read_mv() >= BSP_BTN_MV_RELEASED_MIN;
        key_guard_sync_released(&s_key_guard, released);
        if (!s_key_guard.armed) ESP_LOGI(TAG, "唤醒键在按键驱动启动前已松手，提前撤防");
    }

    esp_err_t batt_err = bsp_battery_init();   // 失败时电量显示 -1，UI 优雅降级
    if (batt_err != ESP_OK) ESP_LOGW(TAG, "电量计不可用，右上角不显示电量");

    esp_err_t app_err = story_app_init();
    if (app_err != ESP_OK) {
        ESP_LOGE(TAG, "应用初始化失败: %s", esp_err_to_name(app_err));
        return;
    }

    s_input_ready = (input_err == ESP_OK && button_err == ESP_OK);
    xTaskCreate(tick_task, "app_tick", 2048, NULL, 2, NULL);

    ESP_LOGI(TAG, "就绪：Button=%d Battery=%d（离网运行，无网络依赖）",
             s_input_ready, batt_err == ESP_OK);
}
