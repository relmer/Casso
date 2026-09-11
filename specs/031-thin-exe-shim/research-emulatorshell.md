# EmulatorShell: survey and candidate seams

**Purpose**: decide where to cut `EmulatorShell` so that User Story 6 holds --
build, run, step, reset, power-cycle and observe a machine from a test, with
nothing faked but the passage of time -- before any of T071--T080 is
implemented.

**Method**: every `m_` member `EmulatorShell.h` declares was assigned one role
by name; every function body in `EmulatorShell.cpp`, `MachineManager.cpp` and
`WindowCommandManager.cpp` was then classified by which roles its member
references reach. The script is a scratchpad artifact, not part of the tree.
Counts are from the branch at the time of writing (`EmulatorShell.h` 2,059
lines, `EmulatorShell.cpp` 15,968 lines).

Roles:

| Role | Meaning | Examples |
|---|---|---|
| **M** machine | the emulated hardware and its lifecycle | `m_memoryBus m_cpu m_mmu m_refs m_ownedDevices m_prng m_videoTiming m_diskStore m_config`, the six soft-switch mirror bools |
| **T** threading | CPU-thread hand-off and frame pacing | `m_cpuManager m_framebufferMutex m_cpuFramebuffer m_lastRender*Sig m_cyclesPerFrame m_colorMode` |
| **A** audio | WASAPI and the mixers | `m_wasapiAudio m_driveAudioMixer m_mockingboardAudioMixer m_drive*Volume` |
| **W** window | Win32, Dxui, D3D, chrome, scene, input state | `m_hwnd m_host m_viewport m_d3dRenderer m_deskScene m_chromeDock`, the bands, tooltips, gesture and pointer state |
| **P** prefs | user config, prefs, theme, file system | `m_userConfigStore m_globalPrefs m_themeManager m_uiFs` |
| **D** debug/observe | debug panels, printer, capture, uptime | `m_disk2DebugPanel m_inputDebugPanel m_printerWorker m_printerPanel m_pendingCapture m_uptimeAnchor` |

## 1. Inventory

### 1.1 Members (191)

| Role | Count |
|---|---|
| W | 113 |
| M | 32 |
| T | 17 |
| D | 14 |
| A | 9 |
| P | 6 |

41 of the W members are objects with non-trivial constructors (`D3DRenderer`,
`MainMenu`, `CommandToolbar`, `DeskScene`, `DxuiHwndSource`, the Dxui banners,
tooltips, surfaces and dock, `DriveWidgetController`, `UiShell`, ...). They
are constructed by `EmulatorShell::EmulatorShell()` whether or not a window
will ever exist. That, and not the file's location, is what
`EmulatorShellResetTests.cpp:22` means by "cannot be built inside a unit test".

### 1.2 Functions, by the roles each reaches

`EmulatorShell`: 232 functions, 15,329 lines of bodies.

| Reaches | Functions | Lines | Reading |
|---|---|---|---|
| W only | 85 | 4,822 | window and chrome; no machine state |
| M only | 27 | 1,095 | machine; already headless-able |
| M + W (any mix) | 49 | 5,780 | **the seam-cutting set** |
| M + T/A/P/D, no W | 16 | 999 | lifecycle, dispatch, audio prefs; headless-able |
| none of the above | 23 | 909 | statics and helpers |
| the rest (P, T, PW, TW, ...) | 32 | 1,724 | prefs and pacing plumbing |

`MachineManager`: 16 functions, 2,134 lines. Eleven are M-only (910 lines).
The two that reach outside the machine are `CreateMemoryDevices` (650 lines;
audio mixers, drive audio sources, printer worker, `m_wasapiAudio` for the
Mockingboard sample rate -- its lines 488--594) and `SwitchMachine` (449 lines;
user config store, `m_hwnd` for two `PostMessageW` calls, debug panel cycle
counters, printer worker stop/save, mixers, `m_cyclesPerFrame`, window title,
global prefs, disk manager).

`WindowCommandManager`: 29 functions, 2,265 lines; reaches 18 shell members
(`m_hwnd` 25x, `m_chromeTheme` 15x, `m_globalPrefs` 14x, `m_refs` 12x,
`m_printerWorker` 11x, `m_config` 10x ...).

