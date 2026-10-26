# 035 branch code review, 2026-10-23

The full text behind Phase 49 in tasks.md: each finder's claim, its failure scenario, and the adversarial verifier's evidence. The base was `321035f85`.

Line numbers are as of `4210c0efe`, the branch tip the review read, and drift as fixes land. Find each spot by the function or member the finding names. A finding's evidence quotes the code as it stood then; if the code has since changed, recheck the claim before fixing it. Tasks with no entry here came from the tool sweep or the earlier review; their line in tasks.md is the whole record.

## T987 (high)

**CassoCore/Debugger/DebugExpressionEvaluator.cpp:931**, crash

The evaluator divides and takes modulo on int32 without guarding INT_MIN / -1. TryParseNumber accepts any 32-bit hex value and stores it as int32, so $80000000 becomes INT_MIN and $FFFFFFFF becomes -1. On x64, `idiv` raises an integer-overflow exception for INT_MIN / -1, and nothing here catches it. The Check-mode early return only covers rhs == 0.

Failure: In AppleWin mode, `CALC 80000000/FFFFFFFF` (or `? 80000000/-1` in WinDbg mode, or `80000000%FFFFFFFF`) raises STATUS_INTEGER_OVERFLOW and takes the process down. A breakpoint condition can do the same at run time: `BP 300 IF (A*1000000)/(0-1)` passes Validate while A is not $80, then crashes the CPU thread in ConditionContext::IsMet on the first hit with A=$80.

Verified: DebugExpressionEvaluator.cpp:483-511 TryParseNumber accepts values up to 0xFFFFFFFF and stores `(int32_t) total`, so 80000000 gives INT_MIN and FFFFFFFF gives -1. Both reach TryResolveName as Name tokens with isHexNumber set (lines 425-428, 825-828). TryApplyBinary (lines 910-924) guards only rhs == 0, then line 931 does `lhs / rhs` and line 932 does `lhs % rhs` on int32. INT_MIN / -1 makes x64 idiv fault. CALC reaches this through AppleWinParser.cpp:1389 TryEvaluate -> ParseAndEvaluate, and the 0..$FFFF range check at 2174 only runs after evaluation. The only SEH in the tree is SetUnhandledExceptionFilter(TraceCrashFilter) at GuiMain.cpp:779, a crash logger; there is no __try. The breakpoint path is ConditionContext::IsMet (ConditionContext.cpp:57), which calls Evaluate with no protection. Validate (line 99, Check mode) skips only a zero divisor, so a condition that is fine at set time can still fault at a later hit.

## T988 (high)

**CassoEmuCore/Debugger/Basic/ApplesoftProgram.cpp:223**, hang

GetArrays and FindValue walk guest-controlled ARYTAB/STREND data with no cycle or iteration bound. When at+next passes $FFFF, the Word arithmetic wraps, and the outer walk can cycle forever. The inner element loop's bound `element + width <= at + next` can then never fail, so it runs `total` (product of garbage sizes) times.

