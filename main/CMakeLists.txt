# 奋斗鹅 · 华为研发故事讲述者应用组件。
#
# 与基线 demo 的区别：
#   - 只编译应用自己的源文件，不编译 demo_*.c / ui_pixel.c 测试外壳。
#   - 不依赖 bt / esp_wifi / esp_netif：本应用全程离网。
#   - key_guard 仍复用（深睡唤醒按键守卫，纯逻辑头文件）。
idf_component_register(
    SRCS "main.c"
         "key_guard.h"
         "app/story_data.c"
         "app/story_engine.c"
         "app/story_font.c"
         "app/story_ui.c"
         "app/story_audio.c"
         "app/story_audio_data.c"
         "app/story_app.c"
    INCLUDE_DIRS "." "app"
    REQUIRES bsp esp_timer nvs_flash lvgl
)

# 应用专用中文字库（由 tools/make_font.sh 生成，精确覆盖全部 UI 文案）。
target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/app_font_16.c"
)