### 1.3 The seam-cutting set, largest first

| Lines | Roles | Function |
|---|---|---|
| 593 | AMPW | `CreateEmulatorWindow` |
| 497 | DMTW | `TryPresentUiFrame` |
| 289 | MW | `OnMouseMove` |
| 251 | MW | `OnLButtonDown` |
| 227 | MW | `OnLButtonUp` |
| 188 | MPTW | `OnSize` |
| 185 | MW | `OnViewportKey` |
| 168 | MTW | `RunMessageLoop` |
| 164 | MTW | `Initialize` |
| 158 | DMTW | `~EmulatorShell` |
| 157 | MTW | `ApplyPersistedChromePrefs` |
| 154 | MTW | `RenderFramebuffer` |
| 142 | ADMPW | `UpdatePrinterPreview` |
| 141 | AMTW | `ExecuteCpuSlices` |
| 128 | MW | `SyncSceneDriveLabels` |
| 112 | MW | `RunSalvageFlow` |
| 111 | MW | `ReflowChromeForMachineChange` |
| 110 | MW | `OnChar` |
| 102 | MW | `FinishUiShellLayout` |

Two groups dominate: the input path (`OnMouseMove`, `OnLButtonDown`,
`OnLButtonUp`, `OnViewportKey`, `OnChar`, `OnKeyDown`, the joystick and
pointer-mapping helpers: about 1,300 lines that read window state and write
keyboard, game port and mouse state through `m_refs`) and the present path
(`TryPresentUiFrame`, `RenderFramebuffer`, `ExecuteCpuSlices`: about 800 lines
that read `m_refs` 30+ times to pick the video mode and drain the speaker).

### 1.4 What the machine lifecycle touches today

| Entry point | Where | Roles | Lines |
|---|---|---|---|
| `SoftReset` | `MachineManager` | M | 30 |
| `PowerCycle` | `MachineManager` | M | 50 |
| `StepInstructionWhilePaused` | `EmulatorShell` | MT | 40 |
| `DispatchCpuCommand` (21 `IDM_` ids) | `EmulatorShell` | AM | 238 |
| `RunCpuThreadFrame` | `EmulatorShell` | MT | 52 |
| `ExecuteCpuSlices` | `EmulatorShell` | AMTW | 141 |
| `BuildMachineDevices` | `EmulatorShell` | MT | 61 (six calls into `MachineManager`) |

None of these reads a window. They are unreachable from a test only because
each is a method on a class that cannot be constructed without one.

`ExecuteCpuSlices`'s single W reach is `m_clipboardManager->DrainPasteBuffer`;
its A reach is `m_wasapiAudio.SubmitFrame (speaker toggles, drive mixer,
Mockingboard mixer)` once per slice plus `GetSampleRate`.

Wall-clock time enters exactly twice: `CpuManager::ThreadProc`
(`QueryPerformanceCounter` and a waitable timer, `CpuManager.cpp:421--500`),
and `ExecuteCpuSlices` reading `m_cpuManager.GetSpeedMode()`. The machine
itself is cycle-driven.

## 2. Findings

**F1. Location is done; constructibility is not.** Every line of the shell is
already in `CassoEmuCore`. What User Story 6 still lacks is a machine that a
test can construct, which the 113 eagerly-built W members prevent. Any seam
that leaves those in the same constructor as the machine leaves the story
unmet.

**F2. `MachineManager` is a builder with no state of its own.** Its only
member is `EmulatorShell & m_shell`; it reaches 43 shell members, is the sole
writer of `m_refs` (85 touches) and of the soft-switch mirror
(`SelectVideoMode`, `MachineManager.cpp:2137--2151`). Its M-only functions are
already the machine's construction and lifecycle; the back-reference is the
only thing binding them to the window.

**F3. The friend list is three-quarters vestigial.** Of six `friend class`
declarations, `SettingsSheet`, `SettingsApplyController`,
`SettingsDisplayCrtBridge` and `SettingsMachineCatalog` touch zero shell
members (they use public accessors). Only `MachineManager` (43 members) and
`WindowCommandManager` (18) use the access.

