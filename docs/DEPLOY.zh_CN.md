[English](DEPLOY.md) · **简体中文**

# 奋斗鹅 · 华为研发故事讲述者 —— 部署手册

本手册把工程从源码带到 AI Passport 设备。全程**离网运行**，不依赖云端服务。

---

## 0. 现状与前提（务必先读）

| 项 | 状态 |
| --- | --- |
| 源码工程 | ✅ 已就绪，位于本仓库 `feature/huawei-storyteller` 分支 |
| 对话引擎逻辑 | ✅ 已用 Python 镜像测试验证（`test_story_engine.py: PASS`） |
| 中文字库 | ✅ 已生成（`assets/fonts/app_font_16.c`，精确覆盖 477 字） |
| 预录语音 | ⚠️ 暂无，当前降级为"提示音 + 屏幕文字"，交互仍完整可用 |
| 故事内容 | ⚠️ 示例文案，`verified=false`，**上线给客户前须内容审核** |
| 本机 ESP-IDF | ❌ 当前电脑未安装，需要先装（见步骤 1） |
| 真机烧录 | ⚠️ 需数据线连接设备，且烧录前需你明确授权 |

**两条红线（上线前必须处理）：**
1. **内容审核**：`content/stories.csv` 里每条故事的 `source` 都标了"待补 URL"、`verified=false`。给客户看之前，内容责任人必须逐条核实事实、补来源、改为 `true`，再重新生成内容包与字库。
2. **字体/形象授权**：当前字库来自 Windows 黑体（仅开发验证用），奋斗鹅是 AI 生成图。产品化须换成有商用授权的字体并确认形象授权，详见 `docs/ASSETS.md`。

---

## 1. 安装 ESP-IDF 5.5.3

本工程基于仓库基线，要求 **ESP-IDF 5.5.3**。二选一：

**方式 A：Windows 安装包（推荐）**
1. 下载 ESP-IDF Windows Installer（Espressif 官网，选 v5.5.3 离线包）。
2. 安装时勾选 ESP32-C3 目标与 USB Serial/JTAG 驱动。
3. 安装完成后，用桌面出现的 "ESP-IDF 5.5 PowerShell" 打开终端——它会自动注入环境变量。

**方式 B：命令行（已有 Python 3.11 环境）**
```bash
mkdir -p ~/esp && cd ~/esp
git clone -b v5.5.3 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32c3
. ./export.sh
```

验证：
```bash
idf.py --version    # 应显示 v5.5.3
```

> 说明：当前电脑的 WorkBuddy 终端默认没有 ESP-IDF 环境。装完后请在"ESP-IDF 5.5 PowerShell"里执行后续命令，或先 `export.sh` / `. ./export.ps1`。

---

## 2. 在本机先跑逻辑测试（不需要 ESP-IDF）

```bash
cd <工程目录>/huawei-storyteller
python tools/test_story_engine.py       # 期望：test_story_engine.py: PASS
python tools/check_font_coverage.py     # 生成字符清单
```

如果装了 C 编译器（gcc/clang），可额外验证 C 版本地逻辑：
```bash
bash tools/run_host_tests.sh            # 期望：Host tests: PASS
```

这一步验证的是：越界优先、追问承接、故事命中、主题切换、澄清兜底、随机可复现。

---

## 3.（可选但强烈建议）录制预录语音

当前所有语音都降级为提示音。要让奋斗鹅"真的开口说话"：

1. 按 `content/stories.csv` 的 `id` 逐条录音，另加一条 `welcome.wav`（开场欢迎）。
2. 统一转成 16kHz / 16bit / 单声道：
   ```bash
   ffmpeg -i 输入.mp3 -ar 16000 -ac 1 -c:a pcm_s16le welcome.wav
   ```
3. 生成音频清单：
   ```bash
   python tools/make_audio.py <wav目录>
   ```
   这会覆盖 `main/app/story_audio_data.c`。
4. 注意 Flash 预算：8MB 里系统占约 1MB，字库约 400KB，建议音频总量 < 3MB。

> 若不录音也能完整验收交互（屏幕文字 + 提示音），可直接进入下一步。

---

## 4.（如需改内容）重新生成内容包与字库

内容团队改完 `content/stories.csv` 后，依次执行：

