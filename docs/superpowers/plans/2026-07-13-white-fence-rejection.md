# White Fence and Adjacent Track Rejection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有逐行搜线中加入黑色间隔检测和双参考赛道锁定，拒绝白色围栏及相邻赛道，同时在识别不足时保持上一帧中线。

**Architecture:** 保留现有边线搜索主体，在 `image.c` 内增加一个无硬件依赖的候选验证函数，并用上一帧同一行与当前帧上一可信行共同约束候选。使用内部上一帧边线缓存锁定当前赛道；锁定后禁止图像中心全局补搜，无效行只作为搜索连续性的回退数据，不参与最终中线权重。

**Tech Stack:** C99、TC264D、TASKING/ADS、MinGW GCC 合成行测试、Python 3、GBK/CP936 应用源码。

---

### Task 1: 建立候选验证的失败测试

**Files:**
- Create: `tests/test_white_fence_filter.py`
- Inspect: `code/image.c`

- [ ] **Step 1: 创建提取并编译生产验证函数的测试**

测试脚本按 GBK 读取 `code/image.c`，提取 `LINE_FILTER_TEST_BEGIN/END` 标记之间的生产函数，通过临时 C 程序验证五种场景：正常赛道、跨越黑色间隔、相邻赛道切换、正常小幅弯道、宽度突变。脚本使用 `tempfile.TemporaryDirectory()`，不在项目中留下编译产物。

```python
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IMAGE_C = ROOT / "code" / "image.c"

source = IMAGE_C.read_bytes().decode("gbk")
match = re.search(
    r"/\* LINE_FILTER_TEST_BEGIN \*/(.*?)/\* LINE_FILTER_TEST_END \*/",
    source,
    re.S,
)
if not match:
    raise AssertionError("production line filter function not found")

harness = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t uint8;
#define MT9V03X_W 188
#define CONFIG_LINE_MAX_BLACK_GAP 3
#define CONFIG_LINE_MAX_MID_JUMP 6
#define CONFIG_LINE_MAX_WIDTH_JUMP 12
''' + match.group(1) + r'''

static void white_segment(uint8 *row, int left, int right)
{
    for(int i = left; i <= right; ++i) row[i] = 255;
}

static int expect(const char *name, int actual, int expected)
{
    if(actual != expected)
    {
        printf("FAIL %s actual=%d expected=%d\n", name, actual, expected);
        return 1;
    }
    return 0;
}

int main(void)
{
    uint8 row[MT9V03X_W];
    int failed = 0;

    memset(row, 0, sizeof(row));
    white_segment(row, 40, 140);
    failed += expect("normal", line_candidate_is_valid(row, 40, 140, 40, 140, 1), 1);

    memset(row, 0, sizeof(row));
    white_segment(row, 30, 80);
    white_segment(row, 90, 150);
    failed += expect("black_gap", line_candidate_is_valid(row, 30, 150, 30, 150, 0), 0);

    memset(row, 0, sizeof(row));
    white_segment(row, 90, 150);
    failed += expect("adjacent_track", line_candidate_is_valid(row, 90, 150, 30, 80, 1), 0);

    memset(row, 0, sizeof(row));
    white_segment(row, 43, 142);
    failed += expect("normal_curve", line_candidate_is_valid(row, 43, 142, 40, 140, 1), 1);

    memset(row, 0, sizeof(row));
    white_segment(row, 35, 155);
    failed += expect("width_jump", line_candidate_is_valid(row, 35, 155, 40, 140, 1), 0);

    if(failed == 0) printf("PASS white fence filter\n");
    return failed != 0;
}
'''

with tempfile.TemporaryDirectory(prefix="white-fence-filter-") as temp_dir:
    temp = Path(temp_dir)
    source_file = temp / "line_filter_test.c"
    exe_file = temp / "line_filter_test.exe"
    source_file.write_text(harness, encoding="utf-8")
    subprocess.run(
        ["gcc", "-std=c99", "-Wall", "-Wextra", "-Werror", str(source_file), "-o", str(exe_file)],
        check=True,
    )
    result = subprocess.run([str(exe_file)], check=True, text=True, capture_output=True)
    if "PASS white fence filter" not in result.stdout:
        raise AssertionError(result.stdout)
    print(result.stdout.strip())
```

- [ ] **Step 2: 运行测试并确认 RED**

Run:

```powershell
& 'D:\CodexTools\python\Scripts\python.exe' tests\test_white_fence_filter.py
```

Expected: FAIL，错误为 `production line filter function not found`。

### Task 2: 增加过滤参数和公开状态

**Files:**
- Modify: `code/config.h`
- Modify: `code/image.h`

- [ ] **Step 1: 在 `config.h` 图像参数区加入四个参数**

