# Application Code Structure and Comments Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不改变智能车运行行为的前提下，整理应用层代码结构、补全中文注释，并修正头文件与源码之间已经确认的接口声明不一致。

**Architecture:** 保持现有图像、电机、PID、舵机、主循环和中断模块边界不变；源文件仅调整注释与空白，头文件按“保护宏—依赖—宏—类型—外部变量—接口”统一组织。使用去除注释与空白后的哈希对比，证明所有 `.c` 文件的可执行内容未改变。

**Tech Stack:** C（Infineon AURIX TC264）、TASKING/ADS 工程、PowerShell、GBK/CP936 源文件编码。

---

### Task 1: 建立结构与行为基线

**Files:**
- Test: `$env:TEMP\codex-smartcar-structure-baseline.json`
- Inspect: `code/config.h`
- Inspect: `code/image.c`, `code/image.h`
- Inspect: `code/motor.c`, `code/motor.h`
- Inspect: `code/pid.c`, `code/pid.h`
- Inspect: `code/steer.c`, `code/steer.h`
- Inspect: `user/cpu0_main.c`, `user/isr.c`

- [x] 用 CP936 读取六个 `.c` 文件，删除块注释、行注释和空白后计算 SHA-256，保存基线。
- [x] 提取 `config.h` 中全部 `CONFIG_` 参数和值，保存参数快照。
- [x] 运行结构断言并确认当前状态为 RED：至少应发现部分头文件在保护宏之前包含依赖，以及 `motor.h` 存在与 `motor.c` 不一致的声明。
- [x] 记录各源文件函数定义顺序，作为调用结构未被重排的辅助基线。

### Task 2: 整理应用层头文件

**Files:**
- Modify: `code/config.h`
- Modify: `code/image.h`
- Modify: `code/motor.h`
- Modify: `code/pid.h`
- Modify: `code/steer.h`

- [x] 将每个头文件统一为：文件职责注释、保护宏、依赖、硬件宏/常量、类型、外部变量、公开函数、结束保护宏。
- [x] 为结构体字段、共享变量和公开接口补充完整中文注释，注明单位、方向约定、调用周期和副作用。
- [x] 让 `motor.h` 与实际定义对齐：声明 `MotorL_SetSpeed()`、保留 `MotorR_SetSpeed()`，移除未定义的 `MotorL_Init()` 与 `Motor_SetSpeed()`。
- [x] 不改变 `config.h` 中任何参数名、参数值和预处理条件。
- [x] 运行头文件结构断言并确认转为 GREEN。

### Task 3: 整理图像与控制模块源码

**Files:**
- Modify: `code/image.c`
- Modify: `code/motor.c`
- Modify: `code/pid.c`
- Modify: `code/steer.c`

- [x] 按现有函数顺序增加“全局状态、预处理、搜线、控制、硬件输出、通用工具”等章节注释，不移动函数。
- [x] 为每个函数补充中文块注释：用途、参数、返回值、调用时机和关键注意事项。
- [x] 为 Otsu、二值化、极点搜索、逐行边线跟踪、丢线保持、中线加权、速度前馈、增量 PID 和动态舵机 PID 补充原理性说明。
- [x] 将遗留英文注释改为中文，不修改对应语句。
- [x] 逐文件重新计算规范化哈希，必须与 Task 1 基线一致。

### Task 4: 整理主循环与中断源码

**Files:**
- Modify: `user/cpu0_main.c`
- Modify: `user/isr.c`

- [x] 在 `cpu0_main.c` 中清晰标注初始化、等待帧、图像处理、舵机控制和显示开关流程，删除无意义模板注释。
- [x] 在 `isr.c` 中说明 CH0 舵机周期、CH1 编码器/按键/电机周期、摄像头场中断、DMA 完成和串口回调职责。
- [x] 对未启用的空中断仅标记为预留，不虚构用途。
- [x] 保持所有中断清除、函数调用和条件判断的原始顺序。
- [x] 重新计算两个文件的规范化哈希，必须与 Task 1 基线一致。

### Task 5: 最终一致性验证

**Files:**
- Verify: all files above

- [x] 比较全部 `.c` 规范化哈希，确认可执行 token 未变化。
- [x] 比较 `CONFIG_` 参数快照，确认参数名和值未变化。
- [x] 检查所有头文件保护宏位于依赖包含之前，括号与预处理指令平衡。
- [x] 比较公开函数声明和源码定义，确认不存在本次整理引入的缺失或多余接口。
- [x] 搜索残留英文功能注释、乱码、临时文件和备份文件。
- [x] 若本机可定位 TASKING 构建工具，执行一次 Debug 构建；否则明确报告静态验证范围，并提示在 ADS 中增量编译。
