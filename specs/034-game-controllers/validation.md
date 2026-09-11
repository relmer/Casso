# Validation: Physical Game Controllers

**Feature**: `034-game-controllers` | **Quickstart**: [quickstart.md](quickstart.md) | **Tasks**: [tasks.md](tasks.md)

Results are recorded as they are produced. A scenario that could not run says so and why; an empty row means not yet run.

## Hardware check (T003)

| Question | Controller / receiver | Result |
|---|---|---|
| XInput packet changes per second | Xbox Series X\|S controller, USB, probe owning its windows, stick moved continuously | **125 changes/s**, steady across 12 s (the 8 ms USB HID interval). Poll period set to 8 ms. An earlier console run (no window of its own) read 0 changes/s over 20 s; not pursued, since Casso owns windows and the user's stick movement in that run was partial. |
| `XInputGetCapabilitiesEx` (ordinal 108) | Same controller, USB then Bluetooth | Present in `xinput1_4.dll` on this machine, returns success on the connected slot. **USB: 045E:02FF** (the generic XINPUT-compatible HID ID, not the controller's own 0B12). **Bluetooth: 045E:0B13**, revision 0511. One physical controller therefore yields two different model keys depending on the connection, which per-model profiles (FR-018, FR-018a) must account for. |
| DirectInput change events fire; polled device? | | |
| HID arrival/removal on wireless power on/off | Xbox Series X\|S controller (045E:0B13), Bluetooth LE | Pass: ARRIVAL at power-on and REMOVAL at power-off, received on a message-only window. Device path carries `IG_00`. |
| HID arrival/removal on USB plug/unplug | Same controller by USB cable | Pass: ARRIVAL at plug-in (15:07:15), REMOVAL at unplug (15:17:49), XInput slot 0 connected while plugged. The HID path reports 045E:**02FF** (generic "XINPUT compatible HID device") with `IG_00`, and the USB composite parent reports 045E:**0B12**, where Bluetooth reported 0B13: one physical controller has a different product ID per connection. |
| Unplugging a paired controller that is still powered on | Same controller | It reconnected itself over Bluetooth 2.3 s after the USB removal (ARRIVAL 045E:0B13 at 15:17:51). A removal is therefore not always a disconnect: the same controller can come straight back under a different unit identity, which the selection policy must survive (FR-032 adoption). |
| XInput while a second top-level window is active | Xbox Series X\|S controller, USB, probe owning two top-level windows | Pass: packet changes continued at the same rate with the second window foreground. With another application foreground for 6 s, changes also continued, which conflicts with the console-process run (0 changes over 20 s with the stick moved for part of it); unresolved. The measured ~64 changes/s is the probe's loop rate (a 1 ms `Sleep` at the default 15.6 ms timer resolution), not the controller's report rate; rerun without the sleep pending. |

| Xbox 360 controller | Wireless 360 pad connected with a Play & Charge cable; no wireless receiver on this machine | **Untestable here.** The cable carries power only: Windows enumerates 045E:028F ("Xbox 360 Wireless Controller via Play & Charge Kit") with no child devices and no HID interface, so no API sees a controller. No arrival was logged and XInput stayed empty. Leaves two questions open: whether a 360 wireless receiver raises HID notifications when a controller powers on (R4's fallback), and what ID a 360 pad reports (R6). |

**Chosen XInput poll period**: 8 ms (125 Hz measured).

## Phase scenario runs

| Task | Scenarios | Build | Result |
|---|---|---|---|
| T017 (mixer migration) | //e: arrows-to-joystick with X/Z, X held across an Alt press and release, left/right Alt and Shift as PB0-PB2, mouse paddle capture and release, focus loss while X held | Debug x64 `7bd2b4ee` + `4db29524`, 2026-09-11 | Pass (checked by the owner). ][+ check skipped. |
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
