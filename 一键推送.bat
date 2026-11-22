@echo off
chcp 65001 >nul
title AI Passport 奋斗鹅 - 一键推送到 GitHub
cd /d "%~dp0"

set "GITBASH=C:\Users\e00827508\.workbuddy\binaries\PortableGit\versions\1.3.0\bin\bash.exe"

echo ============================================================
echo   AI Passport 奋斗鹅 - 一键推送到 GitHub 并触发云编译
echo ============================================================
echo.
echo  本脚本会: 推送代码 - 打 tag - 触发 Build firmware 云编译
echo.
echo  请先确认你已在 GitHub 网页创建了【空仓库】:
echo    https://github.com/722battle/ai-passport-storyteller
echo.
echo  令牌获取: https://github.com/settings/tokens
echo    Generate new token (classic) - 只勾 repo - 复制 ghp_ 开头的串
echo.
echo ============================================================
echo.

set /p "TOKEN=请粘贴 GitHub 令牌(ghp_开头)后按回车: "

if "%TOKEN%"=="" (
    echo [错误] 令牌不能为空。
    pause
    exit /b 1
)

set "GITHUB_TOKEN=%TOKEN%"

echo.
echo 正在推送，请稍候...
echo.

"%GITBASH%" --login -c "cd '/c/Users/e00827508/WorkBuddy/2026-11-22-15-18-35/huawei-storyteller' && bash tools/push_to_github.sh"

echo.
if errorlevel 1 (
    echo [失败] 推送未完成。请把上面的红色报错发给助手(注意遮住令牌)。
) else (
    echo [成功] 已推送。请到 GitHub 仓库的 Actions 页查看 Build firmware 结果。
    echo        https://github.com/722battle/ai-passport-storyteller/actions
)
echo.
pause
