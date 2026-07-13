# GitHub Release and README Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将当前完整 TC264 智能车 ADS 源码工程整理后发布到 `Deng919/2026smartcar` 的 `main` 分支，并提供包含成员分工的中文 README。

**Architecture:** 以现有工程根目录作为 Git 仓库根目录，保留源码、逐飞/英飞凌库和 ADS 工程配置；通过根目录 `.gitignore` 排除构建产物与本机状态。先验证忽略规则和 README 内容，再审计暂存文件、提交，最后在确认远程仍为空后执行非强制推送并核对远程提交哈希。

**Tech Stack:** Git、GitHub、Markdown、PowerShell、AURIX Development Studio 1.10.10、TASKING C/C++、TC264D。

---

### Task 1: 建立源码仓库忽略规则

**Files:**
- Create: `.gitignore`
- Test: existing `Debug/` and `.ads/` paths

- [ ] **Step 1: 运行修改前忽略规则断言并确认失败**

Run:

```powershell
git check-ignore Debug/Seekfree_TC264_Opensource_Library.elf .ads/winIDEAWorkspaces/Debug/ADS.xjrf
```

Expected: exit code 1，两个路径当前都未被忽略。

- [ ] **Step 2: 创建根目录 `.gitignore`**

创建以下完整内容：

```gitignore
# AURIX Development Studio / TASKING 构建目录
/Debug/
/Release/
**/Debug/
**/Release/

# TASKING 编译与链接产物
*.o
*.obj
*.src
*.d
*.elf
*.hex
*.map
*.mdf
*.siz
*.opt

# ADS、调试器与 Eclipse 本机状态
/.ads/
.metadata/

# 编辑器和操作系统临时文件
.vscode/
.idea/
*.user
*.tmp
*.bak
*.swp
Thumbs.db
.DS_Store

# 脚本缓存
__pycache__/
*.py[cod]
```

- [ ] **Step 3: 验证忽略规则生效且工程配置未被误忽略**

Run:

```powershell
git check-ignore -v Debug/Seekfree_TC264_Opensource_Library.elf .ads/winIDEAWorkspaces/Debug/ADS.xjrf
git check-ignore .project .cproject .settings/com.infineon.aurix.buildsystem.prefs
```

Expected: 第一条列出 `.gitignore` 命中规则；第二条 exit code 1，说明 ADS 工程配置仍会纳入仓库。

### Task 2: 编写中文项目 README

**Files:**
- Create: `README.md`
- Inspect: `code/config.h`
- Inspect: `code/image.c`, `code/motor.c`, `code/pid.c`, `code/steer.c`
- Inspect: `user/cpu0_main.c`, `user/isr.c`

- [ ] **Step 1: 写入 README 的项目与环境说明**

README 开头必须明确：

```markdown
# 2026 智能车 TC264 工程

基于逐飞科技 TC264 开源库和 Infineon iLLD 的智能车控制工程，使用 MT9V03X 灰度摄像头完成赛道识别，并通过舵机转向、左右轮差速、速度前馈和增量式 PID 实现车辆控制。

## 开发环境

- 主控：Infineon AURIX TC264D
- IDE：AURIX Development Studio 1.10.10
- 编译器：TASKING C/C++
- 摄像头：MT9V03X
- 显示：TFT180
- 执行机构：舵机、左右驱动电机和双编码器
```

- [ ] **Step 2: 写入目录结构与运行流程**

目录说明必须覆盖 `code/`、`user/`、`libraries/`、`docs/`、`code/config.h`。运行流程按以下真实调用顺序描述：DMA 完整帧标志 -> 复制灰度图 -> Otsu -> 二值化 -> 左右起点 -> 逐行边线 -> 丢线保持 -> 加权中线 -> 舵机/电机周期控制。

- [ ] **Step 3: 写入控制、按键和调参说明**

README 必须明确：

```markdown
## 按键操作

| 按键 | 功能 |
| --- | --- |
| KEY_1 | 短按切换车辆运行/停车状态。 |
| KEY_2 | 短按开关 TFT 图像显示；关闭显示后图像识别和车辆控制仍继续运行。 |
```

参数说明统一指向 `code/config.h`，覆盖图像阈值与搜线范围、舵机增益与限幅、电机 PID 与前馈、目标速度与弯道降速、控制周期和中线权重。

- [ ] **Step 4: 写入成员分工、编译步骤和注意事项**

成员分工使用以下表格，不扩展未确认职责：

