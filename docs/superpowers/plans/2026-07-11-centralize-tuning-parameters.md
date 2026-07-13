# Centralize Tuning Parameters Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将智能车工程中的可调参数集中到 `code/config.h`，并保持小车运动控制逻辑、硬件映射和初始行为不变。

**Architecture:** 使用一个预处理配置头文件作为唯一调参入口。各模块保留自己的接口、运行时状态和硬件引脚，仅把分散的调参宏、PID 初始值、控制周期和中线权重改为引用配置项。

**Tech Stack:** C、AURIX TC264、TASKING C 编译器、现有 ADS 工程。

---

### Task 1: 建立配置项清单并创建统一配置头文件

**Files:**
- Create: `code/config.h`
- Reference: `code/image.c`, `code/image.h`, `code/steer.h`, `code/pid.h`, `code/pid.c`, `code/motor.h`, `code/motor.c`, `user/cpu0_main.c`

- [ ] **Step 1: 在配置文件中定义当前值**

按以下分区建立宏，数值必须与现有代码完全一致：

```c
#ifndef CODE_CONFIG_H_
#define CODE_CONFIG_H_

/* Image tuning */
#define CONFIG_GRAY_SCALE                 256
#define CONFIG_JIDIAN_SEARCH_LINE         120
#define CONFIG_SEARCH_START_LINE          120
#define CONFIG_SEARCH_END_LINE            30
#define CONFIG_LEFT_SEARCH_RIGHT          10
#define CONFIG_LEFT_SEARCH_LEFT           5
#define CONFIG_RIGHT_SEARCH_RIGHT         5
#define CONFIG_RIGHT_SEARCH_LEFT          10
#define CONFIG_MID_WIDTH                  93
#define CONFIG_THRESHOLD_OFFSET           40

/* Steering tuning */
#define CONFIG_SERVO_PWM_MIDDLE           750
#define CONFIG_SERVO_PWM_MIN              675
#define CONFIG_SERVO_PWM_MAX              835
#define CONFIG_SERVO_OUTPUT_LIMIT         80
#define CONFIG_SERVO_KP_BASE              0.7f
#define CONFIG_SERVO_KP_DYNAMIC_GAIN      0.045f
#define CONFIG_SERVO_KD_BASE              5.0f
#define CONFIG_SERVO_LOW_PASS             0.8f

/* Motor tuning */
#define CONFIG_MOTOR_PID_KP               8.0f
#define CONFIG_MOTOR_PID_KI               1.2f
#define CONFIG_MOTOR_PID_KD               0.0f
#define CONFIG_MOTOR_I_LIMIT              30
#define CONFIG_MOTOR_PWM_LIMIT            2300
#define CONFIG_MOTOR_TARGET_SPEED         90
#define CONFIG_MOTOR_SPEED_LIMIT          30

/* Control timing */
#define CONFIG_SERVO_CONTROL_PERIOD_MS    20
#define CONFIG_MOTOR_CONTROL_PERIOD_MS   5

/* Mid-line weight profile. Index is the camera row. */
#define CONFIG_MID_WEIGHT_LIST \
{ \
    1,1,1,1,1,1,1,1,1,1, \
    1,1,1,1,1,1,1,1,1,1, \
    1,1,1,1,1,1,1,1,1,1, \
    1,1,1,1,1,1,1,1,1,1, \
    1,1,1,1,1,1,1,1,1,1, \
    1,1,1,1,1,1,1,1,1,1, \
    1,1,1,1,1,1,1,1,1,1, \
    7,8,9,10,11,12,13,14,15,16, \
    17,18,19,20,20,20,20,19,18,17, \
    16,15,14,13,12,11,10,9,8,7, \
    6,6,6,6,6,6,6,6,6,6, \
    6,6,6,6,6,6,6,6,6,6 \
}

#endif
```

- [ ] **Step 2: 检查配置项覆盖率**

使用 PowerShell 搜索原始调参定义，确保每一项都有对应的 `CONFIG_` 宏；硬件引脚宏和中断优先级不进入配置文件。

Run:

```powershell
rg -n "#define (jidian_search|search_|left_line_|right_line_|MID_W|SERVO_|pid_limit)|PID_CREATE|Kp_base|Kd_base|pit_ms_init|mid_weight_list" code user
```

Expected: 搜索结果只显示待替换的旧定义/使用点，以及配置文件中的新定义，不出现遗漏的调参常量。

### Task 2: 迁移图像模块参数

**Files:**
- Modify: `code/image.h`
- Modify: `code/image.c`

- [ ] **Step 1: 让 `image.h` 引用配置文件**

在 `#include "zf_common_headfile.h"` 后加入 `#include "config.h"`，删除图像搜索宏和 `MID_W` 的本地定义，保留接口和 `extern` 声明。

- [ ] **Step 2: 替换 `image.c` 的图像调参符号**

删除文件顶部的 `GrayScale`、`grayscale`、搜索行和搜索范围宏，使用对应的 `CONFIG_` 宏；将中线权重数组的初始化改为：

```c
uint8 mid_weight_list[120] = CONFIG_MID_WEIGHT_LIST;
```

将特殊阈值的 `40` 替换为 `CONFIG_THRESHOLD_OFFSET`，将 `MID_W`、搜索行和搜索范围全部替换为配置符号。不得修改搜线判断条件和循环顺序。

- [ ] **Step 3: 检查图像模块差异边界**

确认 `image.c` 中除参数符号替换和配置引用外，没有新增/删除控制分支。

Run:

