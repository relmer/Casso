# How 035 handles thread ownership for the disk subsystem (worktree `041-disk-integrity` at 811a6f727)

Paths are relative to `C:\Users\relmer\source\repos\relmer\Casso-worktrees\041-disk-integrity`. Where I say something is new in 035, I checked against the merge base with master, 13095e55.

This record describes 035 at `811a6f727`. 035's fixes for the defects 041 reported on 2026-10-08 (a, b, d, e and f; `origin/035-debugger` at `d3c15b55c`) change sections 1 to 4 where noted below, and tasks.md "What 035 brings" gives their commits.

## 1. CpuManager pause and resume

**There is no handshake.** (Reported to 035 on 2026-10-08 as 041 audit defect b. 035 fixed it in `3698a1a0a`, `f793616db` and `e8290e93f`, merged at `21306089a`: `TogglePaused` and `SetPaused` store and wake under the pause mutex, `MachineHost::RunCycles` stops on the next instruction boundary, and the CPU thread parks, with `CpuManager::IsParked` and `TryWaitUntilParked` as the acknowledgement and the pause wait moved into `WaitWhilePaused`. A parked, paused machine still runs posted commands. 041's deploy hold for the update installer, `HoldForDeploy`, is built on that code after tasks.md T008, with names and an acknowledgement of its own, and holds the posted commands too.)
- `SetPaused` stores `m_paused` and calls `notify_all` under `m_pauseMutex` (`CassoEmuCore/Shell/CpuManager.cpp:215-224`).
- `TogglePaused` reads the flag and then stores it, which is not atomic (`CpuManager.cpp:243-256`).
- Neither one waits, and the CPU thread sends no acknowledgement. Nothing in the code records that the thread is parked. CassoEmuCore has no promise or future.
- `IsPaused` (`CpuManager.cpp:197-200`) reports what was asked for, not where the thread is.

**A paused CPU thread is not guaranteed to be parked when the UI thread moves on.**
- The flag is read only at the top of the loop (`ThreadProc`, `CpuManager.cpp:555-593`).
- A pass already inside `m_onFrame` runs its whole frame budget:
  - The slice loop has no pause check: `RunCpuThreadFrame` → `ExecuteCpuSlices`, `EmulatorShellCpuThread.cpp:986-1030` and `1251-1342`.
  - The pacing wait follows: `CpuManager.cpp:643-648`.
- So after the UI's `TogglePaused` returns (`WindowCommandManager.cpp:723-733`), the CPU thread can run guest code and tick devices for up to the rest of a frame. That is 17,030 cycles at 1x, twice that at Double, and unpaced at Maximum.

**The pause is synchronous only when the CPU thread pauses itself.** The cases:
- `CpuManagerRunDriver::Finish` (`CpuManagerRunDriver.cpp:239-274`)
- `DebuggerController::RequestPause` (`DebuggerController.cpp:182-192`)
- `StopForHeldInputRead` (`EmulatorShellReverse.cpp:527-546`)
- `RunDebugHeatMapAccess` (`EmulatorShellDebugger.cpp:1566-1573`)

After `Finish`, the slice loop breaks (`EmulatorShellCpuThread.cpp:1306-1310`), the frame renders and publishes (`1020-1029`), and then the loop parks.

**A debugger pause is landed by the CPU thread.**
1. `Pause` records `m_pauseRequested` and the tick fraction (`CpuManagerRunDriver.cpp:105-115`).
2. The next pass is shortened (`EmulatorShellCpuThread.cpp:1232-1241`).
3. `OnPausePointReached` calls `Finish` (`1344-1349`; `CpuManagerRunDriver.cpp:193-201`).
4. If the machine is already paused, the run ends where it stands (`CpuManagerRunDriver.cpp:107-111`).

**Service wake-ups.**
- The wait predicate is `!paused || !running || HasPendingCommands()` (`CpuManager.cpp:559-564`).
- Production always sets a service function (`EmulatorWindow.cpp:1343`). The wait is therefore `wait_for(kServiceIntervalMs = 20)` (`CpuManager.h:80-86`; `CpuManager.cpp:566-576`).
- A paused thread comes round at least every 20 ms, and at once on every `PostCommand` (`CpuManager.cpp:144-170`).
- Each pass, paused or running, runs `DrainCommandQueue` (`583`, body `329-351`), then `m_onService` (`585-588`), then `continue` if still paused (`590-593`).

**What runs during those wake-ups**, all on the CPU thread:
- **Service:**
  - `ServiceDebugger`: `m_debugger->Pump()` (pipe requests become session commands, `DebuggerController.cpp:111-114`), then `PublishDebuggerView`, then `ReverseHost::SyncInputGate` (`EmulatorShellCpuThread.cpp:938-951`; `ReverseHost.cpp:129-150`).
  - `ServiceHistoryThumbnails`: publishes the playhead and caption, and hands one packed keyframe to the thumbnail worker (`EmulatorShellReverse.cpp:1003-1060`).
- **Drained commands** (`CpuCommandDispatcher.cpp:45-198`):
  - Mount, eject, write-protect, resolve-change.
  - Reset, which runs `RemountDisks` first and then `SoftReset`, and power cycle, which runs `RemountDisks` after (`CpuCommandDispatcher.cpp:62-76`). A reset's remount that swaps a disk captured a boundary keyframe before `SoftReset`, past the `Reset` journal record (reported to 035 on 2026-10-08 as 041 audit defect d; 035 fixed it in `5ee85434f`, so a disk change while live now becomes a boundary keyframe before the next instruction or reverse command).
  - `IDM_MACHINE_STEP`.
  - `IDM_DEBUG_COMMAND`, `IDM_DEBUG_ACTION`, `IDM_DEBUG_VIEW`.
  - `IDM_DEBUG_REVERSE`.
  - Load and save state, and machine switch.
- **Guest code on a "paused" machine:**
  - `IDM_MACHINE_STEP`.
  - `IDM_DEBUG_REVERSE`: `RunReverseCommand` → `Replayer::RunTo` → `Replayer::Step` → `MachineHost::StepOne` (`EmulatorShellReverse.cpp:203-286`; `Replayer.cpp:127-206`, `269-315`). This can be millions of instructions, each calling `Disk2Controller::Tick`. Keyframe loads also reseat and reload the disks.
  - Any debugger command that starts a run calls `SetPaused(false)` from the CPU thread (`CpuManagerRunDriver.cpp:76`), so the same pass goes on to run a frame.

**What does keep the UI out.** Only two things, and neither excludes ordinary running, steps or replays:
- The lifetime lock (`MachineHost.h:194-199`). It is taken exclusively only by a machine switch; the UI frame takes it shared with `try_to_lock` (`EmulatorShellPresent.cpp:564-579`).
- `HostInputGate` (see section 5).

## 2. How a debugger step runs now

`StepInstructionWhilePaused` was removed in commit c8680f325. On master it ran `StepOne` plus a whole `RunOneFrame` on the UI thread under the lifetime lock.

**Main-window Step.**
- The UI checks `IsPaused`, then posts `IDM_MACHINE_STEP` (`WindowCommandManager.cpp:751-762`).
- On the CPU thread it reaches `StepInstruction` (`EmulatorShellCpuThread.cpp:379-405`).
- **With a debug session** (there is one once the debugger has been opened): it executes the session's `StepInto` (`385-393`).
- **Without a session:** `SampleHostInputs`, one `StepOne`, then `RenderFramebuffer` and `PublishFramebuffer` (`395-404`).

**Session step** (window, client, or the path above):
1. Run setup in `DebugSession.cpp:2443-2534` calls `CpuManagerRunDriver::Start` (`CpuManagerRunDriver.cpp:44-80`). `Start` records the run, has `RunStopHook::Begin` install an every-instruction filter (`RunStopHook.cpp:74-95`), installs the hook, calls `SetPaused(false)`, and returns without running anything (`CpuManagerRunDriver.h:21-27`).
2. `Start` is itself called on the CPU thread, so in the same pass the frame runs the step: `ReverseHost::OnFrame` (`EmulatorShellCpuThread.cpp:997-1001`) → `ExecuteCpuSlices` → `RunWatchedSlice` → `MachineHost::RunCycles` → `StepOne` / `StepOneAsked` (`MachineHost.cpp:341-438`, `874-907`).
3. The hook stops before the next instruction (`RunStopHook.cpp:142-255`, `375-417`), and `RunCycles` returns short (`MachineHost.cpp:891-896`).
4. `OnSliceExecuted` calls `Finish`, which calls `SetPaused(true)` (`EmulatorShellCpuThread.cpp:1306-1310`).

`StepOne` on the live machine runs only on the CPU thread. `Replayer::Step` (`Replayer.cpp:302`) is the other caller, also on the CPU thread. Off-thread `StepOne` happens only on scratch machines that have their own `MachineHost`:
- `ScratchHeatReplayer` (`ScratchHeatReplayer.h:22-49`, `104-113`).
- `ScratchMachineRenderer`, which has no disks (`ScratchMachineRenderer.h:15-29`).
- On 035's tip `d3c15b55c`, `ScratchCallReplayer` as well, which rebuilds the debugger's call record on a pool thread of its own. It and `ScratchHeatReplayer` now each build their scratch machine, with its own disk store, through `ScratchReplayMachine` (`ScratchReplayMachine.cpp:63-112`).

**How many instructions.**
- Step into, or `T` with no count: exactly one (`DebugSession.cpp:2484`; `RunStopHook.cpp:394-396`), or the interrupt entry taken in its place (`RunStopHook.cpp:219-224`). Step into does not run through interrupts (`514-517`).
- More than one when:
  - The step has a count (`T n`).
  - It is a Trace with no count, which runs until a stop or the budget (`UINT32_MAX`, `DebugSession.cpp:2483-2484`).
  - It enters a step-filtered call (`RunStopHook.cpp:466-477`).
  - It is a step over or step out.
  - It is a chained source step (`DebugSession.cpp:2494-2504`).
- The frame prologue also runs keyboard auto-repeat, the paste drain and `SampleHostInputs` (`EmulatorShellCpuThread.cpp:1204-1211`, `1260-1271`).

**`Disk2Controller::Tick` callbacks can fire.**
- Every executed instruction ends in `FinishStep`, which calls `diskController->Tick(cycles)` (`MachineHost.cpp:836-862`).
- `Tick` can fire the motor-off flush callback when spindown expires (`Disk2Controller.cpp:537-571`).
- It can fire the idle callback when 17,030 quiet cycles have accumulated (`Disk2Controller.h:144`; `Disk2Controller.cpp:619-650`).
- Both callbacks run `FlushAllUnlessHeld` and `ApplyPendingReload` (`MachineBuilder.cpp:1513-1546`). So a single step can flush, or swap in a reloaded disk, now on the CPU thread.
- Behind live, the step goes through `ReplayHere` (`ReverseController.cpp:1812-1828`, `1864-1929`). That sets the store to replaying (`1885`, `1913`), which turns both callbacks into no-ops (`DiskImageStore.cpp:1899`, `2747`).

## 3. The flush hold, and how guest disk writes are recorded

**The flag.** `DiskImageStore::m_isFlushHeld` is a plain `bool`, not atomic (`DiskImageStore.h:143-144`, `711`). It is written and read only on the CPU thread.

**Set to true.** In `ReverseController::LeaveLive` (`ReverseController.cpp:1976-2013`, at `2005`). It is reached from `ReverseContinue` (`588`), `Seek` (`811`), `GetStep` (`996`) and `ReplayStackLevels` (`1201`).

**Cleared.**
- `Start` (`98`).
- `Stop` (`142`, only while recording).
- `BecomeLive` (`2070`). `BecomeLive` is called from `OnMachineChanged` (`259`), the Maximum-speed pause (`191`, `205`), reaching the live end (`857`, `1880`, `1924`), `HandleDivergence` (`956`) and `Start` (`109`).

**Thread.** All of the above run on the CPU thread:
- `ReverseController.h:92-95`.
- `RunReverseCommand` (`EmulatorShellReverse.cpp:196-224`).
- `OnCpuThreadStart` and `OnCpuThreadStop` (`EmulatorShellCpuThread.cpp:212-219`, `319-326`).
- The machine switch (`MachineManager.cpp:399-401`).

The comment at `ReverseController.h:88-90` says the flushes are held "while recording". That is out of date: the code holds them only while the machine is behind live. (035 corrected it in `c414dd193`; `fe0427676` then gave a reset as the one exception, which 041's `e32b2b68c` ends, so tasks.md T008 puts the reset back in that comment's list.)

**Two companion flags.**
- **`m_isReplaying`** (`DiskImageStore.h:150-154`).
  - Set by `Replayer::RunTo` for the length of a replay (`Replayer.cpp:162`, `195`).
  - Also set by `ReplayHere` (`ReverseController.cpp:1885`, `1913`), and cleared only by `BecomeLive` (`2071`), and since 035's `c913c9247` (defect a) by `Stop` as well.
  - So after a forward step or run from the past, it stays set while the machine sits behind live.
- **Retention.**
  - Turned on at `Start` (`99`) and off at `Stop` (`143`).
  - `RetireBay` keeps a disk that left its bay, stamped with the position it left at (`DiskImageStore.cpp:2197-2221`).
  - `PruneRetainedMedia` runs after each keyframe (`ReverseController.cpp:1746`; `DiskImageStore.cpp:2179-2182`).

**There is no separate disk-write journal.**
- Guest writes stay in the `DiskImage` track buffers. Every keyframe captures them through `DiskImage::SaveState` with a sharing writer (`DiskImage.cpp:1077-1153`), using copy-on-write track buffers from `GetSharedTrack` (`1192-1224`).
- Dirty flags are deliberately left out of the saved state (`1086-1089`).
- `LoadState` swaps every track vector in and marks tracks that differ as dirty (`1241-1313`).
- Media changes are recorded in two places:
  - As `InputJournal` records (`CpuCommandDispatcher.cpp:245-269`, written at `EmulatorShellCpuThread.cpp:350-355`).
  - As a boundary keyframe, through the store's media-change listener: `DiskImageStore.cpp:640`, `701`, `2328` → `MachineHost.cpp:47`, `60-66` → `ReverseController.cpp:283-296` → `OnMachineChanged` (`227-266`).
- A replay never redoes a media change from the file; it loads the boundary keyframe instead (`Replayer.cpp:452-475`, `520-539`).
- `DiskTrackSnapshot` has no production caller. Only `UnitTest/Devices/Disk2StateTests.cpp` uses it.

**What each operation does while the hold is set** (current behavior):

| Operation | While held |
|---|---|
| Automatic flushes: spindown (`MachineBuilder.cpp:1519-1521`) and `MachineHost::PowerCycle` (`MachineHost.cpp:978-984`) | Skipped through `FlushAllUnlessHeld` (`DiskImageStore.cpp:1893-1906`) |
| Reset, at `811a6f727`: `MachineHost::SoftReset` (`MachineHost.cpp:919-952`) never called the store, and `DiskImageStore::SoftReset` (`DiskImageStore.cpp:2345-2352`) had no production caller. `MemoryBus::SoftResetAll` reached `Disk2Controller::SoftReset`, which called `DiskImage::Flush` on every loaded drive (`Disk2Controller.cpp:931-948`) | Still wrote, ignoring both the hold and `m_isReplaying`, and so did a replay of a recorded reset (`Replayer.cpp:493-496`). Corrected after the first reading of this table. Fixed on this branch in `e32b2b68c` (FR-017): `MachineHost::SoftReset` now calls the store's `SoftReset` (`FlushAllUnlessHeld`) first and the controller's reset writes nothing, so a reset is skipped while held or replaying. Reported to 035 as 041 audit defect c. 035's tip `d3c15b55c` holds no cherry-pick of that commit, only a comment (`fe0427676`) giving the reset as the exception to the held flushes, so 035 reaches `master` with the defect and 041's merge removes it |
| `RemountSlot6Disks` on reset or power cycle | Skipped entirely (`DiskManager.cpp:658-664`) |
| Explicit flushes: `Eject` (`DiskImageStore.cpp:2304`), `FlushAll` on machine switch (`MachineManager.cpp:405-408`) and state save, `FlushAllForShutdown` (`EmulatorShell.cpp:299`), `SetImageWriteProtect` | Still write, and write the disk as it stands at the current, past position (`DiskImageStore.h:133-136`). `FlushEntry` checks only `m_isReplaying` (`DiskImageStore.cpp:1088-1097`) |
| Anything while `m_isReplaying` is set | No flush (`1090`), no reload (`2745-2750`), no media-change notice (`2233-2239`) |
| Eject or mount | Flushes (unless replaying), retires the image (`2322`), then the bay-change notice and `NotifyMediaChanged` lead to `OnMachineChanged`, which drops the recorded future and calls `BecomeLive`, clearing the hold |
| UI-posted mount, eject, write-protect, resolve-change, state load or machine switch behind live | `AllowCommand` raises the divergence question first; on yes it queues `IDM_DEBUG_DIVERGE` ahead of the command, which makes the machine live (`EmulatorShellReverse.cpp:303-333`; `DivergenceGate.cpp:25-45`; `ReverseHost.cpp:210-225`). Commands posted by the CPU thread itself skip this check (`EmulatorShellReverse.cpp:315-318`) |
| Reload of an externally changed file (`ApplyPendingReload`) | Not stopped by the hold, only by `m_isReplaying` (`DiskImageStore.cpp:2733-2787`) |
| Keyframe load that reseats a bay (`MachineHost.cpp:1668-1679`, `1773-1825`; `DiskImageStore.cpp:2117-2164`) | No flush, no bay-change notice, no media notice |
| `CommitHeldWrites` (= `FlushAll`, `1970-1973`) and `DiscardHeldWrites` (reload dirty bays, `1989-2025`) | No production caller; only `UnitTest/EmuTests/DiskFlushHoldTests.cpp:91`, `133` use them |

**Possible bug, found by reading, not tested.** `ReverseController::Stop` clears the hold (`142`) but not `m_isReplaying`. If the user quits while behind live after a forward step, `FlushAllForShutdown` reaches `FlushEntry`, which returns at `DiskImageStore.cpp:1090` and writes nothing. That contradicts the stated intent at `EmulatorShellCpuThread.cpp:321` and `ReverseHost.cpp:66-73`. Reported to 035 on 2026-10-08 as 041 audit defect a; 035 fixed it in `c913c9247` and `65499c194`, and 041's tasks.md T008 and T137 confirm the fix on the merged code before 041 merges.

## 4. Cross-thread readers of disk or drive state that 035 added

Every reader 035 added reads on the CPU thread and hands over a copy.

- **Disk II debugger panel.**
  - `Disk2Controller::GetDiagnostics` (`Disk2Controller.cpp:1042-1078`) is called from `DebuggerViewState::BuildPanels` (`DebuggerViewState.cpp:2731-2758`), through `BuildLive` (`212-216`) and `GatherDebugView` (`EmulatorShellDebugger.cpp:1741-1769`).
  - That runs inside `ServiceDebugger` → `PublishDebuggerView`, on the CPU thread.
  - The values are copied into a `DiagnosticsSnapshot`. The UI's `DiskHeadView` reads only that snapshot (`DebuggerWindow.cpp:276`, `9204-9265`).
- **Unsaved-disk count in the history status.** `ReverseHost::GetStatus` calls `CountUnsavedDisks` (`ReverseHost.cpp:336-365`; `DiskImageStore.cpp:1937-1955`). It is reached through `GetHistoryStatus` (`EmulatorShellReverse.cpp:1105-1124`) from `GatherDebugView` (`EmulatorShellDebugger.cpp:1753`), on the CPU thread. `HistoryBand` shows it from the snapshot (`HistoryBand.cpp:50-52`, `111-113`).
- **Mounted paths for the debugger.** `MachineDebugTarget.cpp:900` is read on the CPU thread into `DebugViewCapture::machineInfo` and into the channel greeting (`DebuggerController.cpp:204-216`).
- **Heat map rebuilds.**
  - `HeatHistory::CopyDisks` (`HeatHistory.cpp:1330-1402`) walks the live store and calls `DiskImage::Serialize` on the CPU thread (`HeatRebuildJob.h:68-83`).
  - It hands `shared_ptr<const vector<Byte>>` images to `ScratchHeatReplayer`, which replays on its own pool thread with its own store.
  - `FindInStretch` runs synchronously on the calling thread, which is the CPU thread (`HeatHistory.cpp:557-596`).
- **Keyframes.**
  - `CaptureNow` → `DiskImage::SaveState` (`ReverseController.cpp:1726-1741`) updates DiskImage's `mutable` shared-track cache (`DiskImage.cpp:1202-1224`), CPU thread only.
  - The result is flattened into a job buffer and packed on a work queue (`KeyframeStore.cpp:320-346`, `736-817`). The worker reads only the copy.
- **Timeline.** It reads no disk state. Its renderer machine has no disks (`ScratchMachineRenderer.h:19-23`), and the playhead values are atomics (`HistoryThumbnails.cpp:1341-1399`).
- **`DiskTrackSnapshot`.** No production reader. Its header documents it as CPU-thread only (`DiskTrackSnapshot.h:26-28`).
- **UI thread into the store.** `NoteExternalChange` (`EmulatorWindow.cpp:2789`) may be called from any thread and is guarded by `m_pendingMutex` (`DiskImageStore.h:381-386`, `715-719`).

**What 035 changed for UI-thread readers that already existed on master.**

035 added unlocked bay changes on the CPU thread:
- `SeatMedia` and `RetireBay` reassign `entry.path` and move `entry.image` (`DiskImageStore.cpp:2117-2164`, `2197-2221`).
- `DiskImage::LoadState` swaps every track vector on each seek, scrub or step back (`DiskImage.cpp:1300-1301`).
- `PruneRetainedMedia` and `SetMediaRetention(false)` free retired images (`2061-2069`, `2179-2182`).

The lifetime lock covers none of these. The UI-thread readers they now race:
- `UpdateDriveWidgets`, called from `TryPresentUiFrame` (`EmulatorShellPresent.cpp:635-638`; `DiskManager.cpp:791-905`, in particular lines 803, 865-878 and 895-898).
- `EmulatorShellPresent.cpp:778-787` and `EmulatorShellScene.cpp:1009`.
- Menu queries (`EmulatorWindow.cpp:751-800`; `EmulatorShellDisks.cpp:572-579`, `769-782`).
- `EmulatorWindowInput.cpp:505`, `525`, `543`.
- `WindowCommandManager.cpp:1221`, `1281`.

**Salvage, a UI-thread write path that now reaches reverse execution.**
- `RunSalvageFlow` runs on the UI thread (`WindowCommandManager.cpp:1497-1501`; `EmulatorShellDisks.cpp:596-678`) and calls `MountDiskInSlot6` directly (`674`).
- That mount goes `DiskImageStore::Mount` → `NotifyMediaChanged` (`DiskImageStore.cpp:640`) → `OnMachineChanged`, which takes a whole-machine keyframe, drops the recorded future and calls `BecomeLive`. All of that happens on the UI thread while the CPU thread is running.
- It also skips both the input journal and the divergence question.
- `AssessSalvage` and `SalvageToFile` read track bits on the UI thread (`EmulatorShellDisks.cpp:610`, `637`).

## 5. Existing mechanisms the disk fix should reuse

- **The debugger view snapshot** (new in 035). This is the published-struct mechanism the disk fix should build on.
  1. The CPU thread gathers state with copies only (`GatherDebugView`, `EmulatorShellDebugger.cpp:1727-1769`; `DebugViewCapture.h:11-51`).
  2. Panes are built on a queue (`DebugViewPublisher.h:10-68`).
  3. `PublishDebugSnapshot` stores a `shared_ptr<const DebuggerViewSnapshot>` plus a freshness flag under `m_debugViewMutex` (`EmulatorShellDebugger.cpp:1781-1789`).
  4. The UI takes it in `TakeDebuggerUpdate` (`1098-1116`; `DebuggerWindow.cpp:6378`).

  Caveat: it publishes only while the debugger window is shown and a build is due (`EmulatorShellDebugger.cpp:1653-1684`). The main-window drive widgets would need an always-on publish built the same way.
- **UI to CPU:**
  - `PostCommand` plus its gate (`CpuManager.h:71-78`).
  - Debugger actions queued under `m_debugViewMutex`, then `IDM_DEBUG_ACTION` (`EmulatorShellDebugger.cpp:856-865`).

  There is no "run on the CPU thread and wait" call.
- **CPU to UI:** `PostMessage` with a heap payload, such as `WM_APP_MOUNT_COMPLETED` and `WM_APP_REPORT_DAMAGE` (`EmulatorWindow.cpp:2645-2668`).
- **Framebuffer handoff** (from before 035): `m_framebufferMutex`, `m_framebufferReady` and `m_frameReadyEvent` (`EmulatorShellPresent.cpp:595-604`, `1109-1149`).
- **Small published values:**
  - Replay progress and seek-landing atomics (`EmulatorShellReverse.cpp:218-226`, `946-984`, `1075-1090`).
  - The replay caption under its own mutex (`898-929`).
  - `DriveWidgetState` atomics (`DiskManager.cpp:883-885`). The values written into them are themselves read without synchronization.
- **Immutable hand-off buffers:** DiskImage's shared tracks (`DiskImage.cpp:1192-1224`) and `HeatRebuildDisk` (`HeatRebuildJob.h:26-34`).
- **`HostInputGate`** for UI writes into the machine (`HostInputGate.h:9-38`; `HostInputGate.cpp:18-80`). Writers call `TryEnter` and hold a shared lock while they write. The CPU thread's `Hold` takes the lock exclusively, so once it returns no host write is in flight. Reverse execution holds it while behind live (`ReverseHost.cpp:129-150`, `182`).
- **Lifetime lock** (from before 035, `MachineHost.h:194-199`). It covers only a machine switch.
