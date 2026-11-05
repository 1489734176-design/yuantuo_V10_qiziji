# Mode Fixes Implementation Plan

> **For agentic workers:** Use executing-plans to implement the authorized corrections in this session.

**Goal:** Correct mode indication and K3 scene entry, and explain measured PWM duty.

**Architecture:** Keep the existing TM1650 foreground polling and tool mode state machine. Correct confirmed board mapping errors at the display/key boundary and trace PWM through the existing motor library.

**Tech Stack:** Bare-metal C, Keil Arm Compiler 6, existing ARM/Unicorn behavior tests.

**Spec:** User request on 2026-11-05: reverse speed LEDs from LED5..LED8 to LED8..LED5, restore green display and K3 scene mode; investigate 42/62/62% measured duty.

## Global Constraints
- Preserve all current, thermal, voltage, watchdog, stall and direction protections.
- Preserve existing unrelated and prior uncommitted changes.
- Isolate build outputs and do not flash hardware.

## Review Focus
- Correct key press, release and unknown-key handling.
- Single LED and correct color for each mode and gear.
- Sleep clears both color banks.
- Switching modes retains per-mode gear and persistence behavior.
- Distinguish requested PWM from modulation and actual measured output.

### Task 1: Correct display mapping
**Files:** USER/led.c; tests/seven_led/test_seven_led.c.
**Interface:** UO_Led_Handler writes TM1650 display shadow through TM1650_SetDigit.
- [ ] Derive expected segment masks and color banks from user observations; update existing behavior expectations.
- [ ] Run seven_led tests to show the mismatch.
- [ ] Correct the speed LED order and color bank mapping with Chinese comments.
- [ ] Run seven_led tests, including power-down checks.

### Task 2: Restore K3
**Files:** Determined by root-cause trace in USER/inc/tool_mode.h, USER/tool_mode.c or TM1650; regression in tests/tool_mode or tests/tm1650.
**Interface:** TM1650_ReadKey feeds tool_mode_key_handle; valid press enters TOOL_MODE_SCENE.
- [ ] Confirm physical key code, transport and foreground scheduling.
- [ ] Write and run a regression exercising the confirmed failure.
- [ ] Apply the smallest repair and run relevant tests plus persistence/debounce regression.

### Task 3: PWM diagnosis and delivery
**Files:** Existing PWM/control files read-only unless a code defect is proven; isolated build script and report in .planning/2026-11-05-mode-fixes.
- [ ] Trace tier target, ramp, active configuration and actual output mapping.
- [ ] Explain which measurements source can account for and record any unresolved bench condition.
- [ ] Run all existing test suites and independently review final changes.
- [ ] Perform a warning-reviewed Keil clean rebuild and deliver verified HEX with bench checklist.
