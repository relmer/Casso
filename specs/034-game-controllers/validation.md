# Validation: Physical Game Controllers

**Feature**: `034-game-controllers` | **Quickstart**: [quickstart.md](quickstart.md) | **Tasks**: [tasks.md](tasks.md)

Results are recorded as they are produced. A scenario that could not run says so and why; an empty row means not yet run.

## Hardware check (T003)

| Question | Controller / receiver | Result |
|---|---|---|
| XInput packet changes per second | | |
| DirectInput change events fire; polled device? | | |
| HID arrival/removal on wireless power on/off | | |
| XInput while a second top-level window is active | | |

**Chosen XInput poll period**:

## Phase scenario runs

| Task | Scenarios | Build | Result |
|---|---|---|---|
| T037 (US1) | 2, 3, 6, 10 | | |
| T053 (US2) | 1, 8 (selection), 11 (controller rows) | | |
| T060 (US3) | 4, 5, 11 (plug in while menu open) | | |
| T080 (US5) | 7, 9, 10a (hand-built rate binding), 10b | | |
| T089 (US6) | 8, 10a (Paddles template), 11 (profile rows) | | |

## Final walk and measurements (T093)

| Scenario | Result |
|---|---|

| Measurement | Method | Result |
|---|---|---|
| SC-002 sample read to machine write | | |
| SC-005 removal to release write | | |
| SC-005 arrival to first controller write | | |
| SC-007 idle CPU, controllers attached, none selected (branch vs master) | | |
