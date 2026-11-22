[English](CLOUD_BUILD.md) · **简体中文**

# 云编译并部署到 AI Passport 官方平台（最短路径）

本机没有 ESP-IDF 也能出固件：用 GitHub 云端编译，下载 bin，上传到官方平台。全程不需要在本机装工具链。

---

## 1. 新建 GitHub 仓库

1. 登录 GitHub，点右上角 `+` → `New repository`。
2. 仓库名随意，例如 `ai-passport-storyteller`。
3. **关键：不要勾选** "Add a README file" / ".gitignore" / "license"——必须是空仓库。
4. 复制仓库地址，形如 `https://github.com/<你的用户名>/ai-passport-storyteller.git`。

## 2. 改脚本并推送

用文本编辑器打开工程里的 `tools/push_to_github.sh`，把这一行改成你的地址：
```bash
REPO_URL="https://github.com/<你的用户名>/ai-passport-storyteller.git"
```

然后在工程目录执行（Git Bash 终端）：
```bash
bash tools/push_to_github.sh
```

看到 `已推送` 即成功。脚本会自动打 `storyteller-v1` 标签触发编译。

> 如果推送时要求登录：GitHub 现在用 Personal Access Token（PAT）代替密码。
> 去 GitHub → Settings → Developer settings → Personal access tokens → 生成带 `repo` 权限的 token，
> 在提示输入密码时粘贴 token（用户名仍是你的 GitHub 用户名）。

## 3. 等云编译完成

1. 打开你的 GitHub 仓库 → **Actions** 标签页。
2. 找到 **Build firmware** 工作流，点进最新一次运行。
3. 等所有步骤变绿（约 3–8 分钟）。
   - 用的是仓库自带的 ESP-IDF 5.5.3，产物名固定为 `FoloToy-AI-Passport-full.bin`。
   - 如果某步失败（红色），点进去看日志，把报错发我，我针对性修代码。

## 4. 下载 bin

两种方式任选：

**方式 A（Artifact）**：本次运行页面往下拉 → **Artifacts** → `passport-storyteller-v1` → 下载 ZIP → 解压出 `FoloToy-AI-Passport-full.bin`。

**方式 B（Release）**：因为脚本打了 tag，会自动生成 Release → 右侧 **Releases** → 下载 `FoloToy-AI-Passport-full.bin`。

## 5. 上传到官方平台

1. 打开 AI Passport 官方固件烧录页面。
2. 在上传处选择"**单个合并固件**"，上传刚下载的 `FoloToy-AI-Passport-full.bin`。
3. 按平台提示完成部署。

> 合并固件是从 0x0 起始的完整镜像，一个文件即可，不需要单独上传分区 bin。

---

## 常见问题

**Q：为什么要打 tag？**
A：仓库的 `Build firmware` 工作流配置了 tag push 自动触发。打 tag 是最省事的触发方式；也可以在 Actions 页手动点 `Run workflow`。

**Q：编译失败怎么办？**
A：把 Actions 日志里的红色报错复制给我。本机没有编译器，云编译日志是我们发现真实编译问题的途径。

**Q：上传后设备没反应？**
A：先确认下载的是合并固件 `FoloToy-AI-Passport-full.bin`（不是别的 bin）；用支持数据传输的 USB 线；按 `docs/DEPLOY.zh_CN.md` 第 9 节做真机验收。

**Q：语音为什么是提示音不是说话？**
A：当前没有预录语音，按设计降级为"提示音 + 屏幕文字"，交互完整。要让奋斗鹅真正开口，按 `docs/DEPLOY.zh_CN.md` 第 3 节录 WAV 并重新推送编译。

**Q：上线前还要注意什么？**
A：两条红线——① `content/stories.csv` 的故事都要内容审核（`verified` 改 true、补来源 URL）；② 字库和形象授权的商用确认。详见 `docs/ASSETS.zh_CN.md`。