**F4. The test tree keeps a second, hand-maintained machine.**
`UnitTest/EmuTests/HeadlessHost.cpp` (675 lines) builds a //e, //c, ][, ][+ and
Enhanced //e by repeating `CreateMemoryDevices`'s sibling wiring in the same
order (`BuildApple2e` lines 20--91 against `MachineManager.cpp:195--224`), and
`EmulatorCore::PowerCycle` repeats `MachineManager::PowerCycle` -- and the two
already disagree: production flushes the disk store first and resets the mouse
before video timing; the test double does neither in that order. 25 test files
reach `EmulatorCore`'s fields directly (`bus` 200 lines across 21 files, `cpu`
93/16, `diskController` 68/15, `mmu` 63/9, `softSwitches` 50/5, `keyboard`
47/5, `languageCard` 46/4, `diskStore` 32/13). `GuestSession` (mount, boot to
prompt, type and collect rows, bytes at) is the run-primitive layer on top of
it and is what the new tests should keep using.

**F5. Observation sinks already exist on the devices.** `Disk2Controller`
carries `SetAudioSink (IDriveAudioSink *)` and `SetEventSink (IDisk2EventSink
*)`; `AppleKeyboard`, `AppleGamePort` and `Apple2eSoftSwitchBank` carry
`SetInputEventSink`. The debug panels implement those interfaces. The one
thing in the way of T053 is that `AttachDebugSinksIfOpen` reads
`m_disk2DebugPanel` and `m_inputDebugPanel` rather than a sink the machine was
handed.

**F6. `IHostShell` is a dead contract.** `Core/IHostShell.h` (from spec 004)
has no production implementer or caller, and `PresentFrame` takes a
`Framebuffer` type that does not exist in the tree. `EmulatorCore` carries a
`MockHostShell` for it. `IAudioSink::PushSamples (const float *, size_t)` from
the same header is also not the production audio call, which is
`WasapiAudio::SubmitFrame (toggle timestamps, mixers...)`.

**F7. The existing seams in the tree are the pattern to follow.** `DiskManager`
takes twelve references in its constructor and has no shell coupling;
`CpuManager` takes four callbacks and owns the thread; `ClipboardManager`,
`ScreenshotCapture`, `WindowManager` are the same. `MachineManager` and
`WindowCommandManager` are the two that were done the other way.

## 3. Candidate seams

### Seam A -- MachineHost: lift the machine out of the shell

**What moves.** The 32 M members (`MachineRefs` with them) into a new
`CassoEmuCore/Shell/MachineHost` class. `MachineManager` keeps its name and its
M-only functions but builds into a `MachineHost &` and takes its remaining
needs by reference the way `DiskManager` does: the two mixers, the drive audio
sources, the printer worker, the user config store and file system, and two
sink interfaces (`IDisk2EventSink *`, `IInputEventSink *`) in place of the
panel pointers. Its two `PostMessageW (m_hwnd, ...)` calls become a
post-command callback. `SwitchMachine` splits along the line already visible
in its body: the machine half (parse, build, reset, remount) in
`MachineHost`/`MachineManager`, the shell half (debug panel cycle counters,
printer stop/save, title, global prefs, toolbar) stays in
`EmulatorShell::SwitchMachine`.

Lifecycle lands on `MachineHost`: `SoftReset`, `PowerCycle`, `StepOne`, and
`RunCycles (budget)` -- the cycle loop from `ExecuteCpuSlices` with the audio
submit and paste drain left in the shell around it. Time stays where it is
(`CpuManager` and the speed mode); the machine takes a cycle budget.

`HeadlessHost::Build*` is replaced by `MachineHost` built from
`MachineDefinitions::Find (id)` plus a `MachineConfig`, with ROM bytes coming
through the `IFileSystem` the tests already root at `UnitTest/Fixtures`.
`EmulatorCore` becomes an adapter whose fields are accessors into the host, so
the 25 test files compile unchanged in the first cut and migrate when touched.

**Cost.** 240 lines in `EmulatorShell.cpp` reference an M member (109 are
`m_refs`) and 12 in `WindowCommandManager.cpp`; each becomes a call through
`m_machine`. `MachineManager`'s constructor grows from one parameter to about
nine. `SwitchMachine` is split (449 lines). `HeadlessHost.cpp` shrinks from
675 lines to an adapter. `AttachDebugSinksIfOpen` inverts to hand sinks in.
Roughly 1,500 lines touched, almost all mechanical; the two judgment cuts are
`SwitchMachine` and the `ExecuteCpuSlices` loop.