```markdown
## 成员分工

| 成员 | 分工 |
| --- | --- |
| HPX | 基础代码构建 |
| DZH | 代码优化 |
```

编译步骤说明 ADS 导入现有工程、选择 Debug 配置、Incremental Build、检查错误后下载。注意事项说明白色围栏和黑色间隔可能造成边线误识别，参数必须结合摄像头安装位置、光照和实车速度调节；保留逐飞开源库和 GPL 许可证说明。

- [ ] **Step 5: 验证 README 与源码一致**

Run:

```powershell
$readme = Get-Content -Raw -Encoding UTF8 README.md
@('TC264D','MT9V03X','code/config.h','KEY_1','KEY_2','HPX','基础代码构建','DZH','代码优化','Incremental Build') |
    ForEach-Object { if (-not $readme.Contains($_)) { throw "README 缺少：$_" } }
```

Expected: exit code 0，无缺失项。

### Task 3: 审计并提交完整源码

**Files:**
- Add: `.gitignore`, `README.md`
- Add: `.project`, `.cproject`, `.settings/`
- Add: `code/`, `user/`, `libraries/`, `docs/`
- Add: project scripts, linker script, launch configuration and pin notes
- Exclude: `Debug/`, `Release/`, `.ads/`

- [ ] **Step 1: 暂存全部允许内容**

Run:

```powershell
git add -A
git status --short
```

Expected: 源码和工程配置显示为新增；没有 `Debug/`、`.ads/`、`.elf`、`.hex`、`.map`。

- [ ] **Step 2: 执行暂存文件安全审计**

Run:

```powershell
$staged = git diff --cached --name-only
$forbidden = $staged | Where-Object {
    $_ -match '(^|/)(Debug|Release|\.ads)/' -or
    $_ -match '\.(elf|hex|map|mdf|o|obj|src|d|siz|opt)$'
}
if ($forbidden) { $forbidden; exit 1 }
git diff --cached --check -- .gitignore README.md docs/
```

Expected: exit code 0，不存在禁入文件；本次新增的 README、忽略规则和文档不存在空白错误。第三方逐飞与 Infineon 原始库保留其已有格式，不进行无关的批量空白清理。

- [ ] **Step 3: 执行源码发布静态检查**

Run:

```powershell
$required = @(
    'code/config.h','code/image.c','code/motor.c','code/pid.c','code/steer.c',
    'user/cpu0_main.c','user/isr.c','libraries/zf_common/zf_common_headfile.h',
    '.project','.cproject','README.md','.gitignore'
)
$required | ForEach-Object { if (-not (Test-Path -LiteralPath $_)) { throw "缺少文件：$_" } }
$gbk = [Text.Encoding]::GetEncoding(936)
@('code/image.c','code/motor.c','code/pid.c','code/steer.c','user/cpu0_main.c','user/isr.c') |
    ForEach-Object { [void]$gbk.GetString([IO.File]::ReadAllBytes((Resolve-Path $_))) }
```

Expected: exit code 0，关键源码存在且可按工程原 GBK 编码读取。

- [ ] **Step 4: 创建源码发布提交**

Run:

```powershell
git commit -m "feat: publish TC264 smart car project"
git status --short
```

Expected: 提交成功，工作区为空。

### Task 4: 配置远程并推送 GitHub

**Files:**
- Modify: Git repository remote configuration
- Remote: `https://github.com/Deng919/2026smartcar.git`

- [ ] **Step 1: 检查远程仍为空**

Run:

```powershell
git ls-remote https://github.com/Deng919/2026smartcar.git
```

Expected: exit code 0 且无引用输出。若出现任何分支或标签，停止，不执行推送。

- [ ] **Step 2: 配置并复核 origin**

Run:

```powershell
git remote add origin https://github.com/Deng919/2026smartcar.git
git remote -v
```

Expected: fetch 与 push 地址都精确等于目标仓库。

- [ ] **Step 3: 非强制推送 main 分支**

Run:

```powershell
git push -u origin main
```

Expected: GitHub 身份验证成功，远程创建 `main`，命令不使用 `--force`。

- [ ] **Step 4: 验证远程提交与本地一致**

Run:

```powershell
$local = git rev-parse HEAD
$remoteLine = git ls-remote origin refs/heads/main
$remote = ($remoteLine -split '\s+')[0]
if ($local -ne $remote) { throw "远程提交不一致：local=$local remote=$remote" }
git status --short --branch
```

Expected: 本地和远程哈希一致，`main` 跟踪 `origin/main`，工作区为空。