```c
/* 围栏与相邻赛道过滤：内部黑色间隔、行间跳变及整帧最少可信行数。 */
#define CONFIG_LINE_MAX_BLACK_GAP         3
#define CONFIG_LINE_MAX_MID_JUMP          6
#define CONFIG_LINE_MAX_WIDTH_JUMP        12
#define CONFIG_LINE_MIN_VALID_ROWS        6
```

- [ ] **Step 2: 在 `image.h` 共享识别结果区声明有效状态**

```c
/** 当前帧各行边线是否通过围栏与相邻赛道过滤。 */
extern uint8 line_valid_list[MT9V03X_H];
/** 当前帧搜线范围内通过过滤的有效行数量。 */
extern uint8 line_valid_count;
```

- [ ] **Step 3: 运行参数和声明断言**

Run:

```powershell
$e = [Text.Encoding]::GetEncoding(936)
$config = $e.GetString([IO.File]::ReadAllBytes((Resolve-Path 'code/config.h')))
$header = $e.GetString([IO.File]::ReadAllBytes((Resolve-Path 'code/image.h')))
@('CONFIG_LINE_MAX_BLACK_GAP','CONFIG_LINE_MAX_MID_JUMP','CONFIG_LINE_MAX_WIDTH_JUMP','CONFIG_LINE_MIN_VALID_ROWS') |
    ForEach-Object { if (-not $config.Contains($_)) { throw "missing $_" } }
@('line_valid_list','line_valid_count') |
    ForEach-Object { if (-not $header.Contains($_)) { throw "missing $_" } }
```

Expected: exit code 0。

### Task 3: 实现黑色间隔和连续性验证

**Files:**
- Modify: `code/image.c`
- Test: `tests/test_white_fence_filter.py`

- [ ] **Step 1: 增加状态和历史缓存**

在图像全局状态区增加：

```c
uint8 line_valid_list[MT9V03X_H];
uint8 line_valid_count;
static uint8 previous_left_line_list[MT9V03X_H];
static uint8 previous_right_line_list[MT9V03X_H];
static uint8 track_history_ready;
```

- [ ] **Step 2: 增加可由测试提取的候选验证函数**

```c
/* LINE_FILTER_TEST_BEGIN */
static uint8 line_abs_diff(uint8 first, uint8 second)
{
    return first >= second ? first - second : second - first;
}

static uint8 line_candidate_is_valid(
    const uint8 *row,
    uint8 left,
    uint8 right,
    uint8 reference_left,
    uint8 reference_right,
    uint8 use_reference)
{
    uint8 black_run = 0;
    uint8 candidate_mid;
    uint8 reference_mid;
    uint8 candidate_width;
    uint8 reference_width;

    if(left < 1 || right > MT9V03X_W - 2 || left >= right)
    {
        return 0;
    }

    for(uint8 column = left; column <= right; column++)
    {
        if(row[column] == 0)
        {
            black_run++;
            if(black_run > CONFIG_LINE_MAX_BLACK_GAP)
            {
                return 0;
            }
        }
        else
        {
            black_run = 0;
        }
    }

    if(!use_reference)
    {
        return 1;
    }

    if(reference_left < 1 || reference_right > MT9V03X_W - 2 || reference_left >= reference_right)
    {
        return 0;
    }

    candidate_mid = (left + right) / 2;
    reference_mid = (reference_left + reference_right) / 2;
    candidate_width = right - left;
    reference_width = reference_right - reference_left;

    if(line_abs_diff(candidate_mid, reference_mid) > CONFIG_LINE_MAX_MID_JUMP)
    {
        return 0;
    }
    if(line_abs_diff(candidate_width, reference_width) > CONFIG_LINE_MAX_WIDTH_JUMP)
    {
        return 0;
    }
    return 1;
}
/* LINE_FILTER_TEST_END */
```

- [ ] **Step 3: 运行合成行测试并确认 GREEN**

Run:

```powershell
& 'D:\CodexTools\python\Scripts\python.exe' tests\test_white_fence_filter.py
```

Expected: `PASS white fence filter`。

### Task 4: 将双参考赛道锁定接入逐行搜线

**Files:**
- Modify: `code/image.c`
- Test: source assertions and synthetic filter test

- [ ] **Step 1: 每帧初始化有效状态并从锁定历史选择起点**

`image_deal()` 开始时清零 `line_valid_list` 和 `line_valid_count`。未锁定时使用 `left_jidian/right_jidian`；锁定后使用上一帧起始行缓存作为 `left_point/right_point`。

- [ ] **Step 2: 锁定后禁止中心全局补搜**

两个 `mid_start_*_search_judge` 分支只在 `track_history_ready == 0` 时执行。局部搜索失败时保留 `left_found/right_found == 0`，交给统一回退处理，不再寻找图像中心附近的其他白色区域。

- [ ] **Step 3: 对每行执行双参考验证**

候选必须先满足 `left_found && right_found`，再通过：