**Makes testable.** T075--T078 exactly as written, against the production
builder and lifecycle rather than a copy; T053 (switch re-attaches the sinks
the host holds); T133 (the //e MMU and //c ROM bank wiring gets a per-model
hook on `IMachine` and `MachineManager` calls it, now that there is a machine
object to hook); the F4 divergence closes because there is one `PowerCycle`.

**Leaves.** The 113 W members and the 49-function seam-cutting set stay in
`EmulatorShell`, now reading machine state through one member. T073 (window
into `Shell/Window`) remains a later, mechanical slice, and after A it is a
relocation rather than a split.

### Seam B -- window first: lift the window and pump out of the shell

**What moves.** The 85 W-only functions (4,822 lines) and 113 W members into
`CassoEmuCore/Shell/Window/ShellWindow`, leaving `EmulatorShell` holding
M/T/A/P/D. This is T071 and T073 read literally.

**Cost.** The 49 M+W functions (5,780 lines) each need a window half and a
machine half, or the new window class needs a back-reference to the shell --
which recreates the `MachineManager` pattern F2 identifies as the problem.
The input path alone is about 1,300 lines of interleaved window and machine
reads. After the move the shell is constructible in a test only if every
remaining member is inert without a device: `WasapiAudio` and `PrinterWorker`
appear to be (both initialize later), but that is unverified for all 78. This
is the largest diff in the spec and the one with the least test coverage
behind it while it is in flight.

**Makes testable.** The same things as A, eventually, plus the window's own
message handling -- but only once the 49 splits are done, and none of it
before.

**Leaves.** Nothing by location; everything by risk.

### Seam C -- lifecycle statics: share the reset/step/run code, move no state

**What moves.** `SoftReset`, `PowerCycle`, `StepOne` and the cycle loop become
class statics taking their devices as parameters (`MemoryBus &`, `Apple2eMmu
*`, `InterruptController &`, `AppleMouse *`, `VideoTiming *`, `EmuCpu &`,
`Prng &`), the way `FramePacing`, `AppleKeyMapping` and `DriveRowLayout` were
done in earlier slices. `MachineManager` and `EmulatorCore` both call them.

**Cost.** About 200 lines. No member moves, no constructor changes, no test
migration.

**Makes testable.** T076--T078 against the production lifecycle code, and the
F4 `PowerCycle` divergence closes.

**Leaves.** T075 unmet as specified (the machine under test is still the
test tree's copy, not production construction); T053 and T133 untouched;
`HeadlessHost` stays a second machine; issue #85's "core machine façade
owning device construction" is not delivered.

## 4. Recommendation

**Seam A, staged, then reassess B as file organization.**

1. **A1** -- `MachineHost` owning the 32 M members and `MachineRefs`;
   `MachineManager` builds into it and takes the rest by reference; the shell
   holds `MachineHost m_machine` and the 240 references are re-pointed.
   `SwitchMachine` splits. Gate: suite green, launch unchanged.
2. **A2** -- lifecycle on `MachineHost` (`SoftReset`, `PowerCycle`, `StepOne`,
   `RunCycles`), `ExecuteCpuSlices` becomes shell pacing around `RunCycles`;
   sinks handed in rather than read from panels. T076--T078 and T053 written
   here.
3. **A3** -- `MachineHost::Build` from a definition, config and file system;
   `HeadlessHost` becomes an adapter; T075 written here; T133 lands as the
   per-model wiring hook.
4. Then decide whether B is worth doing as a move of W-only functions into
   `Shell/Window/*.cpp`, which after A is a relocation with no split.

What A does not answer, and the owner decides:

- **Retire `EmulatorCore` now or later.** The adapter keeps 25 files compiling;
  retiring it in the same slice touches about 700 field-reach lines.
- **`IHostShell` / `MockHostShell`.** Dead in production and wrong about the
  audio call. Delete with A3, or leave.
- **Where `MachineHost` lives.** `Shell/MachineHost` per T072's wording, since
  it is the emulator assembling a machine rather than any one machine; the
  per-model wiring hooks go under `Machines/Apple2/<Model>/`.
