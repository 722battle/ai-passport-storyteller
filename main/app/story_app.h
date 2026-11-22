// story_app.h —— 应用主控：把输入、引擎、UI、音频串起来。
//
// 输入来自 main.c 的按键队列（已在任务上下文），本模块不做硬件初始化，
// 只处理"按键事件 -> 状态转移 -> 播放"。所有 LVGL 操作由本模块持锁完成。
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "bsp_button.h"
#include "esp_err.h"

// 初始化整个应用（UI + 引擎上下文 + 音频）。在 main.c 完成显示/BSP 初始化后调用。
esp_err_t story_app_init(void);

// 反初始化，释放任务与对象。
void story_app_deinit(void);

// 处理一个按键事件。由 main.c 的输入任务调用（非 LVGL 任务）。
// 本函数内部自行加/解 LVGL 锁；按键回调不阻塞的约束由 main.c 队列保证。
void story_app_on_key(bsp_btn_t btn, bsp_btn_ev_t ev);

// 定时刷新（电量等）。由 main.c 的低频定时器/任务调用，内部持 LVGL 锁。
void story_app_tick(void);