```c
row_valid = line_candidate_is_valid(
    index[i], left_point, right_point,
    current_reference_left, current_reference_right,
    current_reference_ready);

if(row_valid && track_history_ready)
{
    row_valid = line_candidate_is_valid(
        index[i], left_point, right_point,
        previous_left_line_list[i], previous_right_line_list[i], 1);
}
```

通过后更新当前帧参考、有效标志和计数。失败后优先沿用当前帧上一可信行，否则沿用上一帧同一行，第一帧无历史时退回极点；失败行保持 `line_valid_list[i] = 0`。

- [ ] **Step 4: 一帧结束后统一更新历史并完成首次锁定**

完成全部搜索行后，将本帧左右边线复制到对应历史数组。只有当 `line_valid_count >= CONFIG_LINE_MIN_VALID_ROWS` 时才将 `track_history_ready` 置 1；置位后不自动清零。

- [ ] **Step 5: 运行赛道锁定源码断言**

Run:

```powershell
$e = [Text.Encoding]::GetEncoding(936)
$source = $e.GetString([IO.File]::ReadAllBytes((Resolve-Path 'code/image.c')))
@(
    'previous_left_line_list', 'previous_right_line_list', 'track_history_ready',
    'line_candidate_is_valid', 'line_valid_list[i]', 'line_valid_count',
    'if(mid_start_left_search_judge == 1 && !track_history_ready)',
    'if(mid_start_right_search_judge == 1 && !track_history_ready)'
) | ForEach-Object { if (-not $source.Contains($_)) { throw "missing $_" } }
```

Expected: exit code 0。

### Task 5: 只使用可信行计算最终中线

**Files:**
- Modify: `code/image.c`
- Test: source assertions

- [ ] **Step 1: 有效行不足时保持上一帧中线**

在 `find_mid_line_weight()` 累加前增加：

```c
if(line_valid_count < CONFIG_LINE_MIN_VALID_ROWS)
{
    return last_mid_line;
}
```

- [ ] **Step 2: 加权时跳过无效行并保护零权重**

循环中只在 `line_valid_list[i]` 为真时累加。循环后若 `weight_sum == 0`，返回 `last_mid_line`。只有成功得到新中线后才执行原有 0.2/0.8 滤波并更新 `last_mid_line`。

- [ ] **Step 3: 运行最终中线源码断言和合成测试**

Run:

```powershell
& 'D:\CodexTools\python\Scripts\python.exe' tests\test_white_fence_filter.py
$e = [Text.Encoding]::GetEncoding(936)
$source = $e.GetString([IO.File]::ReadAllBytes((Resolve-Path 'code/image.c')))
@('line_valid_count < CONFIG_LINE_MIN_VALID_ROWS','if(line_valid_list[i])','if(weight_sum == 0)') |
    ForEach-Object { if (-not $source.Contains($_)) { throw "missing $_" } }
```

Expected: 合成测试 PASS，源码断言 exit code 0。

### Task 6: 回归验证、编译、提交和推送

**Files:**
- Verify: `code/config.h`, `code/image.h`, `code/image.c`
- Verify unchanged: `code/motor.c`, `code/pid.c`, `code/steer.c`, `user/cpu0_main.c`, `user/isr.c`
- Add: `tests/test_white_fence_filter.py`
- Add: this plan document

- [ ] **Step 1: 验证非目标控制文件没有变化**

Run:

```powershell
git diff --exit-code HEAD -- code/motor.c code/pid.c code/steer.c user/cpu0_main.c user/isr.c
```

Expected: exit code 0。

- [ ] **Step 2: 运行完整静态验证**

检查四个参数值、两个公开状态、三个历史状态、双参考调用、锁定后禁用全局补搜、有效行加权、GBK 解码、括号与预处理指令平衡，并再次运行合成行测试。

- [ ] **Step 3: 执行 ADS Debug 增量构建**

优先使用当前 ADS 工程的 Incremental Build。若命令行 TASKING 构建入口不可用，记录静态测试结果，并明确要求在 ADS 中手动构建后再上车。

- [ ] **Step 4: 审计并提交**

Run:

```powershell
git add code/config.h code/image.h code/image.c tests/test_white_fence_filter.py docs/superpowers/plans/2026-07-13-white-fence-rejection.md
git diff --cached --check -- code/ tests/ docs/superpowers/plans/2026-07-13-white-fence-rejection.md
git commit -m "feat: reject white fences and adjacent tracks"
```

Expected: 提交成功且只包含目标文件、测试和计划。

- [ ] **Step 5: 非强制推送并验证远程**

Run:

```powershell
git push origin main
$local = git rev-parse HEAD
$remote = ((git ls-remote origin refs/heads/main) -split '\s+')[0]
if ($local -ne $remote) { throw "remote mismatch" }
```

Expected: `main` 推送成功，本地与远程提交哈希一致。
