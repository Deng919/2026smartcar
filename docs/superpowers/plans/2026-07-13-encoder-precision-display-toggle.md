# Encoder Precision and Display Toggle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Preserve fractional encoder speed and let KEY_2 disable costly TFT image refresh without stopping recognition or control.

**Architecture:** Keep encoder raw counts integral while storing the filtered speed as `float`. Store a volatile display flag in the image module, toggle it from the existing 5ms key-scanning interrupt, and let CPU0 perform all TFT operations including one-time clearing.

**Tech Stack:** TASKING C for AURIX TC264, Seekfree key/TFT/camera drivers, PowerShell source assertions.

---

### Task 1: Preserve fractional encoder speed

**Files:**
- Modify: `code/motor.h`
- Modify: `code/motor.c`

- [ ] **Step 1: Run a failing source assertion**

Check that `encoder_speed` is currently not `float` and the filter does not use both `0.2f` and `0.8f`. The assertion must fail before production edits.

- [ ] **Step 2: Implement the minimal type change**

Change only the filtered speed field to `float`, use float filter constants, and pass the field directly to `PID_Increase`.

- [ ] **Step 3: Run the source assertion again**

Require one `float encoder_speed` field, both float constants, and no redundant cast of `encoder_speed`.

### Task 2: Toggle TFT display with KEY_2

**Files:**
- Modify: `code/image.h`
- Modify: `code/image.c`
- Modify: `user/isr.c`
- Modify: `user/cpu0_main.c`

- [ ] **Step 1: Run a failing source assertion**

Require a volatile display flag, a KEY_2 short-press toggle, a conditional display block, and a one-time `tft180_clear()` path. Confirm it fails before editing.

- [ ] **Step 2: Add display state and key handling**

Define `volatile uint8 image_display_enable = 1` in the image module and declare it in the header. Toggle it after `key_scanner()` when KEY_2 reports `KEY_SHORT_PRESS`.

- [ ] **Step 3: Gate TFT work in CPU0**

Always process camera frames. When display is enabled, draw and refresh normally. When disabled, clear once after a state transition and skip subsequent TFT work.

- [ ] **Step 4: Run the source assertion again**

Require all display-state patterns and verify that image processing calls remain outside the display condition.

### Task 3: Final static verification

**Files:**
- Verify: `code/motor.h`
- Verify: `code/motor.c`
- Verify: `code/image.h`
- Verify: `code/image.c`
- Verify: `user/isr.c`
- Verify: `user/cpu0_main.c`

- [ ] **Step 1: Check source structure**

Check balanced braces, absence of literal escaped newline tokens, and absence of temporary files.

- [ ] **Step 2: Check compiler availability**

Look for `cctc`, `astc`, and `amk`. If unavailable, report that ADS compilation remains required and do not claim a successful firmware build.
