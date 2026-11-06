# Scene seat stop tests

## Goal

Extend the real Cortex-M0 C harness to verify forward scene gears 2 and 3 seat-stop behavior without changing production or protection parameters.

### Phase 1: Existing suite baseline
**Status:** complete

Run the unmodified cases and record existing protection-parameter failures separately.

### Phase 2: Behavioral red
**Status:** complete

Add real-function extraction and boundary/reset/latch tests. Temporarily use a generated weak no-op adapter only while the new function does not exist. Verify failures arise from absent behavior.

### Phase 3: Green verification
**Status:** complete

Remove the temporary weak adapter once production exists, run new cases and the full suite, and report any unchanged existing failures.

## Next Step

Return the corrected final suite result, 46 cases / 66724 checks, to the parent.

### Phase 4: Correct main-loop integration order
**Status:** complete

Review identified that cases 45/46 updated held faults before state control, unlike main.c. Verify a red assertion for the true same-frame state, then execute the real main call order and motor-enable gate. Preload gear 3 seat count so the raw-fault frame competes with its 100th sample.

## Errors Encountered

- Initial batched search named non-existent USER/tool_state.c; actual user_state_control is in USER/user_control.c.
- Original suite baseline failed case 34 at original test line 940 after 65776 checks: hard-coded 28 A/101-call boundary no longer matches active 25 A/120 ms protection.
- Red case 36 fails the mode-enable assertion; red case 37 fails the expected auto-stop latch after the 100th high-current sample. Both compile and link successfully.
- Integration review: test cases 45/46 used current sample -> fault hold -> seat stop -> state control and omitted main's final motor gate. Actual main uses sample -> seat stop -> state -> hold -> motor gate; peak fault first clears motor permission while the state still reads the previous held-fault snapshot.
- Confirmed the old test ordering fails the corrected same-frame state assertion in case 45 at test line 1277 after 53 checks; saved main-order-red.log before fixing the adapter.