```powershell
rg -n "#define (GrayScale|grayscale|jidian_search|search_|left_line_|right_line_|MID_W)|CONFIG_" code/image.c code/image.h
```

Expected: 旧图像调参宏不再定义，所有对应使用点指向 `CONFIG_`。

### Task 3: 迁移舵机和 PID 参数

**Files:**
- Modify: `code/steer.h`
- Modify: `code/pid.h`
- Modify: `code/pid.c`
- Modify: `code/steer.c`

- [ ] **Step 1: 迁移舵机常量**

让 `steer.h` 引用 `config.h`，删除 `SERVO_PWMMID`、`SERVO_MIN`、`SERVO_MAX` 的定义，并在 `steer.c` 中将它们替换为配置宏；将现有 `80` 输出限幅替换为 `CONFIG_SERVO_OUTPUT_LIMIT`。

- [ ] **Step 2: 迁移 PID 初始值和动态参数**

将 `pid.c` 的初始化改为：

```c
PID servo_pid = PID_CREATE(0, 0, 0, CONFIG_SERVO_LOW_PASS);
float Kp_base = CONFIG_SERVO_KP_BASE;
float Kd_base = CONFIG_SERVO_KD_BASE;
PID motor_pid_l = PID_CREATE(CONFIG_MOTOR_PID_KP, CONFIG_MOTOR_PID_KI, CONFIG_MOTOR_PID_KD, 1);
PID motor_pid_r = PID_CREATE(CONFIG_MOTOR_PID_KP, CONFIG_MOTOR_PID_KI, CONFIG_MOTOR_PID_KD, 1);
```

将动态 Kp 中的 `0.045` 替换为 `CONFIG_SERVO_KP_DYNAMIC_GAIN`，将 `pid_out_I_limit` 替换为 `CONFIG_MOTOR_I_LIMIT`。`PID_Normal` 和 `PID_Increase` 的算法表达式不改。

- [ ] **Step 3: 静态确认 PID 行为未扩展**

Run:

```powershell
rg -n "PID_CREATE|Kp_base|Kd_base|0\.045|pid_out_I_limit|CONFIG_" code/pid.c code/pid.h code/steer.c code/steer.h
```

Expected: PID 调参值只来自 `config.h`，控制计算函数仍保持原有结构。

### Task 4: 迁移电机和控制周期参数

**Files:**
- Modify: `code/motor.h`
- Modify: `code/motor.c`
- Modify: `user/cpu0_main.c`

- [ ] **Step 1: 迁移电机限制和启动参数**

让 `motor.h` 引用 `config.h`，删除 `pid_limit`；在 `motor.c` 中将 `pid_limit` 替换为 `CONFIG_MOTOR_PWM_LIMIT`，将 `Final_Motor_Control(90,30)` 替换为 `Final_Motor_Control(CONFIG_MOTOR_TARGET_SPEED, CONFIG_MOTOR_SPEED_LIMIT)`。

- [ ] **Step 2: 迁移 PIT 周期**

在 `cpu0_main.c` 引入 `config.h`，将：

```c
pit_ms_init(CCU60_CH0, 20);
pit_ms_init(CCU60_CH1, 5);
```

改为：

```c
pit_ms_init(CCU60_CH0, CONFIG_SERVO_CONTROL_PERIOD_MS);
pit_ms_init(CCU60_CH1, CONFIG_MOTOR_CONTROL_PERIOD_MS);
```

不要修改初始化顺序和中断函数内容。

- [ ] **Step 3: 检查硬件映射未被移动**

Run:

```powershell
rg -n "MotorL_pwm|MotorL_turn|MotorR_pwm|MotorR_turn|ATOM0_CH1_P33_9" code/motor.h code/steer.c
```

Expected: 硬件引脚仍在原模块中，`config.h` 不包含引脚枚举。

### Task 5: 完成静态验证并尝试工程构建

**Files:**
- Verify: `code/config.h`, `code/image.c`, `code/image.h`, `code/steer.c`, `code/steer.h`, `code/pid.c`, `code/pid.h`, `code/motor.c`, `code/motor.h`, `user/cpu0_main.c`

- [ ] **Step 1: 验证旧参数只剩配置文件定义**

Run:

```powershell
rg -n "#define (GrayScale|grayscale|jidian_search_line|search_start_line|search_end_line|left_line_right_scarch|left_line_left_scarch|right_line_right_scarch|right_line_left_scarch|MID_W|SERVO_PWMMID|SERVO_MIN|SERVO_MAX|pid_limit)|Kp_base=|Kd_base=|PID_CREATE\(" code user
```

Expected: 旧宏定义不再出现在模块头文件中；PID 初始化参数使用 `CONFIG_` 符号。

- [ ] **Step 2: 验证配置入口完整**

Run:

```powershell
rg -n "CONFIG_" code/config.h code/image.c code/image.h code/steer.c code/steer.h code/pid.c code/pid.h code/motor.c code/motor.h user/cpu0_main.c
```

Expected: 所有集中后的调参项均在 `config.h` 定义并至少有一个使用点，且没有未定义的 `CONFIG_` 使用。

- [ ] **Step 3: 尝试使用现有构建入口**

检查 `Debug/makefile` 是否包含可执行的 TASKING 编译器路径；若可用，在项目目录执行现有 make 目标。若工具链不可用，记录具体原因，并使用静态检查结果作为验证，不声称硬件工程构建通过。

- [ ] **Step 4: 交付变更清单**

确认最终只修改配置集中化相关文件和新增文档，不修改电机/舵机控制算法，不提交 `Debug` 生成物。