```bash
python tools/gen_content.py content/stories.csv   # 重新生成 story_data.c
python tools/check_font_coverage.py               # 字符变了要重算
bash tools/make_font.sh                           # 重新生成字库
bash tools/run_host_tests.sh                      # 或 python 镜像测试
```

⚠️ 只要文案有新增汉字，就**必须**重跑字库生成，否则新字显示为方框。

---

## 5. 配置目标芯片

在 ESP-IDF 终端里：
```bash
cd <工程目录>/huawei-storyteller
idf.py set-target esp32c3
```

如需微调配置：
```bash
idf.py menuconfig
```
确认：分区表 8MB、USB Serial/JTAG console、应用专用字库（不启用内置 CJK 子集）。

---

## 6. 编译固件

```bash
idf.py build
```

成功后会生成可烧录镜像。按仓库规范，交付固件应为合并镜像：
```bash
idf.py merge-bin -o full.bin
```

产物：
- `build/full.bin` —— 从 0x0 烧录的合并固件
- `build/ai-passport-app.elf` / `.map` —— 与固件匹配的符号文件（调试用）

---

## 7. 连接设备

1. 用**支持数据传输**的 USB Type-C 线（不能是仅充电线）连接电脑。
2. 开机（按住电源键约 0.5 秒）。
3. 确认串口号：
   ```bash
   idf.py -p COM?? monitor    # ?? 换成实际端口，或用设备管理器查看
   ```
   Windows 下通常是 `COM3`/`COM4` 等；不清楚可在设备管理器"端口(COM 和 LPT)"里看。

---

## 8. 烧录（需要你的明确授权）

> ⚠️ 烧录会覆盖设备现有固件。仓库基线说明：合并烧录可能重置已存数据（NVS）。**这一步必须由你确认后执行。**

```bash
idf.py -p COM?? flash
```

或使用合并镜像从 0x0 烧录：
```bash
esptool.py --chip esp32c3 -p COM?? write_flash 0x0 build/full.bin
```

烧录后打开串口日志，应看到：
```
I (xxx) main: 奋斗鹅 · 华为研发故事讲述者启动
I (xxx) main: 就绪：Button=1 Battery=1（离网运行，无网络依赖）
```

---

## 9. 真机验收清单

| 项 | 验收方法 | 期望 |
| --- | --- | --- |
| 开机欢迎 | 上电 | 屏幕出现奋斗鹅 + "欢迎来到华为松山湖研发中心"，语音/提示音 |
| 主题轮盘 | 按 UP/DOWN | 8 个主题滚动，OK 进入 |
| 故事讲述 | 主题页 OK | 显示故事标题与正文，无方框/空白 |
| 追问 | 讲述页 UP/DOWN 选气泡 + OK | 播放对应预置回答 |
| 结构化提问 | 欢迎页长按 OK | 进入问题列表，OK 选中提问 |
| 越界拦截 | 选中"报价多少" | 走边界话术，不讲任何相关内容 |
| 离网 | **全程不连 Wi-Fi/网络** | 所有流程照常，无卡顿、无网络请求 |
| 电量 | 右上角 | 显示百分比；电量计不可用时留空不报错 |
| 字体 | 逐字对照 `docs/font_chars.txt` | 无方框、无空白、无截断 |
| 形象 | 各页面 | 奋斗鹅 7 种表情正确切换，无水印 |

---

## 10. 交付报告模板（归档用）

```
Build: PASS / FAIL / NOT RUN
Host tests: PASS / FAIL / NOT RUN
Device tests: PASS / FAIL / NOT RUN
Unverified: 
  - 中文显示（需真机逐字验收）
  - 预录语音（当前为提示音降级，待录音入库）
  - 内容事实（示例文案待责任人审核）
```

---

## 11. 已知边界（不虚标）

- **不能听懂任意语音提问**：ESP32-C3 无 PSRAM、8MB Flash，离网跑不了通用语音识别。当前"主动提问"通过**屏幕问题列表**（长按 OK 进入）实现，离线可用且稳定。按住 OK 直接说中文自由提问属于后续 KWS 扩展。
- **回答来自预置内容**：不联网生成，未命中意图时走澄清话术，绝不编造。
- **语音默认降级**：无录音时是提示音 + 屏幕文字，不是真人语音。
- **内容为示例**：给客户前必须完成内容审核（红线 1）。