Failure: BRUN a machine-language game from a BASIC HELLO program. The game reuses zero page $6B-$6E, and the Applesoft ROM stays visible. Pause and open the BASIC variables view, or run the variables command (BasicHandlers::ShowVariables is gated only by IsApplesoftVisible). Example: ARYTAB=$B000, an array header with next=$6000 and two sizes of $2000 each. The element loop runs about 64M times, building a fresh OverlayCpu and running FOUT for every element, so the thread hangs and memory grows without limit. In a second case, next=$F000 at $1000 and next=$1000 at $0000 make the outer loop (and FindValue's mismatch loop at line 506) cycle forever. TryGetListing guards against this with `link <= at` and kMaxLines; these walks do not.

Verified: ApplesoftProgram.cpp:190-231 (GetArrays): `at` and `element` are Word, so `(Word)(at+next)` and `(Word)(element+width)` wrap. `element + width <= at + next` is evaluated in int, so `at + next` can be as large as 0x1FFFE, which a wrapped Word element can never exceed. The only bound left is `i < total`, and total is the product of guest-controlled 16-bit sizes, up to $2000*$2000. The outer loop has no visited set or iteration cap; `at+5 <= end && at < $C000` holds for a $1000 -> $0000 -> $1000 cycle, and arrays.push_back grows without limit. FindValue (lines 491-504) has the same wrapping `at = (Word)(at + next)` in its mismatch path and no cap. The callers BasicHandlers.cpp:216-231 (ShowVariables, gated only by IsApplesoftVisible) and DebuggerViewState.cpp:330-349 add no bound. TryGetListing has kMaxLines and the `link <= at` guard; these walks have neither.

## T989 (high)

**CassoEmuCore/Debugger/BinaryImageReader.cpp:401**, untrusted-input-bounds

ReadSRecord computes dataCount = bytes.size() - 2 - addressSize without checking that the record is long enough for its address. A short S2 or S3 record underflows to about SIZE_MAX, and the 64 KB range check then passes because size_t wraps. S3 (and S7) records with count 3 also read bytes[4] past the end of the vector at line 398.

Failure: Take a file whose first line is `S2030000FC` (count 03, checksum valid). The minimum line-length check passes and auto-detection picks S-record. dataCount = 4-2-3 = SIZE_MAX, and isInRange computes 0xFC + SIZE_MAX, which wraps to 0xFB, so it is <= 0x10000. AddBytes then builds a span of SIZE_MAX bytes and calls vector::assign, which throws length_error (or faults). Nothing in CassoEmuCore/Debugger catches it, so `BLOAD file` on a malformed or hostile S-record crashes the emulator. Fix: require bytes.size() >= 2 + addressSize before reading the address or computing dataCount.

Verified: BinaryImageReader.cpp ReadSRecord: the only length checks are line.size() >= kMinRecord (10) and bytes.size() == bytes[0]+1 (isWhole). `S2030000FC` is 10 chars and decodes to {03,00,00,FC}: isWhole holds, and the checksum is ~(03+00+00) = FC. The address loop reads bytes[1..3], which is in bounds for S2. Then `dataCount = bytes.size() - 2 - addressSize` = 4-2-3, which wraps to SIZE_MAX. `recordAddress + dataCount` promotes to size_t: 0xFC + SIZE_MAX wraps to 0xFB, which is <= 0x10000. AddBytes (around line 620) gets a SIZE_MAX span, and `segment.bytes.assign` throws length_error or faults. LooksLikeSRecord applies the same weak checks, so auto-detection picks this format. For S3 or S7 with count 3, the loop reads bytes[1+3] = bytes[4] on a 4-byte vector, which is out of bounds. Nothing in Read() or the format switch guards against either case.

## T990 (high)

**CassoEmuCore/Debugger/DebugSession.cpp:1358**, lifetime

After a machine switch, the WatchpointTable is no longer the bus's watch sink, so watchpoints, value breakpoints, the call record's stack writes, the I/O log and disk-vector tracking stop working without any error. OnMachineChanged never calls m_target.SetWatchSink(&m_watchpoints) again; only the constructor (line 56) installs it.

Failure: Open the debugger and switch machines. MachineManager runs `GetMemoryBus() = MemoryBus()`, which leaves m_watchSink null, then calls NotifyDebugMachineChanged, which calls OnMachineChanged. Afterwards BPMW $2000 publishes its pages to the new bus, but MemoryBus only reports an access when m_watchSink is non-null (MemoryBus.cpp:455). The watchpoint never fires, and SetCallRecording(true) re-publishes the stack page, but CallStackRecorder::OnStackWrite is never called, so the call stack is wrong.

Verified: The only places that install the sink are the constructor at DebugSession.cpp:56 and the destructor at :188, which sets it to null; grep finds no other SetWatchSink on the live bus. MachineDebugTarget::ReadIo/WriteIo (MachineDebugTarget.cpp:528-559) only save and restore whatever sink is already there. MachineManager.cpp runs `m_shell.m_machine.GetMemoryBus() = MemoryBus();` inside the rebuild, and MemoryBus.h:281 defaults m_watchSink to nullptr, so the sink is wiped. It then calls NotifyDebugMachineChanged (MachineManager.cpp:533), which reaches DebugSession::OnMachineChanged (DebugSession.cpp:1358-1409). That function clears the tables and calls SetCallRecording and UpdateHookInstalled, but never calls SetWatchSink. The state-load path (EmulatorShellState.cpp:217) is the same. Replayer.cpp:214 only puts back the sink it saved, which is null after a switch. MemoryBus::ReportAccess (MemoryBus.cpp:455) tests m_watchSink != nullptr, so WatchpointTable::OnWatchedAccess never runs again. That function feeds the stack-write sink, the I/O log, vector writes, watchpoints and value breakpoints (WatchpointTable.cpp:645-694).

## T991 (high)

**CassoEmuCore/Debugger/Handlers/ExecutionHandlers.cpp:71**, resource-exhaustion

With PROFILE ON, DebugSession::OnInstruction calls the observer on every instruction of a free run (isDebuggerRun includes IsProfiling()). So TF records a trace line for every free-running instruction into m_trace.lines, which nothing trims and which is written out only when a run stops. ToggleTrace's own contract limits the trace to debugger-driven runs.

Failure: TF, then PROFILE ON, then let the machine run freely (not G). Every instruction appends about 70 bytes to an in-memory std::string at hundreds of thousands of instructions per second, which is tens of MB per second. After a few minutes of free running the CPU thread exhausts memory and crashes, or the next pause tries to write a multi-GB file. The stopwatch also starts counting laps in free runs, though ShowStopwatch says 'in runs the debugger starts'.

Verified: DebugSession.cpp:1197 sets isDebuggerRun = DebugRun || Stepping || IsProfiling(), and :1208 then calls the observer. UpdateHookInstalled (:3574) and RefreshHookFilter (:3610) keep the hook installed and fed with every instruction while profiling is on. ExecutionHandlers::OnInstruction (:71) calls RecordTrace whenever m_trace.isOn, with no check of the run state. RecordTrace (:1343) appends each line to m_trace.lines and nothing caps it. The stopwatch (:64-66) is fed in the same way. Both contradict ToggleTrace's comment at :1157 ('a debugger-driven run') and the ShowStopwatch text at :1119.

## T992 (high)

**CassoEmuCore/Debugger/Project/DebugProjectStore.cpp:227**, data-loss

Read() treats a failed ReadAllText the same as a file that is not a project. It writes the empty `text` to the aside copy and opens an empty project at the original path, so the next save overwrites the user's real project.

Failure: An editor, OneDrive or an antivirus scanner briefly holds the .cassoproj open for write. Win32FileSystem::ReadAllText opens with FILE_SHARE_READ only, so it fails with a sharing violation. Read() then writes a zero-byte '<file>.unreadable-<stamp>' and posts a notice with an empty reason. Open() marks the result automatic at the same path, and ShellDebugger takes the isNew path, which clears m_projectSaved. The next SaveProject (CloseDebugger always calls it) writes the empty project over the original. Both the project and its aside copy are now gone.

Verified: Win32FileSystem.cpp:33-39 opens with FILE_SHARE_READ only and clears outContent on failure (lines 71-74). In DebugProjectStore.cpp:224-228 a failed ReadAllText skips the BAIL_OUT_IF, sets isFound=false and writes the empty `text` to the aside path. `error` stays empty, so the notice reads 'could not be read:  It is copied...'. Open (line 95-96) then sets isAutomatic=true and filePath=automatic. In ShellDebuggerProject.cpp:233-241 the empty project counts as isNew, so it calls CaptureProject and m_projectSaved.clear(). SaveProject (lines 859-885) sees text != "" and calls Save, which overwrites the automatic file. When there is no redirect, the automatic file is the user's project. CloseDebugger (ShellDebugger.cpp:2327) and scope switches call SaveProject. Nothing retries the read or tells a read failure apart from a parse failure. For a redirect, the redirect file is overwritten rather than the chosen file.

## T993 (high)

**CassoEmuCore/Debugger/Reverse/ReverseController.cpp:2237**, correctness

Keyframe scheduling is keyed on the absolute cycle counter, which a live power cycle resets to 0 (MemoryBusCpu::PowerCycle sets m_totalCycles = 0). Nothing tells the recorder about the power cycle, so m_nextDueCycle (the store's ScheduleAfter value) stays at the old high cycle and no keyframe is taken until the new count passes it. The CaptureNow guard at line 1797 only runs if some capture is triggered.

Failure: Run the machine for 10 minutes (about 600M cycles) with no disk mounted, so RemountDisks raises no media-change edit, then power cycle from the menu. No keyframe is taken for the next 10 minutes. During that time: (1) the input journal grows without bound, because DiscardBefore only runs at captures. (2) The first StepBack calls BuildStepTable over the whole stretch from the last pre-power-cycle keyframe; m_steps pushes one 24-byte ReplayStep per instruction (tens of millions, i.e. GBs) and replays minutes of emulation. (3) Cycle-based moves land in the wrong place: StepBackCycles, timeline click and scrub (SeekCycle/ScrubCycle) and StepOutByRecord's SeekToCycle. TryFindAtOrBefore(target.cycle) returns an old pre-power-cycle keyframe, and RunTo stops at once because that keyframe's cycle is already >= target. A one-frame step back therefore jumps to before the power cycle. With a disk mounted, the remount's edit instead hits line 1797, which Clears all history before the power cycle.

Verified: MemoryBusCpu.cpp:249 zeroes m_totalCycles on PowerCycle, while MachineHost::m_position (MachineHost.cpp:864) only ever increments. The menu path, CpuCommandDispatcher.cpp:75-77 -> MachineManager::PowerCycle (MachineManager.cpp:637-641) -> MachineHost::PowerCycle (MachineHost.cpp:995-1057), never calls the history recorder. NotifyDebugReset -> DebugSession::OnReset (DebugSession.cpp:1425) touches only the call recorder, and EmulatorShellCpuThread.cpp:232 only journals the record. So m_nextDueCycle keeps the value KeyframeStore::ScheduleAfter set from the last pre-power-cycle capture (KeyframeStore.cpp:1857), and HistoryRecorder::OnInstructionStart (HistoryRecorder.h:48) does not fire until the cycle count climbs back past it. CaptureNow is reached only from OnCaptureDue, CaptureBoundary and Start, so the line-1797 guard never runs. RemountSlot6Disks (DiskManager.cpp:665-678) raises a media change only for bays with a disk; with a disk, the guard Clears history as described. Consequences: (1) journal.DiscardBefore runs only in CaptureNow (line 1833). (2) BuildStepTable replays the whole stretch and pushes one step per instruction (ReverseController.cpp:1139-1145, Replayer.cpp:299-301). (3) StepBackCycles computes target = now - cycles, which is less than oldestCycle, the pre-power-cycle keyframe 0 cycle (ReverseController.cpp:607-612), so it seeks to the start of history. SeekToCycle/TryFindAtOrBefore (KeyframeStore.cpp:1363) return pre-power-cycle keyframes whose cycles are already >= the target. The existing test ReversePowerCycleReplayTests.cpp:72-106 runs only 3 frames after the power cycle, so it does not exercise this.

## T994 (high)

**CassoEmuCore/Seams/Win32FolderWatcher.cpp:177**, race

RunService copies the notification handles under m_lock, releases the lock, then waits on them. Unwatch can close one of those handles in between. The wait then returns WAIT_FAILED, which the loop treats as stop: the service thread exits for good, and because m_service stays joinable, EnsureService never starts it again.

Failure: FolderWatch::SetWatched unwatches two folders in a row (FolderWatch.cpp:113, e.g. navigating away in Cassque). The first Unwatch signals m_wake, so the service thread loops and copies the set, still holding folder B's handle. Before it calls WaitForMultipleObjects, the UI thread's second Unwatch closes B's handle. The wait returns WAIT_FAILED (ERROR_INVALID_HANDLE), running=false, and the thread returns. From then on no folder in any Cassque window is ever reported changed, and later Watch calls return true but do nothing.

Verified: Win32FolderWatcher.cpp:164-175 copies the notification handles under m_lock and lets it go. Line 177 then waits on the copies with the lock released. Unwatch (lines 99-100) calls FindCloseChangeNotification under the lock, so it can run in the gap between the copy and the wait. WaitForMultipleObjects checks every handle before it waits and returns WAIT_FAILED when one is invalid. Line 179 treats WAIT_FAILED like the stop event and breaks out of the loop, which ends the thread. Nothing joins it or resets m_service, so EnsureService (line 120) sees joinable() and returns early. Watch (lines 56-74) still creates the handle and returns true, but nothing waits on it any more. FolderWatch::SetWatched (FolderWatch.cpp:109-113) calls Unwatch for several folders in a row. Each call signals m_wake, which re-arms exactly the copy-then-wait window described. The window is narrow but nothing closes it.

## T995 (high)

**CassoEmuCore/Shell/DebugEventLogs.cpp:164**, correctness

DebugEventLogs::Pump pumps only the Disk II and input replayers. m_video.replayer and m_sound.replayer are never pumped, so their rebuilt stretches never reach the LRU and an 'earlier' request from the video or sound pane can never finish. RebuildIoBefore (line 603) also calls log.runner.Cancel() without clearing that replayer's m_pending, so even with a Pump the canceled stretches would never be resubmitted.

Failure: Open the Video log or Sound log pane with HISTORY on and scroll to its oldest row. The pane sends 'earlier', SendOlder calls m_video.replayer.Request(), and the runner rebuilds the stretches into its m_results deque, but nothing ever calls TryTakeResult for this runner. GetSpan stays incomplete, wantedBefore never returns to 0 and isRebuilding stays true. FollowScroll does not ask again while isRebuilding is set, so older rows never appear and the pane shows 'rebuilding' for good. Running VIDEOLOG/SOUNDLOG meanwhile cancels the job but leaves the stretches in m_pending, and IsPending then blocks any resubmission.

Verified: DebugEventLogs.cpp:162-168 Pump calls only m_disk.replayer.Pump() and m_input.replayer.Pump(). No other code pumps the video or sound replayers (grep finds no other replayer.Pump). BuildDelta for video and sound (line 333) still runs SendOlder, which calls log.replayer.Request (line 928). IoLogPane derives from EventLogPane, and EventLogPane.h:276-285 FollowScroll sends "earlier" -> ShellDebugger.cpp:2574 -> RequestEarlier. Results sit in ScratchEventReplayer::m_results and never reach Remember, so GetSpan (DebugEventReplayer.h:109) stays false, wantedBefore is never cleared, and FollowScroll does not ask again while IsRebuilding() is true. Second part: RebuildIoBefore line 603 calls log.runner.Cancel(), which clears m_next, m_results and m_latest (ScratchEventReplayer.h:128-137) but leaves the DebugEventReplayer's m_pending and m_generation alone. Only OnHistoryCut clears m_pending (DebugEventReplayer.h:160), so IsPending (line 68) then blocks resubmitting the same stretches.

## T996 (high)

**CassoEmuCore/Shell/MachineHost.cpp:1597**, persistence

On the //c, GetRomIdentity hashes `router->GetInternalRom()`. That is the $C100-$CFFF slice of whichever ROM bank is active, because Apple2cRomBank::ApplyBank swaps it in through AttachInternalCxxxRom on every $C028 toggle. So the identity of the same //c changes with its current bank. RomGeneration is bumped on the toggle, so the cached hash is recomputed with the other bank's bytes.

Failure: A //c running mouse or serial firmware is in the alternate bank when a snapshot is taken: a File > Save state, an auto-state at exit or switch, or a reverse-execution keyframe. Loading it later compares romIdentity against the machine's current bank (bank 0 after a cold boot), and the check fails. Three things break this way. (1) MachineStateFile::Check refuses the file with 'different ROM images'. (2) MachineHost::CheckStateHeader refuses the keyframe, so reverse step or seek fails with ERROR_INVALID_DATA. (3) LoadStateForPicture skips the history thumbnail. It fails the other way too: a bank-0 snapshot restored while the live machine is paused in bank 1. The identity should hash the two bank images (and RomDevices) only, not the live internal-ROM slice. No test toggles the bank (MachineHostStateTests only covers patches).

Verified: MachineHost.cpp:1595-1598 hashes router->GetInternalRom(), which is m_internal (CxxxRomRouter.h:52). Apple2cRomBank::ApplyBank (Apple2cRomBank.cpp:59-82), reached from ToggleRomBank (line 94-97) and LoadState (line 192), replaces that buffer through m_mmu.AttachInternalCxxxRom -> CxxxRomRouter::SetInternalRom (CxxxRomRouter.cpp:62-71), and SetInternalRom calls RomGeneration::Bump(). The cache at MachineHost.cpp:1572 therefore misses, and the hash is taken again over the slice of whichever bank is now selected. Hashing both bank images (lines 1600-1603) does not help, because the live slice is still mixed in. The save side writes the hash in WriteStateHeader:1673 and MachineStateFile.cpp:132. Every compare uses the live bank, and each runs before the saved bank is restored: MachineStateFile::Check:246/253, which Apply also calls at line 301 ('different ROM images'); CheckStateHeader:1750-1753 (ERROR_INVALID_DATA); and ReadPictureHeader:1343/1373. So a state saved in bank 1 is refused while the live machine is in bank 0, and the reverse fails the same way.

## T997 (high)

**CassoEmuCore/Shell/MachineManager.cpp:582**, logic

A Settings edit that needs a reboot sends SwitchMachine to the same machine (SettingsApplyController::Commit -> DoMachineSelect(currentMachine)). SwitchMachine now saves that machine's auto state before the teardown (line 425) and calls ResumeAutoState(false) after the rebuild. So the reboot reloads the state it just saved from the old hardware.

Failure: With autoSaveMachineState on, the user disables a slot card or adds a Mockingboard in Settings > Hardware and presses 'OK (reboot)'. The old machine is saved to <name>.autosave.cassostate, and the rebuilt machine runs PowerCycle. Then ResumeAutoState loads the saved state, with three possible results. (a) The card had a ROM: Check fails with 'different ROM images' and a notice says the resume failed. (b) The part list changed: LoadStateOverMountedMedia fails on the part count, the load rolls back, and a notice calls the state 'damaged'. (c) The part list is unchanged: the old session is restored onto the new hardware. In none of these does the requested cold reboot happen as the user expects. A same-machine rebuild should skip both the save and the resume.

Verified: SettingsApplyController.cpp:597-600: a reset-requiring edit calls m_catalog->DoMachineSelect(currentMachineNarrow). That posts IDM_FILE_OPEN (SettingsMachineCatalog.cpp:392), which reaches MachineManager::SwitchMachine (CpuCommandDispatcher.cpp:55-56). SwitchMachine (MachineManager.cpp:197 onward) has no same-machine guard. It calls m_shell.SaveAutoState() at line 425, rebuilds, runs PowerCycle at 556, then calls ResumeAutoState(false) at 582. AutoStatePolicy::ShouldResumeOnSwitch returns isEnabled && !isLoadingState && hasAutoState, and m_isLoadingStateSwitch is false on this path, so the file just written is resumed. ResumeAutoState (EmulatorShellState.cpp:392-438) goes through MachineStateFile::Apply. Apply runs Check first (MachineStateFile.cpp:301), which can fail on ROM identity, and rolls back on a LoadStateOverMountedMedia failure. If both pass, the old session is restored onto the rebuilt hardware. In none of these cases does the requested cold reboot stand.

## T998 (high)

**CassoEmuCore/Shell/ShellDebugger.cpp:2106**, lifetime / use-after-free

GatherDebugView captures DebugViewCapture::instructionSet, a raw pointer into the live CPU's std::vector member (Cpu::instructionSet.data()). It is dereferenced later on the pool thread (BuildCodeViews disassembly through CapturedDebugTarget) and on the UI thread (snapshot->instructionSet in CodeEditCommands, and the GetOpcodeForms cache keyed by that pointer). MachineHost::SetCpu replaces the CPU when the machine is rebuilt, and nothing waits for an in-flight build or drops the published snapshot. NotifyDebugMachineChanged also does not set m_isDebugViewDirty, so a paused machine keeps the stale snapshot.

Failure: With the debugger window open, PublishDebuggerView submits a build. The next command the CPU thread drains is IDM_FILE_OPEN, and SwitchMachine destroys the old EmuCpu while the pool thread is still disassembling the code views through capture->instructionSet, a heap use-after-free. Separately: switch machines while paused, then assemble in the code pane before another snapshot is built, and OpcodeTable(snapshot->instructionSet) reads freed memory. A new CPU allocated at the same address also makes GetOpcodeForms reuse the old CPU type's forms.

Verified: The capture holds a raw pointer into the live CPU's vector. DebugViewCapture.cpp:35 sets out.instructionSet = target.GetInstructionSet(). MachineDebugTarget.cpp:1050 returns m_host.GetCpu()->GetCpu6502()->GetInstructionSet(), and Cpu.h:101/247 shows that is instructionSet.data() of a std::vector member. The pool reads it during the build: CapturedDebugTarget.h:57 returns m_capture->instructionSet, DebugViewBuilder.cpp:43 calls BuildCaptured, which runs BuildCodeViews on the runner, and the session disassembles through m_target.GetInstructionSet() (DebugSession.cpp:1003). DebuggerViewState.cpp:218 copies the same pointer into snapshot.instructionSet, and the UI reads it in CodeEditCommands.cpp:231-236. MachineManager.cpp:487 runs SetCpu(nullptr) and MachineBuilder.cpp:1498 makes a new EmuCpu. The only synchronization there is the machine's lifetime lock (MachineManager.cpp:471), and neither DebugViewPublisher::BuildPending nor ShellDebugger takes it. NotifyDebugMachineChanged (ShellDebugger.cpp:2225-2240) neither waits on m_debugBuildQueue nor drops m_debugViewSnapshot, so a build in flight across a switch reads freed memory. The paused-machine sub-case is less certain: NotifyDebugMachineChanged does not set m_isDebugViewDirty, but a project-scope change after the switch (AddConsoleLines at ShellDebuggerProject.cpp:1027) may dirty the view anyway.

**CassoEmuCore/Debugger/CapturedDebugTarget.h:57**, thread-lifetime

The pane-build thread disassembles through a raw pointer into the live CPU's instruction table, which a machine switch frees. GetInstructionSet returns m_capture->instructionSet, which DebugViewCapture::Take copied from Cpu::instructionSet.data(), a std::vector member of that Cpu instance. Nothing drains the build queue before the switch destroys the CPU.

Failure: GatherDebugView submits an input to the 'Casso debugger build' queue, then the CPU thread switches machines (SetCpu(nullptr) destroys the old Cpu and its vector). The queued or running DebugViewBuilder::Build then disassembles, calls EffectiveAddress::Predict and GetCpuOwnReads through the freed Microcode array. The result is garbage panes or a crash.

Verified: DebugViewCapture.cpp:35 stores `out.instructionSet = target.GetInstructionSet();`, a raw pointer. MachineDebugTarget.cpp:1052 returns m_host.GetCpu()->GetCpu6502()->GetInstructionSet(), and Cpu.h:101/247 show that is instructionSet.data() of a std::vector member of the Cpu. CapturedDebugTarget.h:57 hands the pointer back unchanged. DebugViewBuilder::Build (DebugViewBuilder.cpp:40-43) runs on the 'Casso debugger build' queue (ShellDebugger.cpp:2036) and uses it through DebuggerViewState: GetOpcodeForms, InstructionTouches::Find -> EffectiveAddress::Predict, and TraceLookahead::FindNext (DebuggerViewState.cpp:678/984/1214/1433). MachineManager's switch calls m_machine.SetCpu(nullptr), and MachineHost.h:460 shows the CPU is held by unique_ptr, so the vector is freed. Nothing waits for or drains m_debugBuildQueue or the publisher first: grep finds no Wait/Drain/Flush. The build path also never takes the machine's lifetime lock.

**CassoEmuCore/Ui/Debugger/DebuggerViewState.h:261**, lifetime

The snapshot, and the capture BuildCaptured reads, carry `const Microcode * instructionSet`, a raw pointer into the live Cpu's std::array (Cpu.h:101). Nothing else in the 'copy, not a view' snapshot points into the machine. DebuggerViewState.cpp:218 copies it into the snapshot, and the pool-thread build disassembles through it.

Failure: With the debugger open and the machine running, the user switches machines. MachineManager::SwitchMachine calls SetCpu(nullptr) under the lifetime lock on the UI thread. The pool-thread build does not take that lock, and its in-flight BuildCaptured (Disassemble, InstructionTouches::Find, TraceLookahead) still reads instructionSet[opcode] from the freed Cpu: a use-after-free. Separately, a snapshot published before the switch still holds the dangling pointer, and CodeEditCommands builds OpcodeTable(snapshot->instructionSet) from it on the UI thread. GetOpcodeForms (line 863) also caches by this pointer value, so a new CPU of another type allocated at the same address (6502 vs 65C02) keeps the old opcode forms.

Verified: The Cpu stores the instruction set as a heap std::vector (CassoCore/Cpu.h:247; GetInstructionSet returns .data() at Cpu.h:101). DebugViewCapture::Take copies only that pointer (DebugViewCapture.cpp:35, DebugViewCapture.h:32), and CapturedDebugTarget::GetInstructionSet hands it back (CapturedDebugTarget.h:57) to the pool-thread build (DebugViewPublisher::BuildPending -> DebugViewBuilder.cpp:43 BuildCaptured). That build takes only m_viewLock (DebugViewPublisher.cpp:125). MachineManager::SwitchMachine calls SetCpu(nullptr) (MachineManager.cpp:487), and SetCpu takes a unique_ptr<EmuCpu> (MachineHost.h:163), so the old Cpu and its vector are freed. Nothing in SwitchMachine waits on m_debugBuildQueue or the publisher: StopReverseRecording and NotifyDebugMachineChanged (line 533, after the teardown) do not drain it, and the queue is touched only in ShellDebugger.cpp:2034-2050. One detail of the finding is wrong: the comment at MachineManager.cpp:414 says SwitchMachine runs on the CPU thread, not the UI thread. That does not change the race, because the build runs on the pool. Snapshots already published keep the dangling pointer (DebuggerViewState.cpp:218), CodeEditCommands.cpp:236 builds an OpcodeTable from it, and GetOpcodeForms caches by pointer value (line 863).

## T999 (high)

**CassoEmuCore/Shell/ShellDebuggerReverse.cpp:444**, correctness / data loss

GuardHistoryEdit re-queues the held edit as DebugCommandPayload::Encode(m_debugCommandClient, line, mode), where line is DebugSession::m_guardLine. That field is set only by ExecuteLine. Window actions such as memory pane patches (DebuggerActions::GetPatch), watch edits and register edits run through ExecuteActionCommand -> session.Execute(command), so m_guardLine is empty when the guard fires.

Failure: Behind live, the user edits a byte in a memory pane, which is a RunDebuggerAction with a PatchBytes command. The guard stores an empty line and posts WM_APP_CONFIRM_DIVERGE. The user answers Yes: IDM_DEBUG_DIVERGE discards the recorded future, then IDM_DEBUG_COMMAND runs an empty line. History is lost and the edit the user agreed to is never made.

Verified: m_guardLine is written only in DebugSession::ExecutePaneLine (DebugSession.cpp:468-485) and is cleared once the outer line ends. The guard in DebugSession::Execute (DebugSession.cpp:254) passes m_guardLine. A memory-pane patch takes this path: DebuggerActions::GetPatch (DebuggerActions.cpp:443) makes a PatchBytes action, RunDebuggerAction queues it, and RunDebugActions -> ExecuteAction -> ExecuteActionCommand (DebuggerViewState.cpp:2639-2692) calls session.Execute(command) directly, never through ExecutePaneLine. PatchBytes is in IsMachineWrite. GuardHistoryEdit (ShellDebuggerReverse.cpp:444) therefore encodes an empty line. On Yes, OnConfirmDiverge posts IDM_DEBUG_DIVERGE followed by IDM_DEBUG_COMMAND with that empty line. The UI does nothing to block edits while behind live (no such checks in Ui/Debugger). The finding names ExecuteLine where the field is actually set by ExecutePaneLine, but the conclusion holds.

**CassoEmuCore/Shell/ShellDebuggerReverse.cpp:444**, correctness

m_debugCommandClient is set only in RunDebugCommand, which only the window's IDM_DEBUG_COMMAND reaches (always client 0). Pipe-client commands run inside DebuggerController::Pump without passing through it, so a pipe client's guarded edit is re-queued as client 0. RunDebugCommand then drops it when no handler is attached (the handler is installed only by OpenDebugChannel, the window's IDM_DEBUG_OPEN). When a handler is attached, the line runs in the window console and the reply never reaches the client.

Failure: Launch with --debugger (OnCpuThreadStart calls OpenDebugger, so no handler is set) and seek back. A pipe client writes a register. The guard posts the question and the user answers Yes. DivergeHistory discards the history, and the IDM_DEBUG_COMMAND is dropped because m_debugCommandHandler is empty, so the client's edit never happens even though the history was discarded for it.

Verified: m_debugCommandClient is assigned only in RunDebugCommand (ShellDebugger.cpp:2150). Pipe clients run through DebuggerController::Pump -> m_server.Pump -> RunLine -> m_session.ExecuteLine (DebuggerController.cpp:111-154), which never passes through RunDebugCommand, so the guard encodes client 0 (or whatever ran last). The handler is installed only in OpenDebugChannel (ShellDebugger.cpp:1445-1490). The --debugger path calls OpenDebugger (EmulatorShellCpuThread.cpp:184), which installs the history guard (ShellDebugger.cpp:2441) but no command handler. RunDebugCommand then drops the line when m_debugCommandHandler is empty. When a handler is attached, it handles only client 0 and sends the output to m_debugConsolePending, so the pipe client never gets a reply.

**CassoEmuCore/Shell/ShellDebuggerReverse.cpp:442**, correctness

m_pendingDivergeCommand is a single optional that each guarded edit overwrites. Several machine writes rejected behind live before the UI thread runs OnConfirmDiverge collapse into the last one, and the earlier ones are dropped without notice.

Failure: Behind live, RunDebugActions runs a multi-action batch (for example GetWatchEditActions or GetWatchUndoActions producing two writes), a project's startup commands hold several writes, or a client sends two edits quickly. Each call to GuardHistoryEdit replaces the pending command and posts another WM_APP_CONFIRM_DIVERGE. The first confirm swaps out only the last edit, which re-runs after Yes; the other edits were reported as 'Answer the question ... to make it' and are never applied.

Verified: GuardHistoryEdit assigns m_pendingDivergeCommand, a single std::optional, and posts WM_APP_CONFIRM_DIVERGE on every call (ShellDebuggerReverse.cpp:440-447). OnConfirmDiverge swaps that optional out (ShellDebuggerReverse.cpp:470-478), so the first message takes the last edit and any later message finds nothing and returns. RunDebugActions runs every queued action in one loop (ShellDebugger.cpp:1585-1591), and WatchesPane.cpp:516/839 queue several actions from GetWatchEditActions and GetWatchUndoActions. Each guarded write overwrites the previous one, and the earlier ones are told 'history kept' (DebugSession.cpp:256) and are never re-run.

## T1000 (high)

**CassoEmuCore/Ui/Debugger/BreakpointHistory.cpp:776**, correctness

TryRebuild (run by FromText on every project load) builds each saved side by executing its definition against the live session. For BRK ON, BRKINT ON, BRKBASIC ON and BRKOP xx, the handler merges into an entry of that kind that already exists and calls TrySetCondition on it. Nothing new is added, so the live breakpoint's condition is overwritten, added.size() is 0, TryRebuild fails, and FromText Clear()s the whole undo history.

Failure: The user turns on BRK ON IF A==0 from the pane and saves the project. ShellDebugger::ApplyProject runs ApplyEngine (the live BRK is restored), then SetBreakpointUndoText. The history side "BRK ON" (from the step that first turned it on) is executed: the existing BRK's condition is reset to empty, no id is added, isOk is false, and FromText clears the history. After the reload the BRK breakpoint silently stops on every BRK instead of only when A==0, and Undo/Redo in the Breakpoints pane are empty. BRKINT, BRKBASIC and BRKOP behave the same way. The isDuplicateAllowed flag set at line 774 is only consulted by SetAddress.

Verified: BreakpointHistory.cpp:757-801: TryRebuild executes the definition and requires exactly one new id (line 787). For an existing BRK, BreakpointHandlers.cpp:681-690 (SetBrk) finds the existing Brk entry and calls TrySetCondition with no new entry. SetInterrupt (BreakpointHandlers.cpp:1176-1186), BasicHandlers.cpp:162-173 (BRKBASIC) and AddOpcodeEntry (BreakpointHandlers.cpp:1050-1065) also merge into the existing entry and overwrite its condition. A grep shows isDuplicateAllowed is read only at BreakpointHandlers.cpp:319 (SetAddress). ShellDebuggerProject.cpp:296-297 runs ApplyEngine before SetBreakpointUndoText, so the live entry already exists. added.size()==0 makes TryParseSide fail, and FromText (lines 633-636) Clear()s the whole history. A side whose condition differs from the live one (such as a plain "BRK ON" from an earlier step) resets the live condition before the failure, and nothing restores it.

## T1001 (high)

**CassoEmuCore/Ui/Debugger/FloatingPanes.cpp:1364**, correctness

Moving a floating window that holds a group of tabs and releasing it outside the dock site splits the shown tab off into a window of its own. OnDrag(ended) calls site.EndDrag with a point outside the site, so EndDrag calls m_onFloat(m_dragPane). BeginGroupDrag set m_dragPane to the active pane, so this reaches RequestFloat(active). Because the window holds more than one pane, RequestFloat takes the GetTearOffRect branch and DxuiPaneLayout::Float re-keys the pane into a new window.

Failure: A floating window holds Registers and Watches as tabs. The user drags it by its title bar to another spot on the desktop, away from the debugger. On release, the shown tab (say Registers) becomes a separate window. Its rect is built around the fake point {area.right+1, area.bottom+1} and then overwritten by SaveRects with the old key's rect. Watches stays behind in a second window. Every plain move of a group window therefore breaks the group apart. Single-pane windows are unaffected because RequestFloat keeps their window rect.

Verified: I traced the full path. While a floating window is moved, OnDrag (FloatingPanes.cpp:1344) calls site.BeginGroupDrag(m_windowPanes[window], GetActivePane(window)), and BeginGroupDrag sets m_dragPane = active (DxuiDockSite.cpp:1993). A plain move ends with Event::Ended: DxuiCaptionDragTracker::Finish returns Ended when the size did not change, and that calls OnDrag(ended=true). Released outside the site, OnDrag calls site.EndDrag(POINT{area.right+1, area.bottom+1}) (FloatingPanes.cpp:1364). There is no zone and no strip, so EndDrag (DxuiDockSite.cpp:2679-2681) calls m_onFloat(pane=active, point). DebuggerWindowLayout.cpp:589 wires that to m_floating->RequestFloat. Find(pane) is non-null and the window holds more than one pane, so RequestFloat (FloatingPanes.cpp:415-422) takes GetTearOffRect. DxuiPaneLayout::Float (DxuiPaneLayout.cpp:1056-1066) then sees shared=true and sets existing.window = MakeWindowKey(pane). MakeWindowKey returns 'pane#2' when 'pane' is already in use, so the pane gets a new window. FixActiveTabs then rekeys the old window to 'Watches' and the new one to 'Registers'. SaveRects (FloatingPanes.cpp:727-735) iterates m_windows, which still holds the old key 'Registers', and writes the old window's rect over the torn-off entry. Nothing in OnDrag or EndDrag guards group windows.

## T1002 (high)

**CassoEmuCore/Ui/Debugger/InstructionTouches.cpp:248**, correctness

ShadowCpu always runs the NMOS 6502 table. Its constructor is the plain Cpu6502 one, which builds the NMOS instruction set, and nothing ever installs the CMOS rows that Cpu65C02::InitializeCmos writes. On a 65C02 machine, InstructionTouches::Find gets the machine's CMOS `instructionSet` for EffectiveAddress::Predict, but RunTrial (and GetSourceRegister/GetDestinationRegister at lines 94/99/122/134) execute and decode the opcode as NMOS. That breaks the claim in InstructionEffect.h that 'decimal mode, the undocumented opcodes and every flag edge are right'.

Failure: Apple //e Enhanced or //c, paused at `LDA ($3E)` ($B2). On NMOS that opcode is illegal, which the core runs as a 1-byte 2-cycle NOP, so the result column shows `PC=$xxxx+1` as if it jumped, and A never changes. `STZ $10` ($64) is an NMOS zero-page NOP, so no write is shown. `PHX` ($DA) shows no push and lists no X read. `BRA` ($80) is an NMOS NOP #imm, so the PC change is missing. `INC A` ($1A) shows A unchanged. ADC/SBC in decimal mode reports the NMOS N/Z flags instead of the 65C02's valid ones. The watch pane's auto-watches built from the same touches are wrong in the same way. No test covers a 65C02 table.

Verified: InstructionEffect.h:28,41: `class ShadowCpu : public Cpu6502` with ctor `ShadowCpu(memory) : m_memory(&memory) {}`, so it only gets the base Cpu6502 constructor. The table is a per-instance member, `std::vector<Microcode> instructionSet` (CassoCore/Cpu.h:247). The CMOS rows go into it only from Cpu65C02's constructor via InitializeCmos (Cpu65C02.cpp:58-63, 121-129), and Cpu65C02 is a separate MemoryBusCpu subclass that ShadowCpu never derives from or copies. InstructionTouches::Find (InstructionTouches.cpp:67) and RunTrial (:248) build `ShadowCpu cpu(memory)` with no table parameter. GetSourceRegister/GetDestinationRegister (InstructionEffect.cpp:39-57) read `GetMicrocode(opcode)` from that NMOS table, and `cpu.Step` runs it. Only EffectiveAddress::Predict and `microcode = &instructionSet[opcode]` use the machine's CMOS table that callers pass in (DebuggerViewState.cpp:984/1433/1452/3401 pass session.GetTarget().GetInstructionSet()). 65C02 machines exist: CpuFactory.cpp:37-39 creates Cpu65C02, and MachineDebugTarget.cpp:1037 reports DebugCpuKind::M65C02. Nothing in the debugger UI installs CMOS rows on the shadow CPU. So on a 65C02 machine the results and register reads come from NMOS semantics. The exact NMOS behavior quoted for each opcode in the scenario was not traced, but the defect holds.

## T1003 (high)

**CassoEmuCore/Ui/Debugger/Panes/MemoryPane.cpp:325**, correctness

GoTo computes phase = address % bytesPerRow and hands the full value to the view (SetOriginAddress), but MemoryEditModel::SetPhase masks it to the low nibble (MemoryEditModel.h:105). Whenever a row holds more than 16 bytes, the view's labels and the model's addresses end up 16, 32 or 48 bytes apart.

Failure: Rows hold more than 16 bytes in common setups: Auto columns (the default) in a wide pane, for example 24 bytes a row, or 16 columns grouped by words (32) or longs (64). Take 24 bytes a row and Go to $0315: phase = $315 % 24 = 21, so the view labels the row $0315, but the model's phase is 21 & 0xF = 5 and it reads offset $0300 as $0305. The row labeled $0315 shows the bytes from $0305. Typing over a byte patches an address 16 bytes below the one shown, and that edit also goes into undo. Find (SelectRange calls GoTo), the context menu's GetAddressAt and the byte tips are off by the same amount.

Verified: MemoryPane.cpp:316-328 computes phase = address % GetBytesPerRow() and passes the full value to m_view->SetOriginAddress(phase), but MemoryEditModel.h:105 stores `phase & 0x000F`, and GetAddressOf (h:107) adds only that masked value. The view labels row r as m_originAddress + r*bytesPerRow (DxuiHexView.cpp:1412) and reads bytes through the model at the raw offset (MemoryEditModel.cpp:230 ReadBytes, 322 WriteBytes, 371 tips). Rows wider than 16 bytes are reachable: kDefaultColumns = 0 (MemoryPane.h:142) is Auto, and RecomputeRowWidth (DxuiHexView.cpp:784-797) sizes the row to the pane's width, times the grouping. Worked example with 24 bytes a row: Go to $0315 gives phase 21 and first $0300. The view labels the row 21 + 32*24 = $0315, but the model reads offset $0300 as $0305, 16 bytes off. SelectRange (line 292) and GetAddressAt (line 527) inherit the same error.

## T1004 (high)

**Dxui/Widgets/DxuiTextView.cpp:1173**, crash

FindAllInRows wraps only the std::wregex construction in try/catch. The wsregex_iterator loop that actually matches runs outside it. MSVC's matcher throws std::regex_error (error_complexity or error_stack) on a backtracking-heavy pattern or a long row. Nothing up the call chain catches it (SearchAsTyped, SelectMatch, FindWidgetSet), so std::terminate ends the process.

Failure: In the console or a source document's find box, turn regex on and type a catastrophic pattern such as (a+)+$ or (.|\s)*x while a row holds a run of 30+ matching characters. SearchAsTyped runs on every keystroke, regex_search exceeds MSVC's complexity limit and throws regex_error from ++it, and the emulator aborts.

Verified: Dxui/Widgets/DxuiTextView.cpp:1155-1164: only the std::wregex construction is inside try/catch(regex_error). The matching loop at 1168-1182 (std::wsregex_iterator construction and ++it) is outside it. MSVC's _Matcher throws regex_error(error_complexity / error_stack) once its complexity or stack budget runs out, which a catastrophic pattern or (.|\s)* on a long row reaches. Call chain: GetScopedMatches (960) <- SelectMatch(regex) (827) / SelectMatchInView (894) <- FindWidgetSet::SelectMatch path (FindWidgets.cpp:813) and SearchAsTyped (FindWidgets.cpp:868, run from box->SetOnChange at :152 on every keystroke). A grep for 'catch (' across Dxui/Casso/CassoEmuCore finds only DxuiTextView.cpp:1161 and an unrelated CommandModeHelp.cpp:531, and no set_terminate. Nothing catches the exception.

**Dxui/Widgets/DxuiTextView.cpp:1173**, crash / regex errors

The find widget's regex search can throw std::regex_error, and nothing catches it. FindAllInRows wraps only the std::wregex constructor in try/catch. The wsregex_iterator construction and ++it at line 1173 run outside it, and MSVC's matcher throws regex_error(error_complexity / error_stack) from there when backtracking or recursion limits are hit. Dxui has no catch anywhere else, and FindWidgetSet::SearchAsTyped (FindWidgets.cpp:868) runs the search on every keystroke.

Failure: Turn on '.*' in the console's or a source document's find widget and type a catastrophic pattern such as (\w+\s?)*$ or (.*)*x against a line of a few dozen characters, or (.)* against a line over ~600 characters. The matcher exceeds _REGEX_MAX_COMPLEXITY_COUNT or _REGEX_MAX_STACK_COUNT and throws from the iterator. The exception unwinds through the window procedure and terminates the emulator; a half-typed pattern can trigger it.

Verified: Dxui/Widgets/DxuiTextView.cpp:1157-1164 wraps only the std::wregex constructor in try/catch. The wsregex_iterator constructor and ++it at 1173 run outside it. FindAllInRows is called from GetScopedMatches (line 960), and that is reached from SelectMatch/SelectMatchInView (FindWidgets.cpp:813, 868; SearchAsTyped runs on every box change, FindWidgets.cpp:147). A grep for catch( across Casso, CassoEmuCore, Dxui and CassoCore finds only that one catch, so nothing upstream catches. I checked the installed MSVC STL (14.51.36231 <regex>). _Increase_complexity_count throws error_complexity once the budget of 256*len+300000 is used up (lines 3781-3794), and _Push_frame throws error_stack at _Frames_limit (3753). An exponential pattern such as (\w+\s?)*$ on a few dozen characters exceeds that budget, so the exception escapes into the window procedure. One detail is wrong: this STL sizes the frame limit from about 10 MB plus the input length, so the claim that (.)* fails on lines over ~600 characters does not hold. The complexity path is enough to confirm the finding.

## T1006 (medium)

**CassoEmuCore/Shell/DebugEventLogs.cpp:76**, correctness

Start resets all four caches but sets isResetDue only for the Disk II and input logs. An open Video or Sound pane keeps its old rows, heldFrom and sent state from the previous history. Start also resets m_videoClearedAt and m_soundClearedAt but leaves log.floor, so the pane and the VIDEOLOG/SOUNDLOG commands disagree about what was cleared.

Failure: Keep the Video log pane open, then load a machine state, rebuild the machine, or toggle recording in Tools > Options (each of these calls Start). The cache starts again at the new position, but the pane gets no isReset: new rows are appended after rows from the old timeline, possibly at lower positions, and heldFrom still holds the old value. isAtStart is wrong, and scrolling up rebuilds rows from the new history that the pane already shows, giving duplicates in the wrong order. If the log had been cleared at position P, log.floor = P survives the Start: the pane hides everything before P in the new session while VIDEOLOG lists it.

Verified: DebugEventLogs.cpp:71-77 resets all four caches but sets isResetDue only for m_disk and m_input, even though the function comment (lines 52-53) promises 'an open pane gets everything again'. SetOpen/ForLog handle Video and Sound (lines 208-211), so m_video.isOpen can be true. Start runs on StartReverseRecording and on ApplyReverseOptions toggling recording (ShellDebuggerReverse.cpp:54, 103, 164). log.floor is written only in ClearIoLog (line 494) and never reset in Start, while m_videoClearedAt and m_soundClearedAt go to 0 (lines 79-82). After a Start the pane (heldFrom/GetOldestOf use floor, lines 808, 950, header line 146) and VIDEOLOG/SOUNDLOG (GetIoEntries uses m_videoClearedAt, line 519) disagree.

**CassoEmuCore/Shell/DebugEventLogs.cpp:76**, correctness

Start resets the video and sound caches but sets isResetDue only for the disk and input logs, and Stop clears wantedBefore only for disk and input. So the new video and sound panes keep the previous history's rows and any pending "earlier" request across a restart.

Failure: With the Video or Sound log pane open, the machine is rebuilt or power-cycled, or recording is toggled in Tools > Options, so StartReverseRecording or ApplyReverseOptions calls Start. Disk and input panes get a reset delta. The video and sound panes do not: they keep the old history's rows and heldFrom/isAtStart, and new rows are appended after them, possibly out of position order. That breaks the partition_point that EventLogRows::Apply uses for cuts. A pending video or sound wantedBefore also survives Stop, so the machine rebuilds a stretch against the new history at the old heldFrom.

Verified: Start sets isResetDue for disk and input only (DebugEventLogs.cpp:76-77), although it resets all four caches (lines 71-74). Stop clears wantedBefore for disk and input only (DebugEventLogs.cpp:142-143). The video and sound logs became panes with isOpen/SetOpen/RequestEarlier through ForLog in 4210c0efe, and that commit's diff never touches Start or Stop, so this was left behind. The only other places isResetDue is set are SetOpen (line 188) and ClearIoLog (line 495). Neither runs on StartReverseRecording or ApplyReverseOptions (ShellDebuggerReverse.cpp:54, 103, 164). Without a reset, BuildDelta keeps the old heldFrom and wantedBefore (DebugEventLogs.cpp:792-825). The pane keeps its rows and appends the new cache's entries after them. A surviving wantedBefore makes SendOlder rebuild against the new history at the old heldFrom.

## T1007 (medium)

**CassoCore/CommandLineParser.cpp:4234**, integer-truncation

The new `debug --max-cycles` is parsed into a uint32_t through ParseDecimal. ParseDecimal uses strtol (32-bit long on Windows) and never checks ERANGE, so any value above 2147483647 is silently clamped to LONG_MAX. DebugOptions::maxCycles and DebugBatchRunner ("--max-cycles 64") both promise a 64-bit budget.

Failure: `CassoCli debug --machine Apple2e --script s.txt --max-cycles 5000000000` is accepted, but the budget is 2,147,483,647 cycles. The run stops less than halfway and exits with status 3 (budget stop) with no warning. `--timeout` and `--attach` take the same clamping path, so an out-of-range pid becomes 2147483647 instead of being refused.

Verified: CommandLineParser.cpp:2419-2432: ParseDecimal calls strtol into a long, which is 32 bits on Windows, and accepts the result when `end != text && *end == '\0' && val >= 0`. It never checks errno or ERANGE. An overflow returns LONG_MAX with end at the terminator, so it is accepted as 2147483647. Line 4234 parses `--max-cycles` through this function into `cycles`, a uint32_t, while DebugOptions::maxCycles is uint64_t (CommandLineOptions.h:441). DebugBatchRunner.cpp:163 says '--max-cycles 64'. `--attach` (line 4210) and `--timeout` (line 4224) use the same path, so out-of-range values clamp silently instead of being refused.

## T1008 (medium)

**CassoCore/Debugger/AppleWinParser.cpp:993**, symbol-shadowing

BPR and comparison-BP conditions name the register as a bare word (`A`, `X`, `PC`) rather than `@A`. TryResolveName tries the symbol tables before registers, so a user or loaded symbol with a register's name silently replaces the register in the breakpoint.

Failure: After `SYM X = 300`, or after loading a Merlin or cc65 file that defines a label `X` or `PC`, `BPR X = 5` compares the constant $300 with 5 and never fires. `BP = 1234` builds `PC=1234`, which becomes a constant comparison if a symbol `PC` exists. Emitting `@` + subject avoids this.

Verified: TryParseRegisterCondition (AppleWinParser.cpp:983-996) passes the bare register name (command.text) as the subject, so the expression contains `X=5`, not `@X=5`. The bare word becomes a Name token (DebugExpressionEvaluator.cpp:425-427). TryResolveName (lines 800-806) calls context.TryResolveSymbol before it tries the register (line 819). The session's resolver (DebugSession.cpp:2557-2559) falls through to m_symbols.TryResolve. SymbolHandlers::Add (SymbolHandlers.cpp:613-624) accepts any name, register names included. The register-name rejection at CassoCore/Parser.cpp:753-766 applies only to the assembler's own labels, not to SYM or loaded symbol files. The `@` form is resolved directly as a Register token (lines 567-573, 742-743), so emitting `@` would avoid the problem.

## T1009 (medium)

**CassoCore/Debugger/AppleWinParser.cpp:2605**, precedence

TryParseCondition builds the condition by gluing strings together as `subject + op + rest`, with no parentheses around rest. When the value expression contains an operator that binds looser than the comparison (& ^ | && || or another comparison), the comparison binds first.

Failure: `BPR A = 1|2` becomes `A=1|2`, which parses as `(A==1)|2`. That is always nonzero, so the breakpoint fires on every instruction instead of when A == 3. `BP >= 300 <= 400` becomes `(PC>=300)<=400`, which is always true, so execution stops immediately. The WinDbg-mode `bp` comparison path (WinDbgParser.cpp:926) has the same problem.

Verified: AppleWinParser.cpp:2605 parses `subject + op + rest` with no parentheses around rest. Precedences (DebugExpressionEvaluator.cpp:21-41): comparisons are 6-7, & is 5, ^ is 4, | is 3, && is 2, || is 1. So `BPR A = 1|2` reaches TryParseCondition (via line 996) as op '=' and rest '1|2', giving `A=1|2` = (A==1)|2, which is always nonzero. ConditionContext::IsMet (ConditionContext.cpp:65) returns evaluated != 0, so it fires on every instruction. `BP >= 300 <= 400` goes through line 569 and builds `PC>=300 <= 400`. Both operators are precedence 7, so it reads (PC>=300)<=400, which is always 1. Nothing rejects a second comparison operator. WinDbgParser.cpp:926 calls the same function.

## T1010 (medium)

**CassoCore/Debugger/DebugExpressionEvaluator.cpp:930**, integer-overflow

Add, Subtract, Multiply and Negate are plain int32 arithmetic, so overflow is undefined behavior. In practice the result wraps, and AppleWinParser::TryEvaluate's 0..$FFFF range check then accepts a wrapped result as a valid address instead of reporting overflow.

Failure: `BP 10000*10000` evaluates to $1_0000_0000, which wraps to 0, so the command silently sets a breakpoint at $0000 rather than saying the value is outside $0000-$FFFF. `D 8000*20000` likewise dumps $0000. `-80000000` negates INT_MIN, which is UB.

Verified: DebugExpressionEvaluator.cpp:928-930 are plain int32 `lhs + rhs`, `lhs - rhs` and `lhs * rhs`, and line 865 is `result = -operand`. There is no widening and no overflow check. 0x10000*0x10000 = 2^32 and 0x8000*0x20000 = 2^32, which wrap to 0 with MSVC's imul. AppleWinParser::TryEvaluate (AppleWinParser.cpp:2174) only rejects `result < 0 || result > 0xFFFF`, so 0 passes and is stored as $0000. Negating INT_MIN is UB; in practice it stays INT_MIN and is then rejected as negative.

## T1011 (medium)

**CassoEmuCore/Config/GlobalUserPrefs.cpp:1078**, persistence-concurrency

Both prefs writers (Save, and UserConfigStore::BuildCombinedJson at line 1871) merge only window placements from disk. Every new debugger setting (layout, closedPanes, theme, keyScheme, docks, log options, windowState, source folders, reverse budget) is written from this instance's in-memory copy, so the last instance to save wins even if it never touched those settings.

Failure: Instance A starts and never opens the debugger. Instance B opens the debugger, rearranges panes, closes some, adds source folders and builds console history, then exits and saves. When A exits it writes the debuggerLayout, debuggerClosedPanes, debuggerSourceFolders and debuggerWindowState it loaded at startup. B's whole debugger arrangement and history revert. The placement merge was added for exactly this pattern but covers only the bounds.

Verified: GlobalUserPrefs::Save (GlobalUserPrefs.cpp:1062-1078) loads onDisk but merges only MergeUntouchedPlacements (lines 1140-1157), which restores window.placements and window.debuggerPlacements and nothing else. The rest of `merged = *this` comes from this instance's memory. UserConfigStore::BuildCombinedJson (UserConfigStore.cpp:1855-1871) does the same. Every DebuggerSettings field (DebuggerSettings.h: layout, closedPanes, theme, keyScheme, docks, log options, sourceFolders, windowState, reverse budget) is new on this branch; GlobalUserPrefs.h at 321035f85 has no debugger member. An instance that never opened the debugger still saves through SaveGlobalPrefs (ShellSettings.cpp:348), which runs on window moves, theme changes and disk changes, and so writes back the debugger settings it read at startup. The same last-writer-wins model already covers the older global prefs, but the mechanism described here is accurate.

## T1012 (medium)

**CassoEmuCore/Debugger/Basic/BasicStopMonitor.cpp:249**, correctness

PollBoundary skips disabled variable entries without refreshing or dropping their m_values. MarkPages also stops hooking the boundary pages when no BASIC entry is enabled, so values go stale. When the entry is re-enabled, the first boundary compares against the value from before it was disabled.

Failure: Set a BASIC variable breakpoint on X and run, so X's value is taken. Disable it and let the program change X many times. Re-enable it and continue: the next statement stops with 'X changed from <old> to <new>' and names m_lastLine as the line that changed it. That line is also stale, because nothing polled while the pages were unmarked. Only machine writes, resets and machine changes call TakeValues or Rebase; enabling an entry calls neither.

Verified: BasicStopMonitor.cpp PollBoundary: `if (!entry.enabled || ...) continue;` skips disabled entries and leaves their m_values[id] alone. MarkPages marks the $D8xx statement and program-end pages only for enabled BASIC entries, so with no other BASIC entry enabled, Poll stops running and m_lastLine also goes stale. On re-enable, `found != m_values.end() && found->second != bytes` compares against the value taken before the entry was disabled and reports changeLine = the stale m_lastLine. BreakpointTable::TrySetEnabled (BreakpointTable.cpp:253, called from BreakpointHandlers.cpp:1374/1388/1682/1895) does not refresh it. TakeValues is called only after IsMachineWrite verbs (DebugSession.cpp:265-268, and enable is not in the list at lines 290-319) and from the history restores (CallStackHistory.cpp:172, 845). Rebase is called only on machine change and reset (DebugSession.cpp:1384, 1433).

## T1013 (medium)

**CassoEmuCore/Debugger/BinaryImageReader.cpp:302**, untrusted-input-bounds

Intel HEX record types 02 and 04 read bytes[4] and bytes[5] without checking count == 2. Types 03 and 05 read bytes[count+2] and bytes[count+3] without checking count == 4.

Failure: The record `:00000002FE` has count 0, a valid checksum, and a whole length of 5 bytes. bytes[5] is one past the end of the vector: the MSVC debug runtime asserts ('vector subscript out of range') and aborts; release reads heap garbage into `base`. Then every later data record loads at a garbage segment base, or is refused as out of range. A 03 or 05 record with count 0 silently takes the record's offset bytes as the entry point.

Verified: BinaryImageReader.cpp ReadIntelHex only checks bytes.size() == count+5 and that the checksum sums to zero. `:00000002FE` decodes to {00,00,00,02,FE}: size 5 == 0+5, the sum is 0x100 so the low byte is 0, and the line is 11 chars, so it passes kMinRecord and LooksLikeIntelHex. Case 0x02 then reads bytes[5], which is past the end of the vector (case 0x04 is the same). Cases 0x03 and 0x05 read bytes[count+2] and bytes[count+3] with no count==4 check. With count 0 these are bytes[2] and bytes[3], the offset low byte and the type byte. They stay in bounds but give a garbage entry point. (The finding says 'offset bytes'; it is one offset byte plus the type byte, which is a minor imprecision.)

## T1014 (medium)

**CassoEmuCore/Debugger/Channel/DebugChannelServer.cpp:83**, dos/unbounded-buffer

Pump loops `while (TryReadLine)`, and every TryReadLine re-polls each connection, reading up to 16 x 4 KB while consuming one line. A client that keeps writing therefore keeps Pump from returning, and Connection::lines grows without bound. This contradicts the 'bounded so a flood cannot hold one pump' design in StartRead and the 'BUFFERING IS BOUNDED' header comment.

Failure: A script pipes a large batch of commands, or a buggy client loops sending `{"type":"pause","id":1}`. Each HandleLine runs a command, which is slower than the bytes arrive, and every iteration pulls in up to 64 KB more lines (thousands of short ones). Pump runs on the CPU thread once a frame and never gets back to emulation, so the emulator freezes while the deque grows until the process runs out of memory.

Verified: DebugChannelServer.cpp:83-87 loops `while (m_transport.TryReadLine(...))`. Win32PipeTransport.cpp TryReadLine calls PollRead on every connection before taking one line. PollRead calls StartRead when no read is pending, and StartRead does up to 16 x 4096-byte immediate reads (kMaxImmediateReads, kReadChunkBytes), all appended to the unbounded std::deque `lines` (Win32PipeTransport.h:90). The StartRead bound is therefore re-armed on every iteration, and a client that keeps the pipe fed keeps Pump from returning while the deque grows. 'BUFFERING IS BOUNDED' (header line 32) refers only to line length. Caveat: a flooding client that never reads its replies is marked broken after the 64 KB out-buffer fills plus kWriteTimeoutMs=2000. A client that sends continuously and also drains its replies has no limit.

## T1015 (medium)

**CassoEmuCore/Debugger/Channel/Win32InstanceDirectory.cpp:312**, security

The client opens `\\.\pipe\Casso.Debug.<pid>` with CreateFileW and no SECURITY_SQOS_PRESENT/SECURITY_IDENTIFICATION. It does this for every pipe whose name merely matches the pattern (ListProcessIds), and it never checks the server (for example with GetNamedPipeServerProcessId against the pid).

Failure: Another account on the machine creates a pipe named Casso.Debug.12345 with an Everyone DACL. This can be a service account holding SeImpersonatePrivilege. `CassoCli debug --list` or `--attach 12345` connects to it at the default SecurityImpersonation level. The squatter calls ImpersonateNamedPipeClient and acts as the CLI user, and it also receives every command the script sends.

Verified: Win32InstanceDirectory.cpp:309-312: CreateFileW(name, GENERIC_READ|GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr). It sets no SECURITY_SQOS_PRESENT/SECURITY_IDENTIFICATION flag, so the default SecurityImpersonation level applies. ListProcessIds (lines 268-292) enumerates every \\.\pipe\Casso.Debug.<digits> entry, and InstanceLister::List connects and sends hello to each one. DebugMode.cpp:155 connects to --attach pids the same way. A grep for GetNamedPipeServerProcessId, SECURITY_SQOS_PRESENT and SECURITY_IDENTIFICATION finds nothing, so no server identity check exists. A squatting server holding SeImpersonatePrivilege can impersonate the CLI user and also receives every command the script sends.

## T1016 (medium)

**CassoEmuCore/Debugger/Channel/Win32PipeTransport.cpp:566**, protocol/data-loss

ReleaseConnection calls DisconnectNamedPipe right after the last WriteLine with no FlushFileBuffers. DisconnectNamedPipe throws away any data the client has not read yet, so the `closing` record DebugChannelServer::Close broadcasts just before m_transport.Close() is usually lost.

Failure: The debugger window is closed while a CLI client is attached. Close() broadcasts `closing`; the overlapped write completes as soon as the bytes are buffered; then Close releases every connection with DisconnectNamedPipe. If the client is not blocked in a read at that moment, it gets ERROR_PIPE_NOT_CONNECTED or a broken pipe instead of `closing`. It cannot tell an orderly close from a crash, which is the whole point of the record.

Verified: DebugChannelServer.cpp:113-125: Close broadcasts `closing`, then calls m_transport.Close(). Win32PipeTransport::Close calls ReleaseConnection, which cancels the pending read and then calls m_api.DisconnectPipe (Win32NamedPipeApi.cpp:113 calls ::DisconnectNamedPipe), with no FlushFileBuffers anywhere (a grep finds none in the channel code). WriteLine's overlapped write completes once the bytes are in the 64 KB pipe buffer. DisconnectNamedPipe discards unread data. The client (Win32InstanceDirectory.cpp, Win32PipeClient::ReadLine) only has a read pending after a timed-out ReadLine. A lockstep client that has just taken its reply has no read outstanding, so `closing` sitting in the buffer is thrown away and the client sees a broken pipe.

## T1017 (medium)

**CassoEmuCore/Debugger/CpuManagerRunDriver.cpp:146**, logic

A detached session still pauses the machine. Detach only uninstalls the host's debug hook. The bus still reports to the WatchpointTable, and OnSliceExecuted asks m_hook.HasStopped() whether or not the hook is installed; that call goes through m_conditions to DebugSession::HasPendingStop.

Failure: Set an enabled BPM watchpoint (or BRKUNINIT), then close the channel with detach: SetAttached(false), SetPaused(false), OnUserResumed. The program touches the watched address and WatchpointTable::OnWatchedAccess records m_pendingHit. At the end of the slice, OnSliceExecuted sees !m_isRunning and HasStopped() true and calls Finish, which pauses a machine the debugger was supposed to be detached from.

Verified: Detach (ShellDebugger.cpp:1521-1529) calls EndForUserPause, SetAttached(false), SetPaused(false) and OnUserResumed. SetAttached only changes whether the hook is installed: UpdateHookInstalled at DebugSession.cpp:3574 gates on m_isAttached. The bus sink and the watched pages are left as they were, so WatchpointTable::OnWatchedAccess still calls RecordHit and sets m_pendingHit (WatchpointTable.cpp:679-681, 766-771). hooks.runDriver stays m_debugRunDriver (ShellDebuggerReverse.cpp:997), which only CloseDebugger clears, so OnSliceExecuted is still called after every slice (EmulatorShellCpuThread.cpp:630). With !m_isRunning it calls m_hook.HasStopped() (CpuManagerRunDriver.cpp:146). That goes to RunStopHook::HasPendingStop (RunStopHook.cpp:300) -> m_conditions->HasPendingStop(), and the session's version (DebugSession.cpp:1965) returns m_watchpoints.HasPendingStop() with no attached check. Finish then calls m_cpuManager.SetPaused(true).

## T1018 (medium)

**CassoEmuCore/Debugger/Events/DebugEventReplayer.h:68**, correctness

A stretch stays in m_pending until its result arrives, and IsPending skips any request whose stretches are all already pending. When a job ends early, the stretches after the failing one never produce a result, so they stay pending for good. A job ends early when ScratchEventReplayer::Run stops at a stretch that fails, for example with ERROR_INVALID_DATA on a divergence, or when m_scratch.Build fails before any stretch.

Failure: The Disk II pane asks for an old stretch near the start of history, so the job has only a few older stretches and nothing beyond them to prefetch. The second stretch's replay diverges (end cycle mismatch) and Run returns after passing on that one failed result. The older stretches remain in m_pending. When the user scrolls to them, FindWanted returns exactly those stretches, IsPending is true and Request bails without submitting. GetSpan stays incomplete, wantedBefore is never cleared, and the pane stays 'rebuilding' with no way to recover other than closing and reopening it.

Verified: Only Remember (DebugEventReplayer.h:251) and OnHistoryCut (line 160) remove entries from m_pending. ScratchEventReplayer::Run (ScratchEventReplayer.h:242-249) passes the failing stretch's result and then CHR ends the job, so the stretches after it never produce a result. A failed m_scratch.Build (line 228-229) produces none at all. When the pane next asks for an older stretch near the start of history, FindWanted returns that stretch plus a prefetch that is all in the old job, IsPending is true, and Request bails (line 68) without submitting. GetSpan then returns false, SendOlder bails with wantedBefore set (DebugEventLogs.cpp:940-952), and nothing clears it short of OnHistoryCut or a reset.

## T1019 (medium)

**CassoEmuCore/Debugger/Project/DebugProject.cpp:191**, correctness

Breakpoint names are re-applied by their index in BPL's list even when the replayed script produced a different number of breakpoints. RestoreBreakpointIds explicitly gives up when the counts differ; the name loop has no such guard.

Failure: The case the code itself mentions: a project saved when duplicates were allowed lists the same breakpoint twice, and on load the second is dropped quietly as 'breakpoint already set'. A symbolic `BP LABEL` line whose symbol file has moved fails the same way. Every saved name after the dropped entry then lands on the next breakpoint, so the wrong breakpoints are renamed. The bad names are saved back on the next SaveProject.

Verified: DebugProject.cpp:113-120 (Capture) saves names keyed by index in BPL's list. ApplyEngine (lines 146-158) drops 'breakpoint already set' failures quietly and records any other failed line, then at lines 187-195 applies `breakpointNames[index]` to `list.breakpoints[index]`, checking only `index < size`. RestoreBreakpointIds (lines 300-306) returns when `list.breakpoints.size() != breakpointIds.entries.size()`, but the name loop runs before it with no such guard. Any dropped or failed breakpoint line therefore shifts every later name onto the next breakpoint, and the next Capture/SaveProject persists the shifted names.

**CassoEmuCore/Debugger/Handlers/BreakpointHandlers.cpp:1776**, persistence

BPSAVE (and SAVE) write the 'BPD i' and 'BPCHANGE i' lines by list position, which assumes each definition replays into exactly one new id. Exec breakpoints that duplicate an earlier one are refused on replay by SetAddress's duplicate check, so every id after that point shifts by one. Such duplicates are easy to create through SetSourceLine (which skips the check), FollowSymbols moves, BPEDIT, or the undo rebuild (isDuplicateAllowed).

Failure: BP 0803, then BP main.s:12 (line 12 assembles to $0803), then BPD 1. BPSAVE writes 'BPC *', 'BP 0803', 'BP 0803', 'BPD 1'. When the file is loaded, the second BP fails with 'breakpoint already set' and BPD 1 then reports no such breakpoint. With more entries after it, BPD and BPCHANGE disable or re-flag the wrong breakpoints. BPEDIT # BRKOP 00 next to a BRK ON entry drifts the same way, because replaying BRKOP 00 reuses the Brk entry.

Verified: MakeScript (BreakpointHandlers.cpp:1771-1784) writes BPD/BPCHANGE by list position i. After BPC *, numbering restarts at 0 (DebugSession.cpp:1159-1173). On replay, SetAddress (:319-327) refuses a duplicate Address entry with the same range and condition. SetSourceLine (:397) calls AddAddress with no duplicate check, so the duplicate can exist, and MakeDefinition (:190) writes both as 'BP 0803'. DebugProject::ApplyEngine (DebugProject.cpp:154-159) even drops 'breakpoint already set' quietly, so every later index shifts. RestoreBreakpointIds (:304-307) then gives up because the counts differ.

**CassoEmuCore/Ui/Debugger/BreakpointImport.cpp:92**, correctness

Import maps the script's sequential numbers (BPSAVE writes "BPD i" and "BPCHANGE i" by position) onto `created`, which collects only the ids that each definition line actually added. A line that adds nothing shifts every later BPD/BPCHANGE onto the wrong breakpoint, or past the end so it is dropped. Nothing-added cases: a BP refused as a duplicate, or BRK ON / BRKOP / BRKINT merging into an existing entry. A line that adds several (BRK ON with an invalid-opcode selector) has the same effect.

Failure: BP 300 is already set. The user imports a file BPSAVE wrote from [#0 BP 300, #1 BP 400 (disabled), #2 BP 500]. BP 300 is refused as a duplicate, so created = [id(400), id(500)]. "BPD 1" then disables created[1], which is BP 500. BP 400 stays enabled and BP 500 is disabled. If the disabled entry were the last one, its BPD index would be >= created.size() and would be skipped silently.

Verified: BreakpointImport.cpp:86-93 appends to `created` only the ids that a definition line actually added. TryApply (lines 278-283) and TryApplyFlags (lines 316-323) index `created` with the script's positional number. MakeScript (BreakpointHandlers.cpp:1767-1784) writes BPD/BPCHANGE by list position i. The leading "BPC *" line is not a definition, and TryApply rejects it (verb is neither Disable, Enable nor Change), so breakpoints already live stay in place. SetAddress (BreakpointHandlers.cpp:319-327) refuses a duplicate BP because import never sets isDuplicateAllowed. Every later index therefore shifts onto the wrong breakpoint, and an index at or past created.size() is dropped and counted as skipped.

## T1020 (medium)

**CassoEmuCore/Debugger/Handlers/ExecutionHandlers.cpp:457**, state-clobber

FRAME calls SetBeamBreak with isForOneRun=true, which overwrites the session's only beam-break slot. A BPBEAM breakpoint the user had set is silently replaced, and it is gone for good when the frame run ends (or at once through ClearBeamBreak when the Go is refused).

Failure: BPBEAM 50 10, then FRAME. During the frame run the user's beam break no longer fires. When the run stops, DebugSession resets m_beamBreak because isOneRun is set. BPL no longer shows the BPBEAM, and later runs never stop at scanline $50 cycle $10. Nothing told the user it was removed.

Verified: RunFrame (ExecutionHandlers.cpp:457) calls SetBeamBreak(..., true). DebugSession.cpp:1771 overwrites the single std::optional<BeamBreak> m_beamBreak (DebugSession.h:549). At a stop, :2135-2137 resets it because isOneRun is set, and on a refused Go RunFrame calls ClearBeamBreak (:466). Nothing saves or restores the user's earlier BPBEAM, and docs/Debugger.md:236 does not mention that FRAME replaces it.

## T1021 (medium)

**CassoEmuCore/Debugger/Handlers/ExecutionHandlers.cpp:1367**, resource-exhaustion

The TF buffer m_trace.lines is never trimmed after a flush. At every stop FlushTrace rewrites the whole file with WriteAllText from the start of the trace, so memory grows without bound and file I/O is quadratic in the number of stops.

Failure: TF on, then a long G (millions of instructions) or a script of thousands of T steps. Memory grows by about 70 bytes per instruction. Each step's stop rewrites the entire accumulated trace, so a step-heavy session gets slower with every step and a long G can exhaust memory.

Verified: FlushTrace (ExecutionHandlers.cpp:1365-1367) calls WriteAllText(m_trace.path, m_trace.lines) with the whole buffer and never clears m_trace.lines. The buffer is reset only when tracing is turned off (:1206). OnRunStopped (:141) flushes at every stop, so each stop rewrites everything accumulated so far, and memory grows by one line per traced instruction (:1343).

## T1022 (medium)

**CassoEmuCore/Debugger/Handlers/MemoryHandlers.cpp:495**, wrong-refusal

The 'file too long' check compares the whole segment size against the room left to $FFFF and ignores the length the user gave. The second loop caps the load at a2-a1+1, so a capped load that fits is refused.

Failure: BLOAD dump.bin 9000,100, where dump.bin is a 64 KB memory image, asks for 256 bytes at $9000, which fits. Room is $7000 and the segment is $10000 bytes, so the command fails with 'dump.bin holds 65536 bytes, which run past $FFFF from $9000' and loads nothing.

Verified: The room check (MemoryHandlers.cpp:495) compares segment.bytes.size() against room without applying the user's length. The cap min(count, a2-a1+1) is applied only in the second loop (:509-512). ReadRaw (BinaryImageReader.cpp:132-147, through AddBytes) puts the whole file into one segment. So a 64 KB file with BLOAD at $9000 and a short length is refused with 'file too long'.

## T1023 (medium)

**CassoEmuCore/Debugger/Handlers/MemoryHandlers.cpp:516**, partial-write

LoadBinary's comment promises that a load which would stop short is refused before anything is written. The pre-check covers only file length and the $FFFF limit, not ROM or I/O, so TryPokeRange can fail partway and leave memory half-loaded. In a multi-segment image, earlier segments stay written, the error's 'N of M bytes' counts only the failing segment, and NoteBinaryLoad is skipped.

Failure: BLOAD prog.bin 8000, where prog.bin is $4800 bytes, passes the room check because $8000-$FFFF holds it. $8000-$BFFF are written, then the load fails at $C000 with '$C000 cannot be written. 16384 of 18432 bytes were written.' The user's memory below $C000 is already overwritten. An Intel HEX file whose second record targets ROM leaves its first record loaded, and the error message misstates the total written.

Verified: The pre-check in LoadBinary (MemoryHandlers.cpp:482-501) tests only file length and room up to $FFFF. TryPokeRange (:863-877) writes byte by byte and stops at the first address TryPoke rejects. DebugMemoryView::TryPoke (DebugMemoryView.cpp:170-182) returns false for I/O and for ROM that is not LC RAM. Earlier bytes and segments therefore stay written. The error's 'i of N' counts only the failing segment's span, and the early return at :518 skips NoteBinaryLoad.

## T1024 (medium)

**CassoEmuCore/Debugger/Handlers/MemoryHandlers.cpp:247**, address-wrap

Move never checks that the destination plus the source length stays at or below $FFFF. TryPokeRange casts (Word)(first + i), so the write wraps silently into zero page. Enter and Patch do refuse values that run past $FFFF.

Failure: With language-card RAM write-enabled, M FFF0 1000:101F, or a memory-pane drag-move built by DebuggerActions::GetMoveMemory, writes $FFF0-$FFFF and then $0000-$000F. Zero page and the 6502 vectors are corrupted, and the reply says 'Moved 32 bytes from $1000 to $FFF0.'

Verified: Move (MemoryHandlers.cpp:219-251) never checks a3 + size - 1 <= $FFFF, while Enter (:142) and Patch (:181) do. TryPokeRange computes (Word)(first + i) (:867), so the address wraps to $0000. DebugMemoryView::TryPoke (:174-176) accepts LC bank addresses while LC RAM is selected, so $FFF0-$FFFF succeed and the write carries on into zero page. GetMoveMemory (DebuggerActions.cpp:803-811) adds no check either.

## T1025 (medium)

**CassoEmuCore/Debugger/Reverse/HistoryThumbnails.cpp:1134**, cache-coherence

Thumbnails, base copies, previews and points are keyed by position alone and cleared only when recording restarts. When history is cut and re-recorded (OnMachineChanged in the past, HandleDivergence), new keyframes at the same positions are treated as already drawn (HasPicture/IsWanted), so the strip keeps showing pictures of states that no longer exist. KeyframeUnpacker already guards this case with position plus checksum; the image caches do not.

Failure: Click a strip cell, which seeks exactly to a keyframe at position P. Poke screen memory and accept the divergence. OnMachineChanged drops keyframes from P on and takes a boundary keyframe at P with the edited screen, yet the cell at P still shows the pre-edit picture. Running on, later keyframes land on the same positions (same code path, same cycles), so every previously drawn cell after P stays stale and its preview shows the old screen.

Verified: HasPicture/IsWanted (HistoryThumbnails.cpp:1132-1153) and ResolveShown (lines 201-216) look pictures up by position only (m_thumbs and m_bases). The caches are cleared only by HistoryThumbnails::Clear (lines 552-575), which is called only at ShellDebuggerReverse.cpp:95 when recording starts, and by a cell-size change, which clears only m_thumbs (line 130). ForgetPoint (lines 1048-1058) clears only m_wantedPreview, and HistoryThumbnails registers no drop listener. On an edit in the past, OnMachineEdited -> OnMachineChanged (ReverseController.cpp:316-336) runs DropAfterPosition(P-1) and then CaptureBoundary at the same position P. Cells take their positions from keyframes (PlanCells, lines 645-653), so the cell at P keeps its old picture. ScheduleAfter aligns later captures to cycle multiples (KeyframeStore.cpp:1857), so when the code path does not change, later keyframes land on the same positions and stay stale.

## T1026 (medium)

**CassoEmuCore/Debugger/Reverse/KeyframeStore.cpp:126**, memory-safety

Relayout drops oldest groups only while TryDropOldestGroup succeeds, then copies m_storedBytes into the new arena without checking that it now fits. The newest group can never be dropped, and its side bytes (heat-map deltas) are not bounded by GetLeastArena, which only accounts for packed snapshots. A shrinking ChangeBudget can therefore memcpy past the end of the new buffer.

Failure: Use a large budget (e.g. 1 GB) with the heat map on, while a mostly idle program (e.g. a read-only scan loop) keeps diffs tiny so the open group grows toward longestGroup (1800 keyframes), each with tens of KB of side bytes, so the newest group holds about 90 MB. Lower the budget to 16 MB. ComputeLayout gives an arena of about max(16 MB - table, ~14 MB). The drop loop stops when only the newest group remains with m_storedBytes > arenaBytes, and the copy loop writes about 75 MB past the new heap block: heap corruption or crash. (Separately, if the new[] at line 131 fails, the groups dropped just before are already gone and listeners notified.)

Verified: In Relayout (KeyframeStore.cpp:126-149) the drop loop stops as soon as TryDropOldestGroup returns false, and that function never drops the newest group (KeyframeStore.cpp:1692-1697). The copy loop then memcpy's all m_count entries (m_storedBytes in total) into an arena of arenaBytes with no fit check. ComputeLayout (KeyframeStore.cpp:482-498) floors the arena at GetLeastArena (KeyframeStore.cpp:564-574), which counts only (wholeEvery+6) packed snapshots. Side bytes are not bounded: IsGroupDone (KeyframeStore.cpp:535-546) closes a group on packed diff bytes or on longestGroup=1800, never on side bytes. Collect stores side bytes in storedBytes (KeyframeStore.cpp:1001), and they fit in the old large arena. HeatHistory::OnKeyframeAdding (HeatHistory.cpp:214-273) adds a packed count delta and possibly kept bits to every keyframe. The budget can go down to 1 MB (DebuggerSettings.h:29, ReverseHost.cpp:317), and then the arena is GetLeastArena alone. A newest group holding more side bytes than that overflows the new heap block. The side note also holds: dropped groups have already called NotifyDrop before new[] can fail at line 131.

## T1027 (medium)

**CassoEmuCore/Debugger/Reverse/Replayer.cpp:579**, stale-index

Replayer caches m_nextKeyframe as a store index, and LoadBoundaryIfDue/CheckKeyframe compare GetInfo(m_nextKeyframe).position with the current position. KeyframeStore::ChangeBudget -> Relayout -> TryDropOldestGroup pops the oldest entries and shifts every index, but the cursor is never adjusted. ShellDebugger::ApplyReverseOptions calls ChangeBudget while the machine may be behind live.

Failure: Pause in history, lower the history budget in Tools > Options so that N old keyframes are dropped, then run forward from the past (the ReplayHere -> PrepareStepHere path). m_nextKeyframe now points N keyframes too far ahead, so keyframes reached in between are neither checked nor, if boundary keyframes (disk insert, debugger edit), loaded. The forward run misses the disk change and diverges. The next keyframe that does match the stale index then fails its checksum, and HandleDivergence cuts and drops the whole recorded future.

Verified: Replayer::m_nextKeyframe is a raw store index (Replayer.h:160), set only by FindNextKeyframe on a restore (Replayer.cpp:68, 99, 658-670) and advanced in LoadBoundaryIfDue/CheckKeyframe (Replayer.cpp:579-596, 619-640). ShellDebugger::ApplyReverseOptions (ShellDebuggerReverse.cpp:172) calls ChangeBudget without making the machine live or re-syncing the replayer. ChangeBudget -> Relayout (KeyframeStore.cpp:126-129) -> TryDropOldestGroup -> PopOldest (KeyframeStore.cpp:1747-1756) advances m_first and decrements m_count, which shifts every index. The only notification is NotifyDrop to the drop listeners (HeatHistory, CallStackHistory, DebugEventLogs), and the Replayer is not among them. ReplayHere looks up a stretch only when the position leaves the cached m_hereStart/m_hereEnd range, and it calls RestoreKeyframe only at the end of a gap (ReverseController.cpp:2003-2020). Seek's isHere path likewise skips RestoreAtOrBefore (lines 905-925). So PrepareStepHere/RunTo compare positions against a keyframe N slots too far ahead, and the boundary keyframes in between are never loaded. Recorded media inputs are not redone (Replayer.cpp:497), so a skipped disk boundary diverges the machine, and HandleDivergence then cuts history.

## T1028 (medium)

**CassoEmuCore/Debugger/RomSymbols.cpp:65**, data-correctness

Several built-in symbols name the wrong address, so the disassembler and hover descriptions are wrong and contradict each other. The annunciator SET/CLR names are swapped ($C058 is CLRAN0 / AN0 off, $C059 is SETAN0). FADD=$E7C1 and FSUB=$E7AA are the ARG-based FADDT/FSUBT entries (memory versions: $E7BE/$E7A7), yet the descriptions say 'FAC = memory + FAC'. IBAKVER and IVERSION are one byte high (ProDOS global page: $BFFC IBAKVER, $BFFD IVERSION).

Failure: $C05E carries both SETAN3, described 'Annunciator 3 on (IIe: double hi-res off)', and DHIRESON, described 'Double hi-res on'. A program's `STA $C05E` that turns double hi-res ON can be labeled and described as turning it off. `JSR FADD` resolves to $E7C1, so a breakpoint on FADD never fires for code that calls $E7BE. The same table is in SymbolDescriptions.cpp lines 174-181.

Verified: RomSymbols.cpp has SETAN0=$C058, CLRAN0=$C059 ... SETAN3=$C05E, CLRAN3=$C05F. The emulator itself contradicts this. AppleSoftSwitchBank.cpp AccessAnnunciator says '$C058 + 2n turns annunciator n off and $C059 + 2n turns it on', and the code clears the bit on an even offset. Apple2eSoftSwitchBank.cpp ~743 sets m_doubleHiRes = true on $C05E. So $C05E carries both SETAN3, which SymbolDescriptions.cpp:180 describes as 'Annunciator 3 on (IIe: double hi-res off)', and DHIRESON, described at :262 as 'Double hi-res on'. FADD=$E7C1 and FSUB=$E7AA are Applesoft's FADDT and FSUBT (ARG + FAC). The memory entries are $E7BE and $E7A7, yet SymbolDescriptions.cpp:313-314 says 'FAC = memory + FAC'. The table also mixes conventions: FMULT=$E97F and FDIV=$EA66 are the memory versions. In the ProDOS global page, IBAKVER is $BFFC and IVERSION is $BFFD, but the table has $BFFD and $BFFE. That also collides with KBAKVER at $BFFE.

## T1029 (medium)

**CassoEmuCore/Debugger/RoutineProfile.cpp:108**, correctness

OnInterrupt overwrites m_pending without checking whether a JSR or BRK call is already pending, so the routine that call enters is never begun.

Failure: JSR $2000 runs, and an IRQ is due before $2000's first instruction. ExecutionHandlers::OnInterrupt first calls BillProfile, whose Record of the JSR sets m_pending = {Call, L}, then calls m_routines.OnInterrupt(), which replaces it with {Interrupt, L-2}. The handler gets its Interrupt node and is popped at RTI, but no node is ever begun for $2000. All of that routine's cycles go to the caller's node, its call count is missing from PROFILE's routines and call tree, and calls it makes appear as children of the caller. This happens on every IRQ or NMI that lands right after a JSR, and on an NMI right after a BRK. Fix: keep the pending call (a small stack of pendings) and begin it under the interrupt's frame.

Verified: I traced it through. RunStopHook.cpp:224 checks IsInterruptDue() before every instruction, and nothing exempts the instruction a JSR has just jumped to, so an IRQ can be taken in place of $2000's first instruction. DebugSession::OnInterrupt (DebugSession.cpp:1244) then calls ExecutionHandlers::OnInterrupt (ExecutionHandlers.cpp:92-100). That calls BillProfile first, which runs RoutineProfile::Record for the pending JSR. Record sets m_pending = {Call, spAfter+2} and m_lastSp = spAfter (RoutineProfile.cpp:82-91). Next, m_routines.OnInterrupt() at RoutineProfile.cpp:108 assigns m_pending = {Interrupt, m_lastSp} unconditionally, so the Call pending is lost. BillProfile also reset m_profilePending (line 588), so the handler's first Record calls Begin only for the Interrupt node. At RTI, sp returns to spAfter, so HasReturned(depth==0) pops the interrupt frame. No Begin ever runs for $2000: its cycles go to the caller's node (Record line 75-77), and its RTS (sp = spAfter+2) does not end the caller. Nothing guards against a pending Call or Brk being overwritten.

## T1030 (medium)

**CassoEmuCore/Machines/Apple2/Common/ProDosVolume.cpp:2454**, correctness

BuildRemovalPlan appends the target directory's own entry ('itself') without setting isLocked from that directory's record in its parent. hasLockedEntries therefore never reflects the top directory's destroy-enable bit, and RemoveDirectory's 'locked entry anywhere stops it unless force' check misses the directory actually being removed. RemoveEmptyDirectory checks no access bit either.

Failure: Take a subdirectory whose record has destroy-enable clear (the code notes Merlin's own ProDOS disk ships its subdirectories that way) and that is empty or holds only unlocked files. `disk rmdir /VOL/LIB --yes` (with --recurse if non-empty) removes it without --force. By contrast, `disk delete` on a locked file is refused with ERROR_ACCESS_DENIED.

Verified: BuildRemovalPlan (ProDosVolume.cpp:2448-2459) builds `itself` with only path, isDirectory and blocks set; isLocked keeps its default of false (VolumeTypes.h:170) and never reaches hasLockedEntries. CollectRemovalEntries does gate NESTED subdirectories on destroy-enable (2392-2394, 2411), so the top directory is the only one left out. RemoveDirectory (2596-2597) checks only `force || !plan.hasLockedEntries`. RemoveEmptyDirectory (2479-2554) checks emptiness and existence, never the access byte. The CLI path, DiskCommandRunner::RunRmdir (768-853), adds no lock check of its own: it requires --recurse when non-empty and confirmation, then calls RemoveDirectory with options.disk.force. DeleteEntry, by contrast, refuses on destroy-enable (2271, 2280-2282), and its comment at 2275 confirms Merlin's subdirectories have destroy-enable clear. So an empty locked subdirectory is removed without --force.

## T1031 (medium)

**CassoEmuCore/Machines/Apple2/Common/ProDosVolume.cpp:1961**, correctness

AddFileWithCase and CreateDirectory used to refuse with ERROR_DISK_FULL when TryFindFreeDirectorySlot found no free record. They now always grow the directory through LinkDirectoryBlock, and that includes the volume directory (dirKeyBlock == kDirKeyBlock). The ProDOS volume directory has a fixed size: blocks 2-5, 51 entries, and it cannot be extended (ProDOS 8 returns $49 'directory full' for it). LinkDirectoryBlock's own comment treats the volume directory as growable.

Failure: Take a 140K ProDOS image whose volume directory holds 51 active entries and run `disk put` (or mkdir) for one more top-level file. The grow branch allocates block N, rewrites block 5's next pointer to N and puts the 52nd record there. The result is a volume directory longer than the format allows, which other ProDOS tools and stricter readers treat as non-standard or damaged. Before this branch the same command failed cleanly with 'disk full'.

Verified: At the merge base, ProDosVolume.cpp:1296-1297 refused with `CBREx (slotOk, HRESULT_FROM_WIN32 (ERROR_DISK_FULL))`. Now AddFileWithCase (1931, 1950, 1959-1969) and CreateDirectory (1736, 1741, 1747-1757) treat !slotOk as a reason to allocate one more block and call LinkDirectoryBlock. Neither function checks whether dirKeyBlock == kDirKeyBlock. Write (2072-2088) resolves a one-name path to the volume directory (comment at 2070-2071) and passes it straight through. LinkDirectoryBlock (1580-1595) walks to the end of the chain, which is block 5 of the fixed 2-5 volume directory, and writes the new block into its next pointer. Its comment (1558-1561) says the volume directory 'needs neither' parent update, so it treats that directory as growable. HandBackVerifiedResult/IsSafeToCommitAfter (VolumeIntegrityReport.cpp:225-239) only checks cross-links, claimed-but-free blocks, leaks, broken chains and loss of the catalog, so a longer volume directory passes. The tree's own test says the opposite of the new behavior: ProDosVolumeTests.cpp:2018-2020 and 2052 state that 'the volume directory cannot grow'. No guard was found.

## T1032 (medium)

**CassoEmuCore/Seams/Win32FolderWatcher.cpp:218**, lifetime

The callback is copied under the lock and run after the lock is released. Unwatch (and Stop) do not wait for a callback that is already running, so the object the callback captured can be destroyed while it runs.

Failure: A watched folder changes just as a Cassque window closes. The service thread has copied FolderWatch's lambda (it captures `this`) and is inside FolderWatch::Record. Meanwhile the UI thread runs FolderWatch::StopAll -> Unwatch, which returns at once, and the FolderWatch is destroyed. Record then locks a destroyed m_lock and calls m_onChanged on a freed object (use-after-free). The watcher itself (owned by CassqueShell) outlives the window, so nothing joins the thread first.

Verified: Win32FolderWatcher.cpp:204-216 copies the callback under the lock, and line 220 calls it after the lock is released. Neither Unwatch (lines 87-106) nor any other path waits for a callback already running. The callback is the lambda at FolderWatch.cpp:132, which captures `this` and calls Record. Record (lines 151-162) locks m_lock and calls m_onChanged. m_onChanged is the lambda at CassqueWindow.cpp:562, which captures the CassqueWindow `this` and calls PostMessageW(GetHwnd()). FolderWatch is owned by CassqueWindow through unique_ptr (CassqueWindow.h:392). Its destructor runs StopAll -> Unwatch, which returns at once. The Win32FolderWatcher is a member of CassqueShell (CassqueShell.h:109), so it outlives the window and its thread is not joined first. A callback in flight therefore touches a destroyed FolderWatch and CassqueWindow.

## T1033 (medium)

**CassoEmuCore/Shell/Components/ShellInput.cpp:277**, deadlock

The UI thread takes HostInputGate's shared_mutex shared twice. HandleViewportKey keeps `gate` from TryEnter (taken even when it returns false) for the whole function. It then calls RouteKeyToGamePort -> ApplyAppleModifierKeys / UpdateJoystick* -> GamePortInputMixer::Submit -> ScheduleApply. On the UI apply thread that runs FlushPending -> MachineGamePortSink::TryApply inline, which calls m_inputGate->TryEnter, a blocking lock_shared on the same mutex. ReleaseInputsLetGoBehindLive (line 142) does the same: the mouse release (VK_LBUTTON sorts first) keeps `gate`, and each later key release goes into HandleViewportKey. A recursive shared acquire is UB for std::shared_mutex, and on SRWLOCK it waits behind any queued exclusive waiter.

Failure: On a //e, the user presses Shift, Alt or an arrow key in joystick mode. At the same moment the CPU thread runs ReverseHost::Execute -> HostInputGate::Hold(), or Release() when a replay reaches live. That can be a reverse step from the debugger window or the debug pipe, or a reverse continue ending while the user types behind live. Hold/Release waits for the UI's first shared hold, and the UI's second lock_shared queues behind that exclusive waiter. Both threads hang. A related problem: in ReleaseInputsLetGoBehindLive, HandleViewportKey try-locks the lifetime lock shared a second time. That fails while a machine switch waits on it, so the release is dropped, and an Open Apple or joystick contribution stays stuck in the mixer across the rebuild.

Verified: ShellInput.cpp:257/277: HandleViewportKey holds the shared lock `gate` from HostInputGate::TryEnter for the whole function. TryEnter takes the lock unconditionally (HostInputGate.cpp:20) and returns !m_isHeld, so the lock is held even on false. With the gate still held, the function calls RouteKeyToGamePort at line 300 (the hold path) and line 366. That leads to m_gamePortMixer.Submit (for example ApplyAppleModifierKeys, line 190) -> ScheduleApply (GamePortInputMixer.cpp:314-336), which calls FlushPending inline when the caller is the apply thread. ShellInput.cpp:2719 sets the apply thread to the UI thread. FlushPending calls sink->TryApply (line 243), and MachineGamePortSink::TryApply (MachineGamePortSink.cpp:85) calls m_inputGate->TryEnter(gate). That is a second blocking lock_shared on the same mutex: the gate is wired at ShellInput.cpp:2710. ReverseHost::Execute:183 calls Hold() (unique_lock, HostInputGate.cpp:37) on the CPU thread; SyncInputGate calls Release(). A recursive shared lock is undefined for std::shared_mutex. On SRWLOCK, the second shared acquire queues behind the waiting exclusive locker, which in turn waits for the UI thread's first shared hold, so both threads deadlock. ReleaseInputsLetGoBehindLive:137-145 has the same nesting when the mouse release comes first and later key releases go into HandleViewportKey.

## T1034 (medium)

**CassoEmuCore/Shell/ProfileHistory.cpp:248**, correctness

GetBounds joins neighboring stretches whenever there is no gap between them, including across boundary keyframes that are not gaps. ReverseController::OnMachineChanged creates those boundaries after an external change such as a memory or register edit or a disk swap. GetBlocks splits only at 50M-instruction grid turnovers, and the scratch Replayer uses an empty keyframe store that loads no boundary, so a block is replayed straight through the change without applying it.

Failure: With HISTORY on, poke memory or change a register from the debugger (or swap a disk), let the machine run, then build a history profile over that period. OnMachineChanged marks the new keyframe with isBoundary but not hasGapBefore, so the stretches on both sides are joined into one block. The worker loads only the block's first keyframe and replays the original inputs past the boundary, so everything after the edit is counted from a timeline that never had the edit. There is no end-cycle check on a multi-stretch block (endCycle is 0), so nothing reports the divergence. After a disk swap, IsSavedWith fails for the whole joined block and up to 50M instructions are dropped as unavailable, including the part after the swap that matches the current disks.

Verified: Live debugger edits set m_isEditPending, and ReverseController.cpp:2099-2104 then calls OnMachineChanged -> CaptureBoundary(isAfterGap=false) -> MarkNewestBoundary (lines 1893-1899), which sets isBoundary but not hasGapBefore (KeyframeStore.h:207). DebugEventHistory::GetStretches sets hasGapAfter only from next.hasGapBefore, so ProfileHistory::GetBounds (ProfileHistory.cpp:248-252) joins stretches across a boundary. GetBlocks (313-331) cuts only where the 50M grid turns over (kBlockPositions, ProfileHistory.h:45). The scratch Replayer is built over m_noKeyframes (ScratchReplayMachine.cpp:105), so Replayer::LoadBoundaryIfDue (Replayer.cpp:578-600) never finds a keyframe to load. AddStretch gives endCycle 0 whenever the end is not the start keyframe's immediate successor (DebugEventHistory.cpp AddStretch), so ScratchEventReplayer.h:311 skips the cycle check. RunStretch's IsSavedWith check (line 281) runs once, on the block's first keyframe.

## T1035 (medium)

**CassoEmuCore/Shell/TraceHistory.cpp:253**, correctness

TryGetEntries can ask for a range that covers more stretches than the LRU holds (kRebuiltStretches = 8). Store evicts the extra stretches as it adds them, so outEntries.size() never equals to - from and the call never succeeds; meanwhile every stretch is replayed again on each call.

Failure: With HISTORY on and about 1.5 s or more of history (more than 8 keyframe stretches of roughly 45k instructions each), run TRACE SAVE while the machine is stopped. TraceHandlers::Save asks for up to kMaxSaved = 1,000,000 entries; AddEntries calls TryGetEntries(from, split) across about 22 stretches. Rebuild replays all of them but keeps only the last 8, isReady comes back false, and the file is written with placeholder rows marked 'rebuilding' or unavailable instead of history. While the machine runs, each Pump stores a few stretches and evicts others, so 'missing' never equals m_pending: RequestAhead submits a new job on every call and the background thread replays the same stretches indefinitely.

Verified: TraceHistory.h:40 kRebuiltStretches = 8. Rebuild (TraceHistory.cpp:454-457) stores results newest first, Store does push_front and then pop_back past 8 (lines 535-540), so after a rebuild of more than 8 stretches only the 8 oldest survive. The loop at 235-251 then finds only those, outEntries.size() != to - from (line 253), isReady is false and the entries are cleared. TraceHandlers::Save asks for up to kMaxSaved = 1,000,000 (TraceHandlers.h:31, .cpp:244-249). The default interval is 10 frames (DebuggerSettings.h:35), about 170k cycles per stretch, so the request spans well over 8 stretches. TraceController::AddEntries (TraceController.cpp:329-350) then writes placeholder rebuilding rows. While running, RequestAhead bails only when stretches == m_pending (line 483), and every Pump changes which stretches are missing, so a new job goes out on each call.

## T1036 (medium)

**CassoEmuCore/Ui/Debugger/BreakpointDialog.cpp:776**, correctness

GetFields shows a BASIC line breakpoint at line 0 as an empty Line number field, but 0 is a valid line (TryParseLine accepts 0 to 63999). Editing such a breakpoint therefore opens with a blank field, and OK builds "BPBASIC ", which the validator rejects.

Failure: The user sets BPBASIC 0 and chooses Edit... in the Breakpoints pane to add a condition. The Line number field is empty. Pressing OK shows "The line number must be 0 to 63999." and the dialog will not close until the user types 0 again. Pressing Cancel keeps the old definition.

Verified: BreakpointDialog.cpp:776 sets fields.address to an empty string when info.address == 0 for Type::BasicLine. AddBasicLine (BreakpointTable.cpp:444-453) stores the line in first/address, and MakeInfo copies it, so BPBASIC 0 has address 0. TryParseLine (BasicHandlers.cpp:43-57) accepts "0" and rejects an empty string. MakeEditedDefinition gives "BPBASIC " (line 730), and Confirm (lines 1337-1353) keeps the dialog open with the validator's reason. BreakpointDefinitionCheck.cpp:107 supplies "The line number must be 0 to 63999."

## T1037 (medium)

**CassoEmuCore/Ui/Debugger/CommandCompletion.cpp:235**, bounds

TrySearchHistory continues from m_searchIndex while the box still holds m_lastSearch, without checking that the index is still within the history passed in. ConsoleHistory::SetLines can replace the history with a shorter one between presses.

Failure: The user searches history and the box shows a match at index 150 of 200. A project switch then runs DebuggerWindowProject (line 211), which calls SetLines with the new project's 10 saved lines and leaves the box text alone. The next search press sees text == m_lastSearch, starts at i = 150 and reads history[149] in a 10-element vector: an out-of-bounds read, or a crash or assert.

Verified: CommandCompletion.cpp:229-237 resets m_searchIndex only when the text differs from m_lastSearch; otherwise the loop reads history[i-1] from the old index with no bounds check. ConsolePanes.cpp:381 passes m_history.GetLines() fresh on each F8. ReloadProjectState -> RestoreWindowState(true) (DebuggerWindowProject.cpp:165, 211) calls ConsoleHistory::SetLines, which replaces m_lines (ConsoleHistory.cpp:46-55). Neither path clears the command box or resets the completion's search state. The box is cleared only after a command runs (ConsolePanes.cpp:334), so a project opened from the UI leaves the matched line in the box, and the next F8 indexes past the end of the shorter vector.

## T1038 (medium)

**CassoEmuCore/Ui/Debugger/DebuggerViewState.cpp:1976**, error-path

An edit to a watch's expression runs ClearWatch and then AddWatch as separate commands, and GetWatchError approves anything that starts with 'BASIC ' without checking it (line 1906). When the session then rejects the new text, the original watch has already been removed.

Failure: The user types `BASIC 9X` (or any text that is not a valid Applesoft reference) over an existing watch's expression. GetWatchError returns empty and the edit runs: ClearWatch succeeds, then TryGetBasicName fails TryParseReference, Parse fails, and AddWatch replies with an error. The watch is gone. The WatchUndo was built with restoreText and waits for a moved-to id that never appears, so GetWatchUndoActions returns nullopt forever and Undo cannot bring the watch back.

Verified: GetWatchError returns empty for any text that starts with 'BASIC ' (DebuggerViewState.cpp:1904-1907). WatchesPane.cpp:470-478 relies on that check to keep the old watch ('the watch it would replace remains'), then runs the actions without checking each result: ClearWatch, then AddWatch (lines 508-519; the actions come from DebuggerViewState.cpp:1976). In WatchHandlers::Add the typed path calls TryResolveTyped. For 'BASIC 9X', TryGetBasicName fails because TryParseReference requires a leading letter (ApplesoftProgram.cpp:326). The text is not a symbol or a register, and the hex test and the isName test both fail on the space. DebugExpressionEvaluator::Parse then fails, and Add replies 'invalid arguments' (WatchHandlers.cpp:190-193). The watch is already cleared. The recorded undo has restoreText and movedFromIds, and NoteMovedWatch never finds a new id, so GetWatchUndoActions returns nullopt (lines 2275-2278). TryUndo then returns nullopt and leaves the step on top (WatchHistory.cpp:67-70), which also blocks every undo below it.

## T1039 (medium)

**CassoEmuCore/Ui/Debugger/DebuggerViewState.cpp:2756**, correctness

TryResolveCommand writes a word whenever a typed value exceeds $FF, even for an EnterBytes action that a byte watch or a memory auto-watch built. The undo built for that edit (GetWatchUndo, lines 2143 and 2185) puts back only the one byte.

Failure: The user edits a `.b` watch on $0300, or a memory auto-watch, and types `1234`. GetEnterByte's operand is evaluated to $1234, so isWord is true and the edit writes $0300=34 and $0301=12. Undo runs GetEnterByte($0300, old) and restores only $0300, so $0301 stays corrupted. A word watch given `12 34` writes four bytes, and its undo restores two.

Verified: TryResolveCommand (DebuggerViewState.cpp:2748-2763) sets isWord = verb==EnterWords || value > 0xFF and pushes a second byte whenever that is true, including for EnterBytes. The value edit of a byte watch (IsByteWatch, line 1979) and of an M: auto-watch (line 1999) both use GetEnterByte(address, text), whose operand is the typed text (DebuggerActions.cpp:1439-1449). The editor allows 64 characters (WatchesPane.h:75), and nothing limits the value to a byte. GetWatchUndo builds the undo as GetEnterByte(address, (Byte) value), which restores one byte (lines 2143, 2185; DebuggerActions.cpp:1461-1472). Typing 1234 therefore writes two bytes and the undo restores one. A word watch given '12 34' writes two words (four bytes), and GetEnterWord(address, value) restores two.

## T1040 (medium)

**CassoEmuCore/Ui/Debugger/DebuggerWindow.h:789**, event-routing

IDebuggerPaneHost::GetFocusedControl returns m_focusMgr.GetFocusedControl() (the main window's focus) rather than GetFocused(), which honors a floating window's own focus. Panes that ask the host for the focused control therefore never see a control in a floating window.

Failure: Memory 2 floats and its hex view has the focus; the user presses Ctrl+F. OnKey routes the key through RouteBoxKey (inMemory is true) to OnMappedCommand under the routing scope. MemoryWindowGroup::OnMappedAction ignores its focused argument and calls GetFocused() → m_host.GetFocusedControl(), which is the main window's control, so it returns false. OnMappedCommand then calls m_finds->Open(GetTarget()). GetTarget resolves the memory pane, which has no find view, and falls back to kConsole: the console's find bar opens instead of the memory search. F3 and Shift+F3 fail the same way. EventLogControllers.cpp:654 and MemoryWindows.cpp:1097 make focus decisions from the same wrong control.

Verified: DebuggerWindow.h:789: GetFocusedControl() returns m_focusMgr.GetFocusedControl(), not GetFocused(). MemoryWindowGroup::GetFocused (MemoryWindows.cpp:388-399) compares against m_host.GetFocusedControl(). OnMappedAction (MemoryWindows.cpp:1371) ignores its focused argument. Ctrl+F in a floating hex view: OnKey runs under RouteScope. RouteBoxKey sees inMemory=true, because TakesTyping(GetFocused()) uses the routed focus. DoesBoxKeepKey returns false for ctrl (DebuggerKeySchemes.cpp:294), so RouteMappedKey leads to OnMappedCommand. MemoryWindowGroup::GetFocused returns nullptr, because the main focus cannot be the floated view: OnFloatsSynced moves it to a routable control. Execution then reaches m_finds->Open(GetTarget()). GetTarget takes the routing pane (the memory pane), GetFindView returns nullptr for it (IDebuggerPane.h:396 default; only Console and SourceDocument override it), and it falls back to kConsole. RouteEditKey (MemoryWindows.cpp:1331-1335) has the same wrong-focus check.

## T1041 (medium)

**CassoEmuCore/Ui/Debugger/DebuggerWindowProject.cpp:422**, persistence

Saved column widths are converted to pixels with m_scaler before the window has any DPI. OnCreate → ConfigureWidgets → RestoreWindowState(false) → ApplyColumnWidths runs before the first Layout, so m_scaler is still at its default of 96 DPI. SaveColumnWidths divides by the real DPI, and DxuiListView never rescales m_overrideWPx.

Failure: On a 150% monitor, a column saved as 120 DIP is restored as 120 px, i.e. 80 DIP: two-thirds of its width. When the user next resizes any column, the OnColumnResized lambda calls SaveColumnWidths. That re-saves every list in m_columnWidths at its current, already shrunk width (80 DIP). The next launch shrinks it to 53 DIP, and so on: the restored widths decay geometrically across sessions. A project switch (ReloadProjectState) restores correctly only because by then m_scaler is set.

Verified: DebuggerWindow's m_scaler is assigned only in Layout (DebuggerWindow.cpp:888). DxuiWindow::Create calls OnCreate() before SetContentRootRef, which drives the first layout (DxuiWindow.cpp:100-104). OnCreate calls ConfigureWidgets, which calls RestoreWindowState(false) (DebuggerWindow.cpp:301), which calls ApplyColumnWidths, which calls list->SetColumnOverrideWidthPx(c, m_scaler.ToPx(dip)) (DebuggerWindowProject.cpp:422) while m_dpi is still kBaseDpi (96). DxuiListView never rescales m_overrideWPx: Layout only does m_scaler.SetDpi (DxuiListView.cpp:3932), and the override is used as raw px (line 1349-1351). SaveColumnWidths divides by the real DPI (DebuggerWindowProject.cpp:380) and re-saves every pane in m_columnWidths. That is a 2/3 shrink per session at 150%.

## T1042 (medium)

**CassoEmuCore/Ui/Debugger/FloatingPanes.cpp:374**, window-lifetime

Sync decides whether a floating window should show from IsWindowVisible(owner) alone and calls Show(false) whenever the two disagree. A minimized debugger still has WS_VISIBLE, while Windows hides its owned floating windows on minimize. Every frame then re-shows them with SW_SHOWNA.

Failure: With panes floating, the user minimizes the debugger window. Windows hides the owned DxuiDockedWindows. On the next RenderFrame, visible is true (IsWindowVisible of an iconic window), IsWindowVisible(float) is false and shown is true, so entry.second->Show(false) puts the floating panes back on the desktop while the debugger stays minimized. SyncTimeline guards this case with !IsIconic, but Sync does not.

Verified: Sync (FloatingPanes.cpp:330) sets visible from IsWindowVisible(owner) only. The floating windows are owned by the debugger (params.ownerHwnd = GetFloatOwner(), FloatingPanes.cpp:1057). Sync runs on every UI frame, whatever the debugger's state: ShellRenderer.cpp:734 calls RenderWindowFrame, which calls RenderFrame, which calls m_floating->Sync() (DebuggerWindow.cpp:1107). RenderFrame has no IsIconic early-out. A minimized owner keeps WS_VISIBLE while Windows hides its owned windows, so the loop at lines 371-385 sees shown=true and the float not visible, and calls Show(false), which is ShowWindow(SW_SHOWNA) (DxuiWindow.cpp:181). By contrast, SyncTimeline guards with !IsIconic (DebuggerWindowTimeline.cpp:192). The Windows owned-window minimize behavior is external, but it is standard.

## T1043 (medium)

**CassoEmuCore/Ui/Debugger/FloatingPanes.cpp:1128**, event-routing

A key-scheme command from a floating window runs with no routing pane. SetOnMappedCommand calls m_host.RunFloatedCommand → OnMappedCommand without a RouteScope, unlike the mouse and key paths, which wrap RouteMouse/RouteKey. As a result, GetFocused(), GetPaneOfFocus() and FindWidgetSet::GetTarget() inside OnMappedCommand read the main window's focus.

Failure: Case 1: the BASIC program pane floats with its list focused, and the user presses F10/F11. DebuggerWindow::OnKey does not take the key and the list's OnKey returns false, so the floating window's own RouteMappedKey runs RunFloatedCommand. BasicProgramController::OnMappedAction then sees focused != m_list, so the machine single-steps an instruction instead of stepping the BASIC statement (FR-174). Case 2: a floating source document has the focus and the user presses Ctrl+F. Find opens on whatever pane has focus in the main window (for example the console) instead of on the document.

Verified: FloatingPanes.cpp:1128 wires SetOnMappedCommand to m_host.RunFloatedCommand, and DebuggerWindow.h:506 turns that into OnMappedCommand with no RouteScope. RouteMouse and RouteKey (FloatingPanes.cpp:1278, 1297) do set a scope. DxuiWindow.cpp:1766 runs OnKey(ev) || RouteMappedKey(ev), and the floating window has a key map (SetKeyMap(GetFloatKeyMap()), DebuggerWindow.h:500). For F10 on a floating BASIC list, DebuggerWindow::OnKey falls through: RouteBoxKey decides nothing because there is no box and TakesTyping is false, the list's OnKey has no F10 handling, and RouteDockKey takes only Shift+F10. OnKey returns false, the scope has ended, and the floating window's RouteMappedKey then calls OnMappedCommand. There, GetFocused() (DebuggerWindowInput.cpp:1143) returns m_focusMgr's control because IsRoutedToFloat() is false. BasicProgramController::OnMappedAction (BasicProgramController.cpp:347) returns false when focused != m_list, so DebuggerActions::GetForKey takes a machine step. Find works the same way: FindWidgetSet::GetTarget (FindWidgets.cpp:302) uses GetPaneOfFocus, which with no routing pane resolves from the main window's focus.

## T1044 (medium)

**CassoEmuCore/Ui/Debugger/Panes/DisassemblyViews.cpp:401**, correctness

The context menu's Insert/Remove breakpoint label reads `snapshot->code[(size_t) row]`. `snapshot->code` is view 0's lines indexed by line, but `row` is this view's list row, and source rows count among those rows. It should use `line.hasBreakpoint`, which the function has already resolved.

Failure: Turn on 'Show source code' so source rows sit between the instructions, then right-click near the bottom of Disassembly 1. The row index runs past `snapshot->code.size()`, an out-of-bounds vector read (a checked-iterator assert in Debug). The same happens in Disassembly 2-4 whenever that view is taller than view 0. In-range cases still label the wrong instruction: an instruction with a breakpoint says 'Insert breakpoint', and the toggle action then removes it.

Verified: DisassemblyViews.cpp:371 maps the list row to a code line, `index = m_group.GetCodeLineOfRow(m_view, row)`, and :384 resolves `line = m_group.GetCodeLines(m_view)[index]`, i.e. snapshot->codeViews[m_view] (:497-509). But :401 reads `snapshot->code[(size_t) row].hasBreakpoint`. `snapshot->code` is a copy of codeViews[0] (DebuggerViewState.cpp:209), and `row` is the raw list row. Rows come from DisassemblyOptions rows, where each row carries its own `codeLine` (DisassemblyOptions.cpp:321-329), so with source rows interleaved the row number runs ahead of the line number. Nothing bounds-checks `row` against snapshot->code.size(): out of range it is an OOB read, and in range it labels the wrong instruction. The `index < 0` guard at :376 checks `index`, not `row`. `line.hasBreakpoint` (CodeLine field, DebuggerViewState.h:68) is the correct value and is already in scope.

## T1045 (medium)

**CassoEmuCore/Ui/Debugger/Panes/DisassemblyViews.cpp:1190**, stale-cache

TryGetSourceText caches each file's split lines, keyed only by `debugFilePath`. Nothing clears the cache when the same path is loaded again with new content. SourcePane keys on debugFilePath + programKey (the SHA-1 of the debug file's text), and this cache does not. A lookup that found no file is also cached as an empty vector and never retried.

Failure: Edit the source, reassemble, and reload the debug file at the same path (or unload it and load it again). The Source document reloads, but the disassembly's interleaved source rows keep the old text against the new line numbers, so they show the wrong source above each instruction or vanish where the file grew. Likewise, if the source was not found at first and the user then drops or locates it, the document shows it, but the disassembly's source rows stay empty until a different debug file is loaded.

Verified: DisassemblyViews.cpp:1190-1194 clears m_sourceText only when `m_sourceKey != source->debugFilePath`, and the key leaves out programKey. When there is no snapshot->source (unload), :1179-1182 returns early and resets nothing. :1196-1206 stores the split text the first time a fileId is seen, including an empty vector when FindDebuggerSource found nothing, and `contains(fileId)` stops it from ever being looked up again. A grep of DisassemblyViews.cpp/.h finds m_sourceKey/m_sourceText written only at :1190-1206, with no other invalidation. SourcePane does key on debugFilePath + programKey (SourcePane.cpp:239-248), and SourceDocumentGroup::Apply does too (SourceDocumentPanes.cpp:389-392). So reloading a reassembled debug file at the same path, or locating a source after a failed lookup, leaves the interleaved source rows stale or empty.

## T1046 (medium)

**CassoEmuCore/Ui/Debugger/Panes/EventLogPane.h:608**, correctness

TrimRows always drops the oldest rows, including rows Apply has just prepended in answer to the pane's own "earlier" request. So past kMaxRows, scrolling up throws away the rows it fetched and moves the view forward.

Failure: The log holds S rows, with S between 150k and 200k, and the user scrolls to the top. The machine rebuilds P older rows, with S+P > 200000. Apply prepends them, then TrimFront(150000) drops S+P-150000 rows. That is more than P, so every fetched row goes, plus about 50k more. moveOn sends the top to nullopt, the view lands back on the "scrolling up rebuilds them" row, and "from X" moves heldFrom forward. The user cannot scroll back past roughly the newest 200k rows, and each try costs a history replay and jumps the view forward. Trimming should drop from the end opposite the side rows were added to.

Verified: SendOlder sets log.wantedBefore = 0 in the same pass that sends the older rows (DebugEventLogs.cpp:959-960). So the delta carrying them has isRebuilding == false (BuildDelta, DebugEventLogs.cpp:821), and EventLogRows::Apply stores that before TrimRows runs (EventLogRows.h:61; EventLogPane.h:154-155). TrimRows (EventLogPane.h:599-614) guards only on size <= kMaxRows or IsRebuilding(), and then always calls TrimFront(kRetainedRows). TrimFront erases from begin() (EventLogRows.h:70-88), which is where Apply just inserted delta.older (EventLogRows.h:51-55). For S > 150000 and S+P > 200000, dropped = S+P-150000 > P, so every fetched row goes. In Apply, moveOn(top) gives nullopt because *index+prepended < dropped (EventLogPane.h:147-151, 188-192), so no top is kept. TrimRows also resets m_requestedAt and sends "from X" with the new heldFrom (lines 609-611). FollowScroll then re-requests "earlier" for rows the pane has just dropped. Nothing trims from the newer end.

## T1047 (medium)

**CassoEmuCore/Ui/Debugger/Panes/EventLogPane.h:639**, performance

Once any column is sorted, every delta forces a full BuildView plus SortView over all rows. A text-column sort calls ProvideRow on every row, which formats every cell, including a FormatTime of several Win32 locale and time-zone calls per row. On a large log this stalls the UI thread every frame while the machine runs.

Failure: The Sound log holds 150k-200k rows and the user sorts by Event or Source while the machine runs. Speaker toggles produce a delta nearly every frame. ExtendView is skipped because m_sortColumn >= 0, so each frame runs Matches on every row, then ProvideRow on every row (GetLocaleInfoEx x2, SystemTimeToTzSpecificLocalTime, GetTimeFormatEx, FormatEvent and std::format each), then stable_sort. That is on the order of a second per frame, and the debugger becomes unresponsive until the user unsorts or pauses.

Verified: Every Apply sets m_isDirty (EventLogPane.h:193). Refresh takes the ExtendView fast path only when m_sortColumn < 0 (line 219), so while any column is sorted each dirty frame runs a full BuildView. BuildView calls Matches on every row and then SortView (lines 543-572). For a column other than Time or Cycle, SortView calls ProvideRow on every view row (lines 633-642). ProvideRow formats every cell: FormatTime, then FormatCycle, then MakeCells (lines 457-489). FormatTime calls FileTimeToSystemTime, SystemTimeToTzSpecificLocalTime and FormatClockTime, which uses the locale and GetTimeFormatEx (EventLogText.cpp:33-59, 74+). IoLogPane's MakeCells calls IoEventLog::FormatEvent and NarrowToWide (IoLogPane.cpp:338). A stable_sort over all rows follows. Nothing throttles or caches this, and IoLogPane does not override the sort. The O(rows) work on the UI thread for every frame that carries a delta is fully traced. The per-frame cost of about a second for 150k-200k rows is an estimate and was not measured.

## T1048 (medium)

**CassoEmuCore/Ui/Debugger/Panes/EventLogPane.h:181**, correctness

Apply clears m_keepTop on every delta and takes the top index from m_view as it stood before the frame. The controllers apply every delta in a frame before one Refresh, so a frame holding two deltas loses the anchor set by the first and misplaces the one it does keep.

Failure: The user scrolls to the top, so FollowScroll sends "earlier". In one UI frame, DiskLogController::ApplyLogDeltas receives delta 1 (P older rows prepended, so m_keepTop = T+P) and then delta 2 (newer rows only). Apply for delta 2 runs m_keepTop.reset() and sets nothing, because prepended == 0. Refresh then does a full build with no keepTop, so the list stays at row 0. The P prepended rows are now on screen in place of the rows the user was reading, and the kTopRow is still visible. FollowScroll sees a new heldFrom and requests "earlier" again. This repeats while new rows keep arriving, loading history one stretch per frame. If delta 2 had also prepended, GetTopIndex would read the stale m_view and the kept top would be off by delta 1's P.

Verified: One frame can apply several deltas. ServiceEventLogs runs on every CPU pass and pushes one delta per pass onto m_eventLogsPending (ShellDebugger.cpp:2630-2660). TakeDebuggerEventLogs swaps out the whole vector (ShellDebugger.cpp:1290-1305), and DiskLogController::ApplyLogDeltas, InputLogController::ApplyLogDeltas and IoLogController::ApplyLogDeltas each apply every delta before one Refresh (EventLogControllers.cpp:796-799, 1069-1072, 1413-1416). In EventLogPane::Apply (EventLogPane.h:139-193), line 186 runs m_keepTop.reset() on every call, and m_keepTop is set again only when prepended > 0 or rows were dropped while not at the tail (lines 188-192). So a second delta with newer rows only wipes the anchor the first delta set. m_isFullBuildDue is still true from the first delta, so Refresh does a full BuildView with no m_keepTop and never calls SetTopRow (lines 254-269). The list stays on view row 0, which is the kTopRow. FollowScroll (lines 274-285) then sees a new heldFrom and posts "earlier" again. GetTopIndex (lines 709-723) reads m_view, which is rebuilt only in Refresh, so on a second prepending delta the index it returns does not include the first delta's P. A delta that carries older rows is built with SendOlder (DebugEventLogs.cpp:906-963). Newer rows arriving in a later CPU pass of the same frame are the usual case while the machine runs.

## T1049 (medium)

**CassoEmuCore/Ui/Debugger/Panes/EventLogPane.h:603**, race

TrimRows checks only the pane's last-seen isRebuilding. If an "earlier" request is still in flight, the machine ignores the trim's "from X" (DropBefore does nothing while wantedBefore != 0) and later sends rows from before the old heldFrom. Those land in front of rows that now start at X, leaving a silent gap.

Failure: The log is near 200k rows and the user reaches the top, so FollowScroll sends "earlier" in frame N. In frame N+1, a delta built before the CPU thread processed that request (so isRebuilding == false) brings newer rows past kMaxRows. TrimRows drops 50k rows and sends "from X". On the CPU thread, RequestEarlier sets wantedBefore = H_old and DropBefore(X) is skipped (DebugEventLogs.cpp:252). SendOlder sends rows before H_old with heldFrom = an earlier position, and the pane prepends them. Every row in [H_old, X) is missing, with no Gap row, and the pane's heldFrom is overwritten back to the machine's value.

Verified: Requests are posted FIFO to the CPU thread (SendDebuggerEventLogRequest -> m_cpuManager.PostCommand, ShellDebugger.cpp:1273-1277). The thread handles them in SetDebugEventLog (ShellDebugger.cpp:2570-2582). RequestEarlier sets wantedBefore = heldFrom (DebugEventLogs.cpp:228-231). DropBefore does nothing while wantedBefore != 0 (DebugEventLogs.cpp:248-256). The pane's TrimRows guard reads m_rows.IsRebuilding() (EventLogPane.h:605), which comes from the last delta applied. Deltas queued after a frame's TakeDebuggerEventLogs but before the CPU handles that frame's "earlier" (posted by FollowScroll after Refresh) still say isRebuilding == false. If one of them pushes the rows past kMaxRows, the pane trims to X and sends "from X", which the machine ignores. SendOlder then sends rows from before the old heldFrom and sets log.heldFrom = from (DebugEventLogs.cpp:928-959). EventLogRows::Apply prepends them and overwrites m_heldFrom with delta.heldFrom (EventLogRows.h:51-59). Rows in [H_old, X) are gone, and no Gap row marks the hole. The timing window is narrow, but nothing closes it.

## T1050 (medium)

**CassoEmuCore/Ui/Debugger/Panes/FindWidgets.cpp:185**, persistence / UI state desync

ApplySaved, called from Create and LoadSaved, restores matchCase, wholeWord and isRegex from the project, but the toggle buttons' emphasis is never set. SetEmphasis is only called in SetOptions. The search uses the restored options while the Aa, whole-word and .* buttons all draw as off.

Failure: Turn on Match case or Regex in the console find widget; Save() persists 'C'/'R'. Restart, or switch back to the project: the console widget is created at startup (DebuggerWindow.cpp:280) and LoadSaved applies the options without emphasis. Searching for 'lda' now silently matches only lowercase text, or treats '.' as a regex wildcard, while every toggle shows off. Clicking Aa then turns the option off, the opposite of what the button suggests.

Verified: ApplySaved (FindWidgets.cpp:174-189) sets widget.matchCase, wholeWord, isRegex and history only. The only SetEmphasis calls for these toggles are in SetOptions (FindWidgets.cpp:649-651). Create calls ApplySaved (line 102) and never sets emphasis. LoadSaved (1333-1336) calls ApplySaved on existing widgets. The console widget is created at DebuggerWindow.cpp:280, before RestoreWindowState(false) at 301 calls LoadSaved (DebuggerWindowProject.cpp:213). So the saved options apply while the buttons draw as off. Clicking Aa calls SetOptions(!w.matchCase, ...) (line 131), which turns the restored option off.

## T1051 (medium)

**CassoEmuCore/Ui/Debugger/Panes/HeatRangesPane.cpp:869**, correctness

Renaming a range set updates HeatMapRangeSets::shown (TryRenameSet) but never the heat map's HeatMapOptions::ignoreSet, which names the same set by its old name.

Failure: Set 'Uninitialized reads: not in Zero page' and then rename the set 'Zero page' to 'ZP'. EndEdit calls ApplyRanges, which calls SendIgnore. GetSpans("Zero page") no longer finds the set, so 'ignore' is sent with no spans and the machine stops leaving those reads out, although the set still exists. The bar label still reads 'not in Zero page', the ignore menu checks no row, and the stale name is saved with the options.

Verified: HeatMapRangeSets::TryRenameSet (Debugger/HeatMapRangeSets.cpp:470-513) updates `shown` and set->name only. The only writer of HeatMapOptions::ignoreSet is HeatMapPane.cpp:490, from the ignore menu, and a grep finds no rename hook. HeatRangesPane::EndEdit (lines 869-885) calls ApplyRanges, which calls m_heatMap.SendIgnore (line 990). SendIgnore (HeatMapPane.cpp:515-532) calls GetSpans(old name), finds no set, gets an empty span list and sends 'ignore ' with no spans, so the reads are no longer left out. The bar label (line 754) still shows the old name.

## T1052 (medium)

**CassoEmuCore/Ui/Debugger/Panes/MemoryEditModel.cpp:546**, state-consistency

Undo and Redo move the edit to the other list and draw its bytes before the session has checked memory against `required`. When the session refuses the patch, memory is unchanged, but the model's history says the step happened.

Failure: Edit $0300 in Memory 1, scroll the window away so $0300 is held only in m_keptBytes, then POKE $0300 from the console. FindChanged compares against the stale kept byte, finds no difference, and lets Undo go ahead: the edit moves to m_redo and ShowWritten draws the old value. The session sees that memory differs from `required` and does not write. The next snapshot shows the POKEd value. Undo can no longer reach the edit, and Redo now fails FindChanged against `replaced`, so the edit is stranded and the window drew a value memory never held.

Verified: MemoryEditModel::Undo/Redo (MemoryEditModel.cpp:540-600) run FindChanged against what the model last knew, which TryGetKnown (lines 66-87) takes from the shown bytes or the stale m_keptBytes. They then pop the edit, push it onto the other list, ShowWritten it (overwriting m_bytes and m_keptBytes, lines 640-655) and SendPatch it. The session's check (DebuggerViewState.cpp:2662-2672) refuses the patch with "has changed since the edit, so nothing was written" when memory differs from `required`. Nothing reports that refusal back to the model or rolls back the move between lists: MemoryPane::UndoOrRedo (lines 450-470) only looks at the local `changed`. The history and the drawn bytes therefore show a step that never happened.

## T1053 (medium)

**CassoEmuCore/Ui/Debugger/Panes/MemoryPane.cpp:430**, performance

GetReadStartFor clamps the read start to $F000 (64K less 4096) but still requires visibleEnd <= readFirst + 4096. At the bottom of memory the visible rows can end past $10000, so no read ever satisfies the check. Apply clears m_requested whenever the read arrives, and FollowScroll then posts the same 'memory F000' command again on every Tick, forever.

Failure: Go to $FFFA to look at the vectors: phase is 10 and the top row is clamped to the last page, so the visible rows end at $1000A, past $10000. Rows that hold a count of bytes 64K does not divide (Auto columns, for example 24) do the same once the window is scrolled to the bottom. Every Tick then sends SetDebuggerMemoryWindow(id, $F000), and each one goes to the CPU thread through PostCommand(IDM_DEBUG_VIEW). Each read that comes back clears m_requested and sets off the next request, so the loop keeps going even while the machine is paused. A tall pane with wide rows, where (rows + 8) * bytesPerRow > 4096, loops the same way anywhere in memory.

Verified: GetTopAddress (MemoryPane.h:50) returns the model address, phase included. GetReadStartFor (MemoryPane.cpp:421-440) caps start at 0x10000-4096 = $F000 but still requires visibleEnd <= readFirst+4096. With phase 10 (Go to $FFFA) and the top row clamped by GetMaxTopRow (DxuiHexView.cpp:1264-1277), visibleEnd is $1000A, so the check fails and start comes back as $F000 every time. ReadMemoryWindow (DebuggerViewState.cpp:1508-1518) returns window.first = $F000. Apply (MemoryPane.cpp:209-212) then resets m_requested. MemoryWindowGroup::Tick (MemoryWindows.cpp:1011-1018) calls FollowScroll for every open window on each tick, and with m_requested empty, start != m_requested, so m_move posts again. ShellDebugger::SetDebuggerMemoryWindow (ShellDebugger.cpp:1237-1241) does PostCommand(IDM_DEBUG_VIEW, "memory F000") with no check for a repeat. Nothing breaks the cycle.

## T1054 (medium)

**CassoEmuCore/Ui/Debugger/Panes/ProfileTimeline.cpp:228**, wrong scaling

Paint scales every bar by one frame's cycles (RoutineProfile::kCyclesPerFrame) and ignores ProfileFrameEntry::count. Once RoutineProfile rebuckets (HISTORY ALL / m_isAllFrames with more than 600 frames), each entry holds count x 17030 cycles, so every bar is clamped to full height and the stack is cut off.

Failure: With more than 10 s of history, choose 'Since history start'. Frames merge into buckets of 2, 4, 8 and so on, each carrying about bucket*17030 cycles. tall = height*cycles/17030 is 2x-8x the strip, so every column is drawn full height. The first routines take the whole strip and the higher routines and the 'other' segment are clipped away, making the timeline uniform and misleading. The tip for the same bar reports the correct merged totals.

Verified: ProfileTimeline::Paint (ProfileTimeline.cpp:226-230) computes tall = height*cycles/RoutineProfile::kCyclesPerFrame and clamps it to the space left, without ever reading m_frames[i].count. RoutineProfile::Rebucket (RoutineProfile.cpp:261-280) merges frames into buckets of `bucket` frames and adds up their cycles through AddShares (296-317). ExecutionHandlers.cpp:781-783 copies count into ProfileFrameEntry. Every history profile has SetFramesCoverAll(true) (ProfileHistory.cpp:556, ProfileEventProbe.cpp:24), so any period longer than 600 frames is rebucketed, including 'Last 60 s' and not only 'Since history start'. Each bar then holds about count x one frame of cycles, so the lower routines fill the column and the rest is clipped. GetFrameTip (line 113) does use count, which matches the claim that the tip stays correct.

## T1055 (medium)

**CassoEmuCore/Ui/Debugger/Panes/TracePane.cpp:126**, virtual list window

The window of history read is fixed at kTraceRows = 128 entries, starting kLeadRows = 16 above the top row. When the pane shows more than 112 rows, GetReadStartFor never sees the rows as held and keeps returning the same start. FollowScroll suppresses it because start == m_requested, so the rows past the 112th stay blank for good.

Failure: Set text zoom to 50% (8-DIP rows) with a full-height trace pane on a 1080p screen (about 900+ DIP, over 112 rows), or use a tall 4K or portrait monitor at 100%. Scroll up off the end so the list is not at the bottom. The pane requests top-16, the CPU returns 128 entries, top+visible > first+128 still holds, and the same start is never re-sent. Every row below the 112th row on screen is blank until the user scrolls.

Verified: GetReadStartFor (TracePane.cpp:115-132) returns nullopt only when top+shown <= first+held. Otherwise it returns top-kLeadRows (kLeadRows=16, TracePane.h:89). The CPU reads exactly kTraceRows=128 entries from that start (DebuggerViewState.h:576, DebuggerViewState.cpp:1177-1178, ShellDebugger.cpp:2104-2106). When shown > 112, the check fails again and returns the same start. FollowScroll (98-102) suppresses it because start == m_requested, and ProvideRow (185-188) leaves rows past the window blank. Rows are kPaneRowDip=16 DIP scaled by text zoom (DebuggerWindow.h:394, DebuggerWindow.cpp:596), and kMinTextZoom=0.5 (DebuggerWindow.h:420). 112 rows therefore need about 896 DIP at 50% zoom or about 1792 DIP at 100%, both reachable on the displays the finding describes.

## T1058 (medium)

**Dxui/Core/DxuiPaneLayout.cpp:2079**, persistence

ToText writes an auto-hidden pane as only `hide pane edge home`. The AutoHidden fields that DockBack relies on (homeSplit, homeSide, homeRatio, homeSiblings) are never saved, and TryParse (line 2371) cannot restore them.

Failure: A pane alone on one side of a split, e.g. Registers to the left of the Memory/Stack subtree, is auto-hidden from the dock site menu. AutoHide records homeSplit=true with siblings and ratio, and the layout is saved as the debugger saves on every change. After a restart the restored entry has homeSplit=false. Choosing Dock then takes the `group->panes.push_back` branch of DockBack, so the pane comes back as a tab in Memory's group instead of the split at its old side and ratio. The same action before the restart restores the split, so behavior differs across a save and reload of the same layout.

Verified: AutoHide (DxuiPaneLayout.cpp:1267-1277) fills homeSplit, homeRatio, homeSide and homeSiblings for a pane alone in its group. ToText (line 2079) writes only `hide pane edge homePane`. TryParse (lines 2371-2384) restores pane, edge and homePane only, so homeSplit keeps its default of false (DxuiPaneLayout.h:103). After the round trip, DockBack (line 1330) finds subtree == nullptr and takes the `group->panes.push_back` branch (line 1339), which tabs the pane into homePane's group (the first sibling) instead of re-splitting at homeSide/homeRatio. The debugger saves the layout on every change (DebuggerWindowLayout.cpp:595-599) and reloads it through TryParse (DebuggerLayout.cpp:191). The dock site menu reaches AutoHide (DxuiDockSite.cpp:1545).

## T1059 (medium)

**Dxui/Widgets/DxuiDockSite.cpp:2992**, state-leak

A sash drag (m_sashDrag) ends only when the site receives an Up event. Neither OnDragMouseLost nor CancelDrag resets it, and HasPointerDrag is false during a sash drag. The window therefore does not route every event to the site: the Up passes through the status-bar, menu-bar, command-bar, edit-box and pane-bar routers first. The stored index can also outlive m_splits.

Failure: Drag the horizontal sash downward and release over the status bar or a pane toolbar that takes the Up, or lose capture with WM_CANCELMODE while dragging. m_sashDrag stays >= 0, so every later button-up Move over the site calls SetRatio and Arrange: the split follows the bare pointer until the next click inside the site, and the end position is never saved through NotifyChanged. A related variant: if a pane hides mid-drag and m_splits shrinks, the stale index can later address a different sash. Title-bar presses (m_titlePressed) have the same gap. A press released outside the site leaves the group armed, and a later button-up move past kDragDip calls m_onTitleDrag, which can tear the pane off with no button held.

Verified: Only the site's OnMouse ends a sash drag: line 3006-3010 clears m_sashDrag on an Up. CancelDrag (2321-2336) does not reset m_sashDrag. OnDragMouseLost (2372-2378) acts only when HasPointerDrag() is true, and HasPointerDrag is IsDragging() && m_pointerDrag (DxuiDockSite.h:288), where IsDragging means !m_dragPane.empty(). A sash press sets neither. DebuggerWindow::OnMouse (CassoEmuCore/Ui/Debugger/DebuggerWindowInput.cpp:758-763) sends every event to the site only while HasPointerDrag() is true. Otherwise RouteStatusBarMouse runs first (803), and it returns true for any event inside the status bar (DebuggerWindowStatusBar.cpp:339-345). RouteMenuBarMouse also runs first (810) and returns handled||over (DebuggerWindowMenuBar.cpp:509). Either one swallows the Up, so m_sashDrag stays >= 0. Later button-up Moves that reach the site then hit 2992-3005, which calls SetRatio and Arrange. The mouse-lost filter (DebuggerWindowLayout.cpp:607-609) calls only OnDragMouseLost, which does nothing here. The title-press variant also holds: DxuiTabGroup::OnMouse Move (DxuiTabGroup.cpp:1275-1291) fires m_onTitleDrag once past kDragDip whenever m_titlePressed is set, without checking the button. m_titlePressed is cleared only by an Up (1356-1360) or by CancelPress, and CancelPress runs only from CancelDrag. The stale-index variant is only bounds-guarded (2992 checks < m_splits.size()), so it can address a different sash, though I did not trace that case through.

## T1060 (medium)

**Dxui/Widgets/DxuiDockSite.cpp:2140**, correctness

DropOnStrip passes the strip's insert index straight to DxuiPaneLayout::TabWithAt. The strip index counts only the tabs on show. TabWithAt indexes the layout group, which still holds the hidden panes (m_shown false: closed panes, unopened memory windows, absent device panels). The SetOnMove handler in WireGroup handles this by anchoring on a neighboring pane; DropOnStrip does not.

Failure: Take a layout group [Memory1, Memory2(hidden), Memory3(hidden), Watch], so the strip shows [Memory1, Watch]. Drag Breakpoints onto that strip past Watch. GetInsertIndexAt returns 2, and TabWithAt(Breakpoints, Memory1, 2) inserts it into the layout before Memory2. The group then reads [Memory1, Breakpoints, Watch] rather than [Memory1, Watch, Breakpoints]. Re-ordering a pane within its own group by dragging it off the strip and back lands in the wrong slot the same way, and the wrong order is saved to the layout.

Verified: The strip holds only the panes on show. DxuiPaneLayout::ArrangeNode (Dxui/Core/DxuiPaneLayout.cpp:1642-1648) keeps a pane only when shown(pane) is true, and DxuiDockSite::Arrange (DxuiDockSite.cpp:218-236) fills ReplaceTabs from that filtered list. The index comes from that strip: m_stripIndex = group->GetInsertIndexAt(pointDip) (DxuiDockSite.cpp:2056/2066), and EndDrag passes it on as DropOnStrip(strip, index) (2677). DropOnStrip then calls m_layout.TabWithAt(pane, anchor, at) with that index unchanged (2140). TabWithAt (DxuiPaneLayout.cpp:998-1021) clamps it against group->panes.size() and inserts at that position in the full layout group, hidden panes included. Nothing translates the index, whereas the SetOnMove handler (DxuiDockSite.cpp:339-363) does, by finding a neighbor in m_layout.GetGroup. Worked example: layout [M1,M2h,M3h,W], strip [M1,W], drop index 2. The insert lands at layout index 2, giving [M1,M2,B,M3,W], so the visible order is [M1,B,W] instead of [M1,W,B]. The finding says the insert goes before Memory2; it actually goes before Memory3, but the visible result is the same. Re-ordering within the same group goes through the from==group branch with the same unconverted index.

## T1061 (medium)

**Dxui/Widgets/DxuiListView.cpp:3874**, correctness

QueryCommand enables Select all only when `!m_rows.empty()`. m_rows is always empty in provider (virtual) mode, so Select all is permanently disabled in virtual multi-select lists.

Failure: Event log panes (SetRowProvider plus SetMultiSelect(true)): focus the list and press Ctrl+A, or choose Edit > Select all. OnKeyBodyHeaderNav calls InvokeCommand(SelectAll). QueryCommand reports the command as handled but disabled, so nothing is selected and the key is still consumed. The menu item is grayed out. The condition should use GetRowCount() > 0.

Verified: DxuiListView.cpp:3872-3875: SelectAll is reported handled with outEnabled = !m_rows.empty(). SetRowProvider (210) clears m_rows, and provider mode never fills it; GetRowCount uses m_virtualCount. InvokeCommand (3899-3906) returns handled=true without calling SelectAllRows. The key path OnKeyBodyHeaderNav (5079-5081) sets handled=true, so the DebuggerWindowInput.cpp:1416-1418 fallback is skipped (it would fail the same way). The menu's Query (DebuggerWindowMenuBar.cpp:142) reports it disabled. EventLogPane.h:95-99 sets both a provider and multi-select. SelectAllRows itself (1191) uses GetRowCount() and would work.

## T1062 (medium)

**Dxui/Widgets/DxuiListView.cpp:3130**, correctness

SetColumnFiltered clears m_measuredWPx without calling RequeueFitTexts, unlike SetColumns and ResetAutoFit. A virtual list in precise auto-fit mode then re-measures its fitted columns from the headers alone, and the widths noted with NoteFitText are lost for good.

Failure: Input log or I/O log (EventLogPane: virtual rows, SetPreciseAutoFit(true), kFit columns sized only from NoteFitText): toggle an input lane or source filter so ShowSourceFilter calls SetColumnFiltered. The next Paint sees that m_measuredWPx no longer matches the column count. MeasureColumnsPx then measures the headers and the empty m_rows, and m_pendingFitTexts is empty because NoteFitText de-duplicates. The Time and label columns shrink to their header width and the values clip until the text zoom changes, unless the user has a width override.

Verified: DxuiListView.cpp:3121-3131: SetColumnFiltered clears m_measuredWPx and sets m_measureDirty, but does not call RequeueFitTexts. SetColumns (84-87) and ResetAutoFit (749-752) both call it. Paint (3006) then calls MeasureColumnsPx, which (578-633) re-measures only the headers and m_rows. m_rows is empty in provider mode (SetRowProvider 210). It then adds m_pendingFitTexts, which is empty: NoteFitText (658) queues a text only when it was not already in m_notedFitTexts. In precise mode the width is measuredPx alone (1371). EventLogPane.h:94-113 is virtual and precise, and sizes its fitted columns only through NoteFitText. InputLogController/IoLogController::ShowSourceFilter (EventLogControllers.cpp:1167/1487) runs on a lane toggle (1118/1456) with no refit after it. Only ApplyTextZoom's ResetAutoFit (DebuggerWindow.cpp:598) restores the widths.

## T1063 (medium)

**Dxui/Widgets/DxuiListView.cpp:4311**, correctness

In a multi-select list, a Ctrl+click that deselects a row reverses itself on release. DispatchMouseUp keeps the press's result only when `ranged` (more than one row still selected). Otherwise it falls through to SetSelectedRow(row), which re-selects the row just removed and drops the rest.

Failure: Breakpoints pane or an event log (both multi-select): click row 2, Ctrl+click row 4 (selection {2,4}), then Ctrl+click row 4 again to deselect it. On the press, ClickRow leaves {2}. On the release, ranged is false (size 1), so SetSelectedRow(4) runs and the selection becomes {4}, the opposite of what was asked. The same happens when Ctrl+clicking the only selected row: it cannot be deselected. Where activation is on single click, the release also fires onActivateRow for the row that was just deselected.

Verified: DxuiListView.cpp:4093-4097: a press on a row calls ClickRow(row, ctrl, shift) and sets m_dragSelecting = m_multiSelect, m_rowPressed = true. ClickRow (1153-1166): a Ctrl+click on a selected row erases it, leaving {2} out of {2,4}. DispatchMouseUp (4311): ranged = m_dragSelecting && m_selectedRows.size() > 1, which is false at size 1. dragged is false unless text was dragged. With no band, scroll repeat, resize or thumb drag, it reaches the `row >= 0 && pressed` branch (4363), which calls SetSelectedRow(row). SetSelectedRow (948-971) does m_selectedRows.assign(1, r), so the selection becomes {4}. With single-click activation, onActivateRow then fires at 4388. Ctrl+clicking the only selected row ends the same way: the release puts it back. Nothing in DispatchMouseUp checks Ctrl. The Breakpoints pane (BreakpointsPane.cpp:127) and EventLogPane.h:98 both set multi-select.

## T1064 (medium)

**Dxui/Widgets/DxuiPopupMenu.cpp:520**, input

The wheel handler computes -(delta / WHEEL_DELTA) * rows with integer division on the raw GET_WHEEL_DELTA_WPARAM value and keeps no remainder. Any delta smaller than 120 scrolls nothing. DxuiComboBox.cpp:226 has the same pattern.

Failure: A precision touchpad or a high-resolution wheel sends WM_MOUSEWHEEL deltas of 8-60. Every one truncates to 0, so a menu that has to scroll (a long history or See more list taller than the monitor room) never scrolls with the wheel. The rows past the viewport can be reached only with the arrow keys.

Verified: DxuiPopupMenu.cpp:520 `ScrollByRows (-(delta / WHEEL_DELTA) * s_kWheelRows)` and DxuiComboBox.cpp:226 `ScrollBy (-(delta / WHEEL_DELTA) * 3)` divide integers. DxuiPopupHost.cpp:1212-1218 passes the raw GET_WHEEL_DELTA_WPARAM with no accumulator, and grepping for onWheel finds no other handling. Any |delta| < 120 truncates to 0 and nothing is kept, so high-resolution wheels and precision touchpads never scroll these popups.

## T1065 (medium)

**Dxui/Widgets/DxuiTextInput.h:54**, correctness

SetText resets m_undoBase but leaves m_undo and m_redo intact. A field reused for a new value keeps the undo history of the previous one, and Ctrl+Z restores text that never belonged to the current value.

Failure: DxuiEditableGrid shares one m_editor across every cell, and BeginEdit calls SetText for each cell. In Project Settings, Heat Ranges or Watches: edit row 0's cell, type 'foo', press Enter. Open row 1's cell (SetText loads 'bar') and press Ctrl+Z. Undo pops the state saved during row 0's edit, so row 1's box now shows row 0's old text, and Enter commits it into row 1. The Breakpoints rename editor and the find boxes, which also reuse an input via SetText, are affected the same way. The stacks also grow without limit over a session.

Verified: DxuiTextInput.h:54: SetText resets only m_undoBase and m_lastTyping; m_undo and m_redo (225-226) are never cleared anywhere (DxuiTextInput.cpp:1238-1311 are the only writers). DxuiEditableGrid.cpp:20/385: one member m_editor is reused, and BeginEdit calls SetText for each cell. Ctrl+Z reaches it through grid OnKey -> m_editor.OnKey (EditableGrid.cpp:1307) -> OnKeyWithModifiers -> TranslateKey('Z') = Undo -> InvokeCommand -> Undo() (TextInput.cpp:312-316, 1265-1278). Undo pops the base saved during the earlier cell's edit (FireChange 1238 pushed it) and puts it into m_text. Enter then commits it through CommitEdit, because text != GetEditText(row, col).

## T1066 (medium)

**Dxui/Widgets/DxuiTextView.cpp:24**, stale-state

SetRows replaces the rows but leaves m_highlights, m_scoped/m_scopeStart/m_scopeEnd and m_userAnchor/m_userCaret holding row/offset positions into the old text. It also resets the selection on every call.

Failure: The console calls SetRows on every new line and drops lines from the front once it passes kLineLimit=2000 (ConsolePanes.cpp:98-121). With find open, every highlight then marks text `dropped` rows below the real match, and the scope for 'find in selection' points at other rows. ConsoleOutput::Clear leaves highlights painted on an empty view's future rows. The selection the search made is wiped by each append, so F3 restarts from the top, and IsCurrentMatch/KeepUserSelection make decisions from the stale vector.

Verified: DxuiTextView.cpp:24-33: SetRows resets m_anchor/m_caret/m_dragging and m_topLine, but leaves m_highlights, m_scoped/m_scopeStart/m_scopeEnd and m_userAnchor/m_userCaret (DxuiTextView.h:333-338) alone. Those are rebuilt only by SelectMatch/SelectMatchInView (828, 895) and cleared only by FindWidgetSet::Close (FindWidgets.cpp:594) or a no-match search (821, 863). ConsoleOutput::Append (ConsolePanes.cpp:77-121) erases `dropped` lines from the front past kLineLimit and calls SetRows each time. Stored highlight rows then point at shifted text, which paint uses at 1725/1952, and the selection resets to (0,0). The next forward search therefore starts from `from=max(anchor,caret)=(0,0)` (816). ConsoleOutput::Clear (138) calls SetRows({}) without clearing highlights. IsCurrentMatch (1042) and KeepUserSelection (1108) read the stale m_highlights.

## T1067 (medium)

**Dxui/Widgets/DxuiToolbarEditBox.cpp:310**, input-state

The box ends the DxuiTextInput drag only in OnClick. The toolbar calls OnClick only for a release inside the entry while it is still armed, and it disarms the entry as soon as the pointer leaves its rect. The box does not override IDxuiToolbarCustomEntry::OnLButtonUp, the hook that sees every release, so m_input.m_dragging stays true.

Failure: Press in a pane's Find or Address box, drag-select past its right edge and release outside the box. The input stays in drag mode while it keeps focus. OnToolbarMouseMove forwards every later move to OnMouseMove, then m_input.OnMouseMove, so the caret and selection follow the pointer with no button held until the next click or loss of focus.

Verified: DxuiToolbarEditBox.cpp:310-315: only OnClick ends the drag (m_input.OnLButtonUp). The box overrides OnClick, OnMouseMove and OnLButtonDown (DxuiToolbarEditBox.h:78-81) but not OnLButtonUp; the default in DxuiToolbar.h does nothing, and its comment says it is the hook for this very case. DxuiToolbar::OnToolbarMouseMove (DxuiToolbar.cpp:1689) clears slot.pressed once the pointer leaves the rect. OnToolbarLButtonUp (1832-1880) calls custom->OnLButtonUp for every slot, which is a no-op here, and skips OnClick when !wasPressed. DxuiTextInput::m_dragging is cleared only in OnLButtonUp (TextInput.cpp:120), on a triple click, or in SetFocused(false) (TextInput.h:64). OnMouseMove (TextInput.cpp:237) moves the caret whenever m_dragging is set. The real users are Cassque's Search and Go-to boxes on the preview toolbar (CassqueWindow.cpp:1174), not a debugger pane. That window forwards moves only inside the preview toolbar's bounds (CassqueWindow.cpp:1962-1964), so the caret follows the pointer with no button held while the pointer is over the strip.

## T1068 (medium)

**Dxui/Widgets/DxuiTooltip.cpp:718**, timer-state

m_fadingOut is cleared only by FinishHide and RequestShowNow. HideImmediate, ShowTimed, and RequestShow's re-show branch (lines 445-475, which now also call HideImmediate when leaving an instant tip) leave it set. After that, Tick skips the hide deadline. When the new popup's fade-in ends, AdvanceReveal returns false with m_fadingOut still true, so FinishHide closes the new tip.

Failure: A tip times out and starts its 150 ms fade, setting m_fadingOut. During that fade the owner calls HideImmediate (a click, or InfoTip's key toggle), or a different tip is requested (RequestShow 'changed', or InfoTip::Open calling ShowTimed). The next tip from this DxuiTooltip comes up and vanishes as soon as its 150 ms fade-in completes. Until then WantsTick stays true and keeps an idle host ticking.

Verified: DxuiTooltip.cpp: m_fadingOut is set at 665 and cleared only in RequestShowNow (546) and FinishHide (701). HideImmediate (718-726), ShowTimed (580-597) and RequestShow's re-show branch (447-473, which also calls HideImmediate for an instant tip at 445-448) leave it alone. With it still true, Tick's deadline check (661, `&& !m_fadingOut`) is skipped. The next non-instant ShowPopup uses revealMs=kFadeMs (ShowPopup ~851), and when that fade-in ends AdvanceReveal returns false (DxuiPopupHost.cpp:2135-2150). Tick:681 `if (!more && m_fadingOut) FinishHide()` then closes the new tip. WantsTick (DxuiTooltip.h:197) stays true while m_fadingOut is set. Callers that reach this: CassqueWindow.cpp:6366 calls HideImmediate on a toolbar press, and DebuggerWindow.cpp:1693 calls it on drag start.

## T1074 (low)

**CassoCore/Debugger/AppleWinParser.cpp:278**, missing-validation

The `dest<start.endM` move shorthand evaluates a1 and a2 but never checks a2 >= a1, unlike the `M` command (which goes through TryParseRange) and the Monitor and GSSquared range checks. The reversed range reaches the Move handler, whose GetLast falls back to a1.

Failure: `300<2000.1000M` moves one byte from $2000 to $300 and reports success. `M 300 2000:1000` is correctly refused with "The range ends ... before its start".

Verified: TryParseMoveShorthand (AppleWinParser.cpp:274-282) evaluates a3, a1 and a2 and returns Ok with no a2 >= a1 check. The M command goes through TryParseRange (line 1147), which rejects a reversed range at lines 2286-2288 ('The range ends at ..., before its start at ...'). MemoryHandlers::Move (MemoryHandlers.cpp:219-250) uses GetLast, which is `std::max(a1, a2)` (line 941) and so gives a1 for a reversed range. It copies one byte and reports 'Moved 1 byte'. The handler does no other range validation.

## T1075 (low)

**CassoCore/Debugger/AppleWinParser.cpp:996**, wrong-syntax

TryParseRegisterCondition calls TryParseCondition without passing the context's NumberSyntax, so it uses the default, AppleWin. The BP comparison path does pass the syntax. As a result `!bpr` reached from WinDbg mode parses its value with AppleWin rules.

Failure: In WinDbg mode, `!bpr a = 0x10` is rejected because `0x10` is not a hex number in AppleWin syntax, while `!bp =0x300` works. `!bpr x = a` compares X with register A rather than WinDbg's number-first $0A.

Verified: AppleWinParser.cpp:993 and 996 call TryParseCondition without a syntax argument, and the header (AppleWinParser.h:68) defaults it to NumberSyntax::AppleWin. The BP path at line 569 and the BPR IF path at line 625 do pass args.context->GetNumberSyntax(). WinDbg `!bpr` reaches AppleWinParser::Parse through WinDbgParser::TryParseEngine (WinDbgParser.cpp:309) with a WinDbgExpressionContext, whose GetNumberSyntax returns WinDbg (WinDbgExpressionContext.cpp:90-93). BPR is not excluded, so IsCassoCommandReachable returns true. Under AppleWin syntax, `0x10` is not taken as hex (DebugExpressionEvaluator.cpp:413 needs syntax WinDbg for 0X), and the isdigit-lead check at lines 433-436 fails it with 'is not a hex number'. isNumberFirst (line 431) also needs syntax WinDbg, so a bare `a` resolves to register A.

## T1076 (low)

**CassoCore/Debugger/SymbolFileReader.cpp:320**, performance

AddUnique does a linear case-insensitive scan of every symbol already added, so loading a file is O(n^2) in the symbol count. It runs synchronously from SYM LOAD and from the Merlin listing / cc65 readers.

Failure: A cc65 .dbg or VICE label file with about 50,000 symbols does roughly 1.25 billion _stricmp calls, which freezes the debugger for many seconds or longer on SYM LOAD. A hash set of upper-cased names would make it linear.

Verified: SymbolFileReader.cpp:320-331 AddUnique scans the whole symbols vector linearly with _stricmp before every push_back. It is called once per symbol by every reader (lines 367, 407, 464, 473, 482, 514, 548), and the file has no cap on symbol count, so loading is O(n^2) in name comparisons. The quadratic cost is real. How long 50,000 symbols actually takes depends on name lengths and was not measured.

## T1077 (low)

**CassoEmuCore/Config/WindowPlacementProfile.cpp:191**, compatibility

The topology key no longer includes the work area, flags or active monitor. Every main-window placement saved by an earlier build sits under a key that can never match again. Unlike the debugger target, the main window has no GetAll fallback, and the orphaned keys are never pruned from prefs.

Failure: After upgrading, the first launch on an unchanged monitor setup computes a new key, TryLoad misses, and the main window opens at the default position instead of where the user left it. Each old key stays in window.placements, and MergeUntouchedPlacements keeps carrying them forward on every save.

Verified: The diff of WindowPlacementProfile.cpp against 321035f85 removes rcWork, flags and the `active=` device from the canonical string (lines 191-221 now fold only device and rcMonitor). Every key saved by an earlier build therefore hashes differently. WindowManager::TryLoadSavedWindowPlacement (WindowManager.cpp:82-96) calls m_profile.TryLoad(topologyKey, bounds) and has no other fallback: a miss leaves hMon null and returns false, so the window opens at the default place. Only the debugger path (ShellDebugger.cpp:921) has a further fallback. Nothing in WindowPlacementProfile prunes keys, and MergeUntouchedPlacements (GlobalUserPrefs.cpp:1142-1148) copies every untouched key on disk forward on each save, so the orphaned keys persist. This costs one lost placement per upgrade.

## T1078 (low)

**CassoEmuCore/Debugger/BinaryImageReader.cpp:299**, format-compatibility

The Intel HEX reader keeps parsing after the 01 end-of-file record, and SplitLines keeps any non-whitespace byte. Anything after EOF therefore fails the whole load.

Failure: Some tools pad the file after the EOF record, or end it with a DOS Ctrl-Z (0x1A). The trailing '\x1A' line fails the ':' check and BLOAD reports 'A line does not start with ':'' for a file that is valid up to its EOF record. Fix: stop at the 01 record.

Verified: In ReadIntelHex, `case 0x01: break;` only leaves the switch, and the for loop goes on to the next line. SplitLines (~545) drops only '\r', ' ', '\t' and splits on '\n', so a trailing 0x1A byte becomes a one-character line. That line fails `line.size() >= kMinRecord && line[0]==':'`, and the load is rejected with "A line does not start with ':'..." even though every record up to EOF is valid.

## T1079 (low)

**CassoEmuCore/Debugger/Channel/DebugChannelServer.cpp:113**, stale-state

Close clears m_pending but not m_causeId, and Open does not reset it either. A cause recorded by a run a client started before the channel closed survives a reopen.

Failure: A client sends `G`, so m_causeId is set while the run is in progress, and the user then closes the debugger channel (DebuggerController::Close) and reopens it. The run's eventual stop is broadcast to the new clients with `cause` set to the departed client's request id, which a new client using the same id numbering can mistake for its own request.

Verified: DebugChannelServer.cpp: Close (lines 113-125) clears m_pending and m_isOpen but not m_causeId, and Open (lines 33-40) does not reset it. HandleRequest sets m_causeId for a line that starts a run (lines 202-207), and it is cleared only in OnStopped or when no run started. A plain window close posts IDM_DEBUG_CLOSE with an empty payload (ShellDebugger.cpp:243-246). CloseDebugChannel(false) (ShellDebugger.cpp:1512-1533) does not end the run, unlike detach, which calls EndForUserPause -> Finish -> OnStopped and so resets it. DebuggerController keeps m_server across close and reopen (OpenDebugger reuses m_debugController). A run that spans a close and reopen therefore reports its stop with the old client's request id.

## T1080 (low)

**CassoEmuCore/Debugger/Channel/InstanceLister.cpp:51**, unsigned-underflow

`remaining = (DWORD)(deadline - GetTickCount64())` is computed after a separate `< deadline` check. If the tick passes the deadline between the two calls, the ULONGLONG difference wraps, and the cast gives about 0xFFFFFFF0 ms (49 days, effectively INFINITE) for ReadLine's wait.

Failure: An instance answers the probe with notifications before its hello, and the 15.6 ms tick boundary falls between the while test and the subtraction. ReadLine then waits about 49 days for this instance. If the instance stops pumping, `CassoCli debug --list` hangs instead of skipping it after the hello timeout.

Verified: InstanceLister.cpp:49-51: `while (!answered && GetTickCount64() < deadline) { DWORD remaining = (DWORD)(deadline - GetTickCount64());`. The tick is read twice. If it advances past the deadline between the two reads (GetTickCount64 moves in ~15.6 ms jumps), the unsigned difference wraps and the DWORD cast gives about 0xFFFFFFF0 ms. Win32PipeClient::ReadLine (Win32InstanceDirectory.cpp) builds its own deadline from timeoutMs and waits WaitForSingleObject(m_event, deadline-now), which is about 49 days. This can happen only from the second iteration on, after a non-hello record, and it hangs only if the instance then stops sending. It is rare but real.

## T1081 (low)

**CassoEmuCore/Debugger/CpuManagerRunDriver.cpp:161**, timing

In the emulator, a run's cycle budget is checked only at the end of a slice, and the frame loop never shortens a slice to the budget that is left. Every budgeted run therefore overshoots by up to FrameCycleBudget::kSliceCycles (1023) cycles.

Failure: A client sends `G` with budget 50. RunWatchedSlice runs a full 1023-cycle slice, OnSliceExecuted then sees m_spent >= 50, and the run stops about 1000 cycles past the budget. The stop reports budget, with registers far from where 50 cycles would have left them. The batch driver stops at the budget, so the two drivers disagree.

Verified: ExecuteCpuSlices sets sliceTarget = min(targetCycles - executed, FrameCycleBudget::kSliceCycles) (EmulatorShellCpuThread.cpp:577-582), with nothing derived from the run's budget. The budget is checked only after the slice, in OnSliceExecuted (CpuManagerRunDriver.cpp:160-171: m_spent += cyclesExecuted; budgetSpent = m_spent >= *m_budget). A budget of 50 therefore runs a full slice of up to ~1023 cycles before the stop. SynchronousRunDriver.cpp:60 limits each chunk to min(kChunkCycles, *request.budget - spent), so the batch driver stops at the budget. The header comment (CpuManagerRunDriver.h:35-38) only says budgets are counted across slices, and nothing in docs or specs says the emulator overshoots.

## T1082 (low)

**CassoEmuCore/Debugger/DiskFieldTracker.cpp:358**, null-deref

StepData dereferences state.lastAddress unconditionally, but StepWritten can reset lastAddress while phase is still Data. Step only calls StepWritten for a written nibble and never ends the read phase.

Failure: Code reads an address field and then D5 AA AD, so the phase is Data. Before the field ends it switches to write mode and writes a D5 AA AD prologue; StepWritten matches it and calls lastAddress.reset(). It switches back to reading, and once collected reaches the body length, `*state.lastAddress` dereferences an empty optional. That is undefined behavior and asserts in a debug CRT.

Verified: FindPrologue sets phase = Data while lastAddress is set (DiskFieldTracker.cpp:178-184). Step (:106-131) sends a written entry to StepWritten and never changes the phase. StepWritten (:395-407) matches a data prologue when lastAddress is set and calls lastAddress.reset(), but leaves phase as Data. The next read entries go to StepData, whose else-branch runs `event = *state.lastAddress;` (:358) with no has_value check. The gap flag does not end the field first: Disk2NibbleEngine.cpp:718-722 increments m_readNibbles only when !m_writeMode, so the first read after leaving write mode is count+1 and AddRead (Disk2NibbleRecord.h:69) sets no kAfterGap. Only a D5 nibble or a missed nibble would end the field before the body is complete.

## T1083 (low)

**CassoEmuCore/Debugger/Events/MachineEventFeed.cpp:126**, correctness

Reseed re-seeds the I/O decoder after the machine's state is replaced, but it does not re-read the Disk II controller's active drive. m_drive is read only in Attach, so after a seek or a load from a keyframe without a later drive select, live Disk II events carry the wrong drive.

Failure: Drive 2 is active now. Rewind to a point where drive 1 was active (that keyframe's state is loaded without an OnDriveSelect) and resume live from there. OnLive calls m_feed.Reseed(), which leaves m_drive at 2, so every motor, step and sector event in the Disk II log shows drive 2 until the guest next selects a drive. A rebuilt stretch of the same span shows drive 1, so the log contradicts itself.

Verified: MachineEventFeed.cpp:81 reads m_drive from controller->GetActiveDrive() only in Attach. Reseed (lines 126-132) only reseeds m_decoder. The only other write is OnDriveSelect (line 587). Disk2Controller::LoadState sets m_activeDrive directly (Disk2Controller.cpp:1507) without notifying sinks; notifications fire only on the guest's soft-switch access (lines 347-366). OnLive (DebugEventLogs.cpp:386-395) calls only Reseed, so after a load from a keyframe every EmitDisk(..., m_drive) uses the stale drive. The scratch path attaches its probe after LoadFrom (ScratchEventReplayer.h:290-293), so rebuilt stretches read the right drive.

## T1084 (low)

**CassoEmuCore/Debugger/GSSquaredFormatter.cpp:340**, output-correctness

The GSSquared watch list prints an expression or register watch by its unused address field.

Failure: A typed watch (an expression or register, set in AppleWin mode or the Watches pane) has an empty basicName and address 0. Listing watches in GSSquared mode prints '[3] 00/0000' instead of the expression, so the entry is indistinguishable from a real watch on $0000.

Verified: GSSquaredFormatter.cpp ~340 prints `entry.basicName.empty() ? FormatAddress(entry.address) : entry.basicName` and ignores entry.expression. WatchHandlers.cpp MakeList (~121) copies item.address. A register watch (WatchHandlers.cpp ~313) and an expression watch with no width (~343) never set item.address, and ReadTyped only overwrites entry.address via readAt for Address, Symbol or width-suffixed kinds. Register and plain expression watches therefore list as '[id] 00/0000'. Symbol watches print their resolved address rather than their name, which is a lesser form of the same issue.

## T1085 (low)

**CassoEmuCore/Debugger/Handlers/BreakpointHandlers.cpp:708**, misreport

BRK ON with a new IF on an existing BRK entry does change the condition (line 689) but replies 'Nothing to add: the setting was already on'.

Failure: BRK ON, then BRK ON IF A=0. The condition is replaced, but the reply tells the user nothing happened, so they may assume the IF was ignored and re-enter or clear it.

Verified: In SetBrk, when a Brk entry already exists, the branch at BreakpointHandlers.cpp:681-690 changes the condition through TrySetCondition but does not push anything onto added. At :708, 'isOn && added.breakpoints.empty()' then replies 'Nothing to add: the setting was already on...' even though the condition changed.

## T1086 (low)

**CassoEmuCore/Debugger/Handlers/BreakpointHandlers.cpp:1516**, lost-state

BPEDIT's TryMakeEntry for BP ignores definition.symbol, so an exec breakpoint edited to a symbol loses its symbol binding. Unlike the same BP typed directly, it no longer follows the symbol in FollowSymbols, and BPSAVE writes its address instead of the name.

Failure: BPEDIT 0 BP START, then SYM LOAD of a rebuilt program in which START moved. Breakpoint 0 stays at the old address and silently stops at the wrong place, while 'BP START' would have moved and said so.

Verified: TryMakeEntry's SetBreakpoint case (BreakpointHandlers.cpp:1516-1521) sets kind, first, last and condition but never sets breakpoint.symbol. Edit then adopts that entry (:1466). By contrast, SetAddress calls TrySetSymbol(id, command.symbol) (:331), and the parser fills command.symbol (AppleWinParser.cpp:836). FollowSymbols (:228) skips entries whose symbol is empty, and MakeDefinition (:190) writes the address when there is no symbol.

## T1087 (low)

**CassoEmuCore/Debugger/Handlers/MemoryHandlers.cpp:196**, misreport

When Patch hits an I/O address partway through, the error leaves out how many bytes were already patched, although the ROM/RAM branch of the same error states it.

Failure: PATCH BFFE 01 02 03 patches $BFFE and $BFFF, then fails at $C000 with only '$C000 is I/O. Use OUT to write it.'. The user is not told that two bytes changed.

Verified: In Patch, at MemoryHandlers.cpp:196-198, the I/O branch formats only '${:04X} is I/O. Use OUT to write it.', while the other branch includes '{} of {} bytes were written'. Bytes before index i were already patched by TryPatch in the loop at :187-193.

## T1088 (low)

**CassoEmuCore/Debugger/Handlers/SymbolHandlers.cpp:329**, stale-state

A plain symbol-file SYM LOAD into the table that holds the current debug file replaces that table's symbols but keeps the old debug file: its source lines, call-stack labels and BP file:line resolution.

Failure: SYM LOAD prog.dbg (USER), then SYM LOAD other.sym (USER). Source breakpoints and the source view still resolve against prog.dbg's line table while the symbols are other.sym's, so BP main.s:12 sets breakpoints at addresses from a program that is no longer loaded.

Verified: In SymbolHandlers::Load, the plain path (SymbolHandlers.cpp:329-344) calls SymbolTable::LoadFrom, which replaces the table (SymbolTable.cpp:486 Clear(table)). It never checks GetDebugFileTable() and never calls ClearDebugFile. SYM CLEAR does both (:573-576), which shows the debug file is meant to belong to its table. m_debugFile and the line table therefore stay those of the old .dbg.

## T1089 (low)

**CassoEmuCore/Debugger/HeatAccessJump.cpp:65**, off-by-one

Plan refuses the seek when access.GetPosition() < oldestPosition, but the seek target is GetPosition() + 1, so an access at oldestPosition - 1 is refused although its target is in history.

Failure: The last write to an address was made by the instruction at position oldest-1, the one just before history's oldest keyframe. The seek would go to oldest, the oldest keyframe itself, which history can load. Plan instead reports that the last write is older than the history kept. The check should be access.GetPosition() + 1 < oldestPosition.

Verified: HeatLastAccess::Make is stamped with the position when the instruction starts (AccessHeatMap.cpp:1543-1546), so the state right after an access at position P is at P+1. Plan seeks to GetPosition()+1 (HeatAccessJump.cpp:73), but line 65 refuses the seek when GetPosition() < oldestPosition. For an access at oldest-1, the target is oldest, the position of keyframe 0 (ReverseController::GetOldestPosition, line 811-813). SeekToPosition (line 741-752) can land there, and other paths already land at oldest (e.g. line 430, LandAt(oldest)). Plan still prints 'older than the history kept' in that case. Nothing in the caller (ShellDebugger.cpp:1899-1904) adjusts oldest.

## T1090 (low)

**CassoEmuCore/Debugger/HeatHistory.cpp:476**, correctness

SearchHistory takes a nonzero stretch count to mean the access is replayable, but the counts include accesses made before m_heatFrom (a Reset inside the stretch) while the replay counts only from m_heatFrom; when nothing is found it returns Unknown.

Failure: The map counts from P0. The user resets the heat map at R, partway through the stretch from keyframe K to K+1, and an address was last written between K and R. The machine is rewound to M > R, and the map's record for the address is from the future, so it reads Unknown. HasCountIn(K+1) is > 0, so FindInStretch replays with countFrom = R and finds nothing. BAIL_OUT_IF(true) then returns Unknown. The tooltip and rewind jump say the write is older than the history kept, when the right answer is that there was no write since the map started counting (None).

Verified: HeatHistory::Reset (lines 170-187) subtracts the totals at R from m_anchorTotals, AccessHeatMap::Reset zeros the totals (AccessHeatMap.cpp:184-192), and m_heatFrom = R. At K+1, OnKeyframeAdding encodes delta = now - (totals_K - totals_R) = counts(R..K+1) + counts(K..R), so the stretch K..K+1 includes accesses made before R. For a machine at M in a later stretch, the map's future record reads Unknown (AccessHeatMap.cpp:350-352). In the SearchHistory loop, index K+1 has position > m_heatFrom, so the None break at line 461 is not taken. HasCountIn(K+1) > 0, so FindInStretch runs with job.countFrom = m_heatFrom. ScratchHeatReplayer.cpp:469 sets the replay map's countFrom and lastFrom to R, and line 506 reports hasAccess only for Found. A write made before R is therefore not found, and BAIL_OUT_IF(true) at line 479 returns the initial state, Unknown, where the right answer is None.

## T1091 (low)

**CassoEmuCore/Debugger/HeatHistory.cpp:642**, performance

Each newest keyframe dropped makes ForgetAccessesFrom walk every m_last entry (512K) on the machine thread, and DropNewerThan drops the future one keyframe at a time.

Failure: With a long future in history (hundreds of keyframes after the machine) and the heat map on, the user steps or runs from the past and history is cut. KeyframeStore::DropNewerThan sends KeyframeDrop::Newest once per keyframe, and each call scans all 524,288 last-access records. Dropping 500 keyframes is about 260 million record visits in a row on the CPU thread, which stalls emulation visibly, worst in a debug build. Each drop also uses a cut no later than the keyframe before it, so a single scan with the final cut would do the same work once.

Verified: KeyframeStore::DropNewerThan (KeyframeStore.cpp:1570-1578) calls NotifyDrop(KeyframeDrop::Newest) once per keyframe it drops. Each call reaches HeatHistory::OnKeyframeDrop, which calls m_map.ForgetAccessesFrom(cut) at line 642 for every non-Oldest drop. ForgetAccessesFrom (AccessHeatMap.cpp:369-378) loops over all of m_last, kLastCount = 4*2*0x10000 = 524,288 entries (AccessHeatMap.h:114). The cut is min(machine position, keyframe count-2), which only gets earlier as keyframes are dropped, so the last scan covers what every earlier scan did. The calls come from ReverseController.cpp:329 and :1038 (DropAfterPosition), when history is cut on the machine thread. I did not measure the size of the stall.

## T1092 (low)

**CassoEmuCore/Debugger/MonitorFormatter.cpp:231**, output-completeness

MonitorFormatter::FormatStop prints only the register line, with no PC, plus BASIC and disk lines. A breakpoint, watchpoint, step or BRK stop in Monitor mode never says where the machine stopped or why.

Failure: In Monitor mode a data breakpoint fires. The AppleWin path would print 'Data breakpoint #2: Write $41 to $0300 by $0812', but the Monitor output is just 'A=41 X=00 Y=00 P=30 S=F9'. The user and any script lose the PC, the breakpoint id, the accessed address and the IF condition value.

Verified: MonitorFormatter.cpp:231-255 FormatStop returns FormatRegisters (A/X/Y/P/S, no PC), then lines for a BASIC stop or disk report only. It never looks at stop.reason, stop.pc or stop.watch. DebugBatchSink.cpp:61 uses it for every stop reason when the output format is Monitor. The design comment there and in MonitorFormatter.h only justifies the step case, where the step reply carries the instruction line. A breakpoint, watchpoint or BRK stop from G prints only the register line, while AppleWinFormatter::FormatStop (AppleWinFormatter.cpp:50+) prints the reason, the id, the address and the accessing PC. The JSON channel path (DebugChannelServer::OnStopped) is unaffected, so this applies to the text batch or CLI sink.

## T1093 (low)

**CassoEmuCore/Debugger/Project/DebugProjectStore.cpp:160**, data-loss

A redirect file passes DebugProject::TryParse as a valid empty project, because its format matches. Save also writes the redirect to the automatic path without checking that path is not the project file itself.

Failure: PROJECT shows the automatic file's path; the user runs `PROJECT SAVEAS <that path>`. Save writes the full project to it and then overwrites it with a redirect that points to itself. On the next Open, Read(redirect) parses the redirect as an empty project with isFound true, so the project's contents are silently gone. The same quiet empty result comes from `PROJECT OPEN` on any automatic redirect file.

Verified: DebugProjectStore.cpp:144-161: the redirect object carries `format: DebugProject::kpszFormat`. Save writes the project to project.filePath and then, when !isAutomatic, writes the redirect to GetAutomaticPath(scope) with no check that the two paths differ. SaveAs (lines 178-191) sets isAutomatic=false. RunProjectRequest SAVEAS (ShellDebuggerProject.cpp:679-692) does not reject the automatic path. DebugProject::TryParse (DebugProject.cpp:477-481) requires only `format == kpszFormat`, and every other member is optional, so a redirect parses as a valid empty project. On the next Open, Read(redirect == automatic) returns isFound=true with an empty project. OpenFile on any redirect file also returns S_OK with no notice (ShellDebuggerProject.cpp:697-705), so an empty project opens silently.

## T1094 (low)

**CassoEmuCore/Debugger/ReplyJson.cpp:553**, json-missing-field

The JSON breakpointList omits BreakpointListData.videoBreak, which every text layout appends through AppendVideoBreak.

Failure: After BPV sets a scanline break, the breakpoint list replies {"kind":"breakpointList","breakpoints":[]}, so a JSON client reports no active stops while a video break is armed and the machine then stops for no visible reason. Per-frame `count` in profile frames and symbol `size` are dropped from the JSON the same way.

Verified: ReplyJson.cpp ~553-565 emits only kind and breakpoints for BreakpointListData. Reply.h:193 has `std::optional<std::pair<uint32_t,uint32_t>> videoBreak`. BreakpointHandlers.cpp:93 fills it from session.GetVideoBreak(), and AppleWinFormatter.cpp:530-566 (AppendVideoBreak) prints it in text. The secondary claims are also consistent with the structs. ProfileFrameEntry::count (Reply.h:391) and SymbolInfo::size (Reply.h:255) exist, and the profile-frame and MakeSymbols serializers shown do not emit them. I checked only the cited portions for these two.

## T1095 (low)

**CassoEmuCore/Debugger/ReplyJson.cpp:591**, json-missing-field

The JSON fileIo reply omits FileIoData.address and isLoad, which the text formatters show.

Failure: A debug-channel client BLOADs an Intel HEX, S-record, DOS 3.3 or AppleSingle file without an address, so the load address comes from the file. The JSON reply has only path, requested, transferred, mismatch and companion. The client cannot learn where the bytes landed, or tell a load from a save, without scraping the text array.

Verified: ReplyJson.cpp ~591 builds fileIo from path, requested, transferred and mismatch, plus companion when it is set. Reply.h:276-286 FileIoData also has `Word address` and `bool isLoad`. The handlers fill isLoad (MemoryHandlers.cpp:531, MonitorHandlers.cpp:449), and AppleWinFormatter.cpp:736 prints it, but the JSON never emits either field.

## T1096 (low)

**CassoEmuCore/Debugger/Reverse/KeyframeStore.cpp:984**, error-handling

Collect returns on a failed job (CHR(job->hr)) without retiring it, so m_pendingCount never falls and every later TakeJob, WaitForPending and Restore fails. A failed Add also returns before ScheduleAfter, and ReverseController::CaptureNow's Error path reschedules from the unchanged GetNextDueCycle, so OnCaptureDue fires again on the next instruction.

Failure: Example: ChangeBudget raises the budget beyond what can be allocated. When the table later fills with arena to spare, TryGrowTable -> Relayout's new[] fails. Or a Compress fails in Pack, which wedges the store permanently. SubmitJob fails before ScheduleAfter, m_nextDueCycle stays <= the current cycle, and every following instruction runs CaptureNow: a full SaveState plus a HostState journal record plus a failed Add (or a multi-GB allocation attempt). Emulation slows to a crawl and the journal fills with HostState records, while history silently stops.

Verified: In Collect (KeyframeStore.cpp:984), CHR(job->hr) jumps to Error before m_pendingCount-- (line 1006). The CBRAEx in Place (line 1055) does the same on a non-side failure. Every later Collect finds the same finished, failed job and fails again, which fails TakeJob (line 731), WaitForPending (line 1159) and Restore (line 1196). The two Add overloads (lines 305-372) return before SubmitJob's ScheduleAfter (line 824) when TakeJob fails. SubmitJob's TryGrowTable -> Relayout (lines 786, 641) also fails before line 824 when new[] fails at line 131. CaptureNow's Error path calls ScheduleCaptures (ReverseController.cpp:1843), which copies the unchanged GetNextDueCycle (line 2237), so the value stays <= the current cycle and HistoryRecorder.h:48 calls OnCaptureDue on every instruction. Each call does a full SaveState, a HostState RecordInput (line 1811) and a failing Add. The budget goes up to 1 TB (DebuggerSettings.h:30). After that, ComputeFitLayout (lines 591-605) sizes the table from that budget, so each instruction retries a huge allocation.

## T1097 (low)

**CassoEmuCore/Debugger/RoutineProfile.cpp:176**, correctness

HasReturned treats any stack pointer more than 0x80 below a frame's return point as having returned, so a routine that pushes more than 128 bytes is ended while it is still running.

Failure: A routine saves a buffer with a PHA loop, or builds a stack frame of more than 128 bytes. Once SP drops 129 or more bytes below returnSp, depth = returnSp - sp exceeds 0x80, so EndReturned pops the routine's node at once (and any outer nodes the same test catches). Its remaining cycles, and its later calls, go to the caller in PROFILE's routines and call tree.

Verified: HasReturned (RoutineProfile.cpp:170-177) computes depth = (Byte)(returnSp - sp) and returns true when depth > 0x80. EndReturned (lines 152-158) is called after every instruction with spAfter (Record line 80). So once a routine has pushed 129 or more bytes below returnSp, the next Record pops its frame while it is still running, and later cycles and calls are billed to the caller. The header (RoutineProfile.h:136-137) states this as an assumption ('a frame uses far less than half the stack'), so it is a known limitation rather than an oversight. The failure the finding describes still happens exactly as stated, and nothing else prevents it.

## T1098 (low)

**CassoEmuCore/Debugger/RoutineProfile.cpp:137**, correctness

Once the tree reaches kMaxNodes, FindChild returns the parent, and Begin then adds 1 to the parent's calls and pushes the parent again.

Failure: A long profile reaches 8192 nodes. From then on every new call site, e.g. JSR from MAIN to a routine not yet seen, adds 1 to MAIN's own call count, so the call tree reports MAIN as called thousands of times. Merge (line 213) has the same problem: when FindChild falls back to the parent, it adds the later profile's node.calls to the parent.

Verified: FindChild (RoutineProfile.cpp:373-376) returns `parent` once m_nodes.size() >= kMaxNodes (8192, RoutineProfile.h:78), for any (parent, routine) key not already in m_children. Begin then does m_nodes[node].calls++ and pushes Active{parent node} (lines 137-138), so the caller's call count goes up for every unseen call site. That count is reported: ExecutionHandlers.cpp:729 sums nodes[i].calls into data.routines[].calls, and line 764 puts nodes[node].calls into the tree. Merge line 209/213 is the same: a FindChild fallback returns the parent, and `+= node.calls` adds the later profile's call count to it. The comment at line 356 intends the cycles to go to the caller, but nothing stops the call count from going up too.

## T1099 (low)

**CassoEmuCore/Debugger/RunStopHook.cpp:168**, logic

m_resumePc stays set while the hook is uninstalled and is used up by the first instruction asked about after the hook is reinstalled. That can be a newly re-added breakpoint at the old stop address, which then is not hit.

Failure: Stop at BP #1 at $0300 in a routine that runs once per keypress; End sets m_resumePc to $0300. BPC 1 removes the last breakpoint, so the hook is uninstalled, and the user resumes from the main window. Later the user sets BP $0300 again and presses the key. The page filter asks first about $0300, so isFirst = (*m_resumePc == pc) is true and the breakpoint is skipped; the routine runs through without stopping.

Verified: RunStopHook::End (RunStopHook.cpp:117-127) sets m_resumePc = pc when the stop was not a watch hit. It is cleared only in Begin (:90) and when the hook is next asked about an instruction while inactive (:168-172). Uninstalling the hook (MachineDebugTarget::SetHookInstalled -> m_host.SetDebugHook(nullptr)) does not clear it, and neither does SetConditions. While uninstalled, MachineHost never calls the hook (MachineHost.cpp:358/436), so the value survives. After the breakpoint is set again, the first instruction the page filter passes is $0300, so isFirst = (*m_resumePc == pc) is true. The breakpoint branch `(!isFirst || isUnasked)` (:198) is then skipped, and the only remaining check is ShouldStopAtRunStart, which covers before-mode watchpoints only, so the breakpoint is not hit.

## T1100 (low)

**CassoEmuCore/Machines/Apple2/Common/Disk2NibbleRecord.h:69**, correctness

AddRead computes `assembled - m_lastAssembled[drive] > 1` on uint64_t. Disk2Controller::Reset (soft reset) calls Disk2NibbleEngine::Reset, which zeroes m_readNibbles, but the record's m_lastAssembled is cleared only on a power cycle. The subtraction underflows after a reset.

Failure: Boot DOS, which reads thousands of nibbles (m_lastAssembled ~ 50000), then press Ctrl-Reset and read the disk again. On the first AddRead, assembled (a small number) minus 50000 wraps to a huge value, so that nibble is flagged kAfterGap ('the CPU missed nibbles') although none were missed. The bad flag sits in the recent-nibbles record and in the saved state.

Verified: Disk2NibbleRecord.h:69 computes `(assembled - m_lastAssembled[drive] > 1)` on uint64_t. m_lastAssembled is cleared only by Clear(), which Disk2Controller calls in PowerCycle (Disk2Controller.cpp:1215) and on a LoadState fallback (1487). The soft-reset path is MachineHost::SoftReset (MachineHost.cpp:949) -> MemoryBus::SoftResetAll -> Disk2Controller::SoftReset (1188-1191) -> Reset() -> m_engine[i].Reset() (1166), which sets m_readNibbles = 0 (Disk2NibbleEngine.cpp:569). The next AddRead (Disk2Controller.cpp:469-470) passes GetReadNibbles(), now small, so the subtraction wraps and that one entry gets kAfterGap. DiskFieldTracker reads that flag as a break (DiskFieldTracker.cpp:211, 257, 336). Only the first read after a reset is affected, and m_lastAssembled then resynchronises. It is low impact, but the mechanism is as described.

## T1101 (low)

**CassoEmuCore/Machines/Apple2/Common/ProDosVolume.cpp:1223**, robustness

TryFindFreeDirectorySlot returns false both when the directory is full and when its chain is broken: a next pointer out of range, or a revisited block. The new grow path reads any false as 'full', and LinkDirectoryBlock then overwrites the damaged next pointer of the last reachable block with a fresh block. The old code refused the write.

Failure: In a subdirectory, a bit error leaves a next pointer at a value of 280 or more, while the real following directory blocks (with live entries) still exist. Writing a file into that directory links a new block in place of the bad pointer. The entries in the unreachable blocks are orphaned for good, along with the blocks they own. Before, the write failed and the image was left untouched.

Verified: TryFindFreeDirectorySlot (ProDosVolume.cpp:1231-1260) leaves the loop on an out-of-range or revisited block (1233-1240) and returns false, the same result as a full directory. LinkDirectoryBlock (1580-1593) walks until next fails the same test and then overwrites `last`'s next pointer with the new block, which discards the damaged pointer. Write (2075-2088) collects fullyParsed but never checks it before calling AddFileWithCase. The commit check (VolumeIntegrityReport.cpp:225-239) does not stop this. Blocks that were unreachable before stay unreachable, so the leak count does not grow. The repaired chain parses fully, so catalogFullyParsed does not go from true to false. The one condition the finding leaves out: every record in the reachable blocks must already be in use, or a free slot is found and no relink happens. The merge-base code returned ERROR_DISK_FULL whenever the slot search failed (old 1296-1297).

## T1102 (low)

**CassoEmuCore/Seams/IntentReplyTracker.cpp:74**, stale-state

Pending reload notes never expire. A sender-carrying broadcast reaches every Casso window, and EmulatorWindow notes a reload for the image even in emulators that do not have it mounted. Those entries hold the sender's HWND indefinitely, and a later unrelated change to that path answers it.

Failure: Cassque writes a.dsk and broadcasts with its window as sender. Emulator B, which has no a.dsk mounted, records {a.dsk, cassqueHwnd}. Hours later B mounts a.dsk, and an external edit triggers a reload decision. TryTakeReload returns the stale HWND, and B sends a ReloadDone or ReloadConflict reply to a window that never asked: Cassque, or after the window is closed, whatever window now has that handle value.

Verified: Win32IntentChannel::StateIntent (Win32IntentChannel.cpp:430-447) sends to every window of the emulator class through EnumWindows, carrying m_sender. Cassque sets that sender at CassqueShell.cpp:424. EmulatorWindow.cpp:2168-2175, in the default branch, calls m_intentReplies.NoteReload(path, replyTo) whenever replyTo is non-null, and does so before and regardless of whether any bay holds the image. DiskImageStore::NoteExternalChange (DiskImageStore.cpp:2781-2800) then does nothing for an unmounted path, so no decision consumes the note. IntentReplyTracker (lines 74-83) keeps entries until TryTakeReload matches the path, and has no expiry. A later decision on that path comes from the watcher-driven NoteExternalChange at DiskImageStore.cpp:3745 through m_decisionSink (lines 3039 and 3124). That call reaches the sink in EmulatorShellStorage.cpp:353-366, which takes the stale HWND and posts a reply to it.

## T1103 (low)

**CassoEmuCore/Seams/Win32ShellIcons.cpp:90**, dead-condition

The test `GetCacheKey(path, isDirectory).front() == L'.'` can never be true, because cache keys start with '*' ("*ext:" or "*path:"). The large and jumbo icon path therefore never sets SHGFI_USEFILEATTRIBUTES for ordinary files, which the comment intends.

Failure: At icon sizes above 32px, the first file of each extension makes SHGetFileInfo open the real file instead of resolving by extension. On a slow or offline network share this blocks the UI thread, and for cloud placeholder files it can trigger a recall. A file SHGetFileInfo cannot open falls back to the small-path loader and caches that result for the whole extension.

Verified: GetCacheKey (Win32ShellIcons.cpp:188-212) returns only L"*path:"+path or L"*ext:"+extension, so its front() is always '*'. The test at line 90, `GetCacheKey(path, isDirectory).front() == L'.'`, is therefore always false, and SHGFI_USEFILEATTRIBUTES is never added in LoadLargerForPath. The comment above the test ('An ordinary file shares its type's image, as the cache assumes') says it was meant to apply. SHGetFileInfoW is consequently called on the real file at sizes above the large icon. When that call fails, line 105 falls back to LoadForPath, and Remember caches the result under the *ext: key for every file with that extension. One caveat: the small and large path, LoadForPath (lines 226-243), also touches the real file through GetFileAttributesW, so the extra I/O cost is smaller than the scenario suggests. The dead condition itself is real.

## T1104 (low)

**CassoEmuCore/Shell/DebugEventLogs.cpp:626**, error-handling

RebuildIoBefore passes on the failure of log.runner.Rebuild with CHR. Run returns the failing stretch's hr, so one stretch that fails to replay discards the whole VIDEOLOG/SOUNDLOG listing, including the entries already found, instead of showing an Unavailable row. Results with FAILED(result.hr) are also never turned into Unavailable entries.

Failure: History has one stretch whose replay ends at the wrong cycle (ERROR_INVALID_DATA) or whose keyframe cannot be unpacked. VIDEOLOG run over that range returns an error instead of listing the cache's entries and the stretches that did replay. The pane path, by contrast, shows an Unavailable row for the same stretch.

Verified: ScratchEventReplayer::Rebuild returns Run's hr, and Run returns the failing stretch's hr after passing it on (ScratchEventReplayer.h:242-249). DebugEventLogs.cpp:626-627 applies CHR to it, so RebuildIoBefore and GetIoEntries (line 534) fail. LogHandlers::List (LogHandlers.cpp:113-119) then replies with the 'history not rebuilt' error and drops the cache's entries. The result loop at 630-658 checks only result.isUnavailable, never FAILED(result.hr), whereas DebugEventReplayer::AppendRebuilt (DebugEventReplayer.h:266) treats both as Unavailable.

## T1105 (low)

**CassoEmuCore/Shell/EmulatorShellState.cpp:190**, error-path

LoadMachineState switches to the file's machine before it validates what it can validate without one: CheckDisks needs no machine. It also contradicts its own comment ('checks the whole file before anything changes'). A refusal after the switch leaves the user on another machine with nothing loaded.

Failure: The user loads a state saved by an Apple ][+ while running a //e, and one of the file's disk images fails DiskImage::LoadFromBytes, or the ROM identity differs. SwitchMachine runs first: it saves the //e's auto state, tears the //e down and cold-boots a ][+. Then Check fails, and the notice says only 'Machine state not loaded'. The user's //e session has been replaced by a freshly booted ][+. CheckDisks should run before the switch, and a refusal after it should say that the machine changed.

Verified: EmulatorShellState.cpp:190-199: when isOtherMachine, SwitchMachine(contents.machineName) runs before MachineStateFile::Check. Check calls CheckDisks (MachineStateFile.cpp:256), and CheckDisks (lines 676-710) only reads contents, so it needs no machine. SwitchMachine saves the outgoing auto state (MachineManager.cpp:425), tears the machine down and power cycles it. It skips ResumeAutoState because m_isLoadingStateSwitch is set. A later Check failure ends in PostNotice(FormatStateError(L"loaded", error)) at line 229, which says nothing about the machine having changed. The header comment at lines 161-162 ('Reads and checks the whole file before anything changes') is contradicted.

## T1106 (low)

**CassoEmuCore/Shell/ScratchEventReplayer.h:228**, error-handling

When m_scratch.Build() fails, Run returns before any stretch and passes no result to onStretch. ProfileHistory::Pump counts down m_remaining only as results arrive, so a worker whose scratch machine fails to build leaves m_remaining above 0 and m_isBuilding true for good. This is a separate case from the known SubmitBlocks failure.

Failure: The scratch machine's MachineBuilder::Build fails for one profile worker (for example an allocation failure across 8 scratch machines, or CBR(generation != 0) if SetMachine was skipped). That worker's job produces no results, so its blocks are never subtracted from m_remaining. ProfileHistory::Pump never sums the counts, GetProgress keeps returning a value, ShellDebugger::ServiceEventLogs marks the debug view dirty on every pass, and the profile never arrives.

Verified: ScratchEventReplayer.h:228-229: Run returns on a failed m_scratch.Build() before calling onStretch. RunPending ignores the hr (line 191). ProfileHistory::Pump (ProfileHistory.cpp:512-548) decrements m_remaining only when results arrive, so with none it stays above 0. m_isBuilding stays true, Pump returns false at 550-553, and GetProgress keeps returning a value (586-598), which ShellDebugger.cpp:2626 turns into a dirty debug view on every pass. Only a new Build or Stop clears it. Low likelihood, but nothing guards it.

## T1107 (low)

**CassoEmuCore/Shell/ShellDebugger.cpp:1747**, correctness

SetDebugHeatColumnsShown changes the view state without setting m_isDebugViewDirty, unlike every sibling setter. IsBuildDue never rebuilds a paused machine's view on time alone, so the heat columns do not appear or disappear until something else dirties the view.

Failure: With the machine stopped at a breakpoint, the user turns on the code pane's heat column, which posts 'heatcolumns on'. No snapshot is built, so the column stays empty until the user steps or changes something else.

Verified: SetDebugHeatColumnsShown (ShellDebugger.cpp:1747-1750) calls only m_debugViewState.SetHeatColumnsShown, which sets m_isHeatReadDue but not ShellDebugger::m_isDebugViewDirty. Its siblings at :1762 and :1778 set the dirty flag. DispatchDebugView (CpuCommandDispatcher.cpp:657-660) falls through for 'heatcolumns' and dirties nothing. The UI's path, ToggleOption -> SetDebuggerDisassemblyOptions (ShellDebugger.cpp:686), only saves settings. IsBuildDue (DebuggerViewState.cpp:67-72) never rebuilds a paused machine's view on time alone, and snapshot.heatColumns stays nullptr until the next build (DebuggerViewState.cpp:528). So a paused machine's heat column stays empty until something else dirties the view.

## T1108 (low)

**CassoEmuCore/Shell/ShellDebugger.cpp:1780**, data race

SetDebugBasicShown, SetDebugHeatColumnsShown, SetDebugHeatMapShown, SetDebugHeatMapOptions and SetDebugHeatMapHover modify m_debugViewState without m_debugViewStateLock. The header documents that every change takes that lock because builds on the pool read the view state under it. BuildCaptured reads m_isBasicShown on the pool thread.

Failure: A pool build is inside BuildCaptured under m_debugViewStateLock while the CPU thread handles 'basic on' and writes m_isBasicShown with no lock. This is a data race, which ASan/TSan flags, and the snapshot can be built with either value. Any future field these setters touch that the pool reads inherits the race.

Verified: The header comment (ShellDebugger.h:509-511) says every change the window makes to the view state on the CPU thread takes the view state lock. SetDebugBasicShown, SetDebugHeatColumnsShown, SetDebugHeatMapShown, SetDebugHeatMapOptions and SetDebugHeatMapHover (ShellDebugger.cpp:1747-1860) take no lock, unlike SetDebugTraceView and GoToDebugMemory (:1706, :1729). The pool build reads the view state under m_viewLock (DebugViewPublisher.cpp:125), and BuildCaptured reads m_isBasicShown at DebuggerViewState.cpp:288, a plain bool (DebuggerViewState.h:966). This is an unsynchronized write and read across threads. One overstatement: ASan does not detect data races.

## T1109 (low)

**CassoEmuCore/Shell/ShellDebuggerProject.cpp:150**, correctness

The PROJECT OPEN path in ServiceProject applies the opened project without updating m_restoredScope, unlike SyncProject. If the opened project loads its own symbol files, the scope changes on the next tick and SyncProject treats that as a user-driven switch: it saves the opened project, applies the debug file's automatic project over it, and offers a merge.

Failure: PROJECT OPEN foo.json, where foo lists a debug file whose scope already has an automatic project. On the next service tick the scope differs from m_project.scope. The breakpoints just opened are replaced by the debug-file project's, and the user gets a 'Add the breakpoints ... set for <machine>' prompt that SyncProject suppresses for the equivalent restore.

Verified: The PROJECT OPEN branch of ServiceProject (ShellDebuggerProject.cpp:139-148) calls ApplyProject(opened) after setting opened.scope = m_project.scope, and never updates m_restoredScope. SyncProject does update it, at :247. ApplyEngine runs SYM LOAD for each symbol file (DebugProject.cpp:164-177). A debug file loaded that way calls SetDebugFile and sets m_debugFileKey (SymbolHandlers.cpp:403, DebugSession.cpp:112), which changes GetProjectScope (ShellDebuggerProject.cpp:95). On the next tick SyncProject sees scope != m_project.scope and saves the project. Because scope != m_restoredScope, it computes isOffer true, applies the debug file's project and calls OfferProjectMerge. The restore path would also replace the items on its next tick, but the merge offer is specific to this path.

## T1110 (low)

**CassoEmuCore/Shell/ShellDebuggerReverse.cpp:160**, stale state

Turning recording off while behind live runs GoLive and then StopReverseRecording, but never calls UpdateReplayCaption. The only other caller, ServiceHistoryThumbnails, returns early once TryPublishHistoryPlayhead sees !IsRecording(), so the cached ' [Replaying @ ...]' caption is never cleared.

Failure: Seek back in history, then uncheck recording in Tools > Options. The machine goes live and runs, but the main window title keeps showing [Replaying @ <time>] until recording is turned on again.

Verified: ApplyReverseOptions (ShellDebuggerReverse.cpp:156-163) calls RunReverseCommand(GoLive) and then StopReverseRecording. Neither calls UpdateReplayCaption (RunReverseCommand at :217-305; StopReverseRecording at :116-124). The only callers of UpdateReplayCaption are DivergeHistory (:407) and ServiceHistoryThumbnails (:1157). ServiceHistoryThumbnails returns early when TryPublishHistoryPlayhead fails, and that fails on !m_reverseHost->IsRecording() (:1185). ShellWindow.cpp:604 appends GetReplayCaption() unconditionally, so the stale ' [Replaying @ ...]' stays in the title.

## T1111 (low)

**CassoEmuCore/Shell/TraceHistory.cpp:313**, correctness

FindCycle uses 'found == UINT64_MAX' to mean that the cycle is past the stretch's last instruction. When the machine is running, TryGetEntries only queues a background rebuild and returns false, and FindCycle reads that the same way: it returns the start of the next stretch as if it had found the cycle there.

Failure: While the machine runs, issue TRACE with a starting cycle that falls inside an older stretch that is not rebuilt yet. TryGetEntries returns false, found is set to bounds[i].startPosition, and TraceController::TryFindCycle reports success. The trace window starts up to a whole stretch (about 45k instructions) after the requested cycle, with nothing to show the answer is wrong. The same happens for a stretch that is unavailable.

Verified: TraceHistory.cpp:302-316: when TryGetEntries returns false, which happens while running because it queues RequestAhead and bails (lines 222-227), found stays UINT64_MAX. Line 313 then sets found = bounds[i].startPosition (i was not decremented before the break), the start of the next stretch. The same happens when every entry is unavailable, because find_if requires isAvailable (line 304). TraceController::TryFindCycle (TraceController.cpp:390-436) maps that position to an index and returns true, and TraceHandlers::FindCycle (TraceHandlers.cpp:153) uses it as is. IsStopped is m_cpuManager.IsPaused (ShellDebugger.cpp:2416), so this applies whenever the machine runs.

## T1112 (low)

**CassoEmuCore/Shell/Window/EmulatorWindow.cpp:1212**, error-path

OnDestroy calls SaveAutoState after the CPU thread stops, and on failure it reports through m_chrome->PostNotice. That posts WM_APP_SHOW_NOTICE to an HWND already in WM_DESTROY. The message is discarded with the window and the heap-allocated wstring leaks. The comment ('the window is up for a notice if the write fails') is wrong.

Failure: At exit the automatic state cannot be written: the disk is full, the folder is read-only, or a disk has data its format cannot store. PostMessageW succeeds on the dying window, DestroyWindow then removes the queued message, and the user is never told. The previous auto state, or none, resumes on the next launch with no explanation.

Verified: EmulatorWindow.cpp:1208-1212: OnDestroy (the WM_DESTROY handler) calls SaveAutoState after m_cpuManager.Stop(), and the comment at lines 1210-1211 claims the window is up for a notice. On failure, SaveAutoState (EmulatorShellState.cpp:368-371) calls m_chrome->PostNotice. ShellChrome::PostNotice (ShellChrome.cpp:1762-1780) news a wstring and posts WM_APP_SHOW_NOTICE to m_shell.m_hwnd. m_hwnd is still non-null because nothing in EmulatorShell clears it, so PostMessageW succeeds and the string is not deleted. The window is being destroyed and PostQuitMessage follows at line 1217, so the notice is never shown and the allocation is never freed (the leak is moot at process exit). The user gets no report of the failed write.

## T1113 (low)

**CassoEmuCore/Ui/Debugger/BreakpointColumns.cpp:231**, correctness

The Address sort compares numerically when both rows have an address and by text otherwise. This mix is not a strict weak ordering: an address range and a single address at the same start compare equal numerically but differ as text against a disk row showing the same $XXXX.

Failure: Rows: A=$0300-$0400 (Address kind), B=$0300 (Address kind), C=RWTS breakpoint resolved to $0300. A~B numerically and B~C as text, but C<A as text. std::stable_sort is given an inconsistent comparator, so the order is unspecified and can change between refreshes.

Verified: BreakpointColumns.cpp:226-231 compares numerically only when both rows HasAddress (Address, Memory or MemoryValue, lines 723-727) and otherwise by the lowercased Address cell text. A $0300-$0400 Address row (text "$0300-$0400", GetRange lines 685-689) and a $0300 Address row compare equal numerically. An RWTS disk row shows "$0300" (GetDiskAddress line 672) and is text-equal to the $0300 row, but text-less than the range row. Incomparability is not transitive, so the comparator handed to std::stable_sort is not a strict weak ordering.

## T1114 (low)

**CassoEmuCore/Ui/Debugger/BreakpointHistory.cpp:336**, correctness

When Add re-adds a breakpoint, it moves it back to its old id with BreakpointTable::TryChangeId OR WatchpointTable::TryChangeId. Each table checks only its own entries for a clash, so a re-added breakpoint can take an id a watchpoint now holds, or the reverse, leaving two entries with the same id.

Failure: The pane adds BP 300 (#0) and then deletes it. At the console the user types BPC * (ids restart at 0) and then BPM 400, which becomes watchpoint #0. Pane Undo re-adds BP 300 under a new id; BreakpointTable::TryChangeId(new, 0) finds no breakpoint #0 and succeeds. Breakpoint #0 and watchpoint #0 now both exist, so BPC 0, BPD 0 and the pane's own id lookups hit whichever table is checked first.

Verified: BreakpointHistory.cpp:336-341 calls BreakpointTable::TryChangeId || WatchpointTable::TryChangeId. Each one (BreakpointTable.cpp:715-731, WatchpointTable.cpp:259-275) checks TryFind(to) against its own entries only. Both tables share one id counter (constructed with the same int& nextId, BreakpointTable.cpp:19 and WatchpointTable.cpp:18), and ClearAllBreakpoints resets it to 0 (DebugSession.cpp:1173). Console commands never touch m_breakpointHistory, whose only callers are DebuggerViewState.cpp:2876-2896. A re-added breakpoint can therefore take the id of a live watchpoint.

## T1115 (low)

**CassoEmuCore/Ui/Debugger/BreakpointHistory.cpp:277**, correctness

Apply re-adds the saved entry whenever `from` is empty, without checking whether the handle's breakpoint (or an identical one) is already live. Meanwhile an undo that found nothing to act on still moves the step onto the redo stack. Redo can therefore create duplicate breakpoints, bypassing SetAddress's duplicate check.

Failure: The pane adds BP 300 (#0). The console clears it with BPC 0 and sets BP 300 again (#1). Pane Undo finds #0 gone and reports "No breakpoint changed", but the step still goes to redo. Pane Redo calls Add with the saved entry, giving two BP 300 entries. A duplicate the console would refuse now exists, and each hit is counted twice.

Verified: BreakpointHistory.cpp:277-285: when `from` is empty, Apply calls Add unconditionally. BreakpointHandlers::Add (BreakpointHandlers.cpp:1831-1838) uses AddCopy with no duplicate check. On undo, GetLiveId (lines 388-409) returns nullopt when handle id #0 is gone, so Apply returns an Outcome of None, yet TryUndo still pushes the step onto m_redo (line 101). Redo then re-adds BP 300 alongside the console's #1. TryChangeId(new, 0) succeeds because #0 is free, which leaves two identical BP 300 entries.

## T1116 (low)

**CassoEmuCore/Ui/Debugger/DebuggerViewState.cpp:3031**, parsing

ExecuteWindowLine reads only the first whitespace-delimited token after a window-only command as its argument, so MoveMemoryPane resolves part of an expression and silently ignores the rest.

Failure: In AppleWin mode the user types `MD1 TXTTAB + 2`. argument is `TXTTAB`, and the memory window moves to TXTTAB instead of TXTTAB+2, with no error, even though the comment says the address is read as the Go to box reads an expression.

Verified: DebuggerViewState.cpp:3031 runs `stream >> name >> argument`, which takes only the first whitespace-delimited token. The MiniMemory case passes that token to MoveMemoryPane (line 3058), and the rest of the line is dropped with no check for extra tokens. MoveMemoryPane (lines 3872-3893) resolves only that token with TryResolveMemoryAddress, although its header says the address is read as an expression the way the Go to box reads one. `MD1 TXTTAB + 2` therefore moves the window to TXTTAB and reports success.

## T1117 (low)

**CassoEmuCore/Ui/Debugger/DebuggerViewState.cpp:2047**, correctness

IsRegisterWatch decides a manual watch is a register by comparing its expression text, in any case, with A/X/Y/S/P/PC. WatchHandlers resolves a symbol before a register of the same name, so a symbol watch named `x`, `pc` and the like is taken for a register.

Failure: A user program defines a symbol `X` (or `pc`), and the watch on it is a Symbol watch of the word at that address. Editing its value in the Watches pane builds `R X <value>` (line 1978), which changes the CPU's X register instead of the memory, and the undo (line 2142) is also a register write.

Verified: TryResolveTyped checks for a symbol before a register (WatchHandlers.cpp:296-302) and gives a symbol watch the expression symbol.name + suffix, so a symbol named X or pc gets the expression 'X' or 'pc'. The snapshot's WatchLine carries no kind (DebuggerViewState.cpp:231). IsRegisterWatch compares GetUpper(expression) with A/X/Y/S/P/PC (lines 2043-2050), so that symbol watch counts as a register. A symbol watch's value is editable, so the edit builds GetSetRegister(GetUpper(expression), ...) (line 1978), and the undo is also a register write (line 2142). Both change the CPU register, not memory.

## T1118 (low)

**CassoEmuCore/Ui/Debugger/DebuggerViewState.cpp:288**, data-race

BuildCaptured reads m_isBasicShown on the pool thread under the view lock. ShellDebugger::SetDebugBasicShown writes it on the CPU thread without that lock, unlike every other view-state change the publisher's contract covers.

Failure: A BASIC pane is shown or hidden (the CpuCommandDispatcher calls SetDebugBasicShown) while a DebugViewPublisher build is running on the pool. The unsynchronized read and write of the bool are a data race and undefined behavior. In practice the build reads a stale value, and TSAN flags it.

Verified: BuildCaptured reads m_isBasicShown at DebuggerViewState.cpp:288 on the publisher's pool thread, under m_viewLock (DebugViewPublisher.cpp:125-130; DebugViewBuilder.cpp:43). CpuCommandDispatcher::DispatchDebugView (CpuCommandDispatcher.cpp:653-655) calls ShellDebugger::SetDebugBasicShown, which calls m_debugViewState.SetBasicShown (an unsynchronized bool write, DebuggerViewState.h:591) and never takes m_debugViewStateLock (ShellDebugger.cpp:1778-1782). The other heat setters are also unlocked, but their state is read only in BuildHeatMap on the CPU thread (line 497 onward). The view changes that the pool reads are locked (ShellDebugger.cpp:1621, 1686, 1706, 1729).

## T1119 (low)

**CassoEmuCore/Ui/Debugger/FloatingPanes.cpp:1194**, focus

Dock erases the window's entry in m_focus without calling OnFocusChanged(false) on the control it held, unlike SetRoutedFocus and ForgetFocus. When that control returns to the debugger window, m_focusMgr does not consider it focused, but the control still thinks it is.

Failure: The user clicks into a floating console's command line (OnFocusChanged(true)) and then docks the pane from its pin. Dock moves the box back and drops m_focus[key]. OnFloatsSynced leaves m_focusMgr on its previous control. The command line keeps drawing its focused look and blinking caret while keys go to a different control, until it is focused and unfocused again.

Verified: Dock (FloatingPanes.cpp:1194) calls m_focus.erase(key) without calling OnFocusChanged(false). SetRoutedFocus (134-155) and ForgetFocus (171-183) both call it. Neither OnFloatDocked (DebuggerWindowLayout.cpp:1375-1399) nor DockBackWindow (FloatingPanes.cpp:664-691) refocuses or unfocuses anything. OnFloatsSynced (DebuggerWindowLayout.cpp:1217-1231) keeps m_focusMgr's current control when that control is routable, and DxuiFocusManager::RecoverFocus does not notify the control that left. DxuiTextInput::OnFocusChanged sets m_focused (DxuiTextInput.h:150), and that flag drives the outline (cpp:564) and the caret (cpp:672). So the command line keeps its focused look while m_focusMgr has another control.

## T1120 (low)

**CassoEmuCore/Ui/Debugger/FloatingPanes.cpp:1368**, persistence

A floating window's new place is written only when it is dropped outside the dock site. Two other changes reach only the in-memory window, and SaveRects runs only from a later SaveLayout: resizing the window (the caption tracker reports Canceled, which saves nothing) and moving it to a spot over the debugger that is not a drop zone (inside is true and EndDrag changes nothing). OnWindowClose and shutdown never call SaveLayout either.

Failure: The user resizes a floating Memory window, or drags it over the debugger window and lets go away from any compass or strip, then quits Casso without any other layout change. The saved layout still holds the old rect, so on the next launch the window comes back at its previous size and place.

Verified: SaveRects runs only inside DebuggerWindow::SaveLayout (DebuggerWindowLayout.cpp:1414-1420). OnDrag calls SaveLayout only when !inside (FloatingPanes.cpp:1368-1371). A drop inside the site with no zone and no strip leaves EndDrag's changed=false, so neither NotifyChanged nor OnChanged nor SaveLayout runs. A resize makes the tracker set m_resized, and Finish returns None or Canceled (DxuiCaptionDragTracker.cpp). The cancel lambda (FloatingPanes.cpp:1131-1138) does not save. OnWindowClose (DebuggerWindow.cpp:842-870) calls SavePlacementIfMoved and SaveFocusedPane but not SaveLayout. The shell calls nothing else on the debugger window at shutdown, only DetachHost (ShellDebugger.cpp:2548).

## T1121 (low)

**CassoEmuCore/Ui/Debugger/Panes/BreakpointsPane.cpp:364**, correctness

The selection and the Rename menu item refer to breakpoints by row index. ApplyRows re-sorts or reshapes m_order on every snapshot, while DxuiListView::SetRows keeps the selection by index. Delete, Go to and Rename can therefore act on a different breakpoint than the one the user picked. The Rename lambda captures `row` rather than the id.

Failure: The list is sorted by Hit count while the machine runs. The user right-clicks row 2 (BP A). A snapshot arrives with new hit counts that reorder the rows, and the user then picks Rename (or presses Delete on the selection). BeginNameEdit(2) or GetSelected() now resolves to BP B, which is renamed or deleted instead.

Verified: BreakpointsPane::Apply (line 256-261) calls ApplyRows on every snapshot. ApplyRows recomputes m_order with GetOrder for the active sort (lines 1065-1068), and HitCount sorts by hits (BreakpointColumns.cpp:221-224). DxuiListView::SetRows (DxuiListView.cpp:107-149) replaces m_rows and leaves the selected row indices untouched. GetSelected (BreakpointsPane.cpp:740-761) and GetBreakpointOfRow (lines 1113-1129) resolve rows through the current m_order. The Rename item captures `row`, not bp.id (line 364), unlike the sibling items, which capture bp by value. Nothing in the pane re-selects by id. One step is untraced: whether a snapshot is applied while the context menu is open. The selection drift is traced in full.

## T1122 (low)

**CassoEmuCore/Ui/Debugger/Panes/BreakpointsPane.cpp:453**, correctness

`anyEnabled` for Disable all counts the session stops (BPV, BPBEAM, BRKUNINIT). They are always listed as enabled, and BPD * does not touch them, so the button never switches to enabling all.

Failure: The table holds disabled breakpoints and BPV 64 is set. anyEnabled is true because of the session stop, so each press of Disable all sends BPD * again. The table entries can never be re-enabled with that button, which is meant to enable them all when none is on.

Verified: BreakpointsPane.cpp:453-454 computes anyEnabled over all of snapshot->breakpoints. AddSessionStops (DebuggerViewState.cpp:2783-2816) appends BRKUNINIT, BPV and BPBEAM rows that always have enabled=true. The kDisableAll branch (line 482) sends GetEnableAllBreakpoints(!anyEnabled), which is always a disable while such a stop exists. BreakpointHandlers::Enable with "*" (lines 1370-1386) iterates only the breakpoint and watchpoint tables, so the session stops stay enabled and the button never switches to enabling.

## T1123 (low)

**CassoEmuCore/Ui/Debugger/Panes/FindWidgets.cpp:795**, stale target

Closing a source document leaves its find widget isOpen and possibly active; only PlaceOne hides it. Find() keeps the active widget whenever it is open and the focus pane is not searchable, so F3 searches the invisible text of a closed document.

Failure: Open find in a source document and search 'JSR', then close that document's tab; the widget is hidden but isOpen stays true. Click in the Registers pane and press F3: GetTarget() is the console, but because m_active->isOpen and the focus pane is not the console, the closed document's view is searched. Its hidden status label updates and nothing visible happens. The widget pops back open with the old text the next time a file lands in that slot.

Verified: Source documents are slot panes that persist after close (SourceDocumentPanes.h: m_slot; GetFindView returns m_view unconditionally, line 57). Only Close() sets isOpen=false (FindWidgets.cpp:588); PlaceOne (429-448) only hides the controls. In Find (795), with focus in a pane that has no find view, GetTarget() returns kConsole (302-312). target != m_activePane is true, m_active->isOpen is true, and GetFocusPane() == target is false, so the closed document's widget stays active. Find then calls GetFindView(m_activePane) on that hidden view (811-813) and updates its hidden status label (814-815).

## T1124 (low)

**CassoEmuCore/Ui/Debugger/Panes/FindWidgets.cpp:1333**, persistence leak across projects

LoadSaved clears m_saved, but widgets that already exist (always the console's) keep their previous options and history when the new project has no saved entry for their pane. The next RecordHistory or SetOptions then Save()s that state into the new project. Unlike the console command history, which RestoreWindowState explicitly replaces, find history leaks between projects.

Failure: In project A, search the console for several strings with Regex on. Switch to project B, which has no 'find' window state: LoadSaved("") leaves the console widget's history and isRegex as they were. Up-arrow in B's find box shows A's searches, and the first F3 in B writes A's history and options into B's project file.

Verified: LoadSaved (FindWidgets.cpp:1292-1337) clears m_saved, then calls ApplySaved for each existing widget. ApplySaved returns early when the pane has no saved entry (178-183), so the widget keeps its previous options and history. ReloadProjectState then RestoreWindowState(true) (DebuggerWindowProject.cpp:165, 213) runs this on a project switch, and the console widget always exists (DebuggerWindow.cpp:280). Save (1352-1380) writes every widget's current matchCase, wholeWord, isRegex and history into the new project's "find" state, so project A's find history leaks into project B. The console command history, by contrast, is replaced outright with SetLines (DebuggerWindowProject.cpp:203-211).

## T1125 (low)

**CassoEmuCore/Ui/Debugger/Panes/FlameGraphView.cpp:31**, stale index across data change

SetData replaces m_tree and m_routines but keeps m_zoom whenever it is still in range, and never clears m_click. Node indices belong to one profile's tree, so after a new profile the graph stays zoomed on an unrelated node. A double-click whose first click predates the new data indexes m_tree[m_click->node] (line 375) with an index that may be past the new tree's end.

Failure: Zoom into node 40 of the 'Last 60 s' flame graph, then pick 'Last 10 s' or step, which triggers BuildIfDue. The new tree has more than 40 nodes, so the view opens zoomed on whatever routine is node 40 now, with its unrelated caller chain. If a build finishes within the double-click time after a click on a high-index node and the new tree is smaller, the second click reads past the end of m_tree and m_routines.

Verified: FlameGraphView::SetData (FlameGraphView.cpp:21-38) replaces m_tree and m_routines but resets m_zoom only when it is out of range, and it never touches m_click. ProfilerPane::Apply calls SetData on every snapshot (ToolPanes.cpp:944-945), and each new history build delivers a newly built tree whose node indices are not stable across periods. A kept zoom therefore lands on an unrelated node. In OnMouse (369-375), isDouble checks only time and position, then indexes m_tree[m_click->node] with the old node. If a smaller tree arrived between the clicks, that index is out of range. The window for this is narrow, which fits the low severity.

## T1126 (low)

**CassoEmuCore/Ui/Debugger/Panes/HeatMapPane.cpp:306**, correctness

OnProjectSwitched calls m_view->SetOptions directly and skips SetOptions. The bar's entries, built for the previous project's cumulative setting, are not rebuilt, and the drop-down menus are not rebuilt either.

Failure: Project A's heat map fades and project B's is cumulative. After switching to B the map counts totals, but the bar has no Reset counts entry until the user toggles the mode. Switching the other way leaves a Reset counts button on a map that is fading. The Mode, View and Bank menus also keep A's check marks until something rebuilds them.

Verified: The main claim holds. OnProjectSwitched (HeatMapPane.cpp:298-308) calls m_view->SetOptions directly. Only HeatMapPane::SetOptions (lines 325-343) rebuilds the bar entries, through BuildEntries(options.cumulative), and the Reset counts entry is included only while the map is cumulative (HeatMapBarCommands.cpp:148). The bar therefore keeps the previous project's Reset counts state. The caller, DebuggerWindowProject.cpp:283-286, does nothing more. The drop-down part is wrong: m_bar.SetOnPress calls SetBarMenus (line 126), as does the view bar's RouteToolbarMouse callback (line 647), so the menus are rebuilt with the current check marks every time they open.

## T1127 (low)

**CassoEmuCore/Ui/Debugger/Panes/MemoryMapBar.cpp:606**, undefined-behavior

std::clamp(y, top, bottom - line) is called with hi < lo whenever the strip is shorter than one label line. Passing hi < lo to std::clamp is undefined behavior, and MSVC's STL asserts on it in debug builds.

Failure: Resize the pane holding the memory map bar so its height is between 1 pixel and kHeadingDip plus about 16 pixels. Paint does not return early because the bounds are not empty. PaintBounds then calls std::clamp with bottom - line < top, which in a debug build hits the STL's 'invalid bounds arguments passed to std::clamp' assertion on every paint.

Verified: In PaintBounds (MemoryMapBar.cpp:578-606), top = GetPageY(0) = bounds.top + kHeadingDip (18) and bottom = runs.back().bottom. PlaceRuns (lines 226-296) uses height = max(0, bounds.bottom - top), so bottom = top + height. Once the bounds are shorter than kHeadingDip + line (line = monospace size * 1.3, so about 34 px in total), bottom - line < top, and std::clamp(..., top, bottom - line) is called with hi < lo. The early return in Paint (line 470) covers only empty bounds. This size is reachable: the map sits beside the list (DiagnosticsPane.cpp:48) and is as tall as the list less the inset (DebuggerPaneFrame.cpp:235), and the frame's minimum allows the fill to shrink to kMinFillDip = 20 (DebuggerPaneFrame.h:79, cpp:360). MSVC's debug STL checks clamp's bounds with _STL_VERIFY and fails on hi < lo.

## T1128 (low)

**CassoEmuCore/Ui/Debugger/Panes/SourceDocumentPanes.cpp:396**, correctness

SourceDocumentGroup::Apply clears m_files and m_looseFiles whenever the debug file or its programKey changes. That closes every loose document. SourcePane::Apply says the opposite: 'A loose file keeps its text whatever debug file comes and goes.' The slot is cleared, ApplyDocument passes fileId -1, and SetFile(-1) drops m_isLoose and the text.

Failure: Drop a text or hex-dump file on the debugger (DebuggerWindowInput -> OpenLooseFile). Then load a debug file, unload one, or reload a reassembled one, which changes programKey. The loose document's tab disappears and its contents are lost. The user has to drop the file again after every reassembly.

Verified: SourceDocumentPanes.cpp:389-398: whenever `debugFilePath + programKey` changes, including source going from absent to present or back, Apply runs `m_files.Clear(); m_looseFiles.clear();`. The slot loop at :409-428 then gets fileId -1 from m_files.GetFileId, and ApplyDocument calls `m_model->SetFile(fileId)` (:270). SourcePane::SetFile (SourcePane.cpp:283-304) sets `m_isLoose = false` and, for fileId < 0, clears m_lines/rows/banner. That defeats the `if (m_isLoose)` keep path in SourcePane::Apply (SourcePane.cpp:217, 'A loose file keeps its text whatever debug file comes and goes'). Only the source-beside-a-debug-file drop re-opens itself afterwards, through m_pendingLoose (:432-439). A text or hex file opened with DebuggerWindowInput.cpp:315 -> OpenLooseFile has no such re-open path, so its document is lost on any debug-file load, unload or reassembly.

## T1129 (low)

**CassoEmuCore/Ui/Debugger/Panes/WatchesPane.cpp:600**, correctness

m_isValueEditAfterAdd is set before the add runs and is cleared only once a new watch id appears. If the engine rejects the W command, the flag stays set, and the next watch added from anywhere has its value editor opened unprompted.

Failure: The user Tabs out of the add row with an expression the engine refuses (one the snapshot-side GetWatchError passed). Minutes later a console W 300 adds a watch, and EditAddedValue opens that watch's Value cell for editing, taking keyboard focus to the grid.

Verified: WatchesPane.cpp:600 sets m_isValueEditAfterAdd before RunAction. It is cleared only in EditAddedValue (lines 156-178), once a watch id not in m_idsBeforeAdd appears. GetWatchError (DebuggerViewState.cpp:1906-1909) passes any "BASIC ..." text through to the session. The engine can still reject it (WatchHandlers.cpp:189-207, TryResolveTyped and TryEvaluate errors), and no other path clears the flag. The next watch added from anywhere, the console included, therefore triggers BeginEdit on its Value cell.

## T1130 (low)

**CassoEmuCore/Ui/Debugger/ProjectSettingsDialog.cpp:248**, correctness

OK ignores the result of m_grid.EndEdit(true). If the open edit fails validation (for example an unknown mode), the dialog still copies the old m_commands into m_result and closes, losing what was typed without the error being shown. StepFiltersDialog.cpp:188 has the same problem.

Failure: The user types "BP 300" in the add row with mode "Applewin2" and clicks OK while the cell is still being edited. CommitCell fails, but EndDialog(IDOK) runs anyway: the dialog closes and the command is gone, with no error.

Verified: ProjectSettingsDialog.cpp:246-251 and StepFiltersDialog.cpp:186-191 call m_grid.EndEdit(true), ignore the bool, copy the stored list and EndDialog(IDOK). DxuiEditableGrid::EndEdit (DxuiEditableGrid.cpp:410-444) returns CommitEdit's result. CommitEdit and CommitAddRow (lines 458-569) return false when the add or commit callback fails. ProjectSettingsDialog::AddCommand and CommitCell fail through TryMakeCommand on an unknown mode (lines 188-192), and the Mode column is a free-text cell (ConfigureGrid line 277). The typed command is therefore discarded and the dialog closes.

## T1131 (low)

**CassoEmuCore/Ui/Debugger/RegisterHistory.cpp:46**, correctness

OnSnapshot decides whether a stop is the same one by PC alone, and only clears on a snapshot it actually sees while running. A run that stops again at the same PC within one UI frame keeps the undo list, though registers have changed. StackHistory::OnSnapshot has the same flaw.

Failure: There is a breakpoint inside a loop. The user edits X in the Registers pane (Record before=5, after=7), then presses F5. The loop runs one iteration in under a frame and stops at the same PC with X=6, so the UI only ever sees paused snapshots at that PC and the history is kept. Ctrl+Z then runs R X=05, writing a stale value over the machine's real X. A stack edit undone in the same way writes a stale byte to $01xx.

Verified: RegisterHistory::OnSnapshot clears only when !isPaused || pc != m_pc (RegisterHistory.cpp:44-50), and StackHistory::OnSnapshot is the same (StackHistory.cpp:44-50). RegistersPane calls it with the snapshot's isPaused and pc only (UndoListPanes.cpp:681, 1246). The UI sees only the latest snapshot: PublishDebugSnapshot overwrites m_debugViewSnapshot (ShellDebugger.cpp:2125-2131), and TakeDebuggerUpdate hands over just that one (ShellDebugger.cpp:1411-1426). So a run that ends within one UI frame, at the same PC, is never seen as running. No other path clears the register or stack history on a resume; the only Clear and FromText calls are in project load (DebuggerWindowProject.cpp:246-248). TryUndo returns GetSetRegister(step.name, step.before) without checking the current value (RegisterHistory.cpp:61-75), so it writes the stale value over the machine's real one.

## T1132 (low)

**CassoEmuCore/Ui/Debugger/TrackSectorPredicate.cpp:107**, correctness

TryParseDecimalInt accumulates into a signed int with no bound, so a long digit string overflows (undefined behavior) and can wrap to a small value that is accepted. Sector mode also accepts quarter-track tokens such as 1.5 (isQt ranges, capped at 16) that Matches() always skips, and they are not listed as rejected.

Failure: Typing 4294967298 in the track filter wraps to 2, so track 2 is filtered in with no rejected span shown. Typing 1.5 in the sector filter is accepted, is not highlighted as bad, and matches nothing.

Verified: TrackSectorPredicate.cpp:97-112: TryParseDecimalInt accumulates `v = v * 10 + digit` into an int with no length or bound check, which is signed overflow (UB). With MSVC wrapping, 4294967298 becomes 2, which passes the val < cap check (lines 327-331) with no rejected span. In Sector mode, TryParseDecimalQt accepts "1.5" as qt=6 with isQt=true. GetValueCap (lines 200-219) returns 16 for Sector regardless of isQt, so the range is accepted and not rejected, and Matches (lines 370-393) skips every isQt range.

## T1134 (low)

**Dxui/Widgets/DxuiDockSite.cpp:89**, correctness

SetTitle, SetIndicator, SetLeadingMark and SetTitleExtra update only m_groups, never m_slidGroup. Edge-tab rects, which are sized from the title length in ArrangeEdges, are not recomputed either.

Failure: SetPaneTitle on an auto-hidden pane that is slid out (for example a Source pane whose file changes) leaves the slid pane's title bar showing the old title until something else triggers Arrange. The edge tab keeps its old length, so a longer title is clipped.

Verified: DxuiDockSite::SetTitle (DxuiDockSite.cpp:76-93) updates m_panes and loops only over m_groups. It neither touches m_slidGroup nor calls Arrange. m_slidGroup gets its title only inside Arrange (281, AddTab(slid->second.title, ...)). ArrangeEdges sizes each edge tab from GetTitle(hidden.pane).size()*kCharDip (853), and that too runs only from Arrange. PaintEdges reads the current title (1068) but draws it into the old tab.rect, so a longer title is clipped. Caller: SourceDocumentPane::ApplyDocument (SourceDocumentPanes.cpp:275-279) calls SetPaneTitle when the title changes, and returns relayout only when the shown or banner state changes (285-297). The group then calls RelayoutDock only when relayout is true (430-433), so a title change alone triggers no Arrange. Auto-hide is offered for any docked pane (DxuiDockSite.cpp:1545; DxuiPaneLayout::AutoHide has no document check at 1248-1260). The other setters named, SetIndicator (1182-1199) and the matching SetLeadingMark and SetTitleExtra, follow the same m_groups-only pattern.

## T1135 (low)

**Dxui/Widgets/DxuiEditableGrid.cpp:704**, correctness

Pressing Up in the hint row checks only the current editor's text for emptiness, then calls EndEdit(false), which clears m_pending for every column. Text already typed into other columns of the new row is silently discarded.

Failure: Project Settings grid (Command is required, Mode is optional): click 'Add new', type 'make' in Command, Tab to Mode (whose editor is empty), press Up. MoveEditRow sees that the current text is empty, moves to the last data row and calls EndEdit(false). That runs m_pending.assign(...), so 'make' is lost with no error. Heat Ranges' multi-column hint row loses its Start value the same way.

Verified: DxuiEditableGrid.cpp:684 tests only the current editor's text for emptiness before moving Up out of the hint row. Tab from Command goes MoveEdit -> CommitAddRow(text, next, isMove=true) (636-638), which stores 'make' in m_pending (516) and reopens the hint row on Mode. Mode's editor text is m_pending[Mode], which is empty (GetEditText 305-307). On Up, isAdd is true, so EndEdit(false) runs (702-704), and that runs m_pending.assign(m_columns.size(), L"") (429-431). No error is reported and the typed Command is gone. This contradicts MoveEditRow's own comment (666-667): from a hint row with text, Up goes nowhere.

## T1136 (low)

**Dxui/Widgets/DxuiListView.cpp:4942**, correctness

OnKey handles Space as a checkbox toggle before any keyboard sub-stop routing. When the header sub-stop has focus in a list whose selected row has a checkbox, Space toggles the row's checkbox instead of sorting the focused column.

Failure: Breakpoints pane (rows with enabled checkboxes, keyboard column nav on): Tab to the header sub-stop and press Space to sort the focused column. ToggleChecks(false) is true because the selected breakpoint has a checkbox, so ToggleChecks(true) enables or disables the selected breakpoints. The sort never happens; only Enter sorts.

Verified: DxuiListView.cpp:4942-4945: OnKey handles VK_SPACE through ToggleChecks before any check of m_kbColFocus. ToggleChecks(false) (2831-2877) returns true when a selected row has a check cell, and only then does OnKeyBodyHeaderNav run. Its header branch (5095-5098) would otherwise sort on Space. DebuggerWindow.cpp:538 turns on keyboard column nav for debugger lists. BreakpointsPane sets OnSortColumn (178), its rows have enabled checkboxes, and RouteEditKey (983-1004) does not take Space when no rename is open. On the header sub-stop, Space therefore toggles the selected breakpoints and does not sort; only Enter sorts.

## T1137 (low)

**Dxui/Widgets/DxuiTabGroup.cpp:186**, correctness

ReplaceTabs copies the old Tab, indicator included, and sets m_active directly. DxuiDockSite::Arrange then calls SetActive(active), which returns early because the index is already active, so the indicator is never cleared. SyncStrip draws the dot for any tab with indicator set, the active one included. When a pane moves to another group it gets a fresh Tab{} and loses its indicator, although m_panes still records it.

Failure: MarkPaneTab sets the indicator on a background tab. A breakpoint hit or ActivatePaneTab (EditPaneLayout().Activate followed by Relayout, or DxuiDockSite::ActivatePane) then brings that tab to the front. The selected tab keeps showing the 'changed' dot until the user clicks away and back. Conversely, dragging a marked pane into another group silently drops its dot.

Verified: ReplaceTabs (DxuiTabGroup.cpp:185-199) copies the existing Tab, indicator included, when IndexOf(content) >= 0, and sets m_active straight from the argument. Arrange then calls group->SetActive(active) (DxuiDockSite.cpp:256). SetActive (DxuiTabGroup.cpp:457) returns early when index == m_active, so it never reaches the indicator=false at 463. SyncStrip (905-947) draws the dot for any tab with tab.indicator set and no leading mark, the active tab included. The only guard is in DxuiTabGroup::SetIndicator (248, on && index != m_active), and that runs only when the mark is set. Path: MarkPaneTab (DebuggerWindowPanes.cpp:888-893) calls DxuiDockSite::SetIndicator. Later, DxuiDockSite::ActivatePane (DxuiDockSite.cpp:1163-1169, called from DebuggerWindowPanes.cpp:816 and elsewhere) runs layout.Activate and Arrange, and the dot stays on the now-active tab. In the other direction, a pane landing in a different group gets Tab{} (indicator false). Arrange re-applies leadMark, tip and titleExtra from m_panes (242-244) but never the indicator, so the dot is lost even though Pane::indicator is still true.

## T1138 (low)

**Dxui/Window/DxuiMessageBox.cpp:381**, correctness

The fallback used when the Dxui dialog cannot be created now always shows MB_OK or MB_OKCANCEL. It maps IDOK to the first button's id and everything else to IDCANCEL. This changes the result for the classic DxuiMessageBox(uType) path, which now routes through this overload.

Failure: Dialog creation fails (device lost or out of resources) for MB_YESNO, as ShellDebuggerProject.cpp:483 and CassqueWindow.cpp:4612 use. The user sees OK/Cancel instead of Yes/No, and clicking Cancel to mean 'No' returns IDCANCEL rather than IDNO. A caller testing `== IDNO` takes the wrong branch. For MB_YESNOCANCEL the No choice is gone entirely. On master the fallback passed uType through unchanged.

Verified: The uType overload (DxuiMessageBox.cpp:283-307) now maps MB_YESNO and MB_YESNOCANCEL to buttons and calls the buttons overload. Its fallback (line 381) passes `(uType & ~MB_TYPEMASK) | (buttons.size()>1 ? MB_OKCANCEL : MB_OK)`, and line 382 maps IDOK to buttons.front().id and everything else to IDCANCEL. The merge base passed uType unchanged (`MessageBoxW (owner, text, caption, uType)` at old line 321). So on that path, MB_YESNO shows OK/Cancel, No comes back as IDCANCEL, and MB_YESNOCANCEL loses No. The practical damage is small: the callers I found (ShellDebuggerProject.cpp:483, CassqueWindow.cpp:4612, CodeEditCommands.cpp:297, and others) test for IDYES, and no caller compares against IDNO. The behavior change itself is real.
