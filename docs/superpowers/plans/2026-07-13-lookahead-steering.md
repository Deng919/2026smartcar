# Lookahead Steering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Compute a filtered lookahead steering center from far and near image regions and use it for servo control without changing motor differential or speed scheduling.

**Architecture:** CPU0 computes far/near average center lines after every completed image frame, limits and filters the preview correction, and publishes one volatile byte for the steering ISR. Existing `final_mid_line` remains the source for motor differential and curve-speed logic.

**Tech Stack:** TASKING C for AURIX TC264, Seekfree MT9V03X image buffers, existing PID/servo modules, PowerShell source assertions.

---

### Task 1: Add lookahead configuration and public interface

**Files:**
- Modify: `code/config.h`
- Modify: `code/image.h`

- [ ] **Step 1: Run the failing source assertion**

Require all preview row, gain, limit, and filter macros plus `steering_mid_line` and `find_steering_mid_line`. Expected result before editing: FAIL because these names do not exist.

- [ ] **Step 2: Add exact configuration values**

Add far rows 55–70, near rows 100–115, gain `0.35f`, correction limit `10`, and old-value filter weight `0.5f` to the image parameter section of `config.h`.

- [ ] **Step 3: Add the image-module declarations**

Declare `extern volatile uint8 steering_mid_line;` and `uint8 find_steering_mid_line(void);` in `image.h`.

### Task 2: Implement far/near blended steering center

**Files:**
- Modify: `code/image.c`

- [ ] **Step 1: Implement the minimal algorithm**

Average `mid_line_list` over both configured inclusive regions. If either count is zero, reset the filter state to `final_mid_line` and return it. Otherwise multiply the difference by the configured gain, limit it to ±10 pixels, add it to `final_mid_line`, constrain the target to the image width, and apply the configured old/new weighting.

- [ ] **Step 2: Run the focused source assertion**

Require both averaging loops, the far-minus-near direction, `Limit_float` correction and image-bound limits, fallback to `final_mid_line`, and the low-pass expression. Expected result: PASS.

### Task 3: Connect the result to steering only

**Files:**
- Modify: `user/cpu0_main.c`
- Modify: `user/isr.c`
- Modify: `code/pid.c`

- [ ] **Step 1: Publish a new result after every processed frame**

Immediately after assigning `final_mid_line`, assign `steering_mid_line = find_steering_mid_line();`.

- [ ] **Step 2: Use the preview center in the steering ISR**

Call `servo(steering_mid_line)` while retaining the existing interrupt period.

- [ ] **Step 3: Match dynamic PID gain scheduling to the same center**

Calculate dynamic steering error from `steering_mid_line` instead of `final_mid_line`.

- [ ] **Step 4: Verify separation of responsibilities**

Require servo and dynamic PID to use `steering_mid_line`, while `Final_Motor_Control` and `car_start` still use `final_mid_line`.

### Task 4: Final verification

**Files:**
- Verify: `code/config.h`
- Verify: `code/image.h`
- Verify: `code/image.c`
- Verify: `code/pid.c`
- Verify: `user/cpu0_main.c`
- Verify: `user/isr.c`

- [ ] **Step 1: Run structural checks**

Check balanced braces, one definition for every preview parameter/global, no literal escaped newline tokens, and no temporary source files.

- [ ] **Step 2: Check build capability**

Locate TASKING tools from the project or installation. If unavailable, report static verification only; if available, run the existing Debug build and require exit code zero.
