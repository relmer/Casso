# 041 audit: the confirmed defects, re-checked against 035

Each entry was found on `master` by a four-finder inventory, confirmed by at least two of three adversarial reviewers, and re-checked against `035-debugger` at `811a6f727`. File and line references are to that commit. The status says what 035 did to it.

## failed-mount-dangling-disk-image [changed-but-present]

**Evidence:** The core defect is unchanged in the worktree. DiskImageStore::MountFromBytes (CassoEmuCore/Devices/Disk/DiskImageStore.cpp:242-304) still retires the occupied bay before it has checked that the new bytes load. :263-268 flushes with FlushMoment::Running and calls RetireBay. :271 does `entry.image = make_unique<DiskImage> ()`. :276 calls LoadFromBytes. On !IsLoaded, :280-287 clears the bay and sets hr = E_FAIL. DiskImageStore::Mount's `CHR (hr)` at :620 then jumps to Error (:642), past BeginWatching (:630), EmitBayChange(Inserted) (:635) and NotifyMediaChanged (:640). The only thing that re-points the drive is still EmitBayChange -> DiskManager::OnBayChange -> controller->SetExternalDisk (CassoEmuCore/Shell/DiskManager.cpp:534-565, :564). The controller keeps the old pointer in m_activeDisk[drive] and the engine's m_disk (Machines/Apple2/Common/Disk2Controller.cpp:806-812, Disk2NibbleEngine.cpp:53-57). Nothing on the failure path fixes that. MountDiskInSlot6 only reports the outcome (DiskManager.cpp:466-467, :485-488), HandleMountCompletion only shows a message (Shell/EmulatorShellDisks.cpp:232-259), an Eject of the now-empty bay returns at once (DiskImageStore.cpp:2297), and Disk2Controller::Reset puts the same pointer back (:910-914). The stale pointer is then read by: the write-protect sense (Disk2Controller.cpp:337), StepLss (Disk2NibbleEngine.cpp:595, ReadBit :617, IsWriteProtected :672, WriteBit :692, GetTrackBitCount :695), and SoftReset (Disk2Controller.cpp:942-944). On Reset, RemountDisks runs right before SoftReset (Shell/CpuCommandDispatcher.cpp:69-70), so the drive is dereferenced immediately. Every trigger from master is still live: IDM_DISK_INSERT (CpuCommandDispatcher.cpp:86-89 -> EmulatorShellCpuThread.cpp:434); the machine switch's remount (MachineManager.cpp:606, after PowerCycle at :587); the salvage insert (EmulatorShellDisks.cpp:674); and RemountSlot6Disks (DiskManager.cpp:668/:676). There is also a new one: an external tool's InsertDisk intent (Shell/Window/EmulatorWindow.cpp:2768). The comment there at :2762-2764 says an occupied drive is "flushed and ejected" first; no eject happens. The RemountSlot6Disks header (DiskManager.cpp:644-647) still says the remount goes through Eject + Mount, which is also false. MountExternallyModifiedDisk still shows the safe pattern: it loads into a fresh image first (DiskImageStore.cpp:3498-3525), and its comment at :3520-3524 describes this exact hazard.

What 035 changed:
(1) The bare `entry.image.reset()` became RetireBay (DiskImageStore.cpp:2197-2221). With media retention off, the old image is still freed at once (:2216), exactly as on master. With retention on (ReverseController::Start turns it on at ReverseController.cpp:99 whenever reverse recording runs), the old image moves into m_retained (:2203-2213) and is not freed yet. That turns an immediate use-after-free into a deferred one with extra damage. The guest keeps reading and writing a disk that the bay reports as empty (GetMediaId returns 0, :2047). No flush reaches that disk, because FlushEveryBay walks only m_entries (:2262-2273), so the guest's later writes are lost. Keyframes record the bay as empty, so a reverse step or replay seats an empty drive (SeatMedia, then BindDiskDrives at MachineHost.cpp:1791) and no longer matches what the live run did. The pointer finally dangles when PruneRetainedMedia drops that image (ReverseController.cpp:2156 -> DiskImageStore.cpp:2181), or when Stop turns retention off (ReverseController.cpp:143 -> DiskImageStore.cpp:2065-2068).
(2) RetireBay now calls sharedState.Eject() (:2220), so the master report's "old identity stays recorded" is fixed. EndWatching is still skipped.
(3) RemountSlot6Disks now returns early under the flush hold (DiskManager.cpp:661-664), so a Reset or Power cycle while the machine is behind live in its history no longer gets here. A Reset at the live end, and every insert, still do.
(4) MachineHost::PowerCycle now calls BindDiskDrives (MachineHost.cpp:992), which re-points the drives. But IDM_MACHINE_POWERCYCLE runs RemountDisks after it (CpuCommandDispatcher.cpp:74-75), so the dangling pointer is created after that rebind. A later power cycle or a reverse SeatMedia heals it; nothing else does.
(5) MountRestored (DiskImageStore.cpp:664-705) has the same pattern: it runs EndWatching and RetireBay (:684-685) before MountFromBytes, and `CHR` at :690 skips EmitBayChange. It cannot be reached today, because MachineStateFile::CheckDisks loads every saved image first (Shell/MachineStateFile.cpp:510-514), and Apply rebinds the drives through LoadStateOverMountedMedia (MachineHost.cpp:1671-1673).

The flush inside MountFromBytes is FlushMoment::Running and does not check m_isFlushHeld. Per DiskImageStore.h:133-136 it is one of the flushes that still write while the hold is on, so before the fix a declined mount under the hold also wrote the outgoing disk to its file.

**035 impact:** The right fix is the master fixDirection's first option (load first, then swap), with three points 035 forces.

(a) The outgoing disk must still leave through RetireBay, never a bare reset() and never the move-assign trick in MountExternallyModifiedDisk (`*entry.image = std::move (*loaded)`). Retention must keep the outgoing medium with its own image id so a keyframe taken before the swap can still seat it. And a newly inserted disk needs a new id (DiskImage::RenewIdentity, DiskImage.cpp:1046-1052), which only a new DiskImage object gets.

(b) The flush hold needs no new handling. The mount flush is one of the flushes that write while the hold is on, the same as an eject (DiskImageStore.h:133-136). It moves after a successful load, so a declined mount writes nothing, held or not. That is also what reverse execution needs from a mount that did not happen.

(c) With retention on, a declined mount must retire nothing. Otherwise the keyframes record an empty bay that the live run never had, and the replay no longer matches the live run.

The second fixDirection option, emptying the bay and emitting Ejected, is worse under 035. The guest's disk goes into m_retained with its unsaved writes, after a flush that wrote it anyway. And a Reset at the live end would leave the user with an empty drive because a build tool had left the file half-written.

The step and pause machinery is not involved: everything here runs on the CPU thread inside the command dispatch. RemountSlot6Disks's early return under the hold (DiskManager.cpp:661-664) stays as it is. The fix also gives the remount-discards-dirty defect (FlushEntry's result is ignored at :265-266) a single place to decline before anything changes. It also gives the watch-leak defect the place for EndWatching on the old path, just before RetireBay.

**Proposed fix:** 1. DiskImageStore::MountFromBytes (CassoEmuCore/Devices/Disk/DiskImageStore.cpp:242-304): load into a local image first, and touch the Entry only after the load succeeds. Replace the body after the diagnosis setup with:

    bool                   isValid  = IsValidBay (slot, drive);
    bool                   usable   = false;
    HRESULT                hrAssess = S_OK;
    string                 path     = virtualPath;   // RetireBay clears entry.path, which a caller may have passed by reference
    unique_ptr<DiskImage>  loaded;
    SalvageAssessment      assessment;
    ...
    CBRAEx (isValid, E_INVALIDARG);

    //  THE NEW DISK LOADS BEFORE THE OLD ONE LEAVES. The Disk II holds a raw
    //  pointer to the bay's DiskImage, and only the bay change from a mount
    //  that worked re-points it. A decline emits nothing, so it leaves the
    //  disk the drive is reading exactly where it was: not flushed, not
    //  retired, not freed.
    loaded = make_unique<DiskImage> ();
    loaded->LoadFromBytes (fmt, bytes, path);

    usable = loaded->IsLoaded();
    CBRFEx (usable, E_FAIL, outDiagnosis = ClassifyLoadFailure (fmt, bytes));

    {
        Entry &  entry = GetEntry (slot, drive);

        if (entry.mounted)
        {
            hr = FlushEntry (entry, FlushMoment::Running);
            IGNORE_RETURN_VALUE (hr, S_OK);

            RetireBay (entry);
        }

        entry.image   = std::move (loaded);
        entry.path    = path;
        entry.format  = fmt;
        entry.mounted = true;

        hrAssess             = AssessSalvage (slot, drive, assessment);
        entry.salvageOffered = SUCCEEDED (hrAssess) && assessment.isOffered;
    }

Delete the old failure branch at :278-288. Its "leaves the slot empty rather than half-mounted" now means "leaves the bay as it was", which the new comment says. Update the banner at :231-240 to match. Mount (:584-644) needs no change: on failure it already skips sharedState.Mount, BeginWatching, EmitBayChange and NotifyMediaChanged, and every one of those is correct for a bay that still holds its old disk.

2. MountRestored (DiskImageStore.cpp:679-690), as hardening. It empties the bay itself before calling MountFromBytes, so a failure there still leaves the drive pointing at the retired image. Change the `CHR (hr)` at :690 to `CHRF (hr, EmitBayChange (slot, drive, BayChange::Ejected))`: the bay really is empty at that point, so the sink sets the drive back to its internal disk. This cannot be reached today, because MachineStateFile::CheckDisks pre-loads every image, but nothing in MountRestored itself enforces that.

3. Comments. Rewrite the RemountSlot6Disks header (CassoEmuCore/Shell/DiskManager.cpp:641-647). There is no Eject + Mount; each disk is mounted over itself. The paths are copied first because retiring the bay clears the string a GetSourcePath reference would point at. A file that no longer loads is declined, and the disk already in the drive stays. Also correct EmulatorWindow.cpp:2762-2764 ("flushed and ejected"): the drive's disk is flushed and replaced, and an image that does not load leaves the drive as it was.

**Regression test:** Primary test: extend UnitTest/EmuTests/DiskImageStoreTests.cpp, which already includes Disk2Controller.h (:10) and builds a standalone `Disk2Controller ctrl (6)` at :732. Add TEST_METHOD (Mount_UnloadableOverAMountedDisk_LeavesThatDiskInTheDrive) after MountFromBytes_ShortDsk_RefusesWithALengthReasonAndNoAssert (:1574-1596).

Setup:
- SetImageReader returns MakeDsk (0x11) for "C:\\disks\\Good.dsk" and `vector<Byte> (4096, 0)` for "C:\\disks\\Truncated.dsk".
- SetIdentityReader returns ImageIdentity().
- SetFlushSink counts writes.
- SetBayChangeSink does what DiskManager::OnBayChange does: `if (slot == kSlot) ctrl.SetExternalDisk (drive, store.GetImage (slot, drive));`.

Steps:
- AssertSucceeded (store.Mount (kSlot, kDrive, "C:\\disks\\Good.dsk")).
- Capture `mounted = store.GetImage (kSlot, kDrive)` and assert ctrl.GetDisk (kDrive) == mounted.
- Dirty it with mounted->WriteBit (0, 0, 1).
- Call store.Mount (kSlot, kDrive, "C:\\disks\\Truncated.dsk", diagnosis).

Assertions, in this order so that a red run fails before anything dereferences a freed pointer:
- AssertFailed (hr), and diagnosis.failure == MountFailure::WrongSizeForFormat.
- store.IsMounted (kSlot, kDrive) is true (false before the fix).
- store.GetImage (kSlot, kDrive) == mounted (nullptr before the fix).
- ctrl.GetDisk (kDrive) == store.GetImage (kSlot, kDrive).
- GetSourcePath is still "C:\\disks\\Good.dsk".
- mounted->IsDirty() is true, and the flush count is 0 (1 before the fix: the old code wrote the disk and then threw it away).

Finally, call ctrl.SoftReset(), which reaches Disk2Controller.cpp:942-944, the Reset command's next step. It is safe after the fix and a use-after-free before it.

035-specific companion: extend UnitTest/EmuTests/DiskFlushHoldTests.cpp next to EjectedDiskIsKeptWhileRetained (:143) with TEST_METHOD (AFailedMountRetiresNothingWhileRetained).
- PrepareDirtyDisk, SetPositionSource (&now), SetMediaRetention (true), record mediaId = GetMediaId.
- MountFromBytes (s_kHoldSlot, s_kHoldDrive, "bad.nib", DiskFormat::Nib, std::vector<Byte> (100, 0)) fails.
- Assert GetRetainedMediaCount() == 0 (1 before the fix), GetMediaId == mediaId (0 before the fix), HasUnsavedWrites() is true, and files.writes == 0.
- Then SetMediaRetention (false), as ReverseController::Stop does, and assert the bay still holds mediaId.

Optional shell-level scenario: extend UnitTest/EmuTests/DiskResetRemountHoldTests.cpp with RemountOfAFileThatNoLongerLoadsKeepsTheDisk.
- Build the existing DiskManager rig, but keep `manager` alive and set store.SetBayChangeSink to forward to manager.OnBayChange.
- manager.MountDiskInSlot6 (0, "remount.nib") with MakeImage(); capture the image.
- Switch the reader to return a truncated buffer, then call manager.RemountSlot6Disks().
- Assert store.GetImage (6, 0) == image, and machine.GetRefs().diskController->GetDisk (0) == image.
- Then call machine.SoftReset(), the same order IDM_MACHINE_RESET uses.

**Sites:** CassoEmuCore/Devices/Disk/DiskImageStore.cpp:263, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:268, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:271, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:280, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:620, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:635, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:685, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:690, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2203, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2216, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2067, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2181, CassoEmuCore/Shell/DiskManager.cpp:466, CassoEmuCore/Shell/DiskManager.cpp:564, CassoEmuCore/Shell/DiskManager.cpp:644, CassoEmuCore/Shell/DiskManager.cpp:676, CassoEmuCore/Shell/EmulatorShellDisks.cpp:232, CassoEmuCore/Shell/EmulatorShellDisks.cpp:674, CassoEmuCore/Shell/CpuCommandDispatcher.cpp:69, CassoEmuCore/Shell/CpuCommandDispatcher.cpp:75, CassoEmuCore/Shell/CpuCommandDispatcher.cpp:88, CassoEmuCore/Shell/MachineManager.cpp:606, CassoEmuCore/Shell/Window/EmulatorWindow.cpp:2762, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:337, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:810, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:913, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:942, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:57, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:617, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:692, CassoEmuCore/Debugger/Reverse/ReverseController.cpp:99, CassoEmuCore/Debugger/Reverse/ReverseController.cpp:143, CassoEmuCore/Debugger/Reverse/ReverseController.cpp:2156

## salvage-mount-on-ui-thread [changed-but-present]

**Evidence:** Still present. RunSalvageFlow ends with a direct, synchronous mount: CassoEmuCore/Shell/EmulatorShellDisks.cpp:672-677 calls `m_diskManager->MountDiskInSlot6 (drive, assessment.suggestedPath)`. Both callers are on the UI thread. The menu calls it at Shell/WindowCommandManager.cpp:1501, under the comment at :1500 that says it "Stays on the UI thread". ReportDamagedMount calls it at EmulatorShellDisks.cpp:751, and only after the bounce at :713-720 has moved it to the UI thread. That bounce is reached from WM_APP_REPORT_DAMAGE and WM_APP_MOUNT_COMPLETED (Shell/Window/EmulatorWindow.cpp:2647-2668, through HandleMountCompletion at EmulatorShellDisks.cpp:244). Every other user mount is posted: DiskManager::Mount (Shell/DiskManager.cpp:699-716) calls CpuManager::PostCommand, and the CPU thread runs it through DispatchCpuCommand -> CpuCommandDispatcher.cpp:86-90 -> EmulatorShell::MountDisk (EmulatorShellCpuThread.cpp:432-435). Nothing in RunSalvageFlow pauses the machine or takes the lifetime lock. The only exclusive holder of that lock is SwitchMachine (MachineManager.cpp:489-527).

Every part of the master race is still in the code, mostly at the same lines:
(1) FindSlot6Controller is read unlocked (DiskManager.cpp:446, :550 -> :296-299), and so is m_diskAudioSources (:592-593).
(2) MountFromBytes calls FlushEntry (Devices/Disk/DiskImageStore.cpp:265). That reaches DiskImage::CommitPendingWrite (Devices/Disk/DiskImage.cpp:286-291) -> Disk2NibbleEngine::CommitPendingWrite (Machines/Apple2/Common/Disk2NibbleEngine.cpp:432-447), which clears m_burstActive and m_burstBits while the CPU thread's StepLss (:681) -> RecordFluxWriteBit (:408-419) pushes onto them.
(3) RetireBay (DiskImageStore.cpp:268, body :2197-2221) takes the image the engine holds as m_disk, and the engine keeps stepping on it (StepLss :595, :617, :644, :692, :699). This window lasts through LoadFromBytes (:276), the AssessSalvage decode (:296), sharedState.Mount (:626) and BeginWatching (:630), and ends at EmitBayChange (:635).
(4) OnBayChange then runs on the UI thread:
- SetExternalDisk (DiskManager.cpp:564 -> Disk2Controller.cpp:810-811 -> Disk2NibbleEngine.cpp:53-62) rewrites m_disk, m_bitPos and m_headWindow while StepLss advances m_bitPos (:699, :706).
- NotifyDiskInserted (DiskManager.cpp:580 -> Disk2Controller.cpp:857 -> Ui/Disk2DebugPanel.cpp:1867-1875 PublishToRing) makes the UI thread a second producer on Disk2EventRing. Its contract allows one producer, the CPU thread (Disk2EventRing.h:21).
- The door, audio and sync-event writes are at DiskManager.cpp:612-615, ApplyExternalWriteProtect at :571, and the m_programmaticRemount read at :558.
- The m_currentMachineName read and the UserConfigStore write are at :473-477.

035 changed what this call reaches and widened the damage:
(a) A new last step in DiskImageStore::Mount, NotifyMediaChanged (DiskImageStore.cpp:640, :2233-2238), calls MachineHost::OnMediaChanged (MachineHost.cpp:47, :60-64) -> ReverseController::OnMediaChanged (Debugger/Reverse/ReverseController.cpp:283-296) -> OnMachineChanged (:227-266). OnMachineChanged truncates the input journal, drops keyframes, calls BecomeLive (:2064-2072, which writes the store's m_isFlushHeld and m_isReplaying, the output mute and the held-input watch), and takes a whole-machine CaptureBoundary. All of that now runs on the UI thread while the CPU thread executes and appends its own journal records and keyframes. Reverse recording is on by default (Config/GlobalUserPrefs.h:318) and starts at launch (EmulatorShellCpuThread.cpp:215-219), so this path is live in a default session.
(b) Recording turns media retention on (ReverseController.cpp:99). RetireBay then moves the old DiskImage into m_retained instead of freeing it (DiskImageStore.cpp:2203-2213), so while recording, point (3) is a detached-image write rather than a free; with recording off it is freed as on master. Either way, the UI thread's m_retained.push_back races the CPU thread's CanSeatMedia, SeatMedia and PruneRetainedMedia (:2094-2097, :2144-2150, :2181; callers at MachineHost.cpp:1668-1786 and ReverseController.cpp:1746/2156). It also reads *m_positionSource while the CPU thread advances it (:2211).
(c) 035 put two things on the command queue that the direct call skips. One is the divergence gate: CpuManager::PostCommand (CpuManager.cpp:150) -> EmulatorShell::AllowCommand (EmulatorShellReverse.cpp:303-333), with DivergenceGate::IsStateChangingCommand covering IDM_DISK_INSERTn (DivergenceGate.cpp:25-44). The other is the input journal (CpuCommandDispatcher.cpp:245-250, recorded at EmulatorShellCpuThread.cpp:352-355).
(d) FlushEntry is gated only by m_isReplaying (DiskImageStore.cpp:1090), not by the flush hold, and the header documents that explicit flushes still write (DiskImageStore.h:133-136). A salvage insert made while the machine is behind live therefore writes the past disk to the file and then cuts history, with no question asked.
(e) Nothing new protects the path: HostInputGate and the lifetime lock are taken nowhere in RunSalvageFlow or MountDiskInSlot6.

The CPU-thread users of the same Entry are unchanged and run unlocked: the motor-off FlushAllUnlessHeld and ApplyPendingReload (MachineBuilder.cpp:1521, :1527, :1544), and queued inserts and ejects (CpuCommandDispatcher.cpp:86-95).

**035 impact:** The fix direction from master still holds, and 035 makes posting the only acceptable fix. Running the existing synchronous call under a pause or under the lifetime lock does not fix it, for three reasons.
1. Since 035, the posted path is where the divergence question is asked. EmulatorShell::AllowCommand runs inside CpuManager::PostCommand on the posting thread (EmulatorShellReverse.cpp:303-333). When the machine is behind live it raises the question, which is fine on the UI thread inside the salvage flow. On a yes it queues IDM_DEBUG_DIVERGE ahead of the insert (:326-329), so the CPU thread goes live and lifts the flush hold (ReverseController.cpp:2070) before the mount's FlushEntry writes anything. A no drops the insert.
2. The posted path is also what records the insert in the input journal as InputKind::DiskMount (EmulatorShellCpuThread.cpp:352-355).
3. Posting is the only way NotifyMediaChanged -> ReverseController::OnMediaChanged, the m_retained bookkeeping and BecomeLive run on the CPU thread, which owns them.
The flush hold needs no special handling. A user mount is an explicit flush, which the store is documented to perform (DiskImageStore.h:133-136). The divergence gate makes sure the machine is live before that flush runs.
The removal of StepInstructionWhilePaused does not affect this. A paused machine still drains mount commands (CpuManager.cpp:579-583), so the posted insert lands promptly while paused.
The completion path is unchanged: MountDiskInSlot6 on the CPU thread still fires m_onMountCompleted -> OnMountCompleted -> WM_APP_MOUNT_COMPLETED -> HandleMountCompletion (recent-disks list plus ReportDamagedMount, which stays silent for an undamaged salvaged copy).
The posted payload goes through DiskManager::Mount's fs::path(...).string(). Widening the store's narrow path with fs::path(...).wstring() and narrowing it again uses the same code page both ways, so the payload is the bay's path byte for byte.

**Proposed fix:** 1. Add a static helper on EmulatorShell for the last step of the flow, and declare it in a public test-seam section of EmulatorShell.h near RunSalvageFlow (:1483 is private, so put it in a public: block as SetKeyOwnerFn is at :1434-1442):

    //  Puts the salvaged copy in the drive the way every other user mount
    //  goes in: queued for the CPU thread, which runs the flush, the swap and
    //  the bay change, and journaled, with the divergence question raised
    //  first when the machine is behind live.
    static HRESULT  InsertSalvagedCopy (DiskManager & disks, int drive, const std::string & path);

Define it in EmulatorShellDisks.cpp, with its own banner, spliced ahead of the RunSalvageFlow banner at :585 (insert before the banner, never between a banner and its signature):

    HRESULT EmulatorShell::InsertSalvagedCopy (DiskManager & disks, int drive, const std::string & path)
    {
        HRESULT  hr = S_OK;



        hr = disks.Mount (6, drive, fs::path (path).wstring());
        CHR (hr);

    Error:
        return hr;
    }

2. In RunSalvageFlow (EmulatorShellDisks.cpp:672-677), replace the MountDiskInSlot6 call with:

        HRESULT  hrMount = InsertSalvagedCopy (*m_diskManager, drive, assessment.suggestedPath);

        IGNORE_RETURN_VALUE (hrMount, S_OK);

3. Fix the comments so they state what is now true:
- WindowCommandManager.cpp:1500 becomes "Stays on the UI thread: the flow opens Dxui modals. The insert it ends with is queued for the CPU thread like any other mount."
- EmulatorShellDisks.cpp:710-712 adds the salvage insert to the routes that go through the CPU thread.
- DiskManager.cpp:430-432 is now accurate and can stay.
- Above MountDiskInSlot6 at DiskManager.h:69, add a one-line ownership note: "CPU thread, or the UI thread during Initialize before the CPU thread starts (MountCommandLineDisks); any other UI-thread caller goes through Mount."
This covers the matching part of stale-thread-ownership-comments. RunSalvageFlow's own AssessSalvage and SalvageToFile reads on the UI thread (:610, :637) belong to the separate defect salvage-decode-on-ui-thread and are not changed here.

**Regression test:** Extend UnitTest/EmuTests/DiskResetRemountHoldTests.cpp. Its Shell rig is the only one in the tree that builds a real DiskManager over a TestMachine ("Apple2e", Disk II in slot 6) with a real CpuManager. Add TEST_METHOD (SalvagedCopyIsQueuedForTheCpuThreadAndLeavesTheBayAlone), or a sibling TEST_CLASS in the same file reusing that rig.

Setup:
- Install store.SetImageReader (returning MakeImage()), store.SetIdentityReader (returning ImageIdentity()), and store.SetFlushSink (counting writes), as the existing test does.
- Override the media-change listener with store.SetMediaChangeListener ([&changes] { changes++; }).
- Mount "damaged.nib" into 6/0, keep the DiskImage pointer, and make a guest write with image->WriteBit (0, s_kRemountBitIndex, image->ReadBit (...) ^ 1).
- Set shell->cpuManager.SetCommandGate to a lambda that appends (id, payload) to a vector and returns true.
- Build the DiskManager as the existing test does, then call `hr = EmulatorShell::InsertSalvagedCopy (manager, 0, "damaged (salvaged).nib")`. Use a .nib path so the pre-fix synchronous mount loads cleanly and the failure is the swap itself, not a load error.

Assert:
- hr succeeded.
- Exactly one command was posted: IDM_DISK_INSERT1 with payload "damaged (salvaged).nib".
- shell->cpuManager.HasPendingCommands().
- store.GetImage (6, 0) == image: the disk the drive is running is still in the bay.
- writes == 0: nothing was flushed from the calling thread.
- image->IsDirty(): the guest's write is still the drive's.
- changes == 0: reverse execution was told nothing from the calling thread.
- CpuCommandDispatcher::TryGetJournalInput on the posted command returns true with InputKind::DiskMount, and DivergenceGate::IsStateChangingCommand (IDM_DISK_INSERT1) is true: the queued insert is journaled and gated.

Before the fix, the step is `disks.MountDiskInSlot6 (drive, path)`, which is what RunSalvageFlow ran inline. The test then fails on every check: nothing is posted, the bay holds a new image, writes == 1, and changes == 1. After the fix it passes.

Keep EHM and flat single-exit style in the test, as the test-code rule requires.

**Sites:** CassoEmuCore/Shell/EmulatorShellDisks.cpp:674, CassoEmuCore/Shell/WindowCommandManager.cpp:1501, CassoEmuCore/Shell/EmulatorShellDisks.cpp:751, CassoEmuCore/Shell/EmulatorShellDisks.cpp:713, CassoEmuCore/Shell/Window/EmulatorWindow.cpp:2649, CassoEmuCore/Shell/DiskManager.cpp:446, CassoEmuCore/Shell/DiskManager.cpp:466, CassoEmuCore/Shell/DiskManager.cpp:473, CassoEmuCore/Shell/DiskManager.cpp:550, CassoEmuCore/Shell/DiskManager.cpp:558, CassoEmuCore/Shell/DiskManager.cpp:564, CassoEmuCore/Shell/DiskManager.cpp:571, CassoEmuCore/Shell/DiskManager.cpp:580, CassoEmuCore/Shell/DiskManager.cpp:592, CassoEmuCore/Shell/DiskManager.cpp:612, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:265, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:268, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:296, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:626, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:630, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:635, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:640, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2213, CassoEmuCore/Devices/Disk/DiskImage.cpp:290, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:439, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:57, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:810, CassoEmuCore/Ui/Disk2DebugPanel.cpp:1875, CassoEmuCore/Machines/Apple2/Common/Disk2EventRing.h:21, CassoEmuCore/Debugger/Reverse/ReverseController.cpp:294, CassoEmuCore/Debugger/Reverse/ReverseController.cpp:2070

## restart-power-cycle-detaches-disks [fixed-by-035]

**Evidence:** The restart path itself is unchanged in the worktree. Paths below are relative to C:\Users\relmer\source\repos\relmer\Casso-worktrees\041-disk-integrity.

- CarryOutChangeAction sends ChangeAction::Restart through MountExternallyModifiedDisk (CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3309-3316). That copies the new contents into the same DiskImage object (:3525), emits Swapped (:3537) and NotifyMediaChanged (:3539), then calls m_restartCallback (:3407-3410).
- The callback is still services.requestPowerCycle = m_machineManager->PowerCycle() (CassoEmuCore/Shell/EmulatorShell.cpp:149), installed at CassoEmuCore/Shell/MachineBuilder.cpp:1551. It still fires from the motor-off callback (:1519-1528) and the idle callback (:1542-1545) inside Disk2Controller::Tick.
- MachineManager::PowerCycle still only runs m_machine.PowerCycle(), NotifyDebugReset(true) and ResetUptimeAnchor (CassoEmuCore/Shell/MachineManager.cpp:665-671).
- Disk2Controller::PowerCycle still points both drives at the empty internal disks (CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:973-978).

What changed is MachineHost::PowerCycle. 035 commit df1a163fc ("Keep the Disk II on the disks in the bays across a power cycle") added BindDiskDrives() right after m_memoryBus->PowerCycleAll (CassoEmuCore/Shell/MachineHost.cpp:987-992). BindDiskDrives calls m_refs.diskController->SetExternalDisk(drive, m_diskStore->GetImage(6, drive)) for both drives (MachineHost.cpp:1810-1825). GetImage returns the bay's image, or nullptr for an empty bay, which SetExternalDisk turns back into the internal disk (DiskImageStore.cpp:2411-2415, Disk2Controller.cpp:806-813).

So the restart callback goes callback -> MachineManager::PowerCycle -> MachineHost::PowerCycle, and the drives come back on the bays' images, which already hold the reloaded bytes. The rebind is not undone afterward:
- After either callback returns, Tick touches only the engine tick (skipped when a cycle source is attached) and PumpIdleCallback (Disk2Controller.cpp:567-570, 584-589, 643-646). Neither reassigns m_activeDisk.
- The rebind runs whether or not the flush hold is set. Only the flush at MachineHost.cpp:984 (FlushAllUnlessHeld, DiskImageStore.cpp:1893-1906) checks m_isFlushHeld or m_isReplaying.

On master, `git grep BindDiskDrives master` returns nothing, which confirms that 035 introduced the fix.

What is still missing is a test. The only machine-level test is ReversePowerCycleReplayTests.cpp:55-68 (APowerCycleKeepsTheDrivesOnTheDisksInTheBays). It calls machine.PowerCycle() directly and checks drive 1 only. The restart tests in SharedImageTests.cpp use a bare DiskImageStore and only count callback calls (:212, :1316-1334, :2106-2108). TestMachine's MachineBuildServices leave requestPowerCycle empty (UnitTest/EmuTests/TestMachine.h:73), so in a TestMachine the restart callback does nothing unless a test installs one. As a result, no test runs a stated Restart pick-up and then checks the drives.

There is also a stale comment. CpuCommandDispatcher.cpp:37-41 says the remount after IDM_MACHINE_POWERCYCLE (:73-76) is what keeps the drives from coming up empty. That is now done by MachineHost::PowerCycle; the remount only re-reads the host files.

**035 impact:** 035 already did what the fix direction called the better option: there is one shared rebind, BindDiskDrives, inside MachineHost::PowerCycle. Every power cycle goes through it: the Power cycle command, the restart callback, SwitchMachine (MachineManager.cpp:587), Initialize (EmulatorShell.cpp:605) and a replayed power cycle. The rebind ignores the flush hold, so it still happens while reverse execution holds the disks. In that state RemountSlot6Disks does nothing, which is why df1a163fc moved the rebind into MachineHost in the first place.

The options the fix direction suggested on master are now wrong, not just unnecessary:
- A second SetExternalDisk loop or Inserted events in CarryOutChangeAction would duplicate BindDiskDrives.
- RemountDisks from the callback would re-enter Mount from inside Disk2Controller::Tick, in the middle of ApplyPendingReload. It would also do nothing under the flush hold.

A related issue, not verified end to end: the restart's power cycle never goes through CpuCommandDispatcher::TryGetJournalInput (:241-243), so it is not journaled as InputKind::PowerCycle. Also, the media-change boundary keyframe that ReverseController::OnMachineChanged captures (ReverseController.cpp:227-266, reached from DiskImageStore.cpp:3539) is taken before the power cycle at :3409. That affects history, not whether the drives are attached, and belongs in a separate defect.

**Proposed fix:** The detach needs no production change: MachineHost::PowerCycle (MachineHost.cpp:987-992) already rebinds both slot-6 drives to the store's images, and the restart callback reaches it through MachineManager::PowerCycle.

Two edits only:
1. Add the regression test below so that a later change cannot drop the rebind from the restart path. For example, someone could point requestPowerCycle at a routine that skips MachineHost::PowerCycle, or move BindDiskDrives back out into the command dispatcher.
2. Correct the stale comment above CpuCommandDispatcher::Dispatch (CassoEmuCore/Shell/CpuCommandDispatcher.cpp:37-41). It should say that the power cycle itself puts the drives back on the disks in the bays (MachineHost::PowerCycle), and that the RemountDisks after it re-reads the files so a regenerated image is what boots.

Do not add a rebind or a RemountDisks call in DiskImageStore::CarryOutChangeAction.

**Regression test:** Extend UnitTest/EmuTests/ReversePowerCycleReplayTests.cpp. It already builds a TestMachine and holds APowerCycleKeepsTheDrivesOnTheDisksInTheBays. Add a TEST_METHOD such as AStatedRestartKeepsBothDrivesOnTheDisksInTheirBays:

1. Build `TestMachine machine ("Apple2e")` and call machine.PowerCycle().
2. Replace the store's seams the way DiskHistoryTests.cpp:446-455 and the SharedImageTests rig do:
   - SetClock returns a local nowMs.
   - SetImageReader fills NibblizationLayer::kImageByteSize bytes with a local `fill`.
   - SetIdentityReader returns an ImageIdentity with recorded = true and modifiedUnix taken from a per-path stamp map.
   - SetFlushSink returns S_OK.
3. Install what EmulatorShell.cpp:149 installs, since TestMachine leaves requestPowerCycle empty: `store.SetMachineRestartCallback ([&] () { restarts++; machine.PowerCycle(); })`.
4. Mount "C:\\work\\Boot.dsk" at (6,0) and "C:\\work\\Work.dsk" at (6,1). Call SetExternalDisk on both drives, as the shell's bay-change path does.
5. Set fill = 0x22 and bump Boot.dsk's stamp. Call store.NoteExternalChange("C:\\work\\Boot.dsk", ExternalChangeIntent::Restart), advance nowMs by MountedImageState::kQuietPeriodMs, and call store.ApplyPendingReload().
6. Assert:
   - restarts == 1, which shows the Restart action ran and the machine was power cycled.
   - For both drives, diskController->HasExternalDisk(drive) is true and diskController->GetDisk(drive) == store.GetImage(6, drive).
   - The first track byte of GetDisk(0) reflects the reloaded fill.

To show it is a real regression test, remove the BindDiskDrives() call at MachineHost.cpp:992 (master's behavior). HasExternalDisk is then false for both drives and the test fails; with the line in place it passes. This complements SharedImageTests.cpp:1316-1334, which only counts callback calls on a bare store, and the existing machine-level test, which covers a direct power cycle on drive 1 only.

**Sites:** CassoEmuCore/Shell/MachineHost.cpp:992, CassoEmuCore/Shell/MachineHost.cpp:1810, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3409, CassoEmuCore/Shell/EmulatorShell.cpp:149, CassoEmuCore/Shell/MachineManager.cpp:667, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:976, CassoEmuCore/Shell/CpuCommandDispatcher.cpp:37

## step-on-ui-thread-cpu-not-parked [fixed-by-035]

**Evidence:** 035 commit c8680f325 ("fix(debugger): run the main window's Step on the CPU thread") took the step off the UI thread and deleted EmulatorShell::StepInstructionWhilePaused. A search of the worktree shows no definition or caller of it.

- UI side: in CassoEmuCore/Shell/WindowCommandManager.cpp:751-762, IDM_MACHINE_STEP still gates on IsPaused (:753). It then only calls m_shell.PostCommand(id) (:760), which forwards to CpuManager::PostCommand (EmulatorShell.cpp:1335-1338). The UI thread no longer touches the CPU, the bus, Disk2Controller, the engine, the audio sources or the framebuffer.
- CPU side: a paused CpuManager::ThreadProc wakes on HasPendingCommands (CpuManager.cpp:559-576) and drains the queue (:583) before its pause check (:590). CpuCommandDispatcher.cpp:78-79 routes the step to EmulatorShell::StepInstruction (EmulatorShellCpuThread.cpp:379-405), which runs on the CPU thread.
  - With a debugger session attached, it is the session's StepInto (:385-393).
  - Without one, it runs SampleHostInputs, MachineHost::StepOne, RenderFramebuffer and PublishFramebuffer (:400-404).
  - It runs no ExecuteCpuSlices frame. RunOneFrame (EmulatorShellCpuThread.cpp:963-967) now has no callers.
- Why the race is gone: the step, the drained eject/insert/write-protect/resolve/reset/power-cycle commands and m_onFrame all run in sequence on one thread. Disk2Controller, Disk2NibbleEngine, DiskImage, Disk2AudioSource, the Disk2EventRing producer, the motor-off/idle callbacks (FlushAll, ApplyPendingReloadToBay, MountExternallyModifiedDisk, RepointBayToFile, Restart) and the framebuffer render all have a single writer again. The step is serialized with the in-flight frame, so pause no longer needs to be a handshake.
- 035's journaling flush hold is respected for free: DispatchCpuCommand (EmulatorShellCpuThread.cpp:344-358) journals the step before dispatch, as it does every other command.
- Debugger actions are also posted, not run on the UI thread: IDM_DEBUG_COMMAND and IDM_DEBUG_ACTION at EmulatorShellDebugger.cpp:825/840/864.
- Remaining residue (not this defect's race):
  - The block comment at WindowCommandManager.cpp:687-700 still says Step is driven directly from the UI thread, that a paused CPU thread is "blocked in pauseCV.wait", and that posting would deadlock. All three claims are false in the worktree. This is the separate stale-step-comments defect.
  - RunOneFrame is now dead code (declared at EmulatorShell.h:400).

**035 impact:** 035 already applied the first half of the master fixDirection: post IDM_MACHINE_STEP and render and publish on the CPU thread. The handshake alternative is no longer needed, because no UI-thread code drives the machine. Nothing in 041 should reintroduce a UI-thread step. Any new UI entry point that advances the machine (a Run-one-frame button, for example) must post a command and run in DrainCommandQueue too. It must not call RunOneFrame or MachineHost::StepOne directly, since TogglePaused and SetPaused (CpuManager.cpp:215-256) are still plain flag stores with no parked acknowledgment. The step goes through DispatchCpuCommand, so it is journaled and sits inside 035's flush hold like every other CPU-thread command. No extra flush handling is needed.

**Proposed fix:** No code fix is required for the race. Two optional cleanups:
(1) Under the stale-step-comments defect, rewrite the block comment at CassoEmuCore/Shell/WindowCommandManager.cpp:687-700. It should say that reset, power cycle and step are all posted to the CPU thread's queue, and that a paused CPU thread still wakes for and drains that queue (CpuManager.cpp:579-583). Drop the claims that a paused thread is provably idle in pauseCV.wait, that posting would deadlock, and that the step is delegated through the shell to hide Disk2Controller.
(2) Optionally delete the now-uncalled EmulatorShell::RunOneFrame (EmulatorShellCpuThread.cpp:957-967, declared EmulatorShell.h:400). That way nobody can call it from the UI thread later to get a frame advance. This step is out of scope for this defect.

**Regression test:** Already present in UnitTest/DebuggerTests/EmulatorDebugWiringTests.cpp:
- MainWindowStepTests::StepIsPostedToTheCpuThread (:1752-1763). HandleCommand(IDM_MACHINE_STEP) on a paused shell leaves HasPendingCommands true, the PC at $0300 and the session Paused. Before c8680f325 it fails, because the UI thread ran the step and the frame.
- StepWithTheDebuggerAttachedIsTheSessionsStepInto (:1770-1788). It checks that the CPU-thread dispatch runs exactly one instruction and returns to Paused.

Two optional additions to the same MainWindowStepTests class:
(a) StepWithoutTheDebuggerIsPostedToo. Use a shell with no SetDebugSession, paused through GetCpuManager().SetPaused(true). Assert that HandleCommand(IDM_MACHINE_STEP) leaves HasPendingCommands true. The routing must not depend on a session.
(b) StepWhileRunningIsDropped. Call SetPaused(false), then HandleCommand(IDM_MACHINE_STEP), and assert HasPendingCommands is false. This covers the gate at WindowCommandManager.cpp:753.

**Sites:** CassoEmuCore/Shell/WindowCommandManager.cpp:751, CassoEmuCore/Shell/WindowCommandManager.cpp:760, CassoEmuCore/Shell/EmulatorShell.cpp:1337, CassoEmuCore/Shell/CpuManager.cpp:563, CassoEmuCore/Shell/CpuManager.cpp:583, CassoEmuCore/Shell/CpuManager.cpp:590, CassoEmuCore/Shell/CpuCommandDispatcher.cpp:78, CassoEmuCore/Shell/EmulatorShellCpuThread.cpp:379, CassoEmuCore/Shell/EmulatorShellCpuThread.cpp:401, CassoEmuCore/Shell/EmulatorShellCpuThread.cpp:963, CassoEmuCore/Shell/WindowCommandManager.cpp:694

## step-executes-whole-frame [fixed-by-035]

**Evidence:** 035 commit c8680f325 ("fix(debugger): run the main window's Step on the CPU thread") fixed this. It removed StepInstructionWhilePaused and the StepOne + RunOneFrame pair along with it (`git log -S StepInstructionWhilePaused` lists it as the removing commit). The worktree is at 811a6f727, which contains it.

How Step works now:
- UI side: WindowCommandManager.cpp:751-762 only tests IsPaused, then calls PostCommand. Nothing runs on the UI thread.
- Dispatch: CpuCommandDispatcher.cpp:78-79 sends the command to EmulatorShell::StepInstruction (EmulatorShellCpuThread.cpp:379-405).
- No debug session (the usual case, since a session exists only after OpenDebugger at EmulatorShellCpuThread.cpp:222-224 or EmulatorShellDebugger.cpp:1132-1134): it runs SampleHostInputs, then one StepOne, then RenderFramebuffer and PublishFramebuffer (:400-404). That is exactly the fix direction: a redraw with nothing executed.
- With a session: it runs the session's StepInto (:385-393). CpuManagerRunDriver::Start un-pauses (CpuManagerRunDriver.cpp:76), and the frame loop runs RunCycles. RunCycles breaks after one instruction on the hook stop (MachineHost.cpp:886-896). ExecuteCpuSlices then ends the pass at EmulatorShellCpuThread.cpp:1306-1310, through OnSliceExecuted and Finish, which re-pauses (CpuManagerRunDriver.cpp:138-176, :239-245). The rest of the frame never runs.

Disk II idle callback:
- MachineHost::FinishStep ticks the controller by that one instruction's cycles only (MachineHost.cpp:836-847).
- So a quiet step no longer fires the 17030-cycle idle callback. It fires only after roughly 17030 cycles' worth of steps add up, and it fires on the CPU thread.

Leftover dead code:
- RunOneFrame (ExecuteCpuSlices + RenderFramebuffer) is still defined at EmulatorShellCpuThread.cpp:957-967 and declared at EmulatorShell.h:400.
- It has no callers anywhere in the tree. The DebuggerBeamMenuTests "RunOneFrame" test is about the debugger's F6 verb, not this function.

Existing coverage:
- EmulatorDebugWiringTests.cpp:1752 StepIsPostedToTheCpuThread checks the UI side.
- EmulatorDebugWiringTests.cpp:1770 StepWithTheDebuggerAttachedIsTheSessionsStepInto checks the session path (PC $0300 -> $0302, paused again).
- Nothing covers the no-session path. The MainWindowStepTests rig's own shell machine has no CPU, so in every test that branch returns at :395-398.

Something else seen, a different and minor issue: a debugger step still makes one pass through ExecuteCpuSlices' per-frame host work:
- ServiceEndpointChanges (:1202)
- TickKeyboardAutoRepeat (:1208-1211)
- DrainPasteBuffer, charged the full slice target of up to 1023 cycles for a slice that ran one instruction (:1262)

So paste pacing goes by the slice target rather than the cycles actually run while stepping. No guest code beyond the one instruction runs.

**035 impact:** 035 already did what the fix direction asked:
- Step is posted to the CPU queue, and a paused CPU thread drains it (CpuManager.cpp:583 runs before the pause check at :590).
- The no-session branch runs StepOne, then RenderFramebuffer and PublishFramebuffer.
- The session branch uses the new CpuManagerRunDriver step machinery.

So 041 has no defect left to fix here. The only constraint is not to bring back a UI-thread step or a RunOneFrame call. Any later change to Step has to keep going through PostCommand and StepInstruction. On the session path it has to stay with the run driver, because that is what pauses the machine again and reports the stop.

The flush hold for journaling guest writes does not touch this path. A step's disk Tick advances only by the instruction's cycles, on the CPU thread, so it adds nothing to the flush-hold ordering.

**Proposed fix:** No code fix is needed for the defect itself. Two optional follow-ups:

1. Delete the dead RunOneFrame. Remove the declaration at CassoEmuCore/Shell/EmulatorShell.h:400, and in CassoEmuCore/Shell/EmulatorShellCpuThread.cpp remove the definition with its `////` banner (lines 957-967) plus the five blank lines that separate it from RunCpuThreadFrame. Once it is gone, no helper that runs a whole frame is left for a step path to call again by mistake.
   - Delete the banner together with the function, so RunCpuThreadFrame keeps its own banner and the five-blank-line spacing.
   - Then run `scripts/CheckStyle.ps1 -Mode Tree`. An orphaned or stolen banner gets past the diff-scoped hook.

2. Add the missing no-session regression test described below.

**Regression test:** Add a third method to TEST_CLASS (MainWindowStepTests) in UnitTest/DebuggerTests/EmulatorDebugWiringTests.cpp, after StepWithTheDebuggerAttachedIsTheSessionsStepInto at :1770: StepWithNoDebuggerRunsOneInstructionAndNoFrame.

Setup:
1. Build a bare shell with `std::make_unique<EmulatorShell>()` and pause it with `ControllerRig::Paused (shell->GetCpuManager())`. Attach no session and no run driver.
2. Build a minimal machine on `shell->GetMachine()`, the way the InputJournalTests::Build helper does (UnitTest/DebuggerTests/InputJournalTests.cpp:220-240):
   - a RamDevice covering $0000 through at least $0400
   - a RomDevice of NOPs holding the reset vector
   - `SetCpu (std::make_unique<EmuCpu> (bus))`, then `InitForEmulation`
3. Building no video modes matters, for two reasons:
   - RenderFramebuffer returns at EmulatorShellPresent.cpp:1898-1901.
   - PublishFramebuffer bails on two equal empty buffers at :1136-1140, so the private AllocateFramebuffers (EmulatorShell.h:700) is never needed.
4. Poke $0300: A9 41 8D 00 04 60 (LDA #$41 / STA $0400 / RTS). Set PC to $0300 and A to $00, and clear $0400.

Steps and assertions:
1. Record `GetCpu()->GetTotalCycles()`.
2. Call `shell->HandleCommand (IDM_MACHINE_STEP)`. Assert:
   - `GetCpuManager().HasPendingCommands()` is true
   - PC is still $0300
   - the cycle count is unchanged
3. Call `shell->DispatchCpuCommand ({ IDM_MACHINE_STEP, "" })`. Assert:
   - PC == $0302
   - A == $41
   - $0400 still holds $00 (the STA did not run)
   - the total cycle count rose by exactly 2

Why it fails before the fix: master ran StepOne plus RunOneFrame directly inside HandleCommand. That runs about m_cyclesPerFrame (17030) cycles, so PC goes past the RTS and the cycle delta is a frame's worth. The queue also stays empty. Most of the assertions fail there, and they pass on the worktree.

This test is the only one that reaches the no-session branch: the existing rig's shell has no CPU, so that branch returns early there.

**Sites:** CassoEmuCore/Shell/WindowCommandManager.cpp:760, CassoEmuCore/Shell/CpuCommandDispatcher.cpp:79, CassoEmuCore/Shell/EmulatorShellCpuThread.cpp:390, CassoEmuCore/Shell/EmulatorShellCpuThread.cpp:401, CassoEmuCore/Shell/EmulatorShellCpuThread.cpp:403, CassoEmuCore/Shell/EmulatorShellCpuThread.cpp:963, CassoEmuCore/Shell/EmulatorShell.h:400, CassoEmuCore/Shell/MachineHost.cpp:893, CassoEmuCore/Shell/EmulatorShellCpuThread.cpp:1306

## update-flush-with-cpu-running [not-applicable]

**Evidence:** The defective code is not in this worktree. HEAD is 811a6f727 (same as origin/035-debugger). Its merge base with master is 13095e558, and the 039 update merge f853f4702 is not an ancestor (`git merge-base --is-ancestor f853f4702 HEAD` exits 1). Master has 192 commits this tree lacks, and 15 of them touch CassoEmuCore/Shell/EmulatorShellUpdate.cpp. A grep of the worktree for WM_APP_UPDATE|UpdateService|UpdateResult in *.cpp/*.h/*.vcxproj gives 0 matches, so EmulatorShellUpdate.cpp, HandleUpdateApplyResult, ApplyPendingUpdateNow and UpdateService::StartDeploy are all absent. The only production caller of FlushAllForShutdown is the destructor, CassoEmuCore/Shell/EmulatorShell.cpp:299, which runs after m_cpuManager.Stop() at :236. Stop sets m_running false and joins the thread (CassoEmuCore/Shell/CpuManager.cpp:110-124), and OnCpuThreadStop runs StopReverseRecording first (CassoEmuCore/Shell/EmulatorShellCpuThread.cpp:319-326). So in this tree the shutdown flush is single-threaded, which is the same "contrast" case the defect cites.

The defect comes back unchanged when master is merged in (or 041 merges to master). Everything it races is still here and still owned by the CPU thread:
- FlushEntry (CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1069-1260). It calls CommitPendingWrite at :1095, IsDirty at :1097, RepointBayToFile at :1192 (which rewrites entry.path at :3583), Serialize at :1210, writes the file at :1235 and calls ClearDirty at :1243.
- Disk2NibbleEngine (CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp). RecordFluxWriteBit pushes m_burstBits (:408-420), CommitPendingWrite clears them (:432-448), and the CPU-thread write path is at :679-692.
- DiskImage (CassoEmuCore/Devices/Disk/DiskImage.cpp). WriteBit sets m_dirty at :480-508, and ClearDirty is at :642-651. A guest write between :1210 and :1243 is lost.
- FlushEveryBay walks every bay without a lock (:2253-2276).
- Eject goes through RetireBay (:2322 -> :2197-2221), which moves entry.image into m_retained or resets it. RescueOnTheWayOut then dereferences entry.image at :453 and :465, after the modal sink at :448, and its header (:424-433) still assumes "the pump has gone and so has the thread".
- The motor-off flush (CassoEmuCore/Shell/MachineBuilder.cpp:1519-1528) relies on the comment at :1516: "fires on the CPU thread ... which owns the disk writes, so it races nothing". NoteExternalChange (:2681-2684) states the same invariant.

SetPaused is not a substitute. It is not acknowledged (CpuManager.cpp:215-224), and the paused loop still drains commands at :583 (eject, mount, reverse seek) and runs the debugger and thumbnail service at :585-588. UnitTest/UiTests/CpuManagerCommandTests.cpp:29-35 and :60-116 assert that a paused machine keeps dispatching. Stop is not a substitute either, because the UI loop runs only while IsRunning() is true (CassoEmuCore/Shell/Window/EmulatorWindow.cpp:1367, :1450).

**035 impact:** There is nothing to fix in the 035 base itself. The fix has to be written against the tree as it stands after master merges in, and 035 changes what that fix needs in five ways.

(1) 035 adds more CPU-thread writers that a UI-thread flush would race.
- Reverse execution reseats disks on every keyframe load. MachineHost::SeatMedia (CassoEmuCore/Shell/MachineHost.cpp:1677) -> DiskImageStore::SeatMedia (DiskImageStore.cpp:2117-2164) swaps entry.image, entry.path and entry.sharedState.
- Eject now retires the image into m_retained instead of freeing it (RetireBay, :2197-2221). That is a push_back on a vector, and entry.image is still moved away from the bay.
- The flush gates m_isReplaying and m_isFlushHeld are plain bools (DiskImageStore.h:711-712). The CPU thread writes them at ReverseController.cpp:1885, :1913, :2005 and :2070-2071 and at Replayer.cpp:162 and :195, while FlushEntry reads them at :1090. A UI-thread flush during a seek or a run from the past would read them torn: some bays written, some skipped, or a bay serialized mid-restore.

So the flush must run with the CPU thread parked BETWEEN passes. That means at the top of CpuManager::ThreadProc, before DrainCommandQueue (:583) and the service (:585), never inside a frame or a reverse command.

(2) (Not taken as written: the owner decided on 2026-10-08 that an update installed while the machine is behind live saves the disk at the live end, so the deploy first returns the machine to the live end on the CPU thread and then holds the thread, and its outcome no longer matches exit's behind live; when the return does not complete, the deploy clears the replay flag and saves the disk where the machine stands. The owner confirmed on 2026-10-09 that the rule covers every install path: an MSIX update installed at once holds the thread and saves, and a zip update installed now and an update left until Casso closes return to live first and then save through quit. plan.md section 8; tasks.md T129, T139. The use of FlushAllForShutdown and the hold's exit meaning below are kept.) The flush hold must keep its exit meaning, which is that it is ignored. The deploy ends the process, so it must use the same FlushAllForShutdown as the destructor (DiskImageStore.cpp:1873-1876). That call writes held writes on purpose, as the owner decided (DiskFlushHoldTests.cpp:71-98; DiskImageStore.h:133-137), and writes nothing during a replay (DiskFlushHoldTests.cpp:101-118). Do not switch the deploy to FlushAllUnlessHeld or FlushAll. Parking makes both flags stable, so the deploy's outcome matches exit exactly.

(3) StopReverseRecording, which OnCpuThreadStop runs first at exit (EmulatorShellCpuThread.cpp:321-322), is not needed for this flush. FlushAllForShutdown ignores the hold, so leave history alone. A failed deploy can then simply unpark with history intact, with no restart of recording.

(4) The old EmulatorShell::StepInstructionWhilePaused is gone. The new pause machinery (CpuManagerRunDriver::Pause/Finish, CpuManagerRunDriver.cpp:105-115 and :239-274) is a non-acknowledged SetPaused plus a landing point. It gives no "the thread is quiet" signal and leaves the command drain and the debug service running. A new acknowledged park is required, and it must be separate from pause, because the "pause does not park the queue" contract (CpuManagerCommandTests.cpp:29-35) must stay as it is.

(5) A long reverse command must be stopped first so the park lands within a bounded wait. EmulatorShell::StopReplay (EmulatorShell.h:641) sets the flag that ReverseController::IsStopDue polls (ReverseController.cpp:2046).

Related observation, outside this fix (reported to 035 on 2026-10-08 as 041 audit defect a, which 035 fixed in `c913c9247`; the deploy no longer depends on it, because it clears the flag itself when it saves behind live): ReverseController::Stop (ReverseController.cpp:130-158) clears the hold but not SetReplaying. ReplayHere leaves the flag set after a step or run from the past (:1885, :1913), and only BecomeLive (:2071) clears it. So both the destructor flush and the fixed deploy flush write nothing in that state, which works against the "exit saves" rule. Settle that for both paths together, not in the deploy path alone.

**Proposed fix:** Apply this after merging master into 041-disk-integrity.

(A) CpuManager (CassoEmuCore/Shell/CpuManager.h and .cpp): add an acknowledged park that is separate from pause. (Not taken as written: 035's defect b fix adds a pause park of its own, with `IsParked`, `TryWaitUntilParked`, `TryPark`, `ParkForExit`, `m_isParked` and `m_parkedCV`, and moves the pause wait into `WaitWhilePaused`. The plan's API is therefore `HoldForDeploy`, `ReleaseDeployHold`, `IsHeldForDeploy` and `IsDeployHoldRequested`, on 035's code: the request is part of `WaitWhilePaused`'s wake predicate and is notified on `m_pauseCV`, and the hold check sits between `WaitWhilePaused` and `DrainCommandQueue`. The steps below are otherwise kept. tasks.md T128.)
- New API: `HRESULT Park (DWORD timeoutMs);`, `void Unpark ();` and `bool IsParked () const;`.
- New members, guarded by m_pauseMutex: `bool m_isParkRequested = false;`, `bool m_isParked = false;` and `std::condition_variable m_parkedCV;`.
- ThreadProc: add `m_isParkRequested` to the isWoken predicate (CpuManager.cpp:559-564). Immediately after the wait block, before DrainCommandQueue at :583 and before m_onService at :585, add this check: if a park is requested and m_running is true, set m_isParked = true under m_pauseMutex, notify m_parkedCV, wait on m_pauseCV until `!m_isParkRequested || !m_running`, set m_isParked = false, then `continue`. While parked, no frame, no command drain and no service runs, and posted commands stay queued.
- Park(): assert that it is not called on the CPU thread. Under the lock, set m_isParkRequested and notify m_pauseCV, then wait on m_parkedCV for up to timeoutMs for `m_isParked || !m_running`. On timeout, clear m_isParkRequested under the same lock, so a late arrival does not park, and return HRESULT_FROM_WIN32 (ERROR_TIMEOUT).
- Unpark(): clear the request under the lock and notify.
- Stop() needs no change. Its notify plus !m_running already releases a parked thread, so the join at :120-123 still completes.

(B) EmulatorShell, in the merged tree. (Not taken as written: the audit's Park, Unpark, IsParked and m_isParkedForDeploy below are the deploy hold's HoldForDeploy, ReleaseDeployHold and IsHeldForDeploy in the design, apart from 035's pause acknowledgement, which uses IsParked for a weaker state; the call site also returns to live first and serves the zip and close paths as well. tasks.md T128, T139.)
- Add `static HRESULT FlushForDeploy (CpuManager & cpu, DiskImageStore & store, DWORD timeoutMs)`. (Not taken as written: it is `DeploySave::Flush`, which also clears the replay flag behind live and reports what it found, beside the deploy's return to live; tasks.md T129, T139.) It calls `cpu.Park (timeoutMs)` with CHR, then `store.FlushAllForShutdown()`, and returns with the machine still parked.
- In HandleUpdateApplyResult's ReadyToDeploy branch (master EmulatorShellUpdate.cpp:679-700), replace the bare flush at :686 with:
  1. StopReplay();
  2. hr = FlushForDeploy (m_cpuManager, m_machine.GetDiskStore(), kDeployParkTimeoutMs). On failure, call m_updateDialog->ShowFailure (UpdateFailure::InstallFailed) and return without deploying.
  3. m_isParkedForDeploy = true; FlushDeferredGlobalPrefs();
  4. StartDeploy.
- If StartDeploy fails synchronously, call `ResumeAfterFailedDeploy()`. That function runs m_cpuManager.Unpark() and sets m_isParkedForDeploy = false.
- Call the same function in the wasCanceled and failure branches (master :653-677), because a deploy that starts and then fails comes back there as an asynchronous failure result (UpdateServiceTests Deploy_Failure_IsReported).
- Add `isParkedOrStopped = m_cpuManager.IsParked() || !m_cpuManager.IsRunning();` and `ASSERT (isParkedOrStopped);` ahead of the flush. (The calls are hoisted into a local, because no EHM macro condition may contain a call.)
- Hand disk ownership over with the park (research.md R5 Risks): the CPU thread releases the store before it signals parked and claims it after waking, and the UI claims after Park returns and releases before Unpark (tasks.md T128, T139).

(C) Rewrite the RescueOnTheWayOut header (DiskImageStore.cpp:428-431). (In the design, "parked" here is the deploy hold, tasks.md T128.) It should say that the function runs on the UI thread with the CPU thread either joined (exit) or parked (an update deploy). Its picker pumps messages, but nothing that touches a bay can run, because commands stay queued until the process ends or the machine is unparked. Also fix the matching sentence in ReportPreserveFailure's header (:348-350) and in the master comment over HandleUpdateApplyResult (:636-641).

**Regression test:** (Not taken as written: the three tests below are T128's `HoldForDeploy_HoldsFramesCommandsAndService`, `Stop_WhileHeldForDeploy_JoinsThread` and `HoldForDeploy_TimesOutAndWithdrawsWhileACommandRuns`, beside a fourth, `HoldForDeploy_WhilePausedWithNoServiceFunction_IsHeldBeforeTheDeadline`, and the deploy-flush tests go through `DeploySave::Flush`; tasks.md T128, T129.) 1. Extend UnitTest/UiTests/CpuManagerCommandTests.cpp, using its existing WaitFor and WaitForFramesToStop helpers and the s_kWaitMs and s_kParkMs constants:
- Park_HoldsFramesCommandsAndService: call SetServiceFunction with a counter before Start, and Start with frame and command counters. Assert Park (s_kWaitMs) returns S_OK, then take baselines and PostCommand (IDM_DISK_EJECT1). Sleep 2*s_kParkMs, then assert that frames, commands and service calls are unchanged and that HasPendingCommands() is true. Unpark, then WaitFor commands > 0 and frames advancing. Put it beside PostCommand_WhilePaused_StillDispatches (:60-116), which shows that SetPaused alone keeps draining.
- Stop_WhileParked_JoinsThread: Park, then Stop. Assert that Stop returns and IsRunning() is false.
- Park_TimesOutAndWithdrawsWhileACommandRuns: a command callback blocks on a test-owned event. Post it, assert Park (50) returns HRESULT_FROM_WIN32 (ERROR_TIMEOUT), release the event, and assert that frames keep running, meaning the withdrawn request never parks.

2. Extend UnitTest/EmuTests/DiskFlushHoldTests.cpp:
- DeployFlushLosesNoGuestWrite: use PrepareDirtyDisk (:220-234) and a running CpuManager. Its frame callback flips a bit in the mounted image every frame, which is the guest writing. Its command callback calls store->Eject (s_kHoldSlot, s_kHoldDrive). Call EmulatorShell::FlushForDeploy (cpu, *store, s_kWaitMs) and assert S_OK and files.writes == 1. Then PostCommand (IDM_DISK_EJECT1) and sleep a settle window longer than one frame. Assert that GetImage (...)->IsDirty() is false, so no guest write landed after the flush, that IsMounted (s_kHoldSlot, s_kHoldDrive) is true, so the queued eject did not run, and that files.last equals a Serialize taken now. Finally Unpark, assert that the eject then runs, and Stop.
- Today's master sequence fails this test deterministically. It calls FlushAllForShutdown directly with the thread running, so the frame callback dirties the image again within one frame and the "clean after the settle window" assertion fails. Replacing the park with SetPaused (true) fails the "still mounted" assertion, because the paused loop dispatches the eject.
- A second case, DeployFlushStillSavesHeldWrites, does the same with store->SetFlushHold (true) and asserts files.writes == 1. This pins that the deploy flush keeps the exit meaning of the hold (compare HoldStillSavesOnEjectSwitchAndExit, :73-98).

**Sites:** (master only, absent from the worktree) CassoEmuCore/Shell/EmulatorShellUpdate.cpp:686 -- arrives with the master merge, CassoEmuCore/Shell/CpuManager.cpp:555, CassoEmuCore/Shell/CpuManager.cpp:583, CassoEmuCore/Shell/CpuManager.cpp:215, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1873, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2253, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1090, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1095, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1192, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1210, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1243, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3583, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:424, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:448, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:453, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:465, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2322, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2117, CassoEmuCore/Devices/Disk/DiskImageStore.h:711, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:432, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:692, CassoEmuCore/Devices/Disk/DiskImage.cpp:480, CassoEmuCore/Devices/Disk/DiskImage.cpp:642, CassoEmuCore/Shell/MachineBuilder.cpp:1521, CassoEmuCore/Shell/EmulatorShell.cpp:299

## user-config-store-cross-thread [changed-but-present]

**Evidence:** The defect is still there. Line numbers have moved, and 035 changed the code around it.

No lock. UserConfigStore.h:264 is still the bare `mutable std::map m_machinePrefs`, :281 is still `GlobalUserPrefs * m_prefs`, and the class has no mutex. LoadAll stores the caller's struct at UserConfigStore.cpp:1177, called as `LoadAll (m_globalPrefs, m_uiFs, report)` at EmulatorShell.cpp:776 on the UI thread. DiskManager gets the same `*m_userConfigStore` and `m_uiFs` (EmulatorShell.cpp:685-698).

CPU-thread writers:
- Dispatch runs on the CPU thread (EmulatorShellCpuThread.cpp:344-357). IDM_DISK_INSERT and IDM_DISK_EJECT (CpuCommandDispatcher.cpp:86-95) reach MountDiskInSlot6 (DiskManager.cpp:476-477) and EjectDiskInSlot6 (:515-516), both calling DiskSettings::WriteSavedDiskPath.
- That path runs store.Load (DiskSettings.cpp:317), which can insert into the map (UserConfigStore.cpp:1426, :1485, :1864) and on migration calls SaveCombinedJson(m_prefs) (:1492). Then SpliceUiPrefs (:322) and store.SaveDelta (:324), which assigns `m_machinePrefs[machineName]` (UserConfigStore.cpp:1534) and calls `SaveCombinedJson (m_prefs, ...)` (:1536).
- Inside that save, BuildCombinedJson iterates the map (:1748), copies the UI's struct with `GlobalUserPrefs merged = *prefs;` (:1765), and writes the file (:1912).
- Reset/Power cycle reach RemountSlot6Disks (CpuCommandDispatcher.cpp:62-76), which mounts both drives back to back (DiskManager.cpp:672-679).
- SwitchMachine on the CPU thread calls store.Load directly (MachineManager.cpp:242). Through MountCommandLineDisks (:606) it also reads saved paths (DiskManager.cpp:329, :351), clears them (:341, :363, :391) and mounts twice (:404, :410). It also writes `m_globalPrefs.lastSelectedMachine` and calls SaveGlobalPrefs (MachineManager.cpp:571-575), which ends in SaveAll(m_globalPrefs) (EmulatorShellPrefs.cpp:942).

UI-thread writers:
- WM_APP_MOUNT_COMPLETED (EmulatorWindow.cpp:2657-2668) -> HandleMountCompletion -> RecordRecentDisk assigns recentDisks/recentDiskLoadedAt and saves (EmulatorShellDisks.cpp:133-136).
- PersistInputModeForMachine and PersistColorModeForMachine call WriteSavedUiPrefs (EmulatorShellPrefs.cpp:461, :526).
- The //c switch bar persists through the same path (EmulatorShellChrome.cpp:1517 -> EmulatorShellCpuThread.cpp:593).
- Settings Apply calls SaveDelta (SettingsApplyController.cpp:378).
- The UI-thread salvage mount calls MountDiskInSlot6 directly (EmulatorShellDisks.cpp:674).

Win32FileSystem::WriteAllText uses one fixed `path + L".tmp"` (Win32FileSystem.cpp:124-157), so two concurrent saves also collide on that temp file. The second save fails silently, or its cleanup DeleteFileW removes the first save's staged file.

What 035 changed:
1. RemountSlot6Disks now returns at once under the flush hold (DiskManager.cpp:661-664), so the correlated two-drive Reset/Power cycle trigger fires only while live.
2. BuildCombinedJson now merges debugger placements into its copy of *prefs (UserConfigStore.cpp:1765-1785). 035 also added about 17 UI-thread SaveGlobalPrefsDeferred sites (EmulatorShellDebugger.cpp) that write m_globalPrefs, which the CPU-thread copy races with.
3. m_globalPrefsDirty became atomic (EmulatorShell.h:2238), and SaveGlobalPrefs now documents the CPU-thread SwitchMachine caller (EmulatorShellPrefs.cpp:944-955). The off-thread SaveAll(m_globalPrefs) itself was left in place.
4. Mounts and ejects are journaled (CpuCommandDispatcher.cpp:245-256), but Replayer::ApplyInput skips them (Replayer.cpp:470-475), so replay adds no store writes.

No lock in EmulatorShell.h (only m_divergeMutex and the others at :2333-2539) covers m_globalPrefs or the store.

**035 impact:** Posting only the CPU-thread writes to the UI thread, the first option in the fix direction, is not enough on 035. SwitchMachine still reads the store on the CPU thread: Load at MachineManager.cpp:242, and ReadSavedDiskPath through MountCommandLineDisks at :606 -> DiskManager.cpp:329/:351. LoadMachineState reaches SwitchMachine on the CPU thread too (EmulatorShellState.cpp:176). Load is const but changes the cache (UserConfigStore.cpp:1426, :1485, :1864) and saves on migration (:1492). Confining the store to the UI would mean resolving SwitchMachine's merged config on the UI before IDM_FILE_OPEN, for both callers. So the store needs its own lock in any case.

A plain "every machine-delta save writes the on-disk global section back" change would break SaveDelta_AfterLoadAll_WritesLivePrefs (UnitTest/UiTests/UserConfigStoreTests.cpp:1442-1468). That test requires a store that ran LoadAll to write its live prefs. So the global section has to be gated by thread, not dropped.

The flush hold needs nothing new. The fix lives in the store and adds no remount and no write. RemountSlot6Disks still returns before any mount under the hold (DiskManager.cpp:661-664), and replay still skips media inputs (Replayer.cpp:475).

The 035 deferred saves (SaveGlobalPrefsDeferred -> FlushDeferredGlobalPrefs -> SaveGlobalPrefs) stay on the UI thread. Once SwitchMachine stops calling SaveGlobalPrefs, the offUiThread branch at EmulatorShellPrefs.cpp:932-959 has no caller left, so the dirty-flag rule there stays correct.

The removal of StepInstructionWhilePaused has no bearing on this defect. The inline completion fallback at EmulatorShellDisks.cpp:197-200 stays out of scope: it is mount-completion-fallback-off-thread.

**Proposed fix:** 1. Serialize the store. Add `mutable std::mutex m_lock;` beside m_machinePrefs in UserConfigStore.h (:264). <mutex> is already in CassoEmuCore/Pch.h:49.
   - LoadAll, SaveAll, Load, SaveDelta and Reset each take `std::lock_guard<std::mutex>` at the top. The private helpers (SaveCombinedJson, BuildCombinedJson, LoadCombinedJson, MigrateLegacyFiles, PreserveUnreadableFile) assume the lock is held.
   - Move the bodies of Load and SaveDelta into private `LoadHoldingLock` and `SaveDeltaHoldingLock`, so step 3 can reuse them without locking twice.
   - This makes each cache change, the read-back in BuildCombinedJson (:1716-1745) and the WriteAllText (:1912) one unit. That also stops the shared UserPrefs.json.tmp collision in Win32FileSystem.cpp:124-157.
   - No lock cycle: no store method calls back into the shell, and WindowTrace::Log only appends to a file.

2. Keep the UI's GlobalUserPrefs to the thread that loaded it.
   - In LoadAll, record `m_prefsOwner = std::this_thread::get_id();` next to `m_prefs = &prefs;` (UserConfigStore.cpp:1177), with a new `std::thread::id m_prefsOwner` member.
   - Add a private `const GlobalUserPrefs * GetPrefsForCaller() const` that returns m_prefs only when `std::this_thread::get_id() == m_prefsOwner`, and nullptr otherwise.
   - Pass it instead of m_prefs at :1492 (Load migration), :1536 (SaveDelta) and :1622 (Reset).
   - A save from the CPU thread then takes BuildCombinedJson's null-prefs path (:1787-1790) and writes the on-disk global section back verbatim, never reading the UI's struct. Under step 1's lock, that section is always the last one the UI's SaveAll wrote.
   - A UI-thread SaveDelta still writes the live prefs, so SaveDelta_AfterLoadAll_WritesLivePrefs keeps passing.

3. Make the per-machine read-modify-write atomic.
   - Add `HRESULT UpdateUiPrefs (const std::string & machineName, const JsonValue & defaultJson, const std::vector<std::pair<std::string, JsonValue>> & values, IFileSystem & fs) const;`
   - Under one hold of m_lock, it runs LoadHoldingLock, then BAIL_OUT_IF the merged result is not an object (returning S_OK), then SpliceUiPrefs, then SaveDeltaHoldingLock.
   - Replace DiskSettings::WriteSavedUiPrefs's separate store.Load / SpliceUiPrefs / store.SaveDelta (DiskSettings.cpp:317-325) with one `store.UpdateUiPrefs (machineNarrow, defaultJson, values, fs)`.
   - Without this, a CPU-thread disk1Path/disk2Path write and a UI-thread colorMode, input-mode or //c switch write on the same machine can both Load the same delta. Each then replaces the entry with a delta missing the other's key.

4. Move the CPU-thread write of m_globalPrefs to the UI thread.
   - Delete MachineManager.cpp:568-575 (the lastSelectedMachine assignment and SaveGlobalPrefs).
   - Add `RecordActiveMachineSelection();` to the WM_APP_DXUI_UPDATE_TITLE handler (EmulatorWindow.cpp:2685-2703). SwitchMachine already triggers that handler through UpdateWindowTitle at MachineManager.cpp:566 -> EmulatorWindow.cpp:2180-2184.
   - RecordActiveMachineSelection (EmulatorShellPrefs.cpp:480-491) does the same compare, assign and save on the UI thread. It does nothing when the value is unchanged, so the handler's other posts cost nothing.
   - After this, the only CPU-thread reader of m_globalPrefs in this path is gone, and every write to it is on the UI thread.

Not part of this fix: the CPU-thread inline completion fallback (EmulatorShellDisks.cpp:197-200), which is tracked as mount-completion-fallback-off-thread.

**Regression test:** Extend UnitTest/UiTests/UserConfigStoreTests.cpp. The tests use InMemoryFileSystem, which already has its own lock, and the kpszSeededPrefs fixture (activeTheme "Retro Terminal", Apple2e at $cassoMachineVersion 1). Use defaultJson `{"$cassoMachineVersion":1,"speedMode":"Authentic"}` and currentJson with speedMode "Double", so no migration runs.

(a) SaveDelta_OnAnotherThread_LeavesTheGlobalSectionAsWritten. Place it next to SaveDelta_AfterLoadAll_WritesLivePrefs (:1442). Deterministic, no timing involved.
   1. Seed the file and call LoadAll(prefs) on the test thread.
   2. Set `prefs.activeTheme = "DarkModern"` without saving; this stands in for the UI's unsaved state.
   3. Run store.SaveDelta("Apple2e", currentJson, defaultJson, fs) on a std::thread, join it, then assert.
   4. Assert: hr succeeded, ReadMachineOrFail shows speedMode "Double", and the written global's activeTheme is still "Retro Terminal".
   Before the fix this fails with Expected:<Retro Terminal> Actual:<DarkModern>, because SaveCombinedJson(m_prefs) serializes the loader's struct from the other thread. After the fix it passes, and the existing same-thread test still passes.

(b) SaveAll_WhileAnotherThreadIsSaving_WaitsAndKeepsBothWrites. Add a nested `GatedFileSystem : public InMemoryFileSystem` beside FaultyFileSystem (:49):
   - Arm() makes the next WriteAllText block on a std::mutex and std::condition_variable until Release().
   - WaitUntilHeld(timeout) returns whether a write reached the gate within the timeout.
   Steps:
   1. Seed the file, call LoadAll(prefs), then Arm().
   2. Start thread "cpu" running store.SaveDelta(...).
   3. WaitUntilHeld(5 s). Fail if it is not reached, so the test never waits without a bound.
   4. Set prefs.activeTheme = "DarkModern".
   5. Start thread "ui" running store.SaveAll(prefs, fs) and setting an atomic done flag with a notify.
   6. Wait at most 200 ms for done, and record finishedDuringTheSave.
   7. Release(), then join both threads BEFORE any Assert, so a failure cannot destroy a joinable std::thread.
   8. Assert: finishedDuringTheSave is false, both HRESULTs succeeded, the global activeTheme is "DarkModern", and Apple2e speedMode is "Double".
   Before the fix: SaveAll finishes during the hold, then the held CPU-thread write lands last. The file ends up with Expected:<DarkModern> Actual:<Retro Terminal>, and the overlap assertion fails too. After the fix: SaveAll waits on m_lock, and its read-back picks up the CPU-thread delta.

(c) UpdateUiPrefs_FromTwoThreads_KeepsBothKeys. Same gate.
   - Thread A runs store.UpdateUiPrefs("Apple2e", defaultJson, {{"disk2Path", <non-default path>}}, fs) and is held mid-write.
   - Thread B runs UpdateUiPrefs with {{"colorMode", "green"}}.
   - Release, join, then Load through a fresh UserConfigStore over the same fs. Assert both keys are in Apple2e's $cassoUiPrefs, and that B did not finish during the hold.
   UpdateUiPrefs is new API, so check the test by mutation: replace its body with today's unlocked Load + SpliceUiPrefs + SaveDelta (what DiskSettings::WriteSavedUiPrefs does now). The test then goes red, with colorMode missing from the file.

Step 4 (moving lastSelectedMachine) is shell wiring that no unit test can reach; there is no SwitchMachine harness. Verify it by switching machines in a live build and checking UserPrefs.json.

**Sites:** CassoEmuCore/Config/UserConfigStore.h:264, CassoEmuCore/Config/UserConfigStore.h:281, CassoEmuCore/Config/UserConfigStore.cpp:1177, CassoEmuCore/Config/UserConfigStore.cpp:1426, CassoEmuCore/Config/UserConfigStore.cpp:1485, CassoEmuCore/Config/UserConfigStore.cpp:1492, CassoEmuCore/Config/UserConfigStore.cpp:1534, CassoEmuCore/Config/UserConfigStore.cpp:1536, CassoEmuCore/Config/UserConfigStore.cpp:1622, CassoEmuCore/Config/UserConfigStore.cpp:1748, CassoEmuCore/Config/UserConfigStore.cpp:1765, CassoEmuCore/Config/UserConfigStore.cpp:1864, CassoEmuCore/Config/UserConfigStore.cpp:1912, CassoEmuCore/Config/DiskSettings.cpp:317, CassoEmuCore/Config/DiskSettings.cpp:324, CassoEmuCore/Shell/DiskManager.cpp:329, CassoEmuCore/Shell/DiskManager.cpp:341, CassoEmuCore/Shell/DiskManager.cpp:391, CassoEmuCore/Shell/DiskManager.cpp:476, CassoEmuCore/Shell/DiskManager.cpp:515, CassoEmuCore/Shell/DiskManager.cpp:676, CassoEmuCore/Shell/MachineManager.cpp:242, CassoEmuCore/Shell/MachineManager.cpp:573, CassoEmuCore/Shell/MachineManager.cpp:574, CassoEmuCore/Shell/MachineManager.cpp:606, CassoEmuCore/Shell/EmulatorShellDisks.cpp:133, CassoEmuCore/Shell/EmulatorShellDisks.cpp:136, CassoEmuCore/Shell/EmulatorShellPrefs.cpp:942, CassoEmuCore/Shell/Window/EmulatorWindow.cpp:2663, CassoEmuCore/Config/Win32FileSystem.cpp:124

## ui-derefs-store-diskimage [changed-but-present]

**Evidence:** The defect is still present in the worktree at 811a6f727. Line numbers have moved, one master site does not exist here, and 035 adds new CPU-thread writers.

Source of the raw pointer: DiskImageStore::GetImage (CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2411-2415) returns GetEntry(slot, drive).image.get() with no lock. The store's only mutex is m_pendingMutex (DiskImageStore.h:715-719). Its comment claims the image and path are touched only by the disk-write thread, which is false.

UI-thread dereferences:
- Every frame: EmulatorShell::TryPresentUiFrame (EmulatorShellPresent.cpp:635-638) calls DiskManager::UpdateDriveWidgets, which runs GetImage at DiskManager.cpp:895 and image->GetWriteProtectInfo() at :897. The frame's shared lifetime lock (EmulatorShellPresent.cpp:564) only keeps out a machine switch.
- GetWriteProtectInfo (DiskImage.cpp:546-565) reads six flags. For a damaged disk it also calls DamagedMountReport::GetDamagedQuarterTracks (DiskImage.cpp:561 -> DamagedMountReport.cpp:118-128), which walks m_damagedTracks against the 160-entry m_quarterTrackMap (GetMappedSlot, DiskImage.cpp:141-148). So any frame with a damaged disk in a drive walks two vectors owned by the CPU thread.
- Disk menu, on every draw through DxuiCommand isEnabled/labelText (EmulatorCommands.cpp:182-193):
  - The enable query (EmulatorWindow.cpp:751-761) reaches EmulatorShell::IsWriteProtectToggleOffered (EmulatorShellDisks.cpp:769-782): GetImage at :771, deref at :781.
  - The label query (EmulatorWindow.cpp:763-795): GetImage at :777, deref at :794.
- ReportDamagedMount (EmulatorShellDisks.cpp:694-753) runs on the UI thread from WM_APP_REPORT_DAMAGE (EmulatorWindow.cpp:2647-2649) and HandleMountCompletion (EmulatorShellDisks.cpp:244). It takes GetImage at :696, calls IsDamaged at :722, and FormatBody(*image) at :733, which goes to DamagedMountReport.cpp:154 and then GetWriteProtectInfo.
- The master drive context menu (EmulatorShellStorage.cpp:241-244) is not here: that file does not exist in this worktree.

CPU-thread writers, none of them locked:
- Frees: RetireBay (DiskImageStore.cpp:2197-2221) calls entry.image.reset() at :2216. With retention on, it moves the image into m_retained at :2203-2214 instead; PruneRetainedMedia (:2179-2182) and SetMediaRetention(false) (:2065-2068) free it later. RetireBay is called from MountFromBytes :268 (a new image follows at :271 and is reset at :282 if the load fails), Eject :2322, EjectLostImage :3875 and SeatMedia :2137.
- New in 035: SeatMedia (:2117-2164) swaps unique_ptrs between bays and m_retained on every keyframe restore. The path is MachineHost::CheckStateHeader :1677 -> SeatMedia :1773-1795 -> Replayer::LoadState :384, posted as IDM_DEBUG_REVERSE (EmulatorShellReverse.cpp:185). By design it emits no bay change (comment at :2110-2113).
- In-place move assignment `*entry.image = std::move (*loaded)` at :3525 (MountExternallyModifiedDisk). It is reached from ApplyPendingReload in the motor-off and idle callbacks (MachineBuilder.cpp:1527, :1544; CarryOutChangeAction :3310) and from the new DiscardHeldWrites (:2014). It reallocates m_damagedTracks and m_quarterTrackMap while the UI walks them.
- Flag writers:
  - SetImageWriteProtect :1481
  - FlushEntry SetSourceCrcMismatch :1177 and :1242
  - DiskManager::ApplyExternalWriteProtect (DiskManager.cpp:175-176), called from OnBayChange :571 and ToggleImageWriteProtect :268
  - EmulatorShell::SetDriveUserWriteProtect (EmulatorShellDisks.cpp:814)
  - New in 035: Replayer::ApplyInput (Replayer.cpp:481-485) and DiskImage::LoadState (DiskImage.cpp:1300-1304), which swaps m_trackBits and rewrites m_imageWriteProtected/m_userWriteProtected, called through MachineHost::LoadStateSeating (MachineHost.cpp:1144-1148).
- All of these run on the CPU thread. Its loop drains commands and runs the service hook even while paused (CpuManager.cpp:579-588), and the dispatcher runs insert, eject, write-protect, resolve-change and step there (CpuCommandDispatcher.cpp:78-111).

Concrete failure scenarios:
1. A damaged WOZ is in drive 1 and an external build rewrites the file. The idle callback's ApplyPendingReload move-assigns the image at :3525, freeing m_damagedTracks and m_quarterTrackMap mid-walk in that frame's GetWriteProtectInfo. The result is a read of freed memory: garbage in the padlock tooltip, or a crash.
2. A queued eject, with reverse execution off, resets the unique_ptr at :2216 while an open Disk menu evaluates the label query at EmulatorWindow.cpp:794. The query then dereferences a freed DiskImage.

**035 impact:** On master the fix direction was to publish a per-bay UI snapshot from OnBayChange, the write-protect setters, FlushEntry and the reload. On 035 that is not enough. The publish has to live inside the store, plus one hook in the state load, for these reasons:

1. SeatMedia (DiskImageStore.cpp:2117-2164) moves disks between bays and the retained list on every reverse-execution keyframe restore, and it emits no BayChange by design (:2110-2113). OnBayChange is never called when a rewind puts a different disk in a bay. SeatMedia must publish itself.
2. 035 adds two write-protect flag writers that sit outside every setter on master's list:
   - DiskImage::LoadState rewrites m_imageWriteProtected and m_userWriteProtected (DiskImage.cpp:1303-1304) and swaps the track vectors.
   - Replayer::ApplyInput sets the user flag through a raw GetImage pointer (Replayer.cpp:481-485).
   The state load needs a publish-all after MachineHost::LoadStateSeating, on the Error path too, because a partial load has already changed images. The replayer must go through a store setter.
3. NotifyMediaChanged cannot be the hook. It is suppressed while m_isReplaying (DiskImageStore.cpp:2235), and replays are exactly where SeatMedia, LoadState and ApplyInput run.
4. The flush hold must be respected rather than relied on. FlushAllUnlessHeld returns early under the hold (:1899), so no publish may depend on a flush happening. FlushEntry should publish only in the two branches that clear the CRC flag (:1177, :1242). DiscardHeldWrites (:2014) reaches MountExternallyModifiedDisk and then EmitBayChange, so putting the publish in EmitBayChange covers it.
5. Retention (RetireBay :2203-2214) delays the free, but the raw pointer is still unsafe: SeatMedia can put a different image in the bay, and PruneRetainedMedia or SetMediaRetention(false) free it later. Only "the UI never holds a DiskImage*" fixes this.
6. There is no UI-thread step path left in 035. IDM_MACHINE_STEP runs StepInstruction on the CPU thread (CpuCommandDispatcher.cpp:78-80), so stepping is just another CPU-side writer, and the CPU thread is the only publisher in every case, paused or running.
7. The master-only drive context menu (EmulatorShellStorage.cpp:241-244) is absent here. Once master is merged in, it reaches the bay through IsWriteProtectToggleOffered and IsSalvageOffered, so fixing those two covers it.

**Proposed fix:** Give the UI a published copy of each bay, written only by the thread that owns disk writes, and never a DiskImage*.

(Plan: the published copy is research.md R1's `DriveStatus`, built on the owning thread and read as a whole, in place of the `BayView` table below. The flag-only setter this fix adds is `SetUserWriteProtectFlag` in the plan, written that way below; `SetUserWriteProtect` is FR-015's operation, which saves first (settings-wp-drops-dirty), and a replay never calls it.)

1. DiskImageStore.h
   - Add a public value struct `BayView { bool mounted = false; string path; WriteProtectInfo writeProtect; bool salvageOffered = false; };`.
   - Add public `BayView GetBayView (int slot, int drive) const;` (callable from any thread; returns a copy under the lock, or BayView() for a bad bay).
   - Add public `void PublishAllBayViews ();` (disk-write thread).
   - Add public `void SetUserWriteProtectFlag (int slot, int drive, bool wp);` and `void SetFileWriteProtect (int slot, int drive, bool readOnly, bool noPermission);`. Both set the flag on the image and publish; neither saves.
   - Add private `void PublishBayView (int slot, int drive);`, `mutable std::mutex m_viewMutex;` and `BayView m_views[kSlotCount][kDriveCount];`.
   - Document GetImage as disk-write-thread only, and correct the m_pendingMutex comment (:715-718).

2. DiskImageStore.cpp, PublishBayView: build the view from the Entry without the lock (the publisher owns the entry): mounted, path, `image ? image->GetWriteProtectInfo() : WriteProtectInfo()`, salvageOffered. Then take a lock_guard on m_viewMutex and assign. Call it from:
   - the first line of EmitBayChange (:2661), before the sink and whether or not a sink is installed. This covers Mount, MountRestored, Eject, EjectLostImage, and MountExternallyModifiedDisk, including the DiscardHeldWrites and ApplyPendingReload paths;
   - the end of MountFromBytes, on both the success branch and the failed-load branch (:280-299);
   - the end of SeatMedia, whenever outChanged is set;
   - SetImageWriteProtect, after :1481;
   - FlushEntry, after :1177-1178 and :1242-1243;
   - RepointBayToFile, after :3583;
   - the two new setters.
   PublishAllBayViews loops over every bay.

3. DiskManager.cpp
   - Change ApplyExternalWriteProtect (:152-177) to `(int drive, const std::string & path)`. It probes the file, then calls m_diskStore.SetUserWriteProtectFlag (6, drive, userWp) and SetFileWriteProtect (6, drive, readOnly, noPermission). Update its two call sites, :268 and :571.
   - In UpdateDriveWidgets, take `DiskImageStore::BayView view = m_diskStore.GetBayView (6, drive);` once per drive at the top of the loop. Use view.path in place of GetSourcePath at :803, which also closes entry-path-string-race for this reader, and replace :894-899 with `st.writeProtect = view.writeProtect;`.

4. EmulatorShellDisks.cpp
   - IsWriteProtectToggleOffered (:769-782) becomes `return ShouldEnableWriteProtectMenuItem (view.mounted, view.writeProtect);`.
   - ReportDamagedMount (:694-735): take the view first, keep the off-thread bounce, test `view.writeProtect.IsDamaged()`, and build the body with the existing `DamagedMountReport::FormatBody (view.writeProtect, fs::path (view.path).filename().wstring())` overload (DamagedMountReport.cpp:170).
   - IsSalvageOffered (:572-579) returns `view.mounted && view.salvageOffered`.
   - SetDriveUserWriteProtect (:794-816) calls m_machine.GetDiskStore().SetUserWriteProtectFlag (6, drive, wp) instead of dereferencing GetImage. (The plan then moves this handler to FR-015's saving `SetUserWriteProtect`; tasks.md T027 and T104.)

5. EmulatorWindow.cpp label query (:776-794): use the view. Return an empty label when !view.mounted or the file name is empty; otherwise return GetMenuLabel (IsImageProtected (view.writeProtect), name).

6. Replayer.cpp:479-487: replace the GetImage/SetUserWriteProtected pair with `m_machine.GetDiskStore().SetUserWriteProtectFlag (kDiskControllerSlot, record.value, record.detail != 0);`, so a replay never saves.

7. MachineHost::LoadStateSeating (MachineHost.cpp:1121-1155): call `m_diskStore->PublishAllBayViews();` after the Error: label, so both a full and a partial load republish what DiskImage::LoadState and SeatMedia changed.

After this, every remaining GetImage caller runs on the CPU thread or on a scratch store: DiskManager.cpp:224/:551, MachineHost.cpp:1427/:1823, HeatHistory.cpp:1355, MachineStateFile.cpp:64, DebugBatchRunner.cpp:273, ScratchHeatReplayer.cpp:566. Optional hardening: a Debug-only owner-thread assert in GetImage, armed from CpuManager's thread-enter hook. The salvage decode on the UI thread (AssessSalvage/SalvageToFile) is the separate defect salvage-decode-on-ui-thread and is not fixed by this.

**Regression test:** Primary test. It fails before the fix and passes after, and it uses only API that exists on both sides.

Extend UnitTest/EmuTests/DiskResetRemountHoldTests.cpp, the one test that builds a DiskManager over a TestMachine store. Reuse its Shell harness, its seams and MakeImage, and add TEST_METHOD (DriveWidgetsReadThePublishedBayNotTheLiveImage):
1. Install the image reader, identity reader and flush sink exactly as RemountUnderHoldWritesNothingAndKeepsTheDisk does.
2. store.Mount (6, 0, "view.nib"), construct the DiskManager, call manager.UpdateDriveWidgets(), and assert widgetState[0].writeProtect.userSetting is false.
3. Call store.GetImage (6, 0)->SetUserWriteProtected (true). This is the image changing under the frame without the store publishing, which is what a CPU-thread write in flight looks like from the UI.
4. Call manager.UpdateDriveWidgets() again and assert IsFalse (widgetState[0].writeProtect.userSetting, L"the frame shows the published bay, never the live image"). Today this fails, because UpdateDriveWidgets reads the image at DiskManager.cpp:895-897.
5. store.Eject (6, 0), call UpdateDriveWidgets, and assert widgetState[0].writeProtect == WriteProtectInfo().
6. Mount again, call UpdateDriveWidgets, and assert the widget again matches the mounted disk.

Store-level coverage for the 035 paths, which exercises the new API:
- Extend UnitTest/EmuTests/DiskFlushHoldTests.cpp, which already has PrepareDirtyDisk, retention and SeatMedia, with TEST_METHOD (BayViewFollowsSeatingDiscardAndEject):
  - After the mount, GetBayView (6, 0) shows mounted with path "hold.nib".
  - SetUserWriteProtectFlag (6, 0, true) sets view.writeProtect.userSetting.
  - With SetMediaRetention (true), Eject empties the view.
  - SeatMedia (mediaId) shows the disk again, although no bay change fires.
  - After Dirty, DiscardHeldWrites leaves the view mounted and matching the reloaded image.
- Extend Mount_CrcMismatchedImage_IsWriteProtected in UnitTest/EmuTests/DiskImageStoreTests.cpp (:883) to also assert GetBayView (...).writeProtect.checksumMismatch.
- Add a case to UnitTest/MachineStateTests.cpp: save a state with the user write-protect off, call SetUserWriteProtectFlag (6, 0, true), load the state, and assert GetBayView (6, 0).writeProtect.userSetting is false again (DiskImage::LoadState restored it and LoadStateSeating republished).

A two-thread stress test (a CPU role looping Eject, Mount and DiscardHeldWrites on a damaged WOZ against UpdateDriveWidgets) would fail before the fix only probabilistically, through the Debug CRT's 0xDD fill. It is useful as a manual check, not as a gate.

**Sites:** CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2411, CassoEmuCore/Shell/DiskManager.cpp:895, CassoEmuCore/Shell/DiskManager.cpp:897, CassoEmuCore/Shell/EmulatorShellPresent.cpp:637, CassoEmuCore/Devices/Disk/DiskImage.cpp:546, CassoEmuCore/Devices/Disk/DiskImage.cpp:561, CassoEmuCore/Devices/Disk/DamagedMountReport.cpp:118, CassoEmuCore/Shell/Window/EmulatorWindow.cpp:755, CassoEmuCore/Shell/Window/EmulatorWindow.cpp:777, CassoEmuCore/Shell/Window/EmulatorWindow.cpp:794, CassoEmuCore/Shell/EmulatorShellDisks.cpp:771, CassoEmuCore/Shell/EmulatorShellDisks.cpp:781, CassoEmuCore/Shell/EmulatorShellDisks.cpp:696, CassoEmuCore/Shell/EmulatorShellDisks.cpp:722, CassoEmuCore/Shell/EmulatorShellDisks.cpp:733, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2216, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:282, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2152, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3525, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1481, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1177, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1242, CassoEmuCore/Shell/DiskManager.cpp:175, CassoEmuCore/Shell/EmulatorShellDisks.cpp:814, CassoEmuCore/Debugger/Reverse/Replayer.cpp:485, CassoEmuCore/Devices/Disk/DiskImage.cpp:1303

## entry-path-string-race [changed-but-present]

**Evidence:** The accessor is unchanged in the worktree. DiskImageStore.cpp:2442-2447 GetSourcePath still returns `const string &` straight into Entry::path, with no lock. DiskImageStore.cpp:2463-2485 GetMountedSourcePaths reads entry.mounted (2477) and copies entry.path (2479), also unlocked. The header comment at DiskImageStore.h:715-718 says the path is "touched only by the thread that owns disk writes". That is false.

Writers, all on the CPU thread and none taking any lock:
- MountFromBytes: 272 (assign) and 283 (clear).
- RetireBay: 2217 `entry.path.clear()`. This helper is new in 035. Its callers are MountFromBytes 268, MountRestored 685 (new; reached from IDM_FILE_LOAD_STATE via MachineStateFile.cpp:562), SeatMedia 2137, Eject 2322 and EjectLostImage 3875.
- SeatMedia: 2153 `entry.path = kept.path`. New in 035, and master has neither SeatMedia nor RetireBay. Reverse execution reaches it on every keyframe restore: Replayer.cpp:384 -> MachineHost::LoadState 1083-1085 -> LoadStateSeating 1135 -> CheckStateHeader 1677 -> MachineHost::SeatMedia 1786.
- RepointBayToFile: 3583. Called from FlushEntry 1192 (motor-off flush, MachineBuilder.cpp:1519-1521), ResolvePendingChange 3143/3224 and CarryOutChangeAction 3364. ApplyPendingReload also runs from the motor-off and idle callbacks (MachineBuilder.cpp:1527, 1542-1545).

The command paths into these writers are CpuCommandDispatcher.cpp:86-95 (insert/eject), 109-110 (resolve), 175-177 (reverse) and 191-192 (load state). CpuManager.cpp:579-583 drains the queue even while paused, and the debug service at 585-588 also runs then.

UI-thread readers, still unlocked:
- DiskManager::UpdateDriveWidgets, every frame from EmulatorShellPresent.cpp:637. It binds the reference at DiskManager.cpp:803 and uses it at 837, 839 and 846.
- EmulatorShellPresent.cpp:780 (copy).
- SyncSceneDriveLabels at EmulatorShellScene.cpp:1009. Reached from EmulatorShellPresent.cpp:791/939, and through SyncSceneDriveChrome (EmulatorShellScene.cpp:914) from EmulatorWindow.cpp:838/2005 and EmulatorShellChrome.cpp:789/821/835/873.
- OnMouseMove tooltips at EmulatorWindowInput.cpp:505, 525 and 543. That function, starting at 282, takes no lock.
- The Disk menu label query at EmulatorWindow.cpp:786.
- RunSalvageFlow at EmulatorShellDisks.cpp:621, and ReportDamagedMount at 734, after the UI bounce at 713-720.
- AskAboutChange at EmulatorShellDisks.cpp:1157. It re-reads the bay after the modal at 1143 and hands the reference to AskWhereToSaveLostDisk, which reads it at 1200 and 1211-1212.
- CreateBlankDiskForDrive, through GetMountedSourcePaths at WindowCommandManager.cpp:1221.

Locking does not cover any of this. The only lock is the frame's shared lifetime lock (EmulatorShellPresent.cpp:564), and it is taken exclusively only by SwitchMachine (MachineManager.cpp:489). No path writer takes it, and the mouse and menu paths do not take it at all. m_pendingMutex (DiskImageStore.h:719) is taken around the pending records only. Its one path read is NoteExternalChange at 2694-2707, which runs on the watcher thread, and no writer takes it. ChangePrompt still does not keep the path it was built from (ChangePrompt.h struct; ChangeNotice at EmulatorShell.h:1487-1492), so AskAboutChange has nothing else to use.

Why the status is changed-but-present: the race is the same, but 035 adds new writers, SeatMedia/RetireBay through reverse seeks and MountRestored through state load. These fire far more often during reverse execution while the UI keeps presenting.

**035 impact:** 035 adds a frequent writer that bypasses every notification. SeatMedia (DiskImageStore.cpp:2117-2164, reached from keyframe restores during reverse seeks and replay) changes the path through RetireBay (2217) and 2153. By design it reports no bay change (DiskImageStore.h:167-172 and 181-185) and calls only BindDiskDrives (MachineHost.cpp:1791). RepointBayToFile (3565-3594) also emits no bay change, and NotifyMediaChanged is suppressed while replaying (2235).

So the per-bay UI snapshot proposed in ui-derefs-store-diskimage, if fed from OnBayChange/EmitBayChange or the media-change listener, would show a stale path after a reverse seek across a recorded eject or mount, and after a conflict flush moves the bay to its preserved copy. The path has to be published at the store's own path-write chokepoint. Whichever design lands, every write of m_entries[][].path must go through one helper.

The flush hold is no protection. m_isFlushHeld only stops FlushAllUnlessHeld (1893-1906), which removes the motor-off FlushEntry -> RepointBayToFile writer while the machine is behind live. It does not stop SeatMedia, which runs exactly then. The fix must therefore not be gated on m_isFlushHeld or m_isReplaying.

Removing StepInstructionWhilePaused changes nothing here. A paused machine still drains disk commands (CpuManager.cpp:579-583) and runs the debug service (585-588), so pause does not quiesce the writers.

035 also leaves one constraint on the lock: m_pendingMutex is held in seven short scopes (2694, 2758, 2832, 2881, 2945, 3156, 3387). None of them calls a path writer, so writers can take that mutex without a lock-order risk, provided no path write is ever added inside one of those scopes.

**Proposed fix:** Give callers on other threads a copy made under a lock that every path writer also takes, and stop AskAboutChange re-reading the bay after its modal.

1. DiskImageStore.h
   - Change :229 to `string GetSourcePath (int slot, int drive) const;` and comment that it returns a copy because the UI thread reads it every frame while the thread that owns disk writes moves it.
   - Remove `m_emptyPath` (:693).
   - Declare a private `void SetBayPath (Entry & entry, const string & path);`.
   - Rewrite the m_pendingMutex comment at :715-718. It now guards the pending records and each bay's path, which the UI thread reads and the watcher matches against. The disk-write thread takes it to change a path and reads its own paths without it. No path write may happen inside one of its scopes.

2. DiskImageStore.cpp
   - Add `SetBayPath`: a `std::lock_guard<std::mutex> guard (m_pendingMutex); entry.path = path;`. Reusing this mutex also closes the watcher-thread read in NoteExternalChange (2694-2707), which already holds it.
   - Route every m_entries path write through SetBayPath:
     - SeatMedia 2153: `SetBayPath (entry, kept.path)`.
     - RetireBay 2217: `SetBayPath (entry, string())`. RetireBay itself is fine to keep reading `entry.path` unlocked at 2206.
     - RepointBayToFile 3583: `SetBayPath (entry, newPath)`.
     - MountFromBytes: move the path assignment from 272 into the success branch, just before AssessSalvage (which reads entry.path, around 1670). A failed load then never shows the UI a path, so there is no door flap, and the clear at 283 goes away.
   - GetSourcePath (2442-2447): take the lock and return `GetEntry (slot, drive).path` by value, or `string()` for an invalid bay. Delete the comment about returning a reference.
   - GetMountedSourcePaths (2463-2485): hold the lock across the loop and filter on `!entry.path.empty()` alone. `mounted` is a separately unsynchronized flag (entry-flags-race), and an empty virtual path is skipped either way.

3. DiskManager.cpp:803: declare `std::string src` by value instead of binding a `const std::string &`. The other call sites already copy or pass a temporary and compile unchanged:
   - EmulatorShellPresent.cpp:780 and EmulatorShellScene.cpp:1009.
   - EmulatorWindowInput.cpp:505, 525 and 543.
   - EmulatorWindow.cpp:786.
   - EmulatorShellDisks.cpp:621 and 734.
   - DiskManager.cpp:225, 571 and 668.
   - MachineManager.cpp:417-418, MachineStateFile.cpp:75 and MachineDebugTarget.cpp:900.

4. AskAboutChange
   - Add `std::string imagePath;` to ChangePrompt (ChangePrompt.h).
   - Set `prompt.imagePath = imagePath;` in every composer: ChangePrompt.cpp:140 Compose, 242 ComposeReloadReport, 328 ComposeConflictReport, 366 ComposeSaveFailure and 445 ComposeLostFile.
   - At EmulatorShellDisks.cpp:1157, pass `notice.prompt.imagePath` to AskWhereToSaveLostDisk instead of `m_machine.GetDiskStore().GetSourcePath (notice.slot, notice.drive)`. The picker is then seeded from the file the question was about, even if the bay moved or emptied during the modal at 1143.

5. If ui-derefs-store-diskimage lands as a CPU-published per-bay snapshot, SetBayPath is where that snapshot's path field should be published. OnBayChange is not, for the reasons in fixImpact.

**Regression test:** All tests below are deterministic and single-threaded. Each holds the reference exactly the way DiskManager.cpp:803 does, so before the fix the store's own write shows through the alias, and after the fix the reference binds a lifetime-extended copy.

1. UnitTest/EmuTests/DiskHistoryTests.cpp: add a TEST_METHOD modeled on SeekingForwardAcrossAMountPutsTheSameDiskIn (370-423). It covers the 035 writer.
   - Run to the live end with "second.nib" mounted.
   - Take `const std::string & shown = machine.GetDiskStore().GetSourcePath (6, 0);`.
   - Call `controller.SeekToPosition (empty, result)`, which goes through SeatMedia -> RetireBay, the clear at DiskImageStore.cpp:2217.
   - Assert `IsMounted (6, 0)` is false and `Assert::AreEqual<std::string> ("second.nib", shown)`.
   - Fails before (shown reads ""), passes after.

2. UnitTest/EmuTests/SharedImageTests.cpp: clone AFlushOverAnExternalChangeLeavesTheFileAloneAndMovesTheGuestsVersion (968-999).
   - Bind `const std::string & shown = rig.store.GetSourcePath (kSlot, kDrive);` before `rig.store.Flush`.
   - After the flush (FlushEntry 1192 -> RepointBayToFile 3583), assert `shown == kImagePath` and `GetSourcePath (kSlot, kDrive) == rig.PreservedPaths()[0]`.
   - Fails before (shown reads the preserved file's path), passes after.
   - In the same file, extend AFlushThatCannotKeepTheDisplacedVersionAsksWhereToPutIt (1037-1078). After ResolvePendingChange has moved the bay to `chosen`, assert `rig.questions[0].imagePath == std::string (kImagePath)`. That pins the field AskAboutChange now uses. It does not compile before the fix and passes after.

3. UnitTest/EmuTests/DiskImageStoreTests.cpp: next to MountedSourcePaths_ReportsEveryMountedBay (246-267), add SourcePath_HeldAcrossEject_KeepsTheEjectedPath.
   - `MountFromBytes (6, 0, "a.dsk", ...)`, then `const std::string & held = store.GetSourcePath (6, 0);`, then `store.Eject (6, 0);`.
   - Assert `std::string ("a.dsk") == held` and `store.GetSourcePath (6, 0).empty()`.
   - Add `static_assert (std::is_same_v<decltype (std::declval<const DiskImageStore &>().GetSourcePath (0, 0)), std::string>)` to pin the by-value contract.

4. Optional, not a gate because it is nondeterministic before the fix: a stress test in DiskImageStoreTests.cpp.
   - A writer std::thread alternates MountFromBytes with two long paths and Eject.
   - The test thread copies GetSourcePath in a loop and asserts each copy is "", the first path or the second.
   - Pattern after the thread use in UnitTest/ControllerTests/MachineGamePortSinkTests.cpp:227-256.

**Sites:** CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2442, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2446, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2463, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2479, CassoEmuCore/Devices/Disk/DiskImageStore.h:229, CassoEmuCore/Devices/Disk/DiskImageStore.h:715, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:272, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:283, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2153, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2217, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3583, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1192, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2707, CassoEmuCore/Shell/MachineHost.cpp:1786, CassoEmuCore/Shell/DiskManager.cpp:803, CassoEmuCore/Shell/DiskManager.cpp:837, CassoEmuCore/Shell/EmulatorShellPresent.cpp:780, CassoEmuCore/Shell/EmulatorShellScene.cpp:1009, CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp:505, CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp:525, CassoEmuCore/Shell/Window/EmulatorWindowInput.cpp:543, CassoEmuCore/Shell/Window/EmulatorWindow.cpp:786, CassoEmuCore/Shell/EmulatorShellDisks.cpp:621, CassoEmuCore/Shell/EmulatorShellDisks.cpp:734, CassoEmuCore/Shell/EmulatorShellDisks.cpp:1157, CassoEmuCore/Shell/EmulatorShellDisks.cpp:1200, CassoEmuCore/Shell/WindowCommandManager.cpp:1221

## salvage-decode-on-ui-thread [still-present]

**Evidence:** The flow is unchanged in the worktree; only its line numbers moved, and 035 adds more CPU-thread mutators plus a journal/gate bypass.

UI-thread entry points:
- WindowCommandManager.cpp:1497-1501 calls m_shell.RunSalvageFlow directly ("Stays on the UI thread").
- HandleMountCompletion (UI thread, from WM_APP_MOUNT_COMPLETED, EmulatorWindow.cpp:2657) calls ReportDamagedMount at EmulatorShellDisks.cpp:244. ReportDamagedMount bounces off-thread callers at :713-720, but before the bounce it already calls GetImage at :696. On the UI thread it then reads image->IsDamaged (:722) and FormatBody(*image) (:733), and calls AssessSalvage at :737.
- RunSalvageFlow calls AssessSalvage at :610 and GetSourcePath (entry.path) at :621.
- Modals at :631 (ShowSalvageDialog -> ShowModalDialog :556), :651 and :670.
- SalvageToFile at :637, then MountDiskInSlot6 called DIRECTLY on the UI thread at :674.

Inside the store, with no lock:
- AssessSalvage: mounted check at DiskImageStore.cpp:1643, IsDamaged at :1653, DecodeForSalvage at :1656 -> SalvageSectors(*entry.image) at :1536, entry.path -> suggestedPath at :1666-1671.
- SalvageToFile: mounted check at :1713, entry.path at :1719, BuildSalvagedImage at :1722 -> decode at :1565 and entry.image->GetWozMetadata at :1574.

Nothing pauses the machine: ShowModalDialog (EmulatorShellDialogs.cpp:330-333) does not, and the CPU thread drains commands paused or not (CpuManager.cpp:579-583).

CPU-thread mutators that can run during the modals:
- WriteBit (Disk2NibbleEngine.cpp:692) and SpliceFluxWrite (:444).
- RetireBay's entry.image.reset at DiskImageStore.cpp:2216, reached from Eject (:2322), from MountFromBytes (:268, new image at :271) and from 035's SeatMedia (:2137; image moved in at :2152, MachineHost.cpp:1677/1786).
- MountExternallyModifiedDisk's *entry.image = std::move at :3525. It is reached from the idle callback (MachineBuilder.cpp:1542-1545), the motor-off callback (:1527), CarryOutChangeAction (:3310) and 035's DiscardHeldWrites (:2014).
- The motor-off flush at MachineBuilder.cpp:1521.
- RepointBayToFile, which rewrites entry.path at DiskImageStore.cpp:3583. That is a std::string the UI thread reads at :1666/:1719.

So the bay can hold another disk by the time :637 runs. SalvageToFile then writes that disk into the first disk's suggestedPath, and :674 offers to mount it.

New with 035: the direct MountDiskInSlot6 at :674 skips DispatchCpuCommand, so the insert is never journaled (EmulatorShellCpuThread.cpp:350-355). It also skips the divergence gate, which checks only posted commands (EmulatorShellReverse.cpp:303-331; IDM_DISK_INSERT is state-changing via CpuCommandDispatcher.cpp:245-250). On top of that it runs Mount/RetireBay/the controller re-point on the UI thread while the drive engine is reading the image it frees.

**035 impact:** 1. Media identity (new in 035). DiskImage::GetImageId / DiskImageStore::GetMediaId (DiskImageStore.cpp:2037-2049) are a ready-made token for the "same disk still in the bay" re-check. RenewIdentity runs on every load (DiskImage.cpp:41, :813, :943), so the token changes on eject+insert, on a reload through MountExternallyModifiedDisk (including DiscardHeldWrites) and on SeatMedia. Use it rather than comparing paths, which RepointBayToFile can change without changing the disk.

2. Flush hold (new in 035). While reverse recording holds flushes (ReverseController.cpp:2005/2070), guest writes exist only in entry.image. The decode must keep reading the in-memory image, now on the CPU thread; it must not re-read the source file. The salvage write goes to a separate file through m_flushSink/WriteFileAtomically, so it does not break the hold. Do not route it through FlushEntry/FlushAllUnlessHeld.

3. Journal and divergence gate (new in 035). Inserting the salvaged copy must go through the posted IDM_DISK_INSERT1/2 command (DiskManager::Mount, DiskManager.cpp:699-716), never MountDiskInSlot6 from the UI thread. That way it is journaled as InputKind::DiskMount and, when the machine is behind live, raises the divergence question. The new salvage assess and write commands are not state-changing (DivergenceGate.cpp:25-45), so they pass the gate without a question.

4. More ways the bay changes mid-modal (new in 035). SeatMedia on a state load or reverse seek, and DiscardHeldWrites, can also swap the bay while the dialog stands. Debug-channel commands keep draining while the modal is up. With retention on, RetireBay moves the image into m_retained (:2203-2213) instead of freeing it. A stale decode would then read a valid but wrong disk instead of crashing, which hides the damage, so the identity check is the real guard rather than a lifetime fix.

5. Paused machine. Commands still drain while paused (CpuManager.cpp:579-583), so a posted assess/write works with the machine stopped in the debugger. The removal of StepInstructionWhilePaused does not touch this path.

**Proposed fix:** Use the store's existing ask/answer round trip (WM_APP_CHANGE_ASK -> AskAboutChange -> PostCommand(IDM_DISK_RESOLVE_CHANGE), EmulatorShellDisks.cpp:855-868 and :1099-1172).

1. DiskImageStore.h:76 SalvageAssessment: add `uint64_t mediaId = 0;` and `string sourcePath;`. AssessSalvage fills them from entry.image->GetImageId() and entry.path inside the existing block at DiskImageStore.cpp:1640-1673.

2. Change SalvageToFile to `HRESULT SalvageToFile (int slot, int drive, uint64_t mediaId, const string & path, DenibblizeReport & report);`. After the bay check, before reading entry.path or decoding, hoist `isSameMedium = GetMediaId (slot, drive) == mediaId;` and add `CBREx (isSameMedium, HRESULT_FROM_WIN32 (ERROR_MEDIA_CHANGED));`. This is a plain CBREx because it is a runtime condition, not a coding error. Update the four existing callers in DiskImageStoreTests.cpp to pass store.GetMediaId(...).

3. CPU-thread commands:
- IDM_DISK_SALVAGE1/2 become CPU-thread commands. WindowCommandManager::OnDiskCommand (:1497-1503) does `m_shell.PostCommand (static_cast<WORD> (id));` and its comment changes accordingly.
- Add a free id IDM_DISK_SALVAGE_WRITE in resource.h (40078 or the next unused). Its payload is "<drive> <mediaId> <path>", path last so it may hold spaces; parse it like DispatchResolveChange (CpuCommandDispatcher.cpp:293-318).
- In ICpuCommandTarget (CpuCommandDispatcher.h) add `virtual void AssessSalvage (int drive) = 0;` and `virtual void WriteSalvagedCopy (int drive, uint64_t mediaId, const std::string & path) = 0;`, and dispatch both in CpuCommandDispatcher::Dispatch.

4. EmulatorShell, CPU thread (EmulatorShellCpuThread.cpp):
- AssessSalvage runs the store's AssessSalvage(6, drive, a). If it succeeds and a.isOffered, it posts `new SalvageOffer { drive, a }` with a new WM_APP_SALVAGE_OFFER (EmulatorShellInternal.h, after 0x2E), deleting it if the post fails.
- WriteSalvagedCopy calls SalvageToFile(6, drive, mediaId, path, report) and posts `new SalvageOutcome { drive, hr, report, path }` with WM_APP_SALVAGE_DONE.

5. EmulatorShell, UI thread (EmulatorShellDisks.cpp):
- Split RunSalvageFlow into ShowSalvageOffer(const SalvageOffer &), which is :621-635 using offer.assessment.sourcePath instead of GetSourcePath. On Salvage it calls `PostCommand (IDM_DISK_SALVAGE_WRITE, std::format ("{} {} {}", drive, a.mediaId, a.suggestedPath))`.
- Add ShowSalvageOutcome(const SalvageOutcome &), which is :639-677. For ERROR_MEDIA_CHANGED it says the disk in the drive changed before the copy was written and nothing was written. On Insert it calls `m_diskManager->Mount (6, drive, fs::path (outcome.path).wstring())` instead of MountDiskInSlot6.
- EmulatorWindow.cpp gets handlers for the two messages, next to WM_APP_CHANGE_ASK at :2502.

6. ReportDamagedMount:
- Compute the damage report on the thread that ran the mount. In OnMountCompleted (:167-201) fill new MountCompletion fields: `bool isDamaged`, `std::wstring damageBody` (DamagedMountReport::FormatBody(*image, path)) and `bool salvageOffered` (the store's cached IsSalvageOffered, set at mount at DiskImageStore.cpp:296-298).
- HandleMountCompletion then shows the report from those values and never calls GetImage or AssessSalvage. Its "Salvage readable sectors..." button posts IDM_DISK_SALVAGE1/2 rather than calling RunSalvageFlow inline.
- Remove the WM_APP_REPORT_DAMAGE bounce, which becomes dead (EmulatorShellDisks.cpp:713-720, EmulatorWindow.cpp:2647-2652).

After this, no UI-thread code dereferences entry.image or reads entry.path for salvage. Only plain values are held across the modals, and the CPU thread re-checks identity before it writes.

**Regression test:** Primary test, UnitTest/EmuTests/DiskImageStoreTests.cpp, next to SalvageToFile_RefusesToOverwriteTheSource (:1184): TEST_METHOD (SalvageToFile_AfterTheBayChanged_WritesNothing).
- Set a counting flush sink.
- MountFromBytes(kSlot, kDrive, "broken.woz", Woz, MakeDamagedStandardWoz()), then AssessSalvage -> a.
- MountFromBytes a second damaged standard WOZ ("other.woz", the generator with a different seed) into the same bay. This is what a queued insert does on the CPU thread while the Salvage dialog stands.
- Call SalvageToFile(kSlot, kDrive, a.mediaId, a.suggestedPath, report).
- Assert hr == HRESULT_FROM_WIN32(ERROR_MEDIA_CHANGED) and sinkCalls == 0.
Before the fix, the same sequence through today's SalvageToFile(kSlot, kDrive, a.suggestedPath, report) succeeds and writes other.woz's sectors into broken.salvaged.woz, so the assertion fails. Add a sibling, SalvageToFile_AfterEjectAndReinsertOfTheSameFile_WritesNothing (Eject, then MountFromBytes the same bytes and path). The identity changes even though the path does not, which pins the choice of GetMediaId over a path compare. Also add AssessSalvage_GivesTheMediaIdAndSourcePath: a.mediaId == store.GetMediaId(kSlot, kDrive) and a.sourcePath == "broken.woz".

Routing tests, UnitTest/EmuTests/CpuCommandDispatcherTests.cpp (extend the Notebook at :447):
- SalvageAssessesTheDriveItsIdSelects: Dispatch(IDM_DISK_SALVAGE2, "") records AssessSalvage(1). Today it records nothing.
- ASalvageWriteKeepsTheMediaIdAndThePathWithItsSpaces: "1 42 C:\\My Disks\\broken.salvaged.woz" reaches WriteSalvagedCopy(1, 42, that path).
- ASalvageWriteThatDoesNotParseDispatchesNothing.

Gate test, UnitTest/EmuTests/DivergenceGateTests.cpp: IsStateChangingCommand is false for IDM_DISK_SALVAGE1/2 and IDM_DISK_SALVAGE_WRITE and stays true for IDM_DISK_INSERT1/2. This pins the posted insert as the journaled path for the salvaged copy.

**Sites:** CassoEmuCore/Shell/WindowCommandManager.cpp:1501, CassoEmuCore/Shell/EmulatorShellDisks.cpp:244, CassoEmuCore/Shell/EmulatorShellDisks.cpp:610, CassoEmuCore/Shell/EmulatorShellDisks.cpp:621, CassoEmuCore/Shell/EmulatorShellDisks.cpp:631, CassoEmuCore/Shell/EmulatorShellDisks.cpp:637, CassoEmuCore/Shell/EmulatorShellDisks.cpp:670, CassoEmuCore/Shell/EmulatorShellDisks.cpp:674, CassoEmuCore/Shell/EmulatorShellDisks.cpp:696, CassoEmuCore/Shell/EmulatorShellDisks.cpp:733, CassoEmuCore/Shell/EmulatorShellDisks.cpp:737, CassoEmuCore/Shell/EmulatorShellDisks.cpp:751, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1536, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1574, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1643, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1656, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1666, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1713, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1719, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1722

## note-external-change-reads-entry-unlocked [changed-but-present]

**Evidence:** The defect is still present at 811a6f727. 035 moved and added sites, so the status is changed-but-present.

READER. NoteExternalChange (CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2692-2715) takes m_pendingMutex at :2694. For all 16 bays it reads entry.mounted and copies entry.path through MountedImageState::IsSamePath (:2707; IsSamePath copies both strings, MountedImageState.cpp:187-190). It then writes m_pending through NoteChange (:2709 -> MountedImageState.cpp:125-131). It runs on every watcher worker thread: the BeginWatching lambda at DiskImageStore.cpp:3623-3627, whose thread starts at Win32ImageWatcher.cpp:86, and the callback is invoked at :316 without Win32ImageWatcher::m_mutex. It also runs on the UI thread from EmulatorShell::OnCopyData (CassoEmuCore/Shell/Window/EmulatorWindow.cpp:2789).

WRITERS THAT NEVER TAKE m_pendingMutex:
- MountFromBytes, :271-274, and its failure clear at :282-284.
- Mount :626 and MountRestored :696. Both call MountedImageState::Mount, which resets m_pending (MountedImageState.cpp:18).
- RetireBay. At :2205-2210 it copies the whole sharedState, m_pending included, into the kept entry while a watcher may be writing it. At :2216-2220 it does path.clear(), mounted = false and sharedState.Eject(), which resets m_pending (MountedImageState.cpp:46). In 035, RetireBay is the empty-the-bay step for Eject :2322, EjectLostImage :3875, MountFromBytes :268, MountRestored :685 and SeatMedia :2137.
- SeatMedia, :2152-2157 (new in 035). It assigns path and mounted, and assigns the whole sharedState as one object.
- RepointBayToFile, :3583 and :3587. FlushEntry's conflict branch reaches it at :1192, right after SaveLoadedImage wrote a file into the watched folder (:1157). ResolvePendingChange reaches it at :3143 and :3224, and CarryOutChangeAction at :3364.

The member comment at DiskImageStore.h:715-718 still says the path is touched only by the thread that owns disk writes. NoteExternalChange contradicts that.

WHY THE WATCH JOIN DOES NOT ORDER THEM. EndWatching (:3651-3684) calls Unwatch only when no other bay uses the directory (:3661-3679). NoteExternalChange scans all 16 bays, so any live watch on any folder races a write to any bay. Two writers never join anything:
- SeatMedia into an empty bay: the check at :2134 is false, so no EndWatching runs.
- MountFromBytes over a mounted bay (:263-268): there is no EndWatching at all, so the old folder's watch is never dropped and its thread keeps calling in.

The destructor flush at EmulatorShell.cpp:299 runs on the UI thread (FlushAllForShutdown -> FlushEntry -> RepointBayToFile) while the watcher threads are still alive. m_diskManager owns the watcher (DiskManager.h:191), and it is a member at EmulatorShell.h:2613, so it is destroyed only after the destructor body, and before m_machine (:1706).

CONSEQUENCE. A data race on std::string: `entry.path = newPath` or `path.clear()` can free or reallocate the buffer IsSamePath is copying (a heap read after free). PendingChange records can also be torn.

DEADLOCK CHECK STILL HOLDS. Every m_pendingMutex scope (:2694, :2758, :2832, :2881, :2945, :3156, :3387) is a leaf. Unwatch joins while holding Win32ImageWatcher::m_mutex (Win32ImageWatcher.cpp:137, :186-189), but the callback never takes that mutex.

**035 impact:** 035 sends most writers through RetireBay (DiskImageStore.cpp:2197-2221), so one lock there covers Eject, EjectLostImage, MountRestored, SeatMedia's retire and the mount-over path. That is fewer sites than master's separate Eject and EjectLostImage bodies. But 035 also adds writers master did not have: SeatMedia :2152-2157 and MountRestored :696. RetireBay's sharedState copy (:2210) is also a new unlocked read of m_pending.

Where the locks can go:
1. Inside RetireBay, not around its callers. MountFromBytes (:268), MountRestored (:685), SeatMedia (:2137), Eject (:2322) and EjectLostImage (:3875) all call RetireBay, and std::mutex is not recursive. A caller's own lock scope must start after RetireBay returns.
2. Never across EndWatching or BeginWatching. Win32ImageWatcher::Unwatch joins a worker that may be blocked in NoteExternalChange waiting for m_pendingMutex. In RepointBayToFile, SeatMedia, Eject and EjectLostImage, the scope must sit strictly between EndWatching and BeginWatching.
3. ReadIdentity (a stat) runs before the lock is taken.

The flush hold does not remove the trigger. FlushAllUnlessHeld (:1893-1906) suppresses only the automatic flushes. Eject (:2304) and the mount-over in MountFromBytes (:265) still commit into the watched folder just before they rewrite the path; UnitTest/EmuTests/DiskFlushHoldTests.cpp HoldStillSavesOnEjectSwitchAndExit pins the eject case. m_isReplaying stops FlushEntry (:1090), but it does not stop SeatMedia's path writes or the watcher threads.

Removing StepInstructionWhilePaused does not change the fix. Reverse steps and seeks reach SeatMedia via MachineHost.cpp:1677 -> :1786 and still run on the machine thread (ReverseController.h:92-95). Reads of entry.path on that thread stay lock-free; only the writes need the lock. The UI-thread destructor flush (EmulatorShell.cpp:299) still overlaps live watcher threads, and the RepointBayToFile lock covers it.

**Proposed fix:** All changes are in CassoEmuCore/Devices/Disk/DiskImageStore.cpp and DiskImageStore.h. NoteExternalChange itself does not change.

1. RetireBay (:2197-2221): wrap the kept-copy block (:2203-2214) and the bay reset (path.clear, mounted = false, salvageOffered = false, sharedState.Eject) in one `std::lock_guard<std::mutex> guard (m_pendingMutex);` scope. To keep DiskImage teardown out of the lock, move entry.image into a local unique_ptr inside the scope and let it destruct after the scope.

2. MountFromBytes (:242-304): after the flush and RetireBay, build the image in a local `unique_ptr<DiskImage> loaded = make_unique<DiskImage> ()` and call LoadFromBytes on it outside the lock. On success, publish image (moved in), path, format and mounted in one m_pendingMutex scope, then run AssessSalvage as today. On failure nothing was published, so the clear at :282-285 goes away; keep salvageOffered = false and ClassifyLoadFailure. This also closes the window where mounted is true while the image has not loaded. Moving the unique_ptr keeps the DiskImage address the controller is later given.

3. Mount :626 and MountRestored :696: compute `ImageIdentity identity = ReadIdentity (path);` first, then take m_pendingMutex around `GetEntry (slot, drive).sharedState.Mount (identity)`.

4. SeatMedia: take m_pendingMutex around the six assignments at :2152-2157. BeginWatching (:2159) stays after the scope.

5. RepointBayToFile: read `ImageIdentity identity = ReadIdentity (newPath);` before the lock. Then take m_pendingMutex around `entry.path = newPath; entry.sharedState.Mount (identity);`. EndWatching (:3581) stays before the scope and BeginWatching (:3589) after it, both outside the lock.

6. Rewrite the m_pendingMutex comment at DiskImageStore.h:715-718 to say:
   - It guards each bay's pending record, path and mounted flag against the watcher's lookup in NoteExternalChange, and so every MountedImageState::Mount, Eject and whole-object copy.
   - The thread that owns disk writes writes these only under the lock and may read them without it.
   - It is never held across EndWatching or BeginWatching, because Unwatch joins a watcher thread that may be waiting for it.

The alternative, a separate locked path-to-bay table, was rejected. The m_pending resets in MountedImageState::Mount and Eject, and the whole-object copies in RetireBay and SeatMedia, would still need this lock.

**Regression test:** Extend UnitTest/EmuTests/SharedImageTests.cpp. Its Rig already redirects reads, writes, identities and the clock into memory. <thread>, <mutex>, <condition_variable> and <atomic> already come in through CassoEmuCore/Pch.h.

Add a class-static helper, `static bool IsHeldOffWhileAChangeIsRecorded (Rig & rig, std::function<void ()> mutation)`. It works because NoteExternalChange takes m_pendingMutex at :2694 before it calls GetNowMs at :2695.

1. Install `rig.store.SetClock` with a lambda. On its first call after an atomic flag is armed, it sets `isRecording` under a test mutex, notifies a condition_variable, and waits at most 5 s for `isReleased`. It returns rig.nowMs. While it waits, the store's mutex stays held.
2. Start a std::thread that stands in for the watcher worker and calls `rig.store.NoteExternalChange (kImagePath, ExternalChangeIntent::Unstated)` directly. It does not use rig.watcher.Fire, because FakeImageWatcher is not thread-safe.
3. Wait at most 5 s for isRecording.
4. Start a second std::thread that stands in for the CPU thread. It runs `mutation()`, then sets `isDone` under the test mutex and notifies.
5. Wait 250 ms for isDone, and keep the result as `finishedWhileHeld`.
6. Set isReleased, notify, and join both threads before any Assert, so a failing assertion cannot leave a joinable std::thread.
7. Restore the clock to `[&rig] () { return rig.nowMs; }` and return `!finishedWhileHeld`.

Every wait has a deadline, so a broken build fails instead of hanging.

Four TEST_METHODs, each starting from `rig.WriteImage (kImagePath, 0x11)` and a Mount of kImagePath:
- AnEjectWaitsForAChangeBeingRecorded: mutation `rig.store.Eject (kSlot, kDrive)`. Then assert the bay is not mounted and the pending record has seen == false.
- AMountOverADiskWaitsForAChangeBeingRecorded: write "C:\\work\\Other.dsk", mutation `rig.store.Mount (kSlot, kDrive, other)`. Then assert GetSourcePath returns that path and seen == false.
- SeatingAKeptDiskWaitsForAChangeBeingRecorded: SetMediaRetention (true), take GetMediaId, then Eject. Mutation `rig.store.SeatMedia (kSlot, kDrive, mediaId, changed)`. Then assert the bay is mounted again.
- AFlushThatMovesTheBayWaitsForAChangeBeingRecorded: make the bay dirty the way ADirtyImageMeetingAnExternalChangeKeepsBothVersions does (GetTrackBitsForWrite (0)[0] = 0x7F; SetLoadedForTest (true, true)), then `rig.WriteImage (kImagePath, 0x22)` without firing. Mutation `rig.store.Flush (kSlot, kDrive)`, which takes FlushEntry's conflict branch into RepointBayToFile. Then assert GetSourcePath equals rig.PreservedPaths()[0].

Each test asserts `IsTrue (IsHeldOffWhileAChangeIsRecorded (...), L"... waited for the change being recorded")`.

Before the fix, every mutation finishes inside the 250 ms window, because none of the writers takes m_pendingMutex, and all four fail. After the fix, each one blocks in RetireBay, MountFromBytes, SeatMedia or RepointBayToFile until the recording releases the lock, and all four pass. A plain two-thread stress test would not be reliable here: MSVC has no ThreadSanitizer, so it would pass on broken code.

**Sites:** CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2694, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2707, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2709, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3623, CassoEmuCore/Devices/Disk/Win32ImageWatcher.cpp:316, CassoEmuCore/Shell/Window/EmulatorWindow.cpp:2789, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:271, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:282, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:626, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:696, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2152, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2157, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2210, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2216, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2220, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2322, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3583, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3587, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3875, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1192, CassoEmuCore/Devices/Disk/MountedImageState.cpp:18, CassoEmuCore/Devices/Disk/MountedImageState.cpp:46, CassoEmuCore/Devices/Disk/DiskImageStore.h:715, CassoEmuCore/Shell/EmulatorShell.cpp:299

## drive-widget-sync-events-unlocked [still-present]

**Evidence:** DriveWidgetController.h and .cpp are byte-identical between master and 811a6f727 (git diff --stat shows no change), and the race is unchanged. The class has no mutex and no atomics: its only members are m_syncEvents and m_nextSyncEventId (CassoEmuCore/Ui/DriveWidgetController.h:61-62). PublishSyncEvent post-increments the id (DriveWidgetController.cpp:135) and calls push_back (:140) with no lock. ConsumeSyncEvents swaps the vector out with no lock (:161).

Producers. OnBayChange calls PublishSyncEvent at Shell/DiskManager.cpp:604 (eject), :613 (insert) and :624 (swap). OnBayChange is the store's bay-change sink, wired at DiskManager.cpp:976-979 and fired from DiskImageStore::EmitBayChange (Devices/Disk/DiskImageStore.cpp:2661-2666). EmitBayChange is called by Mount :635, MountRestored :700, Eject :2326, MountExternallyModifiedDisk :3537 and EjectLostImage :3880. The CPU thread reaches these as follows:
(a) Posted commands. CpuManager::ThreadProc calls DrainCommandQueue (CpuManager.cpp:583), which releases m_cmdMutex before it dispatches (:335-347). The dispatch goes to EmulatorShell::RemountDisks/MountDisk/EjectDisk/ResolvePendingChange (EmulatorShellCpuThread.cpp:419, :434, :449, :481). The drain also runs while the machine is paused (CpuManager.cpp:579-583).
(b) Reload swaps. ApplyPendingReload runs from the Disk2Controller motor-off and idle callbacks (MachineBuilder.cpp:1527, :1544), then ApplyPendingReloadToBay -> CarryOutChangeAction (DiskImageStore.cpp:3047) -> MountExternallyModifiedDisk (:3310) -> Swapped.
(c) Machine switch. The switch runs on the CPU thread (MachineManager.cpp:392). Its exclusive lifetime-lock scope closes at :527, so the remount at MachineManager.cpp:606 (MountCommandLineDisks) runs with no lock held.
(d) State load, new in 035. EmulatorShell::LoadMachineState is documented as running on the CPU thread (EmulatorShellState.cpp:148). It calls MachineStateFile::Apply (:185) -> RestoreDisks -> store.MountRestored / store.Eject (MachineStateFile.cpp:562, :567).
(e) A UI-thread producer still exists: the salvage Insert at EmulatorShellDisks.cpp:674.

Consumer. UpdateDriveWidgets calls ConsumeSyncEvents on the UI thread (DiskManager.cpp:795). It is called from TryPresentUiFrame (EmulatorShellPresent.cpp:637). That frame holds only the shared side of the lifetime lock (:564), and none of the producers (a)-(d) takes that lock. Interleaving push_back with swap is a data race on a std::vector. It can lose events (a DoorReinsert, so a swap shows no door animation), duplicate them, or corrupt the heap during reallocation. Ids can also be handed out by one producer and pushed out of order by another, which leaves st.lastSyncEventId (DiskManager.cpp:815) behind.

**035 impact:** The fix is local to DriveWidgetController, so neither the 035 flush hold nor the new pause/step machinery changes it. Neither one serializes the producers against the frame.

The flush hold covers one producer only. It returns early from RemountSlot6Disks (DiskManager.cpp:661-664), and only while reverse execution holds the disks. The other producers are unaffected: insert and eject commands, reload swaps, the machine-switch remount, state load and salvage.

Removing StepInstructionWhilePaused does not matter here. Commands still drain while the machine is paused (CpuManager.cpp:579-583), so a paused machine races the same way.

035 adds a CPU-thread producer that master lacked: LoadMachineState -> MachineStateFile::RestoreDisks -> MountRestored/Eject (EmulatorShellState.cpp:185, MachineStateFile.cpp:562/567, DiskImageStore.cpp:700/2326). DiscardHeldWrites -> MountExternallyModifiedDisk (DiskImageStore.cpp:2014) would be another, but today only DiskFlushHoldTests calls it.

Line numbers moved from the master report:
- The consume call is at DiskManager.cpp:795 (was :787).
- The switch remount is at MachineManager.cpp:606 (was :616).
- The store emits are at DiskImageStore.cpp:635/700/2326/3537/3880.

Producers run on two threads: the CPU thread, and the UI thread through the salvage Insert at EmulatorShellDisks.cpp:674. The single-producer, single-consumer queue option in the master fix direction is therefore wrong unless salvage-mount-on-ui-thread lands first. A mutex is correct either way. The id must also be assigned inside the lock: with two producers, a separately bumped id can be pushed out of order.

**Proposed fix:** Guard both members with one leaf mutex inside DriveWidgetController. Change no callers.

1. CassoEmuCore/Ui/DriveWidgetController.h, private section. Add the mutex first, with the existing column alignment:
    std::mutex                   m_syncMutex;
    std::vector<DriveSyncEvent>  m_syncEvents;
    uint64_t                     m_nextSyncEventId = 1;
Put a comment above it. It should say the queue is published from the CPU thread (OnBayChange) and from the UI thread (the salvage Insert), is drained by the UI frame, and that the mutex guards both the vector and the id counter. <mutex> is already in CassoEmuCore/Pch.h:49.

2. DriveWidgetController.cpp PublishSyncEvent (:126-143). Fill driveId, action and timestampMs outside the lock. Then take the id and push in one locked scope, so ids enter the vector in increasing order whichever thread publishes:
    {
        std::lock_guard<std::mutex>  lock (m_syncMutex);

        evt.eventId = m_nextSyncEventId++;
        m_syncEvents.push_back (evt);
    }

    return evt.eventId;

3. ConsumeSyncEvents (:155-164). Do the swap in a locked scope:
    {
        std::lock_guard<std::mutex>  lock (m_syncMutex);

        out.swap (m_syncEvents);
    }

    return out;

Also update the class banner at DriveWidgetController.h:12-23 with one line on its threading.

The lock is a leaf. Neither method calls out while holding it. The UI frame takes it while it holds the shared lifetime lock (EmulatorShellPresent.cpp:564), and no CPU-thread producer takes the lifetime lock while it holds m_syncMutex, so the two locks cannot deadlock. Contention is a few events per user action, against one swap per frame.

**Regression test:** Extend UnitTest/UiTests/AnimationSyncTests.cpp with a new test. The pattern to follow is UnitTest/Devices/Disk2EventRingTests.cpp TwoThreadStressNoTornReadsNoReorder.

TEST_METHOD (DriveSyncBroker_ConcurrentPublishAndConsume_LosesNothingAndKeepsOrder):
- Start two std::thread producers, standing in for the CPU thread and the UI-thread salvage mount. Each calls controller.PublishSyncEvent (drive 0 or 1, SyncAction::DoorReinsert, i) 100,000 times. A std::atomic<int> counts the producers still running.
- Meanwhile the test thread loops on ConsumeSyncEvents() until both producers have finished, then makes one final ConsumeSyncEvents() call.
- For each batch, append the events to a running list and check that every eventId is greater than the previous one. Ids are assigned under the lock, so they arrive in strictly increasing order across batches.
- After joining both threads, assert:
  - the total consumed is exactly 200,000;
  - every id from 1 through 200,000 appears exactly once (a std::vector<bool> seen of size 200,001, checked as each event is appended);
  - per drive, the counts are 100,000 each.
- Put all Assert calls on the test thread, never inside a producer lambda, and keep the code flat, per the project's test-code rules.

Before the fix, unsynchronized push_back racing swap loses or duplicates events, or crashes on a reallocated buffer (the Debug CRT heap checks fire). Ids can also arrive out of order, because the id increment and the push are separate unguarded steps. At this iteration count that is all but certain on any multi-core machine. After the fix, the test passes deterministically.

The existing DriveSyncBroker_PublishAndConsumeWithinFrame (AnimationSyncTests.cpp:94-118) must keep passing unchanged. No DiskManager-level scenario test is proposed, because UpdateDriveWidgets also reads DiskImageStore::GetSourcePath without a lock (DiskManager.cpp:803), which is a separate race that would make such a test flaky for the wrong reason.

**Sites:** CassoEmuCore/Ui/DriveWidgetController.h:61, CassoEmuCore/Ui/DriveWidgetController.h:62, CassoEmuCore/Ui/DriveWidgetController.cpp:135, CassoEmuCore/Ui/DriveWidgetController.cpp:140, CassoEmuCore/Ui/DriveWidgetController.cpp:161, CassoEmuCore/Shell/DiskManager.cpp:604, CassoEmuCore/Shell/DiskManager.cpp:613, CassoEmuCore/Shell/DiskManager.cpp:624, CassoEmuCore/Shell/DiskManager.cpp:795, CassoEmuCore/Shell/DiskManager.cpp:976, CassoEmuCore/Shell/EmulatorShellPresent.cpp:564, CassoEmuCore/Shell/EmulatorShellPresent.cpp:637, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:635, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:700, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2326, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3537, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3880, CassoEmuCore/Shell/CpuManager.cpp:583, CassoEmuCore/Shell/EmulatorShellCpuThread.cpp:419, CassoEmuCore/Shell/EmulatorShellCpuThread.cpp:434, CassoEmuCore/Shell/EmulatorShellCpuThread.cpp:449, CassoEmuCore/Shell/EmulatorShellCpuThread.cpp:481, CassoEmuCore/Shell/MachineBuilder.cpp:1527, CassoEmuCore/Shell/MachineBuilder.cpp:1544, CassoEmuCore/Shell/MachineManager.cpp:606, CassoEmuCore/Shell/EmulatorShellState.cpp:185, CassoEmuCore/Shell/MachineStateFile.cpp:562, CassoEmuCore/Shell/EmulatorShellDisks.cpp:674

## flux-burst-spliced-into-reloaded-image [still-present]

**Evidence:** Re-verified in the worktree at 811a6f727. Every link in the chain is still there, at new line numbers.

1. The flush path commits before it tests dirtiness: FlushEntry calls entry.image->CommitPendingWrite() at CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1093-1095, then BAIL_OUT_IF(!IsDirty()) at :1097.

2. The reload decision skips that commit. ApplyPendingReloadToBay (:2811) sets situation.guestDirty = entry.image->IsDirty() at :2914 with no CommitPendingWrite anywhere in the function. ExternalChangePolicy::Decide (ExternalChangePolicy.cpp:47-58) returns Conflict only when guestDirty is true. With an open burst and no other writes, the result is ReloadInPlace, Restart or Ask instead of Conflict.

3. The burst can be open when this runs. The idle callback fires every kIdleCallbackCycles = 17030 (Disk2Controller.h:144). "Quiet" means only that no spin-up happened in the window (Disk2Controller.cpp:625-646). Write mode is not checked. MachineBuilder.cpp:1542-1545 installs ApplyPendingReload as that callback. A burst opens in RecordFluxWriteBit (Disk2NibbleEngine.cpp:408-417, sets m_burstActive and SetPendingWriteOwner(this)). It closes only when write mode ends, the head steps, the motor stops or the drive resets (:105, :174, :83, :500, :683-686).

4. The swap keeps the object and drops the owner. MountExternallyModifiedDisk does *entry.image = std::move(*loaded) (DiskImageStore.cpp:3525). DiskImage declares no copy or move members (DiskImage.h:99-320), so the implicit memberwise move copies the fresh image's m_pendingWriteOwner (nullptr, DiskImage.h:310) over the old one. The engine still has m_burstActive = true, and its m_disk still points at the same address.

5. The stale burst lands in the new contents. EmitBayChange(Swapped) (:3537) calls the sink DiskManager::OnBayChange (DiskManager.cpp:976-979, :534). That calls controller->SetExternalDisk (:562-565), then Disk2Controller::SetExternalDisk (Disk2Controller.cpp:806-812), then Disk2NibbleEngine::SetDiskImage, then CommitPendingWrite (Disk2NibbleEngine.cpp:53-55). CommitPendingWrite (:432-445) splices m_burstBits, recorded against the old contents, into the freshly loaded flux track at :444. DiskImage::SpliceFluxWrite/SpliceFluxBulk (DiskImage.cpp:237-271) then call MarkTrackDirty (:269, :621-626). The reloaded image is now dirty with the guest's partial sector in it. No preserved copy was made. The next flush writes the mix over the other program's file, because the identity was just refreshed at :3529.

The motor-off path is still safe. Disk2Controller.cpp:543-544 calls SetMotorOn(false), which commits (Disk2NibbleEngine.cpp:81-84), before the callback at :567-569 runs FlushAllUnlessHeld and ApplyPendingReload (MachineBuilder.cpp:1519-1528).

Two more routes reach the same splice on 035:
(a) A question's answer. ResolvePendingChange runs on the CPU thread from a posted command (EmulatorShellCpuThread.cpp:479-481, CpuCommandDispatcher.cpp:317), at any instruction boundary, including mid-write. Its path is CarryOutChangeAction ReloadInPlace/Restart (DiskImageStore.cpp:3308-3310) then MountExternallyModifiedDisk. The guest keeps running while a question is up (SharedImageTests.cpp:1451-1456 relies on that).
(b) The new 035 DiscardHeldWrites (DiskImageStore.cpp:1989-2025). It tests IsDirty uncommitted at :2005 and then calls MountExternallyModifiedDisk at :2014. Nothing in production calls it yet; only DiskFlushHoldTests.cpp:133 does.

**035 impact:** 035 does not change the primary fix, but it changes where the commit should go and adds two more call sites.

1. The new commit must not run during a replay. ApplyPendingReload already returns under m_isReplaying (DiskImageStore.cpp:2747-2750), and FlushEntry bails there too (:1090). Putting the commit inside ApplyPendingReloadToBay keeps it on the live run only.

2. Place the commit after the early returns (ask outstanding :2862, file held by another process :2871, identity unchanged :2879-2886). Then the 60 Hz common path never ends a burst early. Committing splits one burst into two splices, and FluxTrack::SpliceWrite (FluxTrack.cpp:362-416) quantizes each start tick. A commit taken at a host-timed moment is a small machine-state change the journal does not record. Confining it to a real pick-up keeps it next to the swap, which 035 already treats as a history boundary.

3. 035's history makes the backstop more important. MountExternallyModifiedDisk calls NotifyMediaChanged (:3539), which goes through MachineHost::OnMediaChanged (MachineHost.cpp:60-64) to ReverseController::OnMediaChanged (ReverseController.cpp:283-295). That takes a boundary keyframe of the disks "as they now stand", after EmitBayChange (:3537) has already spliced. Today the keyframe therefore records the corrupted reloaded disk, so rewinding cannot recover the clean one. The burst must be closed before the move assignment, so neither the swap nor the keyframe holds it.

4. The flush hold does not change the fix. FlushAllUnlessHeld skips under the hold (:1899), but ApplyPendingReload still runs, and the Conflict branch writes only a preserved copy to a new file (SaveLoadedImage :3812-3840), never the bay's own file. That is already true today for committed writes. The fix only makes an open burst count the same way, so the hold is respected unchanged.

5. 035 adds DiscardHeldWrites as a third way into the swap. It needs the same commit before its dirtiness test, or an open burst survives a discard (clean bay skipped, burst committed later) or gets spliced into the restored contents (dirty bay).

The pause/step rework (StepInstructionWhilePaused removed) does not touch this path. The idle and motor-off callbacks still fire from Disk2Controller::Tick on the CPU thread.

**Proposed fix:** Three edits in CassoEmuCore/Devices/Disk/DiskImageStore.cpp. There are no header changes.

(1) Primary, in ApplyPendingReloadToBay: insert immediately before line 2914 (situation.guestDirty = entry.image->IsDirty();), after the trial load:

    //  A write still open on a flux track is a guest write that has not
    //  reached the image yet. It is committed here, as FlushEntry does, so it
    //  counts toward the conflict and goes into the preserved copy rather than
    //  reading as clean and riding the swap onto the reloaded disk.
    entry.image->CommitPendingWrite();

With this, an open burst sends the decision to Conflict. The burst is in the image when SaveLoadedImage serializes it (:2933), so the preserved copy holds it. ClearDirty (:2977) and the re-decision follow as for any other conflict.

(2) Backstop, in MountExternallyModifiedDisk: insert after CBR (usable); (:3518) and before *entry.image = std::move (*loaded); (:3525):

    //  A write the drive still holds was recorded against the outgoing
    //  contents, and the move below drops the image's record of who holds it
    //  while the drive keeps the bits. Finished here, it lands in the disk that
    //  is leaving instead of in the one arriving.
    entry.image->CommitPendingWrite();

After the move the engine's m_burstActive is false, so SetDiskImage's commit during EmitBayChange (:3537) does nothing. The NotifyMediaChanged keyframe (:3539) then records the reloaded disk exactly as the file has it. This covers the ResolvePendingChange route (an answer arriving mid-write) and DiscardHeldWrites. On both, the outgoing contents are either already preserved or deliberately discarded.

(3) In DiscardHeldWrites (:2005), split the guard so the commit runs before the dirtiness test:

    if (!entry.mounted || entry.image == nullptr || entry.path.empty())
    {
        continue;
    }

    entry.image->CommitPendingWrite();

    if (!entry.image->IsDirty())
    {
        continue;
    }

Optional, not required for this defect:
- CountUnsavedDisks (:1937-1955, const) also misses an open burst. A const DiskImage::HasPendingWrite() { return m_pendingWriteOwner != nullptr; } would let it count one.
- Disk2Controller::PumpIdleCallback (:629-632) treats only spin-up as busy. Also marking the window busy while Q7 write mode is on would stop pick-ups mid-sector on bit tracks as well. That is a separate weakness of the "quiet" premise, not this splice.

**Regression test:** Extend UnitTest/EmuTests/SharedImageTests.cpp. Its Rig already redirects reads, writes and identities into memory (:78-274). Add #include "Machines/Apple2/Common/Disk2NibbleEngine.h" and #include "FluxTestImages.h".

Helpers:
- MakeFluxWoz(Byte fill): WozLoader::BuildSyntheticV21 with a bit track at quarter track 0 and a flux track at quarter track 4 (track.isFlux = true, track.data = FluxTestImages::BitsToNominalFlux(bits filled with `fill`, 51200)). This is the pattern in WozFluxLoaderTests.cpp:28-48 and :308.
- A local WriteSome(eng, 400) copied from Disk2NibbleEngineFluxTests.cpp:374-382.
- In each test, replace the rig's bay-change sink with one that pushes the change and also calls eng.SetDiskImage(rig.store.GetImage(kSlot, kDrive)). This mirrors DiskManager::OnBayChange -> Disk2Controller::SetExternalDisk (DiskManager.cpp:562-565, Disk2Controller.cpp:806-812).

Test A, AFluxWriteOpenWhenAChangeSettlesIsKeptAndNotSplicedIntoTheNewDisk (pins fix 1):
1. Put rig.files["C:\\work\\Loader.woz"] = MakeFluxWoz(0xFF), Stamp it, and Mount it.
2. Bind the engine to the image, SetCurrentTrack(4), SetMotorOn(true), then WriteSome.
3. Assert the image is not dirty yet (the burst is open).
4. Replace the file with MakeFluxWoz(0xAA), Stamp it, and call rig.FireAndSettle(path, ExternalChangeIntent::ReloadInPlace).
5. Assert:
   - rig.PreservedPaths().size() == 1, and that path differs from the .woz path. Before the fix this is 0: the policy read the bay as clean and reloaded without keeping anything.
   - The preserved copy, loaded into a fresh DiskImage, has flux track 1 bytes that differ from the original's (it holds the guest's write).
   - rig.store.GetImage(...)->IsDirty() is false. Before the fix it is true, because the splice marked track 1 dirty.
   - The image's GetFluxTrack(1).GetBytes() equals a fresh DiskImage loaded from the 0xAA bytes. Before the fix the stale burst is mixed in.

Test B, AnAnswerArrivingMidWriteLeavesTheReloadedDiskAsTheFileHasIt (pins fix 2):
1. Mount the 0xFF WOZ and bind the engine as in Test A.
2. Replace the file with the 0xAA WOZ and call FireAndSettle with no intent. Assert questions.size() == 1 (the bay was clean, so this is a question, not a conflict).
3. Call WriteSome to open a burst while the question stands.
4. Call rig.store.ResolvePendingChange(kSlot, kDrive, ChangeAction::ReloadInPlace).
5. Assert the image is not dirty and its flux track 1 equals the 0xAA file's. Both fail before fix 2, because the swap's SetDiskImage splices the burst in.

Test A still fails with only fix 2 applied (no preserved copy), so each test pins its own edit. If either rig plus its DiskImages comes near the C6262 frame limit noted at SharedImageTests.cpp:1492-1496, allocate the Rig with make_unique.

Optional Test C in UnitTest/EmuTests/DiskFlushHoldTests.cpp, DiscardTakesAnOpenFluxWriteWithIt (pins fix 3):
1. Mount a clean flux WOZ through the store's seams, as PrepareDirtyDisk does.
2. Open a burst with an engine bound to the image, then call SetFlushHold(true) and DiscardHeldWrites().
3. Assert the engine's later SetWriteMode(false) leaves the image clean, and Serialize equals the file's bytes.

**Sites:** CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2914, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3525, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3537, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3539, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2005, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3310, CassoEmuCore/Devices/Disk/DiskImage.h:310, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:55, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:444, CassoEmuCore/Shell/DiskManager.cpp:564, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:636

## debug-sink-attach-race [still-present]

**Evidence:** 035 left the race in place. Only line numbers in MachineManager and MachineHost moved.

UI side: WindowCommandManager.cpp:1032-1034 runs OpenDisk2DebugDialog from WM_COMMAND on the UI thread. It takes only a shared try-lock (EmulatorShellDebug.cpp:515, CBR at :525). It assigns m_disk2DebugPanel = make_unique at :544, before Create, which pumps messages (:546), and resets it on failure (:551). It then writes Disk2DebugPanel::m_cycleCounter (:560, Disk2DebugPanel.h:58), Disk2Controller::m_eventSink and the address-mark watcher's sink (:563 -> Disk2Controller.cpp:1023-1027, Disk2AddressMarkWatcher.h:64), and each Disk2AudioSource::m_audioEventSink (:565-571 -> Disk2AudioSource.h:120-123).

CPU side: SwitchMachine runs from CpuCommandDispatcher.cpp:53-54 (IDM_FILE_OPEN; LoadMachineState also reaches it, EmulatorShellState.cpp:176). Its only lifetime-lock section is the exclusive scope at MachineManager.cpp:489-527. The slice loop takes no lock: every other GetLifetimeLock() caller is a UI-thread shared lock. Outside that scope, SwitchMachine reads m_shell.m_disk2DebugPanel and writes the panel's cycle counter at :424-427 (before the lock) and :547-550 (after it). It then calls AttachDebugSinksIfOpen at :560, which reads both panel unique_ptrs unlocked (EmulatorShellDebug.cpp:696) and writes the same sinks through MachineHost::AttachObservers (MachineHost.cpp:1884-1907, controller at :1888) and the audio loop (EmulatorShellDebug.cpp:701-707). ResetUptimeAnchor also reads the panel on the CPU thread (EmulatorShell.h:297-314), called from MachineManager.cpp:652 and :670, including the PowerCycle at :587 inside the switch. The CPU thread reads m_eventSink on every Disk II switch, phase, tick and mount (Disk2Controller.cpp:177-253, :463-473, :557, :734, :766, :855, :878), and PublishToRing dereferences m_cycleCounter (Disk2DebugPanel.cpp:1501-1503). The comment at Disk2Controller.cpp:1018-1019 still says the attach is "Safe to call from the UI thread between CPU slices", and nothing enforces that.

Concrete failure: the user opens Disk II Debug after a switch has passed :527. The UI assigns the new panel at :544 and enters Create. The CPU thread at :547/:560 reads the non-null pointer and points the new controller and the new audio sources at it. Create then fails, and CHRF at :551 frees the panel while those sinks still point at it. The PowerCycle at :587 (eject -> OnDiskEjected) or the next $C0Ex access then calls into freed memory. Even when Create succeeds, the unique_ptr itself is read and written on two threads with no ordering. In the opposite order (the UI opens while the switch is between :424 and :489), the panel keeps the old CPU's cycle-counter pointer across SetCpu(nullptr) at :501 until :549 replaces it.

Sibling with the same mechanism: OpenInputDebugDialog (EmulatorShellDebug.cpp:613-670) writes the keyboard, //e soft-switch and game-port sinks and the cycle counter (:648-662) on the UI thread with no lifetime lock at all. Meanwhile SwitchMachine nulls those sinks at MachineManager.cpp:434-450 outside the lock and re-attaches them at :560.

Latent: the `GetHwnd() == nullptr` re-create test at :541 would free a panel that is still attached, through the :544 assignment. Today the close box only hides the window (DxuiWindow.h:331, and Disk2DebugPanel does not override OnWindowClose), so that branch is reached only after a failed Create.

**035 impact:** 035 did not touch this race. The flush hold (MachineManager.cpp:403-408), StopReverseRecording/StartReverseRecording (:401, :610) and the removal of StepInstructionWhilePaused have no effect on the sinks. What 035 does change is the route for the fix, which is posting a CPU-thread command:

(1) 035 added the AllowCommand gate (EmulatorShellReverse.cpp:303-333, installed at EmulatorWindow.cpp:1344). The new command id must stay out of CpuCommandDispatcher::TryGetJournalInput (CpuCommandDispatcher.cpp:216-277) and DivergenceGate::IsStateChangingCommand (DivergenceGate.cpp:25-43). Otherwise, opening the panel while the machine is behind live would ask to discard history (and a No would drop the command), and a journaled attach would replay.

(2) The queue is serviced while the machine is paused: DrainCommandQueue runs before the pause check (CpuManager.cpp:579-583). So an attach posted while the debugger has the machine stopped still lands. Commands drain once per loop pass ahead of the frame, so an attach posted during a switch runs after SwitchMachine returns and before the new machine's first slice.

(3) ICpuCommandTarget now has default-bodied virtuals (CpuCommandDispatcher.h:78-111), so the new method can follow that pattern, and the test Notebook needs one override.

(4) In 035, AttachObservers (MachineHost.cpp:1884-1907) covers the input panel as well as the disk panel. The fix must therefore also move OpenInputDebugDialog's sink writes to the CPU thread, or the same race remains for keyboard, //e soft-switch and game-port sinks.

The fix direction's other option, writing the sinks under the exclusive lifetime lock, is weaker. The slice loop never takes that lock, so it would only order the writes against SwitchMachine, not against the per-access sink reads, and it would block the UI thread for the length of a rebuild.

**Proposed fix:** Make the CPU thread the only writer of device sinks and panel cycle counters.

1. CassoEmuCore/resource.h: add IDM_DEBUG_ATTACH_SINKS beside IDM_DEBUG_OPEN (40151) and IDM_DEBUG_PAUSE_CHANGED (40156). Its comment says it is a CPU-thread command that points the open debug panels at the machine's devices.

2. CpuCommandDispatcher.h: add `virtual void AttachDebugSinks () {}` to ICpuCommandTarget. In CpuCommandDispatcher.cpp Dispatch, add `case IDM_DEBUG_ATTACH_SINKS: target.AttachDebugSinks(); break;`. Do not add the id to TryGetJournalInput or IsStateChangingCommand.

3. EmulatorShell.h: add CPU-thread-only copies, `Disk2DebugPanel * m_cpuDisk2Panel = nullptr;` and `InputDebugPanel * m_cpuInputPanel = nullptr;`. Implement the AttachDebugSinks override with the other overrides in EmulatorShellCpuThread.cpp. It copies m_disk2DebugPanel.get() and m_inputDebugPanel.get() into the copies, then calls AttachDebugSinksIfOpen. Reading the unique_ptrs there is ordered after the UI's write because PostCommand and DrainCommandQueue take the same m_cmdMutex (CpuManager.cpp:144-160, :329-338).

4. AttachDebugSinksIfOpen (EmulatorShellDebug.cpp:694-708) reads only the copies and also sets each panel's cycle counter from m_machine.GetCpu(). Move that work there from MachineManager.cpp:547-555.

5. MachineManager::SwitchMachine: the revokes at :424-432 use the copies, and :547-555 fold into the AttachDebugSinksIfOpen call at :560. ResetUptimeAnchor (EmulatorShell.h:301-313) uses the copies. After this, nothing on the CPU thread reads the UI-owned unique_ptrs except inside the posted command.

6. OpenDisk2DebugDialog: build the panel in a local std::unique_ptr and call Create on it. On failure only the local is destroyed; nothing is published and nothing is attached. On success, apply the icon, uptime anchor and multi-controller hint, move the panel into m_disk2DebugPanel, then call m_cpuManager.PostCommand (IDM_DEBUG_ATTACH_SINKS). Delete :558-571 (the cycle counter, controller sink and audio-sink writes). Create a panel only when m_disk2DebugPanel == nullptr, and never replace a published panel, because it stays a live sink until the CPU thread detaches it: drop the `|| GetHwnd() == nullptr` test at :541, or assert it. The shared try-lock is still needed for the FindSlot6Controller gate and the slot count, but it no longer needs to span Create.

7. OpenInputDebugDialog (:613-670): same pattern, with a local unique_ptr, publish, then post the same command. Delete the UI-thread writes at :646-662.

8. ~EmulatorShell detaches at EmulatorShell.cpp:238-262, after m_cpuManager.Stop() at :236, so it can stay as it is.

9. Correct the comments at Disk2Controller.cpp:1018-1019 (say "CPU thread only; the shell attaches through IDM_DEBUG_ATTACH_SINKS"), EmulatorShellDebug.cpp:500-507 and :512-514, EmulatorShell.h:275-291, and AttachDebugSinksIfOpen's banner.

Optional: :555 and :639 read m_uptimeAnchor, which the CPU thread writes (EmulatorShell.h:299). The CPU-thread handler can stage it with the atomic RequestResetAnchor (Disk2DebugPanel.h:63-67) instead; on a fresh panel, the clear it requests has nothing to clear.

**Regression test:** Extend UnitTest/EmuTests/CpuCommandDispatcherTests.cpp. Give the Notebook (around :447) `void AttachDebugSinks () override { calls.push_back ("AttachDebugSinks"); }`. Add TEST_METHOD (AttachingTheDebugSinksIsTheCpuThreadsJob), which calls `Dispatch (IDM_DEBUG_ATTACH_SINKS, "", target)` and asserts `calls.size() == 1` and `calls[0] == "AttachDebugSinks"`. It also builds an EmulatorCommand with that id and asserts `CpuCommandDispatcher::TryGetJournalInput (cmd, input)` is false. Before the fix, Dispatch's default case drops the id and calls is empty.

Extend UnitTest/EmuTests/DivergenceGateTests.cpp: add IDM_DEBUG_ATTACH_SINKS to the id list in DebuggerAudioAndHistoryCommandsLeaveTheMachineAlone (:70-76). This pins that opening a debug panel behind live neither raises the divergence question nor gets dropped by the 035 gate.

Limit: the cross-thread interleaving cannot be reproduced deterministically in a unit test. OpenDisk2DebugDialog needs an HWND and a D3D device, which a test-built shell lacks (EmulatorShellResetTests.cpp:23-31, EmulatorDebugWiringTests.cpp:157-164), and MSVC has no thread sanitizer.

Manual confirmation in an ASan Debug build: temporarily force Disk2DebugPanel::Create to return E_FAIL after DxuiWindow::Create. Then open Disk II Debug repeatedly while switching machines with a disk spinning. Before the fix, ASan reports a heap-use-after-free at a Disk2Controller sink call (for example OnDiskEjected from the PowerCycle at MachineManager.cpp:587). After the fix, a failed Create attaches nothing.

**Sites:** CassoEmuCore/Shell/EmulatorShellDebug.cpp:515, CassoEmuCore/Shell/EmulatorShellDebug.cpp:541, CassoEmuCore/Shell/EmulatorShellDebug.cpp:544, CassoEmuCore/Shell/EmulatorShellDebug.cpp:551, CassoEmuCore/Shell/EmulatorShellDebug.cpp:560, CassoEmuCore/Shell/EmulatorShellDebug.cpp:563, CassoEmuCore/Shell/EmulatorShellDebug.cpp:569, CassoEmuCore/Shell/EmulatorShellDebug.cpp:648, CassoEmuCore/Shell/EmulatorShellDebug.cpp:651, CassoEmuCore/Shell/EmulatorShellDebug.cpp:696, CassoEmuCore/Shell/EmulatorShellDebug.cpp:705, CassoEmuCore/Shell/MachineManager.cpp:424, CassoEmuCore/Shell/MachineManager.cpp:434, CassoEmuCore/Shell/MachineManager.cpp:489, CassoEmuCore/Shell/MachineManager.cpp:547, CassoEmuCore/Shell/MachineManager.cpp:560, CassoEmuCore/Shell/MachineManager.cpp:652, CassoEmuCore/Shell/MachineManager.cpp:670, CassoEmuCore/Shell/EmulatorShell.h:301, CassoEmuCore/Shell/MachineHost.cpp:1888, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:1018, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:1023, CassoEmuCore/Machines/Apple2/Common/Disk2AddressMarkWatcher.h:64, CassoEmuCore/Machines/Apple2/Common/Disk2AudioSource.h:122, CassoEmuCore/Ui/Disk2DebugPanel.h:58, CassoEmuCore/Ui/Disk2DebugPanel.cpp:1501

## layout-reads-machine-refs-unlocked [changed-but-present]

**Evidence:** The writer is unchanged in 035. SwitchMachine still runs on the CPU thread. It is reached from CpuCommandDispatcher.cpp:54 and, new in 035, from LoadMachineState at EmulatorShellState.cpp:176. Under the exclusive lifetime lock (MachineManager.cpp:489-527) it clears the owned devices (:505), resets the refs (:508), rewrites the machine name (:512) and assigns the whole MachineConfig in place (:513). It then rebuilds, and MachineBuilder.cpp:615 nulls diskController and :622 sets it again.

The shared side is still taken only by TryPresentUiFrame (EmulatorShellPresent.cpp:564), some input handlers (EmulatorWindowInput.cpp:1797/1853/2099/2342/3017/4279), HandleSwitchBarClick (EmulatorShellChrome.cpp:1478), the debug panel (EmulatorShellDebug.cpp:515) and the reverse paths (EmulatorShellReverse.cpp:721/755/796/832). None of these covers the layout paths.

Unlocked UI-thread readers in the worktree:
- **OnSize:** EmulatorWindow.cpp:1934, :1956 (HasSlot6Controller -> DiskManager.h:67 -> DiskManager.cpp:298), :1935/:1996 (MachineHasCaseSwitches), :1968/:1974/:2013 (ShouldShowExternalDrive).
- **SyncChromeBands:** EmulatorShellChrome.cpp:933/:935. It is reached from ComputeViewportRect :1028 and from GetClientSizeForCenterPx EmulatorWindow.cpp:915, which OnGetMinMax :1796 calls.
- **ReflowChromeForMachineChange:** EmulatorShellChrome.cpp:1094/:1095/:1103.
- **ShouldShowExternalDrive:** EmulatorShellChrome.cpp:1190/:1199. In 035 it iterates `slots` and each slot's nested `ports` vector and compares strings (MachineConfig.h:336-362).
- **DeskSceneDriveCount:** EmulatorShellScene.cpp:745/:757/:764. It is reached from UpdateViewportLayout EmulatorShellChrome.cpp:808/:864/:866, which InvalidateSceneComposition EmulatorShellScene.cpp:561 calls; that runs from the mouse orbit at EmulatorWindowInput.cpp:335 and the tilt at :348.
- **Machine-id lookups:** MachineHasCaseSwitches EmulatorShell.cpp:1262 and MachineHasBuiltInDrive :1284 look up GetConfig().machineId. They are called from mouse handlers EmulatorWindowInput.cpp:451/1227/1545 and from SettingsSheet.cpp:474/1065/1127.
- **Machine name:** UpdateWindowTitle EmulatorWindow.cpp:2202-2204.
- **Worse than the master report:** OnSize EmulatorWindow.cpp:2001 calls LayoutSwitchBar, which calls SyncSwitchBarState (EmulatorShellChrome.cpp:1414 -> :1436). That dereferences `GetRefs().iieKeyboard` at :1443-1444 with no lock. If it reads the pointer between MachineManager.cpp:505 and :508, it calls into a freed Apple2eKeyboard.
- **New in 035:** OnCopyData DescribeMachine (EmulatorWindow.cpp:2773-2774) reads AttachedDiskIiDriveCount and the name for a WM_COPYDATA that another process can send at any moment.
- **A UI-thread writer:** OnExternalDriveCommand writes the running config with SetDiskIiPortAttached (WindowCommandManager.cpp:579) and m_externalDriveConnected (:571) with no lock. That is a write-write race with :513's assignment.
- **Bools written outside the lock:** SwitchMachine also writes the plain bools m_externalDriveConnected and m_mouseConnected (MachineManager.cpp:327/:338) before it takes the lock.

The diskController reads only compare against null, so they give a stale answer. The real crash risk is the MachineConfig strings and vectors. Iterating a `slots` vector while it is reassigned (for example, a //e -> //c switch that empties it) reads destroyed SlotConfig and PortConfig objects, and the long `name` strings can be freed mid-read.

The IDM_STORAGE_DRIVE2 label site no longer exists. The label query at EmulatorWindow.cpp:763-800 reads only the disk store.

**035 impact:** 035 does not touch the mechanism, but it changes the sites the fix must cover.

1. The IDM_STORAGE_DRIVE2 label site is gone, so it drops out of the fix.
2. LoadMachineState (EmulatorShellState.cpp:176) is a second CPU-thread route into SwitchMachine. The refresh point must therefore hang off SwitchMachine's own completion post (MachineManager.cpp:566 -> EmulatorWindow.cpp:2184 -> handler :2685), not off the IDM_FILE_OPEN dispatch.
3. ShouldShowExternalDrive and DeskSceneDriveCount now walk the nested slots[].ports vectors and compare strings (MachineConfig.h:336). That widens the unlocked read surface from a pointer compare to heap-backed containers, which raises the stakes.
4. OnCopyData DescribeMachine (EmulatorWindow.cpp:2771-2778) is a new unlocked reader with the same exposure.
5. OnExternalDriveCommand writes the running config from the UI thread (WindowCommandManager.cpp:579), so a reader-only snapshot is not enough. That writer also has to be serialized with the switch.
6. TryPresentUiFrame still holds the shared lock with try_to_lock (EmulatorShellPresent.cpp:564). The frame's own reads (:803-804, :895, :910, :977) are safe. Moving them to the snapshot keeps the frame and the layout agreeing, and changes nothing else.
7. The removal of StepInstructionWhilePaused, the flush hold and the reverse-execution lock sites do not affect this fix. None of them puts the layout paths under the lock.

**Proposed fix:** Give the UI thread its own copy of the per-machine layout facts, captured only under the shared lifetime lock.

**1. New pure type `CassoEmuCore/Shell/MachineChromeFacts.h/.cpp`**
- Fields: `hasDiskController`, `hasCaseSwitches`, `hasBuiltInDrive`, `hasBankedSystemRom` (`systemRom.romBankSize != 0`), `isSlotless` (`slots.empty()`), `attachedDiskIiDrives` (`AttachedDiskIiDriveCount()`), `externalDriveConnected`, and `displayName` (`config.name`).
- `static MachineChromeFacts Capture (const MachineConfig &, const MachineRefs &, bool externalDriveConnected)`: pure.
- `static bool TryCapture (MachineHost & host, const bool & externalDriveConnected, MachineChromeFacts & facts)`: takes `std::shared_lock (host.GetLifetimeLock(), std::try_to_lock)`. When it does not get the lock it returns false and leaves `facts` as it was.
- `bool ShouldShowExternalDrive() const`: the body moved from EmulatorShellChrome.cpp:1190-1199.
- `int GetDeskSceneDriveCount() const`: the body moved from EmulatorShellScene.cpp:745-764.

**2. EmulatorShell**
- Add `MachineChromeFacts m_chromeFacts` (UI thread only, in the protected section beside m_swallowMetaChar so a TestShell can reach it).
- Add `void RefreshMachineChromeFacts()`: DXUI_ASSERT_UI_THREAD, then TryCapture into m_chromeFacts.
- Call it at three points:
  - in Initialize right after EmulatorShell.cpp:395, and again after :424-425 (both on the UI thread before the CPU thread starts at EmulatorWindow.cpp:1346);
  - as the first statement of the WM_APP_DXUI_UPDATE_TITLE handler (EmulatorWindow.cpp:2687, ahead of UpdateWindowTitle);
  - in OnExternalDriveCommand ahead of WindowCommandManager.cpp:599.

  If a second switch already holds the machine, the refresh keeps the old facts, and that switch's own post at MachineManager.cpp:566 refreshes them.

**3. Point every UI reader at m_chromeFacts**
- `m_chromeFacts.hasDiskController` at EmulatorWindow.cpp:1934/:1956, EmulatorShellChrome.cpp:933/:1094, EmulatorShell.cpp:1079 and EmulatorShellPresent.cpp:804.
- `ShouldShowExternalDrive()` returns `m_chromeFacts.ShouldShowExternalDrive()`.
- `DeskSceneDriveCount()` returns `m_chromeFacts.GetDeskSceneDriveCount()`.
- `MachineHasCaseSwitches()` and `MachineHasBuiltInDrive()` return the cached bools (they have no CPU-thread callers).
- UpdateWindowTitle (EmulatorWindow.cpp:2202-2204) and OnCopyData DescribeMachine (:2773-2774) use `displayName` and `attachedDiskIiDrives`.
- DiskManager::HasSlot6Controller stays as it is for the CPU-thread caller (MachineManager.cpp:600) and for callers that hold the lock.

**4. LayoutSwitchBar**
At EmulatorShellChrome.cpp:1414, stop calling SyncSwitchBarState from layout. The frame already syncs it under the lock every UI frame (EmulatorShellPresent.cpp:977-979). If the first paint after a resize must show the latch state, take a shared try_to_lock there and sync only when it holds the lock. This removes the unlocked iieKeyboard dereference reachable from OnSize.

**5. SwitchMachine**
Keep `connected` and `mouseConn` in locals. Move the stores at MachineManager.cpp:327 and :338 inside the exclusive section next to :512-513, so the capture reads them in a consistent state.

**6. OnExternalDriveCommand**
At WindowCommandManager.cpp:569-582, do the flag write and the SetDiskIiPortAttached call under a shared try_to_lock. When the lock is not available, re-post the same WM_COMMAND and return. Release the lock, then eject, refresh the facts and reflow.

**Why a snapshot rather than try_to_lock in each layout entry**
- OnSize and OnGetMinMax must answer every call and cannot be skipped.
- The mouse-orbit path would need a "layout owed" flag.
- Today a WM_SIZE that lands mid-switch records the new machine's band state into m_chromeSizedForHasDisk and m_chromeSizedForApple2c. That defeats ReflowChromeForMachineChange's window-delta resize; the snapshot fixes it too.

**Regression test:** Extend UnitTest/UiTests/ChromeBandLayoutTests.cpp, which already tests "the chrome bands against synthetic machine state". Include "EmuTests/TestMachine.h" and "Shell/MachineChromeFacts.h".

**1. ChromeFactsTakenDuringARebuildKeepTheMachineTheyHad**
Use the thread pattern of MachineGamePortSinkTests.cpp:227 (RebuildHoldingTheMachine_RefusesTheWrite).
- Build `TestMachine machine ("Apple2e")` and `TestMachine next ("Apple2c", TestMachine::Slots::Empty)`.
- TryCapture the //e and assert hasDiskController, !hasCaseSwitches and GetDeskSceneDriveCount() == 2.
- Start a rebuild thread that takes `std::unique_lock` on machine.GetLifetimeLock(), then does what MachineManager.cpp:508/:513 do: `machine.GetConfig() = next.GetConfig(); machine.GetRefs().diskController = nullptr;`. It sets isLocked and spins until isRelease.
- On the test thread, TryCapture again. Assert it returned false and that the facts still report the //e: !hasCaseSwitches, hasDiskController, two drives.
- Release and join, then TryCapture once more. Assert it returned true and reports hasCaseSwitches and !hasDiskController (the post-switch refresh).

**2. Rule-parity tests for the logic moved out of the shell**
Reuse the MachineConfigTests.cpp:641-709 fixtures:
- AnExternalDriveOnTheIIcFollowsTheConnectedFlag: hasBankedSystemRom, slotless, 1 or 2 drives depending on the flag.
- ADetachedSecondPortHidesTheExternalDrive: a Disk II card with one occupied port gives one drive and ShouldShowExternalDrive false.
- ACardWithUndeclaredPortsShowsBothDrives.
- NoControllerMeansNoDrives: GetDeskSceneDriveCount 0.

**Why this cannot fail before the fix**
On the old code the answer comes straight from the config the rebuild is rewriting, so there is no copy to hold. A shell-level assert would need private access: the readers are private (EmulatorShell.h:781/932/1264) and OnGetMinMax bails without an HWND. With m_chromeFacts and RefreshMachineChromeFacts in the protected section, a TestShell like the one in ShellKeyWiringTests.cpp:59 can repeat test 1 through the shell.

**Manual check**
In a Debug build, run an application-verifier or ASan session that orbits the desk scene (EmulatorWindowInput.cpp:335) or drag-resizes while switching //e <-> //c in a loop. Before the fix it reports reads of freed SlotConfig or string memory from DeskSceneDriveCount, ShouldShowExternalDrive or SyncSwitchBarState; after the fix it is clean.

**Sites:** CassoEmuCore/Shell/MachineManager.cpp:489, CassoEmuCore/Shell/MachineManager.cpp:505, CassoEmuCore/Shell/MachineManager.cpp:508, CassoEmuCore/Shell/MachineManager.cpp:513, CassoEmuCore/Shell/MachineManager.cpp:327, CassoEmuCore/Shell/MachineBuilder.cpp:622, CassoEmuCore/Shell/Window/EmulatorWindow.cpp:1934, CassoEmuCore/Shell/Window/EmulatorWindow.cpp:1956, CassoEmuCore/Shell/Window/EmulatorWindow.cpp:1996, CassoEmuCore/Shell/Window/EmulatorWindow.cpp:2001, CassoEmuCore/Shell/Window/EmulatorWindow.cpp:2202, CassoEmuCore/Shell/Window/EmulatorWindow.cpp:2773, CassoEmuCore/Shell/EmulatorShellChrome.cpp:933, CassoEmuCore/Shell/EmulatorShellChrome.cpp:1094, CassoEmuCore/Shell/EmulatorShellChrome.cpp:1199, CassoEmuCore/Shell/EmulatorShellChrome.cpp:1414, CassoEmuCore/Shell/EmulatorShellChrome.cpp:1443, CassoEmuCore/Shell/EmulatorShellScene.cpp:745, CassoEmuCore/Shell/EmulatorShellScene.cpp:764, CassoEmuCore/Shell/EmulatorShell.cpp:1262, CassoEmuCore/Shell/EmulatorShell.cpp:1284, CassoEmuCore/Shell/WindowCommandManager.cpp:579

## eject-keeps-salvage-offered [fixed-by-035]

**Evidence:** 035 added DiskImageStore::RetireBay (CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2197-2221) in commit 9514f86e4 ("feat(debugger): Hold disk flushes during reverse execution"). Master does not have that commit. Every path that empties a bay now runs through it, and it clears the flag along with the rest of the bay: `entry.mounted = false;` at :2218 and `entry.salvageOffered = false;` at :2219. If retention is on, it first copies the flag into the retained Entry (:2209).

Callers:
- Eject calls RetireBay at :2322, after the IsEjectWhenAnswered early return at :2312-2315. That early return leaves the disk mounted, so keeping the flag set there is correct.
- EjectLostImage calls it at :3875.
- SeatMedia calls it at :2137, then puts the retained flag back at :2156 along with the image it describes.
- MountRestored calls it at :685.
- MountFromBytes calls it on an occupied bay at :268. Its failed-load branch clears the flag directly at :284-285, and a successful load reassesses at :296-298.
- MountExternallyModifiedDisk reassesses at :3531-3532.

The deferred eject in ResolvePendingChange calls Eject at :3172, and PowerCycle calls Eject at :2377. Both therefore reach RetireBay. No other code in CassoEmuCore sets `mounted = false` or resets an Entry. A grep gives only :284 and :2218.

What is still the same:
- IsSalvageOffered (:1595-1603) still returns the bare flag without checking `mounted`. That is the backstop the fix direction asked for, and it is absent, but no reachable state makes the flag stale.
- The only consumer on this branch is the Disk menu enable query: CassoEmuCore/Shell/Window/EmulatorWindow.cpp:757-758 calls EmulatorShell::IsSalvageOffered (CassoEmuCore/Shell/EmulatorShellDisks.cpp:572-579).
- The drive context menu in master's EmulatorShellStorage.cpp does not exist on this branch.
- RunSalvageFlow still returns silently when AssessSalvage fails (EmulatorShellDisks.cpp:610-614; AssessSalvage gives ERROR_NOT_READY at DiskImageStore.cpp:1643-1644), but a stale enable no longer leads there.

No test covers the flag after an eject. IsSalvageOffered has no callers anywhere under UnitTest. The salvage tests in UnitTest/EmuTests/DiskImageStoreTests.cpp:1025-1243 and UnitTest/EmuTests/DamagedDiskMountTests.cpp:228 check AssessSalvage only on a mounted disk.

**035 impact:** The fix direction from master does not apply as written. It says to clear entry.salvageOffered in Eject next to `mounted = false`, and on this branch Eject has no such line: RetireBay already clears the flag at :2219.

Clearing the flag in Eject before the RetireBay call at :2322 would introduce a new defect. RetireBay copies the flag into the retained Entry at :2209 before clearing it, and SeatMedia puts it back at :2156. That is how a damaged disk that reverse execution or a .cassostate load returns to its drive gets its salvage offer back. Zeroing the flag first would store `false` in the retained copy, and the returned disk would show "Salvage readable sectors" disabled until it was remounted.

Merge consequences:
- When master and 035 merge, drop master's own Eject-side fix in favor of RetireBay.
- Master's EmulatorShellStorage.cpp context-menu site is not on this branch, so it needs no change here. After a merge it reads the same IsSalvageOffered and inherits the fix.
- The flush hold does not affect this. The early return for a pending answer (:2312-2315) leaves the disk mounted, so keeping the flag set there is correct, and the answered eject at :3172 runs the full Eject.

**Proposed fix:** No functional fix is needed in the worktree. As an optional backstop, change DiskImageStore::IsSalvageOffered (CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1602) to check the mount as well:

```cpp
const Entry &  entry = GetEntry (slot, drive);

return entry.mounted && entry.salvageOffered;
```

The local goes in a nested block, the way IsMounted and GetMediaId do it at :2044-2048. That way any future path that empties a bay without RetireBay still cannot enable the menu item for an empty drive. Do not clear the flag in Eject itself, because that would break the retained-media restore described under fixImpact.

**Regression test:** Two lock-in tests. Both pass in the worktree today and fail on master. They also fail here if `entry.salvageOffered = false;` is removed from RetireBay (:2219), or if the flag is cleared before RetireBay copies it.

1. Extend UnitTest/EmuTests/DiskImageStoreTests.cpp in the salvage section, after AssessSalvage_DamagedStandardDisk_IsOffered (:1025). Add TEST_METHOD (IsSalvageOffered_AfterEject_IsFalse):
   - Run `store.MountFromBytes (kSlot, kDrive, "broken.woz", DiskFormat::Woz, MakeDamagedStandardWoz())`.
   - Assert `store.IsSalvageOffered (kSlot, kDrive)` is true (precondition).
   - Run `store.Eject (kSlot, kDrive)`.
   - Assert IsMounted is false and IsSalvageOffered is false, with the message "an empty drive offers no salvage".
   - Optionally, remount a healthy WOZ into the same bay (MakeDamagedStandardWoz with byte 8 flipped back, as at :1052-1054) and assert IsSalvageOffered is still false.

2. Extend UnitTest/EmuTests/DiskFlushHoldTests.cpp next to EjectedDiskIsKeptWhileRetained (:143). Add TEST_METHOD (SeatedDiskGetsItsSalvageOfferBack):
   - Mount a damaged standard WOZ into s_kHoldSlot/s_kHoldDrive, using MountFromBytes with a WOZ whose header CRC is broken, as in DiskImageStoreTests' MakeDamagedStandardWoz.
   - Call SetPositionSource and SetMediaRetention (true), and record GetMediaId.
   - Eject, then assert IsSalvageOffered is false.
   - Call `SeatMedia (slot, drive, mediaId, changed)`, then assert IsSalvageOffered is true, with the message "the disk came back with its salvage offer".
   - Call `SeatMedia (..., 0, changed)` and assert IsSalvageOffered is false again.

Test 2 guards against applying master's fix direction literally on this branch.

**Sites:** CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2219, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2209, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2322, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2156, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3875, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3172, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1602, CassoEmuCore/Shell/EmulatorShellDisks.cpp:578, CassoEmuCore/Shell/Window/EmulatorWindow.cpp:757

## entry-flags-race [changed-but-present]

**Evidence:** The defect is still present. Entry.mounted and Entry.salvageOffered are still plain bools (CassoEmuCore/Devices/Disk/DiskImageStore.h:470, :475). m_pendingMutex guards only the pending records (DiskImageStore.h:715-719). Neither accessor takes a lock: IsMounted returns `GetEntry (slot, drive).mounted` at DiskImageStore.cpp:2429, and IsSalvageOffered returns `.salvageOffered` at DiskImageStore.cpp:1602.

UI-thread readers:
- The menu enable query (EmulatorWindow.cpp:751-761, cases :755-758) calls EmulatorShell::IsWriteProtectToggleOffered, which reads IsMounted at EmulatorShellDisks.cpp:772, and EmulatorShell::IsSalvageOffered, which reads the store at EmulatorShellDisks.cpp:578.
- The create-disk occupancy check reads `IsMounted (6, drive - 1)` at WindowCommandManager.cpp:1281. That runs after the modal create dialog (:1275) and before the insert is posted at :1343 through EmulatorShell::Mount -> DiskManager::Mount -> PostCommand (DiskManager.cpp:712).

CPU-thread writers in 035:
- MountFromBytes writes at :274, :284, :285 and :298.
- RetireBay writes at :2218-2219. Every path that empties a bay now goes through it: MountFromBytes :268, MountRestored :685, SeatMedia :2137, Eject :2322, EjectLostImage :3875.
- SeatMedia (new in 035) writes at :2155-2156.
- MountExternallyModifiedDisk writes at :3532.

The command queue is still serviced while paused (CpuManager.cpp:134).

The ordering behind the replace prompt still holds:
- A drive-slot click posts the eject (EmulatorWindowInput.cpp:1599 / :1622 -> DiskManager::Eject, post at DiskManager.cpp:760), then opens the picker (:1604 / :1631).
- The eject can be deferred. FlushEntry -> ReportPreserveFailure sets EjectWhenAnswered at DiskImageStore.cpp:410-413, and Eject returns at :2312-2315 with the disk still in the drive. ResolvePendingChange finishes the eject later, at :3168-3173.

New in 035, beyond master:
1. SeatMedia changes occupancy and the salvage bit on the CPU thread during reverse execution. The path is Replayer.cpp:384 -> MachineHost::LoadState :1135 -> CheckStateHeader :1677 -> MachineHost::SeatMedia :1786 -> DiskImageStore::SeatMedia :2117. Reverse execution runs on the CPU thread (ReverseHost.h:24). SeatMedia emits no bay change, so a seek or scrub while paused flips the bits with no event.
2. A machine-state load (EmulatorShellState.cpp:148, CPU thread) also flips them, through MountRestored -> RetireBay + MountFromBytes.
3. 035 also fixed eject-keeps-salvage-offered: RetireBay now clears salvageOffered at :2219, so the two bits change together.

The other new IsMounted callers run on the CPU thread and are not part of this race:
- MachineHost.cpp:1751 (AreBaysAsSaved) and :1427.
- MachineStateFile.cpp:64 and :565, which run on the CPU thread per EmulatorShellState.cpp:101 and :148.
- MachineDebugTarget.cpp:900, reached through DebugViewCapture.cpp:33.

**035 impact:** 1. Entry can no longer hold atomics. 035 moves Entry into and out of `std::vector<Entry> m_retained` (DiskImageStore.h:709; DiskImageStore.cpp:2149-2150, :2181, :2213). A std::atomic<bool> member would delete Entry's move operations, so the "make them std::atomic<bool>" option in the original fix direction no longer compiles. The published copy has to sit beside m_entries, or in the per-bay UI snapshot from ui-derefs-store-diskimage. DiskImageStore is already non-copyable and non-movable because of m_pendingMutex, so an atomic array member costs nothing.

2. A publish hooked to EmitBayChange would miss changes. SeatMedia changes a bay with no bay change at all, and the failed-load branch of MountFromBytes emits none either. The publish has to happen where the fields are written: MountFromBytes, RetireBay, SeatMedia and MountExternallyModifiedDisk. RetireBay is now the one chokepoint for emptying a bay.

3. The window is much easier to hit than on master. A reverse seek or scrub flips occupancy on the CPU thread while the user is paused and using the menus.

4. The flush hold does not change this fix. Eject still flushes under the hold and can still defer on a conflict, and both answers to the deferred question still end with the drive empty.

5. A UI-side "eject posted" record alone is not safe as the replace-prompt answer. The posted eject may be deferred. An insert that lands before the answer goes through MountFromBytes -> FlushEntry(Running), which fails again, then RetireBay. RetireBay calls sharedState.Eject(), which clears askOutstanding and ejectWhenAnswered (MountedImageState.cpp:49-51). So the disk waiting on the answer is dropped, and the answer is then ignored at DiskImageStore.cpp:3091. The CPU-side mount therefore has to be the authority. This overlaps remount-discards-dirty.

**Proposed fix:** Part A: publish the two flags race-free.

1. In DiskImageStore.h, beside m_entries (not inside Entry), add:
   - `static constexpr uint8_t s_kBayMounted = 0x01, s_kBaySalvageOffered = 0x02;`
   - `std::atomic<uint8_t> m_bayFlags[kSlotCount][kDriveCount] = {};`
   - a private `void SetBayFlags (Entry & entry, bool isMounted, bool isSalvageOffered);`
   SetBayFlags writes entry.mounted and entry.salvageOffered, then does `m_bayFlags[entry.slot][entry.drive].store (bits, std::memory_order_release)`. The constructor sets entry.slot and entry.drive (DiskImageStore.cpp:34-35), and SeatMedia and MountFromBytes never overwrite them.

2. Use SetBayFlags at every write to a bay's flags:
   - MountFromBytes failure branch (:284-285): `SetBayFlags (entry, false, false)`.
   - MountFromBytes success branch (:298): `SetBayFlags (entry, true, SUCCEEDED (hrAssess) && assessment.isOffered)`.
   - Keep the provisional `entry.mounted = true` at :274 as an unpublished internal write. AssessSalvage needs it through hasImage at :1533, and leaving it unpublished means the UI never reads a mounted bay that has no salvage answer yet.
   - RetireBay (:2218-2219): `SetBayFlags (entry, false, false)`. The `kept` writes at :2208-2209 stay plain, because kept entries are never read off the CPU thread.
   - SeatMedia (:2155-2156): `SetBayFlags (entry, true, kept.salvageOffered)`.
   - MountExternallyModifiedDisk (:3532): `SetBayFlags (entry, true, SUCCEEDED (hrAssess) && assessment.isOffered)`.

3. IsMounted (:2429) and IsSalvageOffered (:1602) return `(m_bayFlags[slot][drive].load (std::memory_order_acquire) & bit) != 0` after the IsValidBay check. Internal CPU-thread checks keep reading entry.mounted.

4. If the ui-derefs-store-diskimage snapshot (mutex-guarded per-bay WriteProtectInfo and damage summary) lands first, put isMounted and isSalvageOffered into that struct and write them from the same SetBayFlags calls, rather than adding a second mechanism.

Part B: the replace prompt.

1. Add a UI-thread-only `bool isEjectPosted` to DriveWidgetState.
   - Set it in DiskManager::Eject next to StartDoorTransition (DiskManager.cpp:771).
   - Clear it in UpdateDriveWidgets where the path change is seen (:850 BeginEject, :854 BeginInsert).
   - Expose `bool DiskManager::IsReplacePromptNeeded (int drive) const`, returning `m_diskStore.IsMounted (6, drive) && !m_driveWidgetState[drive].isEjectPosted`.
   - Call it at WindowCommandManager.cpp:1281 in place of the direct store read.

2. Make the CPU-side mount the authority, so a stale answer cannot drop a disk. In MountFromBytes's replace branch (:263), before FlushEntry, hoist `isEjectPending = entry.sharedState.IsEjectWhenAnswered();` and add `CBRFEx (!isEjectPending, HRESULT_FROM_WIN32 (ERROR_BUSY), outDiagnosis.failure = MountFailure::AwaitingAnswer)`. (Not taken: research.md R2's decline already keeps the disk in this case, so the plan adds no AwaitingAnswer value; tasks.md T088 tests it.)
   - Add the new MountFailure value, and a sentence for it in FormatMountFailureMessage (for example "Drive 1 is waiting for an answer about the disk in it.").
   - The existing m_onMountCompleted path (DiskManager.cpp:485-488) and the "could not be mounted" message (WindowCommandManager.cpp:1344-1346) report it.
   - The disk then stays in the drive until the answer finishes its eject.

**Regression test:** 1. Fails before the fix: extend UnitTest/EmuTests/SharedImageTests.cpp, after AnEjectThatCannotKeepTheDisplacedVersionAsksBeforeEmptyingTheDrive (:1086). Add TEST_METHOD AnInsertWhileAnEjectWaitsOnItsAnswerLeavesTheDiskInTheDrive.
   - Setup: Rig rig; rig.WriteImage (kImagePath, 0x11); Mount; dirty track 0 and SetLoadedForTest (true, true); rig.WriteImage (kImagePath, 0x33); rig.refusePreserve = true.
   - store.Eject: assert one question and IsMounted still true.
   - rig.WriteImage ("C:\\work\\Other.dsk", 0x55). Assert that Mount (kSlot, kDrive, "C:\\work\\Other.dsk") FAILED, that IsMounted is still true, that GetSourcePath is still kImagePath, and that GetImage()->IsDirty() is still true.
   - Then rig.refusePreserve = false; ResolvePendingChange (kSlot, kDrive, ChangeAction::PreserveCopy, chosen). Assert IsMounted is false and rig.files.count (chosen) != 0.
   - Before the fix, the Mount succeeds. RetireBay drops the waiting disk and clears the question, ResolvePendingChange returns early at DiskImageStore.cpp:3091, and the chosen copy is never written.

2. Fails before the fix: extend UnitTest/EmuTests/DiskResetRemountHoldTests.cpp. It already builds a DiskManager over a TestMachine with a CpuManager that is never started. Add TEST_METHOD APostedEjectNeedsNoReplacePrompt.
   - Mount a disk in (6, 0) through the store seams, then call manager.Eject (6, 0). This posts IDM_DISK_EJECT1, which nothing drains.
   - Assert store.IsMounted (6, 0) is still true and manager.IsReplacePromptNeeded (0) is false.
   - Mount a disk in (6, 1) and assert IsReplacePromptNeeded (1) is true.
   - Before the fix, the decision at WindowCommandManager.cpp:1281 is store.IsMounted alone, which answers true for drive 0.

3. Guards the publish sites: extend UnitTest/EmuTests/DiskFlushHoldTests.cpp, which has the retention rig in EjectedDiskIsKeptWhileRetained (:143). Add TEST_METHOD BayFlagsReadOffThreadFollowEveryBayChange.
   - Steps: PrepareDirtyDisk; Eject with retention on; SeatMedia (mediaId); SeatMedia (0); a MountFromBytes of truncated bytes (fails); a good mount followed by DiscardHeldWrites (the MountExternallyModifiedDisk path). Use the damaged salvageable fixture from DamagedDiskMountTests.cpp:228 for the salvage bit.
   - After each step, start a std::thread (from Pch.h) that reads IsMounted and IsSalvageOffered, join it, and compare against GetMediaId != 0 and the expected verdict.
   - This test passes before the fix. After the fix it fails if any SetBayFlags call is missing.
   - The data race itself cannot be shown deterministically with MSVC, which has no thread sanitizer.

**Sites:** CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2429, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1602, CassoEmuCore/Devices/Disk/DiskImageStore.h:470, CassoEmuCore/Devices/Disk/DiskImageStore.h:475, CassoEmuCore/Devices/Disk/DiskImageStore.h:709, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:274, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:284, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:298, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2155, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2218, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3532, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:412, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2312, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3172, CassoEmuCore/Shell/EmulatorShellDisks.cpp:578, CassoEmuCore/Shell/EmulatorShellDisks.cpp:772, CassoEmuCore/Shell/Window/EmulatorWindow.cpp:755, CassoEmuCore/Shell/WindowCommandManager.cpp:1281, CassoEmuCore/Shell/WindowCommandManager.cpp:1343, CassoEmuCore/Shell/DiskManager.cpp:760, CassoEmuCore/Shell/MachineHost.cpp:1786

## engine-counters-not-atomic [changed-but-present]

**Evidence:** The race is still there in the worktree. The line numbers have moved, and 035 added writers.

UI-thread reader: EmulatorShell::TryPresentUiFrame (CassoEmuCore/Shell/EmulatorShellPresent.cpp:556) calls m_diskManager->UpdateDriveWidgets() every frame (:637). DiskManager::UpdateDriveWidgets reads engine.IsMotorOn(), GetReadNibbles(), GetWriteNibbles() and GetCurrentTrack() at CassoEmuCore/Shell/DiskManager.cpp:867-871. The getters return plain members: Disk2NibbleEngine.h:74, :76, :83, :84. The members are declared at Disk2NibbleEngine.h:236 (int m_currentTrack), :237 (bool m_motorOn), :271 (uint64_t m_readNibbles) and :272 (uint64_t m_writeNibbles). The comment at DiskManager.cpp:860-863 still says the reads use "relaxed atomics semantics". The frame's shared lifetime lock (EmulatorShellPresent.cpp:564) only keeps the controller from being destroyed. The CPU thread takes that lock exclusively only for a machine switch (MachineManager.cpp:489). EmulatorShellCpuThread.cpp takes no lock while it runs slices. So nothing orders the engine fields between the two threads.

CPU-thread writers in the worktree:
- Disk2NibbleEngine::SetMotorOn writes m_motorOn at Disk2NibbleEngine.cpp:86. Callers: Disk2Controller.cpp:206, :498-499 and :543-544.
- SetCurrentTrack reads it at :161 and writes it at :178. Callers: Disk2Controller.cpp:436 and :500.
- StepLss does m_readNibbles++ at :663, running under Tick (:534, called from Disk2Controller.cpp:586 and :702).
- WriteLatch does m_writeNibbles++ at :754, called from Disk2Controller.cpp:130.
- Reset writes all of them at :502 and :511-512, called from Disk2Controller.cpp:912.

New in 035 (commit 0c0405b17): Disk2NibbleEngine::LoadState writes all four at :997-998 and :1009-1010, and SaveState reads them at :887-888 and :899-900. LoadState is reached from Disk2Controller::LoadState (Disk2Controller.cpp:1189) on a state load (CpuCommandDispatcher.cpp:192 -> EmulatorShellState.cpp:155, CPU thread). It is also reached from a reverse-execution keyframe restore (Replayer.cpp:384), which the CPU thread's frame drives (EmulatorShellCpuThread.cpp:999). A restore can move the counters backward, so the "monotonic counters" premise in the comment is now false as well. The diff at DiskManager.cpp:873-874 uses !=, so it still behaves correctly.

035 also made these fields protected state that a test subclass touches directly: StateProbeEngine at UnitTest/Devices/Disk2StateTests.cpp:89 writes them at :103-104 and :115-116 and compares them at :140-141 and :152-153.

A related stale comment is still in place: CassoEmuCore/Ui/DriveWidgetState.h:26-28 says motorOn, diskActive and headQuarterTrack are "written by CPU thread". The only writer is UpdateDriveWidgets on the UI thread (DiskManager.cpp:883-885), so those atomics guard nothing.

Practical severity: on x64 and ARM64, aligned plain loads of these sizes do not tear, so the visible symptom today is nil. It is a data race under the C++ memory model (undefined behavior), and the comment claims a guarantee that does not exist. The 035 flush hold (DiskImageStore.cpp:1090) and CommitPendingWrite (Disk2NibbleEngine.cpp:432) never touch these four fields, so they do not interact with this defect.

**035 impact:** 035 does not fix this, but it changes what the fix has to cover.

1. The new SaveState/LoadState code (Disk2NibbleEngine.cpp:887-888, :899-900, :997-998, :1009-1010) must move to explicit relaxed loads and stores. LoadState is now a third kind of CPU-thread writer: a file state load, and a reverse-execution keyframe restore through Replayer.cpp:384.
2. The StateProbeEngine test subclass in UnitTest/Devices/Disk2StateTests.cpp reads and writes these protected fields directly. Its Assert::AreEqual calls at :140-141 and :152-153 will not compile once the fields are std::atomic, because the test framework has no ToString for std::atomic. They have to compare .load() values. The Fill writes at :103-104 and :115-116 still compile through atomic operator=, but should use relaxed .store() calls to match.
3. Rewind can lower the counters, so "monotonic" in the comment is wrong. The != diff at DiskManager.cpp:873-874 is correct and must stay; do not tighten it to >.
4. The flush hold (DiskImageStore.cpp:1090, m_isReplaying) and CommitPendingWrite never read or write these four fields, so the fix does not need to respect them.
5. Removing StepInstructionWhilePaused does not matter here. Debugger steps run on the CPU thread like ordinary slices, so they are writers of the same kind, and while the machine is paused nothing writes.

Publishing the values into DriveWidgetState from the CPU thread, the alternative the defect entry suggested, fits 035 worse. DriveWidgetState is UI-side state whose lifetime is not tied to the controller, and keyframe restores would have to republish into it. Keeping the atomics inside the engine covers every writer, LoadState included, with no new wiring.

**Proposed fix:** Make the four fields relaxed atomics inside the engine. Only the CPU thread writes them, so the increments can be a relaxed load plus store rather than a locked read-modify-write, which keeps StepLss free of a lock prefix.

CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.h:
- :236 becomes `atomic<int>   m_currentTrack  { 0 };` and :237 becomes `atomic<bool>  m_motorOn  { false };`
- :271-272 become `atomic<uint64_t>  m_readNibbles  { 0 };` and `atomic<uint64_t>  m_writeNibbles  { 0 };`. Re-align the surrounding columns per copilot-instructions.
- Rewrite the comments at :79-82 and :268-270 to say that the UI thread samples these four every frame while only the CPU thread writes them, which is why they are relaxed atomics, and that the four reads together are not one snapshot.
- Getters: :74 `return m_motorOn.load (memory_order_relaxed);`, :76 `return m_currentTrack.load (memory_order_relaxed);`, :83 and :84 likewise for the two counters.
- Optionally add `static_assert (atomic<uint64_t>::is_always_lock_free);` so a target where the 64-bit counter would take a lock fails the build.

CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:
- SetMotorOn :86 `m_motorOn.store (on, memory_order_relaxed);`
- SetCurrentTrack :161 compares against `m_currentTrack.load (memory_order_relaxed)`, and :178 does `m_currentTrack.store (clamped, memory_order_relaxed);`
- ResolveSlot :200 and GetCurrentTrackBits :467 pass GetCurrentTrack().
- Reset :502, :511 and :512 use `.store (…, memory_order_relaxed)`.
- Tick :543 `if (!m_motorOn.load (memory_order_relaxed))`
- StepLss :663 `m_readNibbles.store (m_readNibbles.load (memory_order_relaxed) + 1, memory_order_relaxed);`
- WriteLatch :754 does the same for m_writeNibbles.
- SaveState :887-888 and :899-900 use `.load (memory_order_relaxed)`; LoadState :997-998 and :1009-1010 use `.store (…, memory_order_relaxed)`.

There are no copies of Disk2NibbleEngine anywhere: every instance is default-constructed or lives in Disk2Controller::m_engine[] (Disk2Controller.h:246). The implicit copy constructor that atomic members delete is never used.

Comments:
- DiskManager.cpp:860-863: "The CPU thread writes the engine's motor flag, head position and nibble counters while this runs. They are relaxed atomics, so each read is whole, but the four are not one snapshot, which an activity LED does not need. A counter that changed in either direction since the last frame counts as activity; a rewind lowers them."
- DiskManager.cpp:785: "any forward movement" becomes "any change".
- DriveWidgetState.h:17-34: motorOn, diskActive and headQuarterTrack are written by the UI thread in DiskManager::UpdateDriveWidgets, sampled from the engine's atomics, not by the CPU thread.

UnitTest/Devices/Disk2StateTests.cpp:
- :140-141 and :152-153 compare `m_x.load()` against `other.m_x.load()`.
- :103-104 and :115-116 use `.store (…, memory_order_relaxed)`.

Run PerformanceTests.cpp's engine benchmark (around :353) to confirm that StepLss shows no cost. On x64 and ARM64, relaxed atomic loads and stores compile to plain moves.

**Regression test:** A runtime test cannot reproduce this race deterministically on x64 MSVC: aligned word-size loads do not tear, and MSVC has no thread sanitizer. The deterministic check that fails before the fix and passes after has to be at compile time, paired with a threaded test that pins the cross-thread contract.

1. Compile-time check, in the existing StateProbeEngine at UnitTest/Devices/Disk2StateTests.cpp:89, which already opens the protected fields. Add class-scope assertions:
   `static_assert (std::is_same_v<decltype (m_motorOn), std::atomic<bool>>);`
   and the same for m_currentTrack (`std::atomic<int>`), m_readNibbles and m_writeNibbles (`std::atomic<uint64_t>`), plus `static_assert (std::atomic<uint64_t>::is_always_lock_free);`. Before the fix the UnitTest project fails to build. After it, the build passes, and the existing round-trip tests in that file (Fill, SaveState/LoadState, AssertSameState) show that the save format and the 035 LoadState path are unchanged.

2. Threaded contract test, new TEST_METHOD ActivityGettersSampleWhileTheEngineRuns in UnitTest/EmuTests/Disk2NibbleEngineTests.cpp, following the std::thread producer pattern in UnitTest/Devices/Disk2EventRingTests.cpp:198-237. Set up a DiskImage with a resized track 0, SetDiskImage, and SetMotorOn (true). A std::thread plays the CPU thread for a fixed number of iterations: eng.Tick (kCyclesPerBit * 64), eng.WriteLatch (0xAA), eng.SetCurrentTrack (i % (Disk2NibbleEngine::kMaxTrack + 1)), then it sets an atomic done flag. The test thread plays UpdateDriveWidgets: until done, it samples GetReadNibbles(), GetWriteNibbles(), GetCurrentTrack() and IsMotorOn(). It counts violations in locals rather than asserting inside the loop, so a failure cannot destroy a joinable thread: a counter that went backward, a track outside [kMinTrack, kMaxTrack], or the motor off. After join(), assert zero violations and both counters above zero.

This test passes before and after the fix on today's targets. It documents the contract, a race detector would flag it, and it catches a later change that breaks single-writer monotonicity, such as a counter reset done outside Reset/LoadState.

The existing ResetClearsLifetimeNibbleCounters (Disk2NibbleEngineTests.cpp:150), SetCurrentTrackClampsToValidRange (:137) and EmptyDriveReadsNoiseAndHoldsPosition (:111) confirm that the getters still return the same values after the change.

**Sites:** CassoEmuCore/Shell/DiskManager.cpp:868, CassoEmuCore/Shell/DiskManager.cpp:869, CassoEmuCore/Shell/DiskManager.cpp:870, CassoEmuCore/Shell/DiskManager.cpp:871, CassoEmuCore/Shell/DiskManager.cpp:860, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.h:74, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.h:76, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.h:83, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.h:84, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.h:236, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.h:237, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.h:271, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.h:272, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:86, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:178, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:502, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:511, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:663, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:754, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:997, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:1009, CassoEmuCore/Ui/DriveWidgetState.h:26, UnitTest/Devices/Disk2StateTests.cpp:140, UnitTest/Devices/Disk2StateTests.cpp:152

## stale-step-comments [changed-but-present]

**Evidence:** 035 moved the step to the CPU thread, which makes most of the master findings moot. The one comment that is still wrong is the OnMachineCommand banner, and now the code ten lines below it contradicts it directly.

What 035 changed:
- The UI handler posts the step. WindowCommandManager.cpp:751-761 checks IsPaused, then calls m_shell.PostCommand(IDM_MACHINE_STEP). The comment at :758-759 says "The step runs on the CPU thread ... A paused CPU thread still drains its queue, so it arrives."
- CpuCommandDispatcher.cpp:78-80 sends the command to EmulatorShell::StepInstruction (EmulatorShellCpuThread.cpp:379-405). There it is either the debug session's StepInto or SampleHostInputs + StepOne + Render/Publish, all on the CPU thread.
- The pause wait still wakes on a queued command (CpuManager.cpp:448-449, 559-564), DrainCommandQueue still runs before the pause check (:579-583, :590-593), and PostCommand still notifies the paused thread (:134-139, :163-169).
- EmulatorShell::StepInstructionWhilePaused is gone, along with the master comment at EmulatorShell.cpp:1371 ("MUST have verified the CPU thread is paused"). A grep for that text returns nothing. Fixed.
- Master's note at WindowCommandManager.cpp:728 ("The paused CPU thread runs no slices") is now :737-738: "pausing only sets a flag, so the write is queued to run there between slices". That is accurate. Fixed.
- Every path that ticks the machine is now on the CPU thread: EmulatorShellCpuThread.cpp:401 and :1122 (RunCycles), Replayer.cpp:302 under the CPU-thread debug session, and SynchronousRunDriver.cpp:62 for headless runs. MachineHost::FinishStep calls Disk2Controller::Tick (MachineHost.cpp:844-847). So the motor-off and idle callbacks no longer fire on the UI thread. The "races nothing" / "race-free" comments at Disk2Controller.cpp:562-566, Disk2Controller.h:116-122 and MachineBuilder.cpp:1513-1518 are no longer contradicted by a step. They still claim more than is true, though: the UI thread reads and mutates the store. That access is covered by the separate defects salvage-mount-on-ui-thread, ui-derefs-store-diskimage, entry-path-string-race and stale-thread-ownership-comments. Disk2Controller.h:120-121 also still says the shell wires the hook to "DiskImageStore::FlushAll". MachineBuilder.cpp:1519-1528 actually wires FlushAllUnlessHeld followed by ApplyPendingReload.

What is still stale:
- The banner at WindowCommandManager.cpp:694-700 still says: "Step is driven DIRECTLY from the UI thread ... a paused CPU thread is provably idle -- blocked in pauseCV.wait ... Posting it would in fact deadlock: the CPU thread cannot drain its command queue while it is parked. It is delegated back through the shell to keep Disk2Controller's full definition out of this header." Every clause is now false. The step is posted (:760). The paused thread drains its queue (CpuManager.cpp:579-583). The handler does not touch Disk2Controller.
- The banner at :702-703 says "Speed and pause are plain CpuManager calls ... so they need no marshalling at all". The pause case at :723-733 now also posts IDM_DEBUG_PAUSE_CHANGED (:729) so the debugger receives the change on the CPU thread.
- The banner's claim that reset and power cycle are posted (:690-692) still holds: RequestReset posts IDM_MACHINE_RESET (EmulatorShellCpuThread.cpp:564), and :719 posts IDM_MACHINE_POWERCYCLE.

**035 impact:** 035 already did the half that changes behavior. The step now runs on the CPU thread through the command queue, so the fallback in the defect's fix direction (state the contract while the step stays on the UI thread) no longer applies.

What is left is comment-only, and the comments should describe the 035 mechanism:
- Step uses the same posted path as reset and power cycle.
- A paused thread drains its queue: the wait wakes on HasPendingCommands, and the drain runs before the pause check.
- Pause also posts IDM_DEBUG_PAUSE_CHANGED.
- The Tick callbacks fire on the thread that runs the machine, which is the CPU thread in the GUI.

The Disk2Controller and MachineBuilder comments no longer need the "whichever thread ticks the controller" hedge, because every tick path is on the CPU thread. Their "races nothing" / "race-free" wording should still be narrowed to "no guest write is in progress". UI-thread access to the store is a live problem tracked by other defects, and an absolute claim would hide it. The flush hold is unaffected; only the Disk2Controller.h text needs to say FlushAllUnlessHeld instead of FlushAll. No code changes.

**Proposed fix:** This is comment-only. Do not change code. (035's `e8290e93f`, part of its defect b fix, rewrites the banner item 1 covers, so on the merged code tasks.md T130 checks it and changes it only if it still says Step runs on the UI thread.)

1) CassoEmuCore/Shell/WindowCommandManager.cpp:687-703. Replace the paragraphs from "These commands do NOT all reach the CPU the same way" through "so they need no marshalling at all." with:

//  Reset, power cycle and step are POSTED to the CPU thread's queue, because
//  they change or run machine state the CPU thread is actively using and must
//  land between instructions rather than mid-execution. A paused CPU thread
//  still drains that queue -- its pause wait wakes on a pending command, and
//  the drain runs before the pause check -- so a step posted while paused runs
//  at once, on the same thread as every other instruction.
//
//  Speed and pause are plain CpuManager calls -- atomics the CPU thread reads
//  on each pass. Pause also posts IDM_DEBUG_PAUSE_CHANGED so an attached
//  debugger is told on the CPU thread. A frame already in flight when the
//  pause flag flips still finishes; the machine stops at the next pass.

Keep the first paragraph at :684-685 ("The Machine menu: ...") as it is.

2) CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:562-566. Replace with:

            // Motor-idle auto-flush: the operation is complete, and Tick runs
            // on the thread that runs the machine and makes the guest's disk
            // writes, so no guest write is in progress here (see
            // SetMotorOffFlushCallback). Fires after the sinks so a
            // debug-panel observer still records MotorDisengaged first.

3) CassoEmuCore/Machines/Apple2/Common/Disk2Controller.h:116-122. Change "a naturally debounced, race-free point to persist dirty images (this thread owns the writes). The shell wires it to DiskImageStore::FlushAll" to "a naturally debounced point, with no guest write in progress, to persist dirty images. The shell wires it to DiskImageStore::FlushAllUnlessHeld, then ApplyPendingReload".

4) CassoEmuCore/Shell/MachineBuilder.cpp:1515-1516. Replace "The callback fires on the CPU thread inside Tick, which owns the disk writes, so it races nothing." with "The callback fires inside Tick, on the CPU thread that makes the guest's disk writes, so no guest write is in progress."

Afterward, run scripts/CheckStyle.ps1 -Mode Tree. The edits stay inside existing comment blocks and leave banners and signatures where they are.

**Regression test:** This fix only changes comments, so no test can fail before it and pass after. The behavior the corrected comments describe is already covered:
- UnitTest/DebuggerTests/EmulatorDebugWiringTests.cpp:1751-1763, MainWindowStepTests::StepIsPostedToTheCpuThread: HandleCommand(IDM_MACHINE_STEP) leaves a pending command and runs nothing on the UI thread.
- UnitTest/UiTests/CpuManagerCommandTests.cpp:60, PostCommand_WhilePaused_StillDispatches: a paused CpuManager dispatches a posted command.

The gap is the no-debugger case. The existing step test only runs with a debug session attached. Add MainWindowStepTests::StepWithoutTheDebuggerIsPostedToTheCpuThread to the same file:
1. Construct a bare EmulatorShell with no SetDebugSession.
2. Call shell.GetCpuManager().SetPaused(true).
3. Call shell.HandleCommand(IDM_MACHINE_STEP).
4. Assert GetCpuManager().HasPendingCommands() is true ("the step waits for the CPU thread"), and that the shell machine's position or cycle count did not move.

This test passes on the 035 worktree today. It guards against the step moving back to the UI thread. That move would make the rewritten banner wrong again, and would make the Disk2Controller and MachineBuilder Tick-thread comments false the way they were on master.

**Sites:** CassoEmuCore/Shell/WindowCommandManager.cpp:694, CassoEmuCore/Shell/WindowCommandManager.cpp:702, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:562, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.h:116, CassoEmuCore/Shell/MachineBuilder.cpp:1513

## stale-thread-ownership-comments [still-present]

**Evidence:** All five comments are word for word what master has; only their line numbers moved. Master's DiskImageStore.h:627 is now 718, Disk2Controller.cpp:1016 is now 1018, EmulatorShellDisks.cpp:570 is now 710, DiskManager.cpp:431 is unchanged and :854 is now 862. Each claim is still false in the worktree.

(1) DiskImageStore.h:715-718 says m_pendingMutex guards only the pending records, and that "the image, the path and the identity are touched only by the thread that owns disk writes". DiskImageStore.h:383-385 says NoteExternalChange is "CALLED FROM ANY THREAD", which implies the same safety.
- The UI thread reads the path every frame. TryPresentUiFrame (EmulatorShellPresent.cpp:637) calls UpdateDriveWidgets, which binds a reference into Entry.path at DiskManager.cpp:803 through GetSourcePath. GetSourcePath (DiskImageStore.cpp:2442-2447) takes no lock.
- The UI thread also dereferences the image every frame, at DiskManager.cpp:895-897, through GetImage (DiskImageStore.cpp:2411-2415, no lock). RunSalvageFlow (EmulatorShellDisks.cpp:621) and ReportDamagedMount (:696, :733-734) do the same on the UI thread.
- NoteExternalChange (DiskImageStore.cpp:2692-2712) runs on watcher threads and the UI thread. It reads entry.mounted and entry.path under m_pendingMutex, but no writer takes that lock:
  - MountFromBytes :271-274 and :282-284
  - RetireBay :2216-2220, reached from :268, :685, SeatMedia :2137, Eject :2322 and EjectLostImage :3875
  - SeatMedia :2152-2157, which is new in 035
  - RepointBayToFile :3583-3587

(2) Disk2Controller.cpp:1018-1019 says SetEventSink is "Safe to call from the UI thread between CPU slices". Nothing makes that true.
- OpenDisk2DebugDialog runs from WindowCommandManager.cpp:1034 on the UI thread. It holds only a shared try-lock (EmulatorShellDebug.cpp:515) and writes the sinks at :563 and :569.
- The CPU thread never holds the lifetime lock during a slice. The only CPU-side taker is MachineManager.cpp:489, exclusively, during a switch.
- The CPU thread reads m_eventSink at Disk2Controller.cpp:177, 223, 241, 251, 463, 557, 734, 766, 855 and 878.
- SwitchMachine also writes the sinks on the CPU thread: CpuCommandDispatcher.cpp:54 leads to MachineManager.cpp:560, then AttachDebugSinksIfOpen (EmulatorShellDebug.cpp:694-708), then MachineHost.cpp:1888. This happens after the exclusive scope closes at :527.

(3) EmulatorShellDisks.cpp:710-712 says "Mounts run on the CPU thread -- the picker and the menu both route through it". The salvage Insert at :674 calls m_diskManager->MountDiskInSlot6 directly on the UI thread. RunSalvageFlow is reached from the menu (WindowCommandManager.cpp:1501) and from ReportDamagedMount (:751), which first moves itself to the UI thread at :713-719.

(4) DiskManager.cpp:430-432 says MountDiskInSlot6 "runs on the CPU thread for every user-initiated mount". The same call at EmulatorShellDisks.cpp:674 contradicts it.

(5) DiskManager.cpp:860-863 says "the bool + monotonic counters" are read "with relaxed atomics semantics". Two problems:
- The reads at :868-871 hit plain fields: the getters at Disk2NibbleEngine.h:74, 76, 83 and 84 return m_currentTrack/m_motorOn (:236-237) and m_readNibbles/m_writeNibbles (:271-272). The CPU thread writes them at Disk2NibbleEngine.cpp:86, 178, 663 and 754.
- Under 035, "monotonic" is false as well. LoadState writes the counters back (Disk2NibbleEngine.cpp:997-998, 1009-1010), and Reset zeroes them (:502, :511-512).

**035 impact:** 035 widens what each corrected comment has to cover, and changes two of the fixes.

(a) New CPU-thread writers of path, image and mounted. SeatMedia (DiskImageStore.cpp:2117-2164) and RetireBay (:2197-2221) arrived with reverse execution's media retention. The corrected DiskImageStore.h comment must cover them, plus m_retained and the plain m_isFlushHeld and m_isReplaying flags (:709-713). Those flags are touched only on the CPU thread: ReverseController.cpp:2005 and DiskManager.cpp:661. The mutex fix in note-external-change-reads-entry-unlocked must lock inside RetireBay and SeatMedia. Locking only master's writer list misses every bay change a snapshot load makes.

(b) The salvage Insert now also skips two 035 mechanisms:
- The divergence gate. A UI post of IDM_DISK_INSERTn passes through AllowCommand (EmulatorShellReverse.cpp:303-333). Disk inserts are journal inputs (CpuCommandDispatcher.cpp:245-248), so IsStateChangingCommand returns true for them (DivergenceGate.cpp:43).
- The input journal, which records the mount at EmulatorShellCpuThread.cpp:352-355.

The direct MountDiskInSlot6 call skips both. A salvage Insert behind live therefore mounts into history without asking to diverge, and is never recorded. The fix must go through EmulatorShell::Mount (EmulatorShellDisks.cpp:82-93), not MountDiskInSlot6. The corrected comments should say that posting the mount is what earns the CPU thread, the divergence question and the journal entry. Under the flush hold, the mount's flush of the outgoing disk still writes, because it counts as an eject-like flush (DiskImageStore.h:133-137). The comment must not suggest otherwise.

(c) "Between slices" can now be made true. The CPU manager drains commands before every slice, paused or not (CpuManager.cpp:579-583). A posted attach command is therefore the right fix for SetEventSink. A new command id is neither a journal input nor state-changing, so AllowCommand passes it without asking (EmulatorShellReverse.cpp:315-320). debug-sink-attach-race also offers the UI holding the lifetime lock exclusively; that alternative does not make the comment true, because the CPU thread never takes that lock while executing.

(d) 035 saves and restores the engine counters (Disk2NibbleEngine.cpp:899-900, 997-998, 1009-1010). If they become std::atomic, SaveState, LoadState and Reset must use load and store. The word "monotonic" must go: a state load moves the counters backward. The UI tests with != (DiskManager.cpp:873-874), so activity detection survives that.

The removal of StepInstructionWhilePaused does not touch this defect.

**Proposed fix:** Land each comment correction in the same commit as the paired fix that makes it true. If a paired fix is deferred, delete the false claim instead of keeping it.

1. DiskImageStore.h:715-718, with note-external-change-reads-entry-unlocked and ui-derefs-store-diskimage. The lock is taken only around the field writes, never across FlushEntry, a sink or EmitBayChange. Check that no RetireBay caller already holds the lock: ApplyPendingReloadToBay takes it in scoped blocks at :2832, :2881 and :2945. Replace the comment with:
"//  Guards each bay's path, its mounted flag and its pending record. NoteExternalChange matches a path against the bays from a watcher thread and from the UI thread while the CPU thread mounts, ejects, seats and repoints them, so every write of those fields takes this lock -- MountFromBytes, RetireBay, SeatMedia, RepointBayToFile. The image, the kept disks, the flush hold and the replay flag are touched only by the thread that owns disk writes; the UI reads a bay through its published snapshot, never through the entry."

2. DiskImageStore.h:383-385. Replace with:
"//  CALLED FROM ANY THREAD and does no work beyond recording. It reads the bays' paths only under m_pendingMutex, which every writer of a path takes; acting on a change belongs to the thread that owns disk writes."

3. Disk2Controller.cpp:1018-1019, with debug-sink-attach-race. OpenDisk2DebugDialog creates the panel and posts a new attach command, for example IDM_DEBUG_ATTACH_SINKS. That command runs AttachDebugSinksIfOpen on the CPU thread. The revoke and reattach steps at MachineManager.cpp:424-432 and :547-560 move inside the exclusive scope at :489-527. The panel replacement at EmulatorShellDebug.cpp:544 must keep the old panel alive until the CPU thread has dropped it. Replace the comment with:
"//  CPU thread only, like every fire site that reads the sink. The debug panel is attached by a posted command, which the CPU manager drains between slices whether or not the machine is paused, and detached by the shell's destructor only after the CPU thread has stopped. Pass nullptr to detach."
Add "CPU thread only" to Disk2Controller.h:96-103 and Disk2AudioSource.h:116-119. Change "wires it as the controller's event sink" in EmulatorShellDebug.cpp:502-504 to say the dialog posts the attach.

4. EmulatorShellDisks.cpp:710-712, with salvage-mount-on-ui-thread, which replaces :674 with Mount (6, drive, fs::path (assessment.suggestedPath).wstring()). Replace the comment with:
"// A user mount runs on the CPU thread -- the picker, the menu, a drop and the salvage flow's Insert all post it, so a flush never races the drive engine -- and this raises a modal. Bounce to the UI thread rather than building a dialog from there. A mount from the command line runs on the UI thread before the CPU thread starts, which is why this checks rather than always posting."
This is accurate: EmulatorShell.cpp:626 runs inside Initialize, before m_cpuManager.Start in RunMessageLoop (EmulatorWindow.cpp:1346).

5. DiskManager.cpp:430-432. Replace the first sentence with:
"//  Whatever the outcome, it leaves through the completion callback: every user-initiated mount reaches this through a posted IDM_DISK_INSERTn, on the CPU thread, so the HRESULT returned here reaches nobody who can act on it."

6. DiskManager.cpp:860-863, with engine-counters-not-atomic: std::atomic fields, with relaxed loads and stores in the getters, the increments, Reset, SaveState and LoadState. Replace the comment with:
"// motorOn + diskActive sampling. The engine's motor flag, head position and nibble counters are relaxed atomics that only the CPU thread writes, so each read here is whole but the four can come from different instants, which a widget sampled every frame tolerates. Activity is any change in either counter, not growth: a power cycle zeroes them and loading a machine state moves them back."

**Regression test:** No test can observe comment text, so this defect closes through the paired fixes' tests. Add these with the commits that rewrite the comments.

Primary test, pinning the DiskImageStore.h:715 claim. Extend UnitTest\EmuTests\SharedImageTests.cpp with TEST_METHOD (AnEjectWaitsWhileAChangeIsBeingMatchedAgainstTheBays):
- Use the existing Rig and mount kImagePath in bay 6/0.
- Replace the clock (store.SetClock, DiskImageStore.h:371) with one that blocks only when called on the watcher thread. It signals "inside", then waits up to 5 s on "release". NoteExternalChange calls GetNowMs while holding m_pendingMutex (DiskImageStore.cpp:2694-2695), so the watcher thread holds the lock while it waits.
- Start a std::thread that calls store.NoteExternalChange (kImagePath, ExternalChangeIntent::Unstated), and wait for "inside".
- Start a second std::thread that runs store.Eject (6, 0) and sets an atomic "ejected" flag when it returns.
- Wait 200 ms, then assert that "ejected" is still false.
- Set "release", join both threads, then assert that "ejected" is true and !store.IsMounted (6, 0).

Before the fix, Eject's RetireBay writes path and mounted without the lock, finishes inside the 200 ms, and the assertion fails. After the fix, the eject waits on m_pendingMutex. Add the same test with SeatMedia (with retention on) and with RepointBayToFile in place of Eject, so 035's writers are covered too. A loop-based stress test would not do: this repo builds with neither TSan nor ASan, so a race test is nondeterministic. The blocking clock seam makes this one deterministic.

Secondary tests for the other corrected comments:
- UnitTest\EmuTests\CpuCommandDispatcherTests.cpp: the new attach command dispatches to the target's attach override.
- UnitTest\EmuTests\DivergenceGateTests.cpp: DivergenceGate::IsStateChangingCommand (the attach id) is false, so opening the panel behind live does not ask to diverge. Both fail before the fix, because the command does not exist yet.
- UnitTest\EmuTests\Disk2NibbleEngineTests.cpp: the existing SaveState/LoadState round trip must keep restoring the read and write counts once the fields are atomic.
- The salvage Insert routing is covered by salvage-mount-on-ui-thread's own test.

**Sites:** CassoEmuCore/Devices/Disk/DiskImageStore.h:715, CassoEmuCore/Devices/Disk/DiskImageStore.h:383, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:1018, CassoEmuCore/Shell/EmulatorShellDisks.cpp:710, CassoEmuCore/Shell/DiskManager.cpp:430, CassoEmuCore/Shell/DiskManager.cpp:860

## nib-reflush [still-present]

**Evidence:** The mechanism is unchanged in the worktree (811a6f727). Only the line numbers moved.

1. Clean tracks are copied from the mount-time bytes. DiskImage::Serialize sends a Nib image to NibbleImageCodec::Serialize (*this, m_rawSourceBytes, out) (CassoEmuCore/Devices/Disk/DiskImage.cpp:686-695). Serialize works out the geometry from the source size (NibbleImageCodec.cpp:397-415). In Render, hasSource is true when the size matches (NibbleImageCodec.cpp:461), and every track that is not dirty is memcpy'd straight from sourceBytes (NibbleImageCodec.cpp:475-479). Only dirty tracks are derived from the live bits (NibbleImageCodec.cpp:481-501).

2. The source bytes are only ever set at load. m_rawSourceBytes (DiskImage.h:320) is assigned in exactly three places: LoadFromBytes (DiskImage.cpp:807), Load (DiskImage.cpp:885, .dsk only) and Eject, which clears it (DiskImage.cpp:935). It has no setter (DiskImage.h:150-262). Grep shows no other writer in the tree.

3. Every successful save clears all the per-track dirty bits without moving that baseline:
   - ClearDirty (DiskImage.cpp:642-651).
   - FlushEntry: Serialize (DiskImageStore.cpp:1210), write through the sink or WriteFileAtomically (1220-1237), then ClearDirty (1243).
   - DiskImage::Flush: write (DiskImage.cpp:997-1001), then ClearDirty (1004-1005). Disk2Controller::SoftReset calls this one directly (Disk2Controller.cpp:944).
   - The preserve/rescue paths: RescueOnTheWayOut (DiskImageStore.cpp:453-465); SaveLoadedImage (3821-3833) followed by ClearDirty at 1178, 2977 and 3359; ResolvePendingChange save-as at 3117-3140 and 3193-3222. Each of these then repoints the bay at the new file (1192, 3143, 3224, 3364), while the baseline still holds the ORIGINAL file's mount bytes.

4. How the data is lost. The guest writes track A through Disk2NibbleEngine.cpp:692 -> DiskImage::WriteBit, which sets m_trackDirty[A] (DiskImage.cpp:503-504). Flush #1 writes A correctly and clears every dirty bit. The guest then writes track B, and flush #2 copies A from the mount-time bytes, which puts A's pre-session content back in the file and reports S_OK.

What 035 changed around the defect, without fixing it:
- The spindown flush now goes through FlushAllUnlessHeld (MachineBuilder.cpp:1519-1521). It is skipped only while m_isFlushHeld or m_isReplaying is set (DiskImageStore.cpp:1899). Live spindowns, SoftReset (2347), PowerCycle (MachineHost.cpp:984), machine switch (MachineManager.cpp:406), save-state (MachineStateFile.cpp:229), CommitHeldWrites (DiskImageStore.cpp:1970-1973), Flush (1384-1392), remount (265) and Eject (2304) all still flush, so any two of them in one mount trigger it.
- 035 adds new routes to the defect:
  - Media retention moves the same DiskImage object, still holding its stale m_rawSourceBytes, into m_retained (RetireBay, 2197-2214) and back (SeatMedia, 2149-2157). A disk flushed on eject and then reseated reverts that eject's writes on its next flush.
  - DiskImage::LoadState marks a track dirty only if it differs from the in-memory bits (DiskImage.cpp:1241-1245, CommitStateTrack 1391-1408). The comment at 1241-1245 states this as relying on the file holding what the disk held before the load (paraphrased), and that is false for .nib once one flush has happened.
- The write-protect toggle listed on master does not apply to .nib here: SetImageWriteProtect accepts WOZ only (DiskImageStore.cpp:1442-1443).
- Existing 035 tests cannot see the defect:
  - DiskHistoryTests::CommitWritesTheDiskAtTheCurrentPosition (UnitTest/EmuTests/DiskHistoryTests.cpp:264-308) flushes twice in one .nib mount, but its second flush targets the mount-time content.
  - DiskFlushHoldTests::HoldStillSavesOnEjectSwitchAndExit (UnitTest/EmuTests/DiskFlushHoldTests.cpp:73-98) flushes four times, but always dirties track 0 (Dirty, 237-248).

**035 impact:** The fix stays inside DiskImage and its save sites. It adds no flush and moves none of 035's gates.

- **Flush hold and replay gate.** The rebase goes only where bytes were actually written, after BAIL_OUT_IF (m_isReplaying) at FlushEntry (DiskImageStore.cpp:1090) and after FlushAllUnlessHeld's hold check (1899). A held or replayed moment therefore still writes nothing and still moves no baseline.
- **Where the baseline lives.** It must stay on the DiskImage, not on Entry. RetireBay and SeatMedia (2197-2214, 2149-2157) move the DiskImage object in and out of m_retained, so a baseline kept on the image travels with the disk across an eject and a step back. A baseline on Entry would need copying in both places.
- **The 035 LoadState rule.** The fix is what makes that rule true for .nib: CommitStateTrack (DiskImage.cpp:1391-1408) marks only the tracks that differ from memory, and after the rebase every clean track's baseline is the last thing written. A flush after a step back then writes the disk at that position.
- **DiscardHeldWrites (1989-2025)** needs no change. It reloads from the file through MountExternallyModifiedDisk -> LoadFromBytes, which rebuilds the baseline from the file.
- **Existing tests.** DiskHistoryTests::CommitWritesTheDiskAtTheCurrentPosition still passes after the fix. Its stepped-back track is a full-length legal-nibble track, which derives back byte-identical, and every other track copies from the rebased baseline, which still holds the pattern.
- **Separate defect (softreset-second-write-path).** DiskImage::Flush is a second write path, reached from Disk2Controller::SoftReset (Disk2Controller.cpp:944), that bypasses the hold. It still needs the rebase for as long as it exists, so that a soft reset followed by a spindown does not revert. If that defect's fix removes the call, the rebase in DiskImage::Flush just stays correct.
- **Removed pause/step machinery.** The removal of EmulatorShell::StepInstructionWhilePaused does not touch this path.

**Proposed fix:** Move the copy baseline whenever the image's state is written out.

1. **DiskImage.h.** Add a public method next to ClearDirty: `void MarkSaved (const vector<Byte> & savedBytes);`. Its comment should explain the rule: the bytes Serialize produced are now what the file holds, and a serializer that copies clean tracks copies them from these bytes. Clearing the dirty bits without moving the baseline hands the next flush the file as it was at mount.

2. **DiskImage.cpp.** Implement it after ClearDirty, inserted before the next `////` banner. Fix the stale LoadFromBytes comment (790-794) so it says m_rawSourceBytes is the file's bytes as last read or written.

```cpp
void DiskImage::MarkSaved (const vector<Byte> & savedBytes)
{
    m_rawSourceBytes = savedBytes;

    ClearDirty();
}
```

3. **Replace ClearDirty with MarkSaved wherever Serialize's bytes were just written successfully:**
   - DiskImage::Flush (DiskImage.cpp:1004-1005): call `MarkSaved (bytes)` inside `if (hasPath)` after WriteFileAtomically succeeds. Keep `ClearDirty()` for the no-path case, and drop the redundant `m_dirty = false;`.
   - FlushEntry (DiskImageStore.cpp:1243): call `entry.image->MarkSaved (bytes)` when the sink or WriteFileAtomically wrote. Keep ClearDirty only for the no-sink, empty-path branch.
   - SaveLoadedImage (DiskImageStore.cpp:3832-3833): call `entry.image->MarkSaved (bytes)` after WritePreserved succeeds. This covers the conflict-preserve callers at 1157/1178, 2933/2977 and 3344/3359, whose own ClearDirty calls become no-ops. Keep the one at 3359 for the IsPreservedWritten branch.
   - RescueOnTheWayOut (465): `entry.image->MarkSaved (held)`.
   - ResolvePendingChange (3140 and 3222): `entry.image->MarkSaved (held)`.

4. **Leave these ClearDirty calls alone:** the write-protected gate (1101) and the user discard (3152). Nothing is written at either, so the file and the baseline still match.

5. **Nothing changes in NibbleImageCodec.** Render's copy-clean-tracks rule is correct once its source is the file's current bytes. Render's output always has the source's size, so .nb2 stays .nb2.

The rebase is a whole-image copy per save for every format. Only the Nib serializer reads the bytes, so MarkSaved could skip the copy for other formats. The flush already serializes the whole image, though, and the uniform rule is simpler.

**Regression test:** Extend UnitTest/EmuTests/DiskImageStoreTests.cpp, next to Flush_WozGuestWriteSurvivesReloadThroughStore (around line 640), with:

TEST_METHOD (Flush_NibbleSecondFlushKeepsTheTrackTheFirstSaved)

1. Set up the mount:
   - Build `file` as kNibImageSize bytes of `0x80 | (i % 127)`. Every byte is a legal nibble, so a derived full track is the same length and is not rotated.
   - `store.SetFlushSink` captures the bytes into `captured`.
   - `MountFromBytes (kSlot, kDrive, "two.nib", DiskFormat::Nib, file)`.

2. First write and flush:
   - `image->WriteBit (3, 7, image->ReadBit (3, 7) ^ 1)`. Bit 7 is byte 0's low bit, so the high bit stays set.
   - `store.Flush (kSlot, kDrive)`, then `first = captured`.
   - Assert that track 3 of `first` differs from track 3 of `file`: the first flush saved the write.

3. Second write and flush:
   - `image->WriteBit (20, 7, image->ReadBit (20, 7) ^ 1)`.
   - `store.Flush (kSlot, kDrive)`.

4. Assertions:
   - `memcmp (&first[3 * kNibTrackSize], &captured[3 * kNibTrackSize], kNibTrackSize) == 0`: the second flush keeps the track the first one saved.
   - Track 20 of `captured` differs from `file`.
   - `NibbleImageCodec::Load (captured, reloaded)` succeeds, and `reloaded.ReadBit (3, 7)` equals `image->ReadBit (3, 7)`.

Before the fix, the second flush memcpy's track 3 from the mount-time `file` (NibbleImageCodec.cpp:475-479), so the first memcmp and the reload check fail. After the fix both pass.

Optional 035 coverage in UnitTest/EmuTests/DiskFlushHoldTests.cpp, reusing PrepareDirtyDisk and Dirty (track 0, bit 100, high bit untouched):
- **Commit after an earlier save.** FlushAll; record files.last; write track 5 bit 100; SetFlushHold (true); FlushAllUnlessHeld (assert no write); CommitHeldWrites. Assert track 0 of files.last equals track 0 of the first write.
- **Eject with retention, then reseat.** SetMediaRetention (true); Eject (write #1); SeatMedia back; write track 5; FlushAll. Assert track 0 still matches write #1.

Both fail before the fix and pass after.

**Sites:** CassoEmuCore/Machines/Apple2/Common/NibbleImageCodec.cpp:461, CassoEmuCore/Machines/Apple2/Common/NibbleImageCodec.cpp:475-479, CassoEmuCore/Devices/Disk/DiskImage.cpp:694, CassoEmuCore/Devices/Disk/DiskImage.cpp:807, CassoEmuCore/Devices/Disk/DiskImage.cpp:885, CassoEmuCore/Devices/Disk/DiskImage.cpp:642-651, CassoEmuCore/Devices/Disk/DiskImage.cpp:1004-1005, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1243, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:465, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3832-3833, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1178, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2977, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3140, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3222, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3359, CassoEmuCore/Devices/Disk/DiskImage.h:320, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:944

## remount-discards-dirty [changed-but-present]

**Evidence:** The defect is still in the worktree (811a6f727). 035 changed the code around it but closed none of the paths that lose data.

1. MountFromBytes still drops the flush result. DiskImageStore.cpp:263-269 runs `hr = FlushEntry (entry, FlushMoment::Running); IGNORE_RETURN_VALUE (hr, S_OK); RetireBay (entry);`, then puts a new DiskImage in the bay at 271-276. FlushEntry keeps the dirty bit on failure (comment at 1105-1110, CHRN exits at 1218/1223/1236 before ClearDirty at 1243). That dirty bit lives on the image RetireBay just took out of the bay. Mount (584-644) still exempts the bay being mounted into (603-606) and needs ReadImageFile to succeed first (616-617). So any file that can be read but not written loses its writes: full volume, folder ACL on the sibling temp file (1306-1339), rename blocked (1357-1358), or the attribute set after mount.

2. The message is still false on these paths. On a write failure recoveryPath is empty, so FormatFlushLossMessage (877-916) ends with "Your recent writes have not been saved. The disk in the drive still has them." (911-912), and then the bay is replaced or emptied.

3. Callers that reach it with a mounted, dirty bay:
- **Reset.** CpuCommandDispatcher.cpp:62-71 runs RemountDisks before SoftReset. That goes EmulatorShellCpuThread.cpp:417-420, then DiskManager::RemountSlot6Disks (DiskManager.cpp:650-682), then MountDiskInSlot6 (443-491), then Mount.
- **Power cycle.** CpuCommandDispatcher.cpp:73-76 runs PowerCycle: MachineHost.cpp:984-985 calls FlushAllUnlessHeld and ignores it, and the mounts persist. Then RemountDisks hits the same MountFromBytes.
- **Insert.** Re-inserting the same file, or inserting a different one: CpuCommandDispatcher.cpp:86-90, then EmulatorShellCpuThread.cpp:432-435.
- **Machine switch.** MachineManager.cpp:401 calls StopReverseRecording. 406-407 calls FlushAll and ignores it. 587 power-cycles, and 606 calls MountCommandLineDisks, which goes to MountDiskInSlot6 (DiskManager.cpp:404/410) and then MountFromBytes.

4. Eject has the same problem. DiskImageStore::Eject (2290-2330) calls FlushEntry at FlushMoment::Ejecting and ignores the result (2304-2305). Then it runs EndWatching and RetireBay (2319-2322). The eject waits only when IsEjectWhenAnswered (2312) is set, and only ReportPreserveFailure sets it (410-413), which is the external-change path. A Serialize or write failure goes through the plain CHRN (1218/1223/1236), so the "still has them" text appears and the bay empties anyway. That contradicts the Ejecting contract in DiskImageStore.h:528-532. EjectDiskInSlot6 (DiskManager.cpp:507-519) also clears the saved path.

5. New in 035, same loss: loading a state.
- MachineStateFile::Apply calls FlushAll and ignores it (MachineStateFile.cpp:229-230).
- RestoreDisks (534-574) then calls MountRestored. MountRestored retires the bay with no flush (DiskImageStore.cpp:680-686, "the caller flushes first when it wants one"), or RestoreDisks calls Eject (MachineStateFile.cpp:565-567).
- EmulatorShellState.cpp:183 stops recording before Apply, so retention is off and the image is freed outright.

6. What 035 changed, and why none of it fixes this:
- **Retention.** RetireBay (2197-2221) now moves the image, with its dirty bit, into m_retained while retention is on. Retention is on whenever reverse recording runs (ReverseController.cpp:98-99), and recording is on by default (GlobalUserPrefs.h:318). But nothing ever flushes m_retained. CountUnsavedDisks (1937-1955) and FlushAllForShutdown (1873-1876) look only at m_entries. PruneRetainedMedia (2179-2182) drops the image as history ages, and SetMediaRetention(false) drops it at Stop (2061-2068, ReverseController.cpp:142-143). So the writes survive only until the image is pruned, and only a reverse step back to before the remount can bring them back.
- **Flush hold.** RemountSlot6Disks now returns early under the flush hold (DiskManager.cpp:658-664), so reset and power cycle do not remount while the machine is behind live. Under the hold, FlushEntry still writes for insert, eject, switch and load state (DiskImageStore.h:133-137). FlushEntry bails only for m_isReplaying (1090).

ScratchHeatReplayer.cpp:491 installs an always-S_OK flush sink, so it is unaffected.

**035 impact:** 1. **Keep the flush hold and the replay bail as they are.**
   - RemountSlot6Disks' early return under the hold (DiskManager.cpp:658-664) stays, and so does FlushAllUnlessHeld (1893-1906). The fix must not add a flush on any path that the hold or m_isReplaying suppresses.
   - The decline test in MountFromBytes must be FAILED(hr), not "image still dirty". While m_isReplaying, FlushEntry returns S_OK and leaves the image dirty (1090). The hold-skipped paths also leave dirty images, legitimately.

2. **Do not add the decline to MountRestored or SeatMedia.** They retire without flushing by design, to put a snapshot's disks back (2106-2164, 652-662). For load state, the check belongs in MachineStateFile::Apply, the caller that is supposed to flush first.

3. **Retention changes nothing about the fix.** RetireBay's retention (2203-2214) parks the dirty image in m_retained, where it is never flushed and is freed by PruneRetainedMedia or Stop. The bay must keep the disk; letting retention hold it is not enough.

4. **Machine switch: the declined disk stays attached.**
   - StopReverseRecording runs first (MachineManager.cpp:401), so the hold is off. The FlushAll and PowerCycle flushes then run (FlushAll is unconditional; PowerCycle goes through FlushAllUnlessHeld).
   - A declined remount of the kept disk leaves the old dirty image in the store's bay. MachineHost::PowerCycle's BindDiskDrives (MachineHost.cpp:992, 1810-1825) has already pointed the new controller at it, so the new machine still has the disk.

5. **The eject question can reuse 035's ask path.** The FlushMoment::Ejecting route already exists for the external-change case: ReportPreserveFailure sets EjectWhenAnswered (410-413), Eject waits on it (2312-2315), and ResolvePendingChange's Conflict branch finishes the eject (3107-3176). The plain write-failure path should join that route rather than add a second one.

6. **Both remount paths are journaled inputs.** A declined mount must not call EmitBayChange or NotifyMediaChanged. Mount's CHR at 620 already leaves before 626-640, so no boundary keyframe is recorded for a change that did not happen. Replay ignores media inputs (Replayer.cpp:470-475).

**Proposed fix:** 1. **DiskImageStore::MountFromBytes (DiskImageStore.cpp:263-269): decline to replace a bay whose flush failed.** Replace the IGNORE_RETURN_VALUE with:
```cpp
            //  A DISK WHOSE WRITES DID NOT REACH ITS FILE STAYS IN THE DRIVE.
            //  Replacing it would throw away the only copy of them, and the
            //  notice FlushEntry just raised says the drive still has them.
            hr = FlushEntry (entry, FlushMoment::Running);
            CHRF (hr, outDiagnosis.failure = MountFailure::UnsavedWrites);

            RetireBay (entry);
```
   - Mount's CHR at 620 then leaves before the identity stamp, BeginWatching, EmitBayChange and NotifyMediaChanged (626-640). The old image keeps its path, watch and identity, and the controller stays on it.
   - On reset and power cycle, RemountSlot6Disks keeps the dirty disk in the drive. Reset's SoftReset flush, or the next spindown, retries.
   - With this change, the "still has them" text at 911-912 becomes true for every Running caller.

2. **Add a mount failure for the decline.** Add `UnsavedWrites` to MountFailure (MountDiagnosis.h:57-70, plus its doc line), and a MountDiagnosis::Describe case (MountDiagnosis.cpp:40-140), for example: "was not inserted. The disk already in that drive has changes that could not be saved to its file, and inserting this one would have thrown them away. That disk is still in the drive with its changes". (Final wording: contracts/user-messages.md section 5, "was not inserted, because the disk has changes that could not be saved".)
   - In EmulatorShell::HandleMountCompletion (EmulatorShellDisks.cpp:248-259), skip the EhmNotifyUser when failure is UnsavedWrites and completion.path IsSamePath as the bay's current source path. That covers a remount from reset, power cycle or machine switch, where FlushEntry's notice already said all of it.
   - A different file still gets the sentence.
   - Update the DebugBatchRunner.cpp MountFailure switch (around line 301) if it lists cases.

3. **MachineStateFile::Apply (MachineStateFile.cpp:229-230): act on the flush's result.** Make it `hr = machine.GetDiskStore().FlushAll(); CHRF (hr, outError = MakeError ("disk not saved", "A disk in a drive has changes that could not be saved, so the state was not loaded."));`. Check has already run, so a decline here changes nothing else. Update the comment at 227-228 and MountRestored's header comment (DiskImageStore.cpp:659-660) to match.

4. **Make FlushEntry's loss report depend on the moment.** Route the three CHRN sites (1218, 1223, 1236) through one helper, for example `ReportWriteFailure (entry, moment, hr, recoveryPath)`, declared beside ReportPreserveFailure in DiskImageStore.h:547-552.
   - **Running:** use FormatFlushLossMessage, as today.
   - **Ejecting, with recoveryPath empty:** if m_askSink is set and no ask is outstanding, call SetAskedAction(ChangeAction::Conflict). Set AskOutstanding from m_askSink(slot, drive, ChangePrompt::ComposeSaveFailure(entry.path, drive, entry.path, hr, <new cause>)), and set EjectWhenAnswered when it is outstanding, exactly as at 400-413. Eject's existing check at 2312-2315 then keeps the disk. ResolvePendingChange's Conflict branch (3107-3176) already does "Save as..." (write the chosen file, repoint, finish the eject) and "Eject and discard" (ClearDirty, then Eject). Without a sink, give a notice that ends with the wording FormatDiscardedWritesMessage uses (507-508), "The disk is leaving the drive, so these changes cannot be recovered.", in place of "still has them".
   - **ShuttingDown, with recoveryPath empty:** try RescueOnTheWayOut (435-468) first, then give the same leaving notice.
   - **ChangePrompt:** add a SaveFailureCause, for example `Unwritable`, to ChangePrompt.h:43-59. In ComposeSaveFailure (ChangePrompt.cpp:380-412), its first sentence says "Your changes to <file> could not be saved." in place of "Another program modified <file>", and it offers the Ejecting answers (402-412). Pass the cause through the Conflict branch's re-ask at 3132-3134.
   - Update Eject's comment at 2301-2303.

5. **Leave the rest alone, and note two side effects.**
   - Leave RemountSlot6Disks' hold bail, FlushAllUnlessHeld, MountRestored, SeatMedia and the m_isReplaying bail unchanged.
   - Reset will now show two notices for a disk that cannot be written: the remount's flush, then SoftReset's. A machine switch already shows three today. That is acceptable against the data loss; removing the duplicates is a follow-up.
   - A Serialize failure whose .recovered.woz did land also returns failure, so the remount is declined there too. That is conservative, but each retry writes another recovery file. Coordinate with the recovery-file-per-spindown fix. If it records that the current dirty generation was rescued, MountFromBytes can let that case through.

**Regression test:** 1. **UnitTest/EmuTests/DiskImageStoreTests.cpp: add TEST_METHOD (FlushError_remountKeepsTheDiskWhoseWritesWereNotSaved).**
   - Setup: use ScopedFlushNotifyCapture. Set a flush sink that returns HRESULT_FROM_WIN32 (ERROR_DISK_FULL). MountFromBytes (kSlot, kDrive, "keep.dsk", DiskFormat::Dsk, MakeDsk (0)), take `before = GetImage (kSlot, kDrive)`, and dirty it with WriteBit (0, 0, 1).
   - Act: call `hr = MountFromBytes (kSlot, kDrive, "keep.dsk", DiskFormat::Dsk, MakeDsk (0x11), diagnosis)`.
   - Assert:
     - AssertFailed (hr), and diagnosis.failure == MountFailure::UnsavedWrites.
     - GetImage returns `before`, before->IsDirty() is true, and GetSourcePath is still "keep.dsk".
     - s_flushNotifyCount == 1.
   - Repeat with "other.dsk" and assert the same.
   - Add a companion where the sink succeeds: the dirty bay is written once and replaced. This guards against declining too much.
   - Before the fix, hr is S_OK and the bay holds a new, clean image, so the test fails.

2. **Same file: add TEST_METHOD (FlushError_ejectWaitsForAnAnswerAndNeverClaimsTheDriveKeepsTheWrites).** Use a failing sink and a dirty .dsk.
   - Part (a), no ask sink: after Eject, s_flushNotifyLast does not contain "still has them", and it does say the changes cannot be recovered.
   - Part (b), with SetAskSink capturing the ChangePrompt and returning true:
     - After Eject, IsMounted is still true and the image is still dirty.
     - The prompt offers "Eject and discard".
     - After ResolvePendingChange (kSlot, kDrive, ChangeAction::Discard, ""), IsMounted is false.
   - Before the fix, the bay is empty after Eject, no prompt is raised, and the notice says the drive still has the writes.
   - FlushError_surfacesThroughVoidEjectPath (469-484) still asserts one notice and should keep passing.

3. **UnitTest/EmuTests/DiskResetRemountHoldTests.cpp: add TEST_METHOD (RemountAfterAFailedFlushKeepsTheDisk).**
   - Setup: use the same scaffold as RemountUnderHoldWritesNothingAndKeepsTheDisk (59-92): TestMachine ("Apple2e"), whose slot-6 Disk II is wired, so MountDiskInSlot6 gets past its CBR at DiskManager.cpp:452. Use the image and identity reader seams, and leave the hold OFF. Set a flush sink that counts attempts and returns HRESULT_FROM_WIN32 (ERROR_DISK_FULL). Install a SetNotifyFunction capture for the test's lifetime, because without a notifier EhmNotifyUser falls back to stderr or a MessageBox (Ehm.cpp:247-268).
   - Act: mount, flip a bit, construct a DiskManager, and call RemountSlot6Disks().
   - Assert: one write was attempted, store.GetImage (6, 0) is the same image pointer, and the image is still dirty.
   - This is the end-to-end reset and power-cycle path through DiskManager, Mount and MountFromBytes. Before the fix, the image is replaced and clean.

4. **UnitTest/EmuTests/MachineStateFileTests.cpp: add TEST_METHOD (AStateIsNotLoadedOverADiskWhoseWritesCouldNotBeSaved).**
   - Setup: use the Prepare, Build and FileLog helpers (276-344). Build a state from a source machine. Dirty the target's slot-6 drive-1 disk. Swap the target store's flush sink for one that fails, and capture the notifier.
   - Act: call MachineStateFile::Apply.
   - Assert: Apply fails, error.label is "disk not saved", and the bay still holds the same dirty image.
   - Before the fix, Apply succeeds, and the dirty disk is retired and freed.

**Sites:** CassoEmuCore/Devices/Disk/DiskImageStore.cpp:263-276 (MountFromBytes: FlushEntry ignored, RetireBay, image replaced), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:877-916 (FormatFlushLossMessage; 911-912 says the drive still has the writes), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1210-1237 (FlushEntry Serialize/write-failure CHRNs, not moment-aware), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2290-2330 (Eject: 2304-2305 ignored, 2312 waits only for ReportPreserveFailure, 2319-2322 EndWatching + RetireBay), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2197-2221 (RetireBay: retention keeps the image in m_retained, never flushed), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:664-705 (MountRestored: retires at 680-686 without a flush), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:584-644 (Mount: 603-606 same-bay exemption, 616-620 read then MountFromBytes), CassoEmuCore/Shell/DiskManager.cpp:650-682 (RemountSlot6Disks: 661-664 hold bail, 676-677 result ignored), CassoEmuCore/Shell/DiskManager.cpp:443-491 (MountDiskInSlot6), CassoEmuCore/Shell/DiskManager.cpp:507-519 (EjectDiskInSlot6), CassoEmuCore/Shell/CpuCommandDispatcher.cpp:62-76, 86-95 (reset, power cycle, insert, eject), CassoEmuCore/Shell/MachineManager.cpp:401-407, 587, 606 (machine switch), CassoEmuCore/Shell/MachineHost.cpp:984-985, 992 (PowerCycle flush ignored; BindDiskDrives), CassoEmuCore/Shell/MachineStateFile.cpp:229-233, 562, 565-567 (load state: FlushAll ignored, MountRestored/Eject retire the dirty bays), CassoEmuCore/Shell/EmulatorShell.cpp:299-300 (shutdown: same 'still has them' text on a write failure as the process exits), CassoEmuCore/Shell/EmulatorShellDisks.cpp:222-260 (HandleMountCompletion reports a declined mount), CassoEmuCore/Devices/Disk/MountDiagnosis.h:57-70 (MountFailure has no entry for this decline), CassoEmuCore/Devices/Disk/ChangePrompt.cpp:366-423 (ComposeSaveFailure's Ejecting wording assumes an external change)

## settings-wp-drops-dirty [still-present]

**Evidence:** The defective code is the same in the worktree as on master; only its line numbers moved.

1. Missing flush. EmulatorShell::SetDriveUserWriteProtect (CassoEmuCore/Shell/EmulatorShellDisks.cpp:794-816) stores m_userWriteProtect[drive] (805) and then calls image->SetUserWriteProtected(wp) on the live image (814). It never flushes and never checks IsDirty. SetUserWriteProtected is a bare setter (CassoEmuCore/Devices/Disk/DiskImage.h:134). IsWriteProtected ORs in m_userWriteProtected (CassoEmuCore/Devices/Disk/DiskImage.cpp:527-534).

2. The gate. DiskImageStore::FlushEntry (CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1069) commits any open flux write (1095), then passes the dirty check (1097), and then reaches `if (entry.image->IsWriteProtected()) { entry.image->ClearDirty(); BAIL_OUT_IF (true, S_OK); }` (1099-1103). The writes are discarded with S_OK, and no CHRN message appears. This drop is intended and tested (UnitTest/EmuTests/DiskWritePathTests.cpp:662-691, RegularFlush_DropsDirtyOnceProtected), so callers that protect a disk must flush first. Both sibling mechanisms do:
- SetImageWriteProtect flushes before patching the flag (DiskImageStore.cpp:1455-1460).
- DiskManager::ToggleImageWriteProtect flushes before setting the read-only attribute (CassoEmuCore/Shell/DiskManager.cpp:243-247; the reason is given at 197-198).
The user-switch path is the only one that does not.

3. Every flush lands on the gate: Eject (DiskImageStore.cpp:2304), remount (263-266), FlushAll and FlushAllForShutdown, the motor-off FlushAllUnlessHeld (MachineBuilder.cpp:1519-1521, DiskImageStore.cpp:1893-1906), MachineHost.cpp:984, and the new CommitHeldWrites (DiskImageStore.cpp:1970-1973).

4. Thread and producer (unchanged). The Disk page checkboxes (CassoEmuCore/Ui/Settings/DiskPage.cpp:308-309) set the pref. SettingsPanelState::Apply sends ApplyWriteProtect for both drives on every Apply (SettingsPanelState.cpp:1054-1057). SettingsApplyAdapter::ApplyWriteProtect then calls PostCommand(IDM_DISK_WRITEPROTECT1/2, "1"/"0") (SettingsApplyAdapter.cpp:147-162). That reaches EmulatorShell::PostCommand (EmulatorShell.cpp:1335-1338) and CpuManager::PostCommand (CpuManager.cpp:144-153), which consults the command gate first. On the CPU thread, EmulatorShell::DispatchCpuCommand (EmulatorShellCpuThread.cpp:344-358) runs CpuCommandDispatcher::Dispatch, which calls SetDriveUserWriteProtect (CpuCommandDispatcher.cpp:97-101). So the function runs on the CPU thread. The Disk menu's items use IDM_DISK_WP1/2, which goes to ToggleImageWriteProtect (CpuCommandDispatcher.cpp:103-107). The comment at EmulatorShell.h:1149-1154 says "the write-protect menu items post" this command; that is still stale.

5. What 035 adds around the defect (none of it fixes it):
- IDM_DISK_WRITEPROTECT1/2 is now a journaled input, kind DriveWriteProtect (CpuCommandDispatcher.cpp:258-263). It is recorded before dispatch (EmulatorShellCpuThread.cpp:350-355) and replayed as a bare SetUserWriteProtected (Debugger/Reverse/Replayer.cpp:479-487).
- It is therefore a state-changing command for the divergence gate (Debugger/Reverse/DivergenceGate.cpp:25-45). Behind live, AllowCommand (EmulatorShellReverse.cpp:303-333) raises the divergence question on the UI thread. A yes queues IDM_DEBUG_DIVERGE ahead of the command, and its handler ends the flush hold before SetDriveUserWriteProtect runs: ReverseHost::Diverge (ReverseHost.cpp:210-225), then ReverseController::OnMachineChanged (ReverseController.cpp:227-266), then BecomeLive and SetFlushHold(false) (2064-2070). BecomeLive does not flush.
- While the hold was on, FlushAllUnlessHeld skipped the motor-off flushes (DiskImageStore.cpp:1899), and keyframe restore puts back m_dirty (DiskTrackSnapshot.cpp:97-99). So at the moment of divergence the image normally holds unflushed writes. The protect that follows silently drops them at the next flush, and the file keeps the abandoned future's contents instead of the disk as it now stands. The 035 hold makes the window wider, not narrower.

**035 impact:** 1. Flush through the store's explicit flush, not through FlushAllUnlessHeld. The store's own contract (DiskImageStore.h:133-137) limits the hold to the three automatic moments: motor-off, reset and power cycle. Every explicit flush still writes the disk as it stands. Both sibling protect paths flush without checking IsFlushHeld: SetImageWriteProtect calls FlushEntry (DiskImageStore.cpp:1459) and ToggleImageWriteProtect calls m_diskStore.Flush (DiskManager.cpp:245). The user switch should do the same.

2. In practice the hold is already off when the flush runs. The divergence gate's yes queues IDM_DEBUG_DIVERGE, which reaches BecomeLive and SetFlushHold(false) before the write-protect command dispatches. A no drops the command (CpuManager.cpp:150-153).

3. The flush matters more after 035. Writes held behind live are still dirty when the machine diverges, and this flush is the only thing that gets them into the file. Without it, they are dropped and the file keeps the discarded future's contents.

4. Leave the replay path a bare setter: Replayer::ApplyInput (Replayer.cpp:479-487). Replay must never write a file. FlushEntry already returns early while replaying (DiskImageStore.cpp:1090) and leaves the dirty bit alone, so a flush there would do nothing anyway.

5. The other places that set the user flag need no flush:
- DiskManager::ApplyExternalWriteProtect (DiskManager.cpp:175) runs on freshly mounted, clean images.
- The re-apply at the end of ToggleImageWriteProtect (DiskManager.cpp:266-269) writes back the same pref value, so the flag does not change.

6. Flush only when the flag actually turns on. Settings Apply sends the command for both drives on every Apply (SettingsPanelState.cpp:1054-1057), so the flush must not run on every Apply.

7. Out of scope but adjacent: because the command is now state-changing, any Settings Apply behind live (even a color change) goes through the divergence question for both drives.

**Proposed fix:** Move the flush-first ordering into a store operation, as SetImageWriteProtect did for the image flag (see DiskWritePathTests.cpp:617-620, "the ordering ... now lives inside the operation").

1. CassoEmuCore/Devices/Disk/DiskImageStore.h: next to SetImageWriteProtect (line 202), declare
   `HRESULT  SetUserWriteProtect (int slot, int drive, bool writeProtected);`
   Its comment should say that protecting flushes first, because the flush gate drops a protected image's unsaved writes, and that a flush that fails leaves the disk writable with its writes in memory for a later flush.

2. CassoEmuCore/Devices/Disk/DiskImageStore.cpp: after SetImageWriteProtect, splice the new function ahead of the next `////` banner. Use EHM with a single exit:
```
bayOk = IsValidBay (slot, drive);
CBRAEx (bayOk, E_INVALIDARG);
{
    Entry &  entry = GetEntry (slot, drive);
    BAIL_OUT_IF (!entry.mounted || entry.image == nullptr, S_OK);
    isProtecting = writeProtected && !entry.image->IsUserWriteProtected();
    if (isProtecting)
    {
        // Guest writes go out FIRST, while the image still accepts a flush.
        hr = FlushEntry (entry, FlushMoment::Running);
        CHR (hr);
    }
    entry.image->SetUserWriteProtected (writeProtected);
}
Error:
    return hr;
```
   On a failed flush, FlushEntry has already reported the loss through CHRN and kept the dirty bit, and the image stays writable. The same choice as ToggleImageWriteProtect stopping at CHR (DiskManager.cpp:246) is right here: a failed WriteFileAtomically writes no recovery copy, so protecting anyway would turn a reported failure into a silent loss at the next flush.

3. CassoEmuCore/Shell/EmulatorShellDisks.cpp:794-816: keep the range check and `m_userWriteProtect[(size_t) drive] = wp;`. Replace the GetImage/SetUserWriteProtected block with:
```
hr = m_machine.GetDiskStore().SetUserWriteProtect (6, drive, wp);
IGNORE_RETURN_VALUE (hr, S_OK);
```
   Add an HRESULT local and an Error label, per the EHM rule. The pref stays recorded on failure, so the next mount re-applies it (DiskManager.cpp:168-175). That remount flushes the old entry first while it is still writable (DiskImageStore.cpp:263-266), which retries the write. (Not taken as written: keeping the pref on a failed save leaves it saying protected while the disk is writable, so the next Settings apply that sends "1" would count as unchanged, change nothing and raise no notice, and the next mount would apply a protection the save never got. The plan stores the pref only when the drive is empty or the store's `SetUserWriteProtect` succeeds, so the next apply runs the save again; tasks.md T092, T104, T105.)

4. CassoEmuCore/Shell/EmulatorShell.h:1149-1154: change the comment to say the command is posted by the Settings apply path only. The Disk menu's items go through IDM_DISK_WP1/2 and ToggleImageWriteProtect.

5. Do not change the FlushEntry gate (DiskImageStore.cpp:1099-1103) or Replayer.cpp:485.

**Regression test:** Extend UnitTest/EmuTests/DiskWritePathTests.cpp next to WriteProtectToggle_PersistsPendingGuestWritesFirst (615-659) and RegularFlush_DropsDirtyOnceProtected (662-691). Add #include "Shell/EmulatorShell.h" and "resource.h".

1. TEST_METHOD (DriveWriteProtectSwitch_PersistsPendingGuestWritesFirst). This one fails before the fix.
- Set up: `auto shell = std::make_unique<EmulatorShell>();`, as EmulatorDebugWiringTests.cpp:1694 does. DiskManager is not needed; it is built in Initialize. Take `DiskImageStore & store = shell->GetMachine().GetDiskStore();` and `vector<Byte> file = BuildBlankWoz();`.
- Install `store.SetFlushSink ([&file] (const string &, const vector<Byte> & bytes) { file = bytes; return S_OK; });`, then `AssertSucceeded (store.MountFromBytes (6, 0, "t.woz", DiskFormat::Woz, file));`.
- Flip track 0 bit 0 with `img->WriteBit (0, 0, bit0 ^ 1)` and assert IsDirty as a precondition.
- Run the production path: `shell->DispatchCpuCommand (EmulatorCommand { IDM_DISK_WRITEPROTECT1, "1" });`.
- Assert img->IsUserWriteProtected(). Then call `store.Eject (6, 0);` and `AssertSucceeded (WozLoader::Load (file, reloaded));`, and assert `reloaded.ReadBit (0, 0) == bit0 ^ 1` with the message "the guest write made before the switch moved must reach the file".
- Before the fix: the Eject flush reaches the gate at DiskImageStore.cpp:1099-1103, the sink never runs, and `file` stays blank, so the assertion fails. After the fix: the toggle's flush writes the bit first.
- Variant in the same test or a sibling, at the store: call `store.SetFlushHold (true)`, then `store.SetUserWriteProtect (6, 0, true)`, and assert the bit still reaches the file. This pins the decision that the switch's save is an explicit flush, not an automatic one. (The plan moves this variant from the shell to the store. Through the shell, a `DispatchCpuCommand (IDM_DISK_WRITEPROTECT1, "1")` under the hold is a command that reached the emulation thread behind live, and the plan's command handler changes nothing and raises the behind-live notice; tasks.md T092, T093 and T105.)

2. TEST_METHOD (UserWriteProtect_FlushFailure_LeavesTheDiskWritable). This is the store contract test.
- Use a plain DiskImageStore whose sink returns HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT); DiskImageStoreTests.cpp:394-397 is the precedent for a failing sink. Mount, dirty a bit, call `store.SetUserWriteProtect (6, 0, true)`.
- Assert FAILED(hr), !img->IsUserWriteProtected() and img->IsDirty().
- Then call `store.SetUserWriteProtect (6, 0, false)` on a clean image and assert the sink is not called, so un-protecting never flushes.

**Sites:** CassoEmuCore/Shell/EmulatorShellDisks.cpp:794-816 (SetDriveUserWriteProtect; unflushed set at 814), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1099-1103 (FlushEntry write-protect gate: ClearDirty + S_OK), CassoEmuCore/Devices/Disk/DiskImage.h:134 (bare SetUserWriteProtected setter), CassoEmuCore/Devices/Disk/DiskImage.cpp:527-534 (IsWriteProtected ORs in m_userWriteProtected), CassoEmuCore/Shell/CpuCommandDispatcher.cpp:97-101 (IDM_DISK_WRITEPROTECT1/2 dispatch), CassoEmuCore/Shell/CpuCommandDispatcher.cpp:258-263 (now journaled as DriveWriteProtect), CassoEmuCore/Ui/Settings/SettingsApplyAdapter.cpp:147-162 (posts the command), CassoEmuCore/Ui/Settings/SettingsPanelState.cpp:1054-1057 (sent for both drives on every Apply), CassoEmuCore/Shell/EmulatorShell.h:1149-1155 (stale comment: menu items post IDM_DISK_WP1/2, not this), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1970-1973 (CommitHeldWrites, new in 035, also lands on the gate)

## recovery-file-per-spindown [changed-but-present]

**Evidence:** The defect holds in the worktree at 811a6f727. The trigger now runs through 035's held flush, but the repeat is unchanged.

1. Trigger. On every spindown, Disk2Controller.cpp:537-570 calls m_motorOffFlushCallback (567-570). MachineBuilder.cpp:1519-1528 sets that callback to DiskImageStore::FlushAllUnlessHeld. FlushAllUnlessHeld (DiskImageStore.cpp:1893-1906) returns early only while m_isFlushHeld (reverse execution behind live, set at ReverseController.cpp:2005) or m_isReplaying is true. Otherwise it calls FlushAll (1866-1869), then FlushEveryBay(Running) (2253-2276), then FlushEntry for every bay. SoftReset (2345-2352) and MachineHost::PowerCycle (MachineHost.cpp:984) take the same path.

2. No early exit in FlushEntry (DiskImageStore.cpp:1069) applies to a dirty .dsk whose writes do not decode:
   - The replay bail (1090) and the not-mounted bail (1091) do not apply.
   - IsDirty is still true (1097), because a failed flush never clears it.
   - IsWriteProtected (DiskImage.cpp:527-534) depends on IsDamaged (DiskImage.h:159), which counts only load-time CRC mismatch or load-time damaged tracks.
   - At the identity check (1131-1208) the original is unchanged, because nothing wrote it.

3. Serialize fails again every time. DiskImage::Serialize (DiskImage.cpp:668-680) calls the strict NibblizationLayer::Denibblize for Dsk/Do/Po, which ends in CBR(!coverage.HasDataLoss()) (NibblizationLayer.cpp:1193-1194).

4. Every failure writes a new copy and shows a new notice. FAILED(hr) leads straight to TryWriteRecoveryImage (1212-1216), which has no memory of an earlier copy: its loop (995-1011) takes the first name that does not exist yet, and CREATE_NEW (1033-1034) creates it. The result is Session.recovered.woz, then .recovered.1.woz, and so on, up to kMaxRecoveryNameAttempts = 64 (DiskImageStore.h:605). CHRN (1218) then shows FormatFlushLossMessage (877-916) again, still telling the user to mount the copy. Nothing on the Entry (DiskImageStore.h:465-492) or in MountedImageState (MountedImageState.h:175-184) records that a copy was already written.

In sink-based tests the loop always takes attempt 0 (999-1004), so the repeat shows up as repeated writes to one path, not as new files. No existing test flushes the same damaged image twice. DamagedDiskFlushTests.cpp:223-254 and DiskImageStoreTests.cpp:1311-1352 each flush once.

**035 impact:** 1. The fix belongs inside FlushEntry, below both 035 gates: the replay bail at DiskImageStore.cpp:1090 and FlushAllUnlessHeld's hold/replay check at 1899. The hold is then respected with no extra work, and the repeat only ever happens at live. Do not add a flush or a bypass to the hold.

2. 035 provides a ready content stamp: per-track generations. TouchTrack (DiskImage.cpp:1026-1032) gives every changed track a generation number that has never been used before; WriteBit (506), MarkTrackDirty (628), GetTrackBitsForWrite (DiskImage.h:232) and flux splices all call it. DiskTrackSnapshot::Restore (DiskTrackSnapshot.cpp:83-95) puts the saved generations back on the same medium. So an equal (GetImageId, generation vector) pair means equal content, even after a step back and resume from an earlier point. Do not use m_lastGeneration alone: it never drops on a restore, so a disk rewound to different content would look unchanged.

3. The record must live in MountedImageState (entry.sharedState), not as new Entry fields. 035's RetireBay (DiskImageStore.cpp:2197-2221) and the retained-media re-seat (2149-2157) copy Entry field by field, but copy sharedState whole. MountedImageState::Mount/Eject (MountedImageState.cpp:15-56) already reset everything per mount, and RepointBayToFile calls sharedState.Mount (DiskImageStore.cpp:3587). MountedImageState.h:128-131 records why per-mount bookkeeping kept on the bay keeps breaking.

4. Master's StepInstructionWhilePaused removal plays no part in this defect.

**Proposed fix:** The rule: one recovery copy and one notice per mount. Later guest writes update that same copy. Leave every message string unchanged.

(1) DiskImage.h, beside GetTrackGeneration (line 250): add
    const vector<uint64_t> & GetTrackGenerations () const { return m_trackGeneration; }

(2) MountedImageState.h/.cpp: add a per-mount recovery record:
    - Members: m_recoveryPath (string), m_recoveryImageId (uint64_t), m_recoveryGenerations (vector<uint64_t>), m_recoveryIdentity (ImageIdentity).
    - Accessors: GetRecoveryPath(), IsRecoveryCurrent(imageId, generations) (true only when a path is recorded and both values match), SetRecovery(path, imageId, generations, identity), ClearRecovery().
    - Call ClearRecovery() from Mount() and Eject(), next to ClearPreserved().

(3) FlushEntry, DiskImageStore.cpp:1210-1218. On FAILED(hr) from Serialize:
    a. The copy is current when IsRecoveryCurrent(image->GetImageId(), image->GetTrackGenerations()) holds AND the copy is undisturbed. Undisturbed means the recorded identity is unrecorded (a sink, with no host file, so skip the check, the same rule as 1127-1131), or ReadIdentity(GetRecoveryPath()).Matches(that identity).
    b. If the copy is current, write nothing and show nothing: use plain CHR(hr). The image stays dirty and Flush still returns the failure.
    c. Otherwise, call TryWriteRecoveryImage. When the mount already holds an undisturbed copy and IsFileInAnotherBay(copy) is false, rewrite that same path: WriteFileAtomically, or m_flushSink with that path. Only when there is no copy, or it was changed, deleted or mounted in another bay, run the existing free-name loop with CREATE_NEW. On success, call SetRecovery(path, imageId, generations, ReadIdentity(path)).
    d. Show the notice (CHRN/CHRF with EhmNotifyUser) only when the copy went to a path this mount has not reported yet, or when the recovery write failed. Refreshing a copy already reported uses plain CHR.

(4) Update the comments at 931-933 (MakeRecoveryPath) and 962-973 (TryWriteRecoveryImage): the copy still never overwrites any other file, and it refreshes only the copy this mount wrote and still owns. That copy is always a later state of the same session, the same way an ordinary flush replaces the original.

Out of scope: the image keeps its dirty bit, because the original file still lacks the writes. Whether the exit prompt (HasUnsavedWrites, DiskImageStore.cpp:1920) should count a disk whose recovery copy is current is a separate decision.

**Regression test:** Extend UnitTest/EmuTests/DiskImageStoreTests.cpp with two tests after FlushEntry_UnserializableImage_WritesLosslessRecoveryBesideOriginal (1311-1352). Both reuse ScopedFlushNotifyCapture (107-132) and CorruptOneAddressField (40-62).

(A) Test name: FlushEntry_UnserializableImage_IsPreservedOnceUntilTheGuestWritesAgain.
  Setup:
  - Install a sink that counts writes to paths ending ".recovered.woz", records the last such path, and flags any write to the original.
  - MountFromBytes(6, 0, "C:\\disks\\Session.dsk", Dsk, MakeDsk(0x5A)), then CorruptOneAddressField(track 3) and SetLoadedForTest(true, true).
  - Wire a Disk2Controller ctrl(6) with SetMotorOffFlushCallback([&]{ store.FlushAllUnlessHeld(); }), as MotorOffFlush_persistsDirtyWozThroughStore does (713-745).
  - Run three motor cycles. Each is Write(0xC0E9), Write(0xC0E8), Tick(1100000).
  Asserts after the three cycles:
  - recoveryWrites == 1 (3 before the fix).
  - s_flushNotifyCount == 1 (3 before the fix).
  - The image is still dirty, and the original was never written.
  Then a guest write on another track (WriteBit(10, 0, 1)) and one more cycle. Asserts:
  - recoveryWrites == 2.
  - The path is still "C:\\disks\\Session.recovered.woz".
  - s_flushNotifyCount is still 1.

(B) Test name: FlushEntry_UnserializableImage_LeavesOneRecoveryFileOnDisk. This one shows the visible symptom.
  (Not taken: this test creates a real folder, which the constitution's test-isolation rule forbids in unit tests; `Win32DiskFileIoTests`, the disk seams' real-file class, moves to the scenario suite, tasks.md T057. The plan runs the same steps through `FakeDiskFileIo` as `FlushEntry_UnserializableImage_LeavesOneRecoveryFile`, research.md R3 store test 7, written with the fix and checked by mutation because before it `TryWriteRecoveryImage` reaches the real file system; tasks.md T065.)
  Setup:
  - Create a fresh folder under fs::temp_directory_path(), following the casso_atomic_ helper at 767 or SharedImageTests.cpp:2248.
  - Write Session.dsk there and Mount it by its real path, with no sink.
  - Corrupt track 3, set the image dirty, and call FlushAll() three times.
  Assert: exactly one file matching Session.recovered*.woz exists. Before the fix there are three: .recovered.woz, .recovered.1.woz and .recovered.2.woz.
  Then a guest write and one more FlushAll. Asserts:
  - Still one file.
  - Reloading it with MountFromBytes as Woz shows the new bit.
  Clean up the folder at the end.

Optionally, in UnitTest/EmuTests/DiskFlushHoldTests.cpp: with SetFlushHold(true), FlushAllUnlessHeld writes no recovery copy. This confirms the fix stays below the hold.

**Sites:** CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1210-1218 (FlushEntry: Serialize failure -> TryWriteRecoveryImage -> CHRN, unconditionally), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:975-1051 (TryWriteRecoveryImage: free-name loop 995-1011, CREATE_NEW 1033-1034), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:936-952 (MakeRecoveryPath), CassoEmuCore/Devices/Disk/DiskImageStore.h:605 (kMaxRecoveryNameAttempts = 64), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:877-916 (FormatFlushLossMessage, the repeated notice), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1893-1906 (FlushAllUnlessHeld, the spindown/reset/power-cycle entry), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2253-2276 (FlushEveryBay), CassoEmuCore/Shell/MachineBuilder.cpp:1519-1528 (spindown callback), CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:537-570 (spindown expiry fires the callback), CassoEmuCore/Devices/Disk/MountedImageState.h:175-184 and MountedImageState.cpp:15-56 (per-mount state, no recovery record)

## watch-leak [still-present]

**Evidence:** The defect is still present in the worktree. The line numbers have moved, and 035 added one call (RetireBay) to the same block.

1. Nothing ejects first. Both the picker and drag-and-drop go straight to the store's Mount:
   - Picker: WindowCommandManager.cpp:1134, 1343 and 1435 -> m_shell.Mount -> EmulatorShellDisks.cpp:88 -> DiskManager::Mount (DiskManager.cpp:699-716), which posts IDM_DISK_INSERTn at 710-712.
   - Drop: DriveWidget.cpp:952 -> m_sink->Mount, which reaches the same DiskManager::Mount.
   - From there: CpuCommandDispatcher.cpp:86-89 -> EmulatorShell::MountDisk (EmulatorShellCpuThread.cpp:432-435) -> DiskManager::MountDiskInSlot6 (DiskManager.cpp:443-491) -> m_diskStore.Mount (DiskManager.cpp:466).

2. DiskImageStore::Mount (DiskImageStore.cpp:584-644) never drops the old watch:
   - IsFileInAnotherBay skips the target bay (606).
   - It calls MountFromBytes (619), then BeginWatching (630), which watches only the new path's directory (3613-3633).

3. MountFromBytes (DiskImageStore.cpp:242-304), in its occupied-bay branch (263-269), calls FlushEntry(Running) (265) and then RetireBay (268). RetireBay is new in 035 (2197-2221) and clears entry.path at 2217. After that the code sets entry.path to the new file (272). EndWatching is never called on this path.

4. EndWatching (3651-3684) is the only caller of m_watcher->Unwatch (3678), and it works out the directory from entry.path (3654), so it has to run before RetireBay. Its callers are:
   - MountRestored 684 (new in 035)
   - SeatMedia 2136 (new in 035)
   - Eject 2319
   - RepointBayToFile 3581
   - EjectLostImage 3873

   Every one of them pairs EndWatching with RetireBay or with the path change. Mount is the only path that swaps a bay's file without it.

5. Win32ImageWatcher keeps one watch per directory (Win32ImageWatcher.cpp:60 and 97). Each watch holds an open FILE_LIST_DIRECTORY handle (67-73) and a worker thread (86). Only Unwatch (135-149) or the destructor (18-30) closes one.

6. Effect: after swapping from C:\a\X.dsk to C:\b\Y.dsk without ejecting, the C:\a thread and handle stay open until the process exits. That thread keeps calling NoteExternalChange (2692-2715), which takes m_pendingMutex and then matches nothing, because it only looks at mounted bays (2707).

7. A load failure leaks the same way. If the new image fails to load into an occupied bay, MountFromBytes has already retired the old disk (268) and returns E_FAIL (287). The old directory stays watched with no bay using it.

**035 impact:** 035 does not fix the leak. What it changes:

1. Order: 035 put RetireBay (2197-2221) at the exact point where the old watch has to be dropped. Because RetireBay clears entry.path, EndWatching has to come before it, not after.

2. The pattern already exists: 035's new paths, MountRestored (684-685) and SeatMedia (2136-2137), already pair EndWatching with RetireBay. The fix copies that pair into MountFromBytes.

3. Retained disks: they need no watch. NoteExternalChange matches only mounted bays (2707), and SeatMedia calls BeginWatching again when a retained disk returns (2159). RetireBay also copies sharedState into m_retained (2210), so with EndWatching first the retained copy correctly records that it is not watching, just as Eject leaves it.

4. The flush hold is unaffected:
   - The hold is SetFlushHold/IsFlushHeld (DiskImageStore.h:143-144), checked in FlushAllUnlessHeld (DiskImageStore.cpp:1899) and in RemountSlot6Disks's early return (DiskManager.cpp:661-664).
   - The fix adds no flush and does not touch FlushEntry, so the hold behaves as before.

5. MountRestored: its own EndWatching at 684 becomes redundant but stays harmless. It empties the bay before calling MountFromBytes, so the new branch does not run there.

6. The removed StepInstructionWhilePaused plays no part in this path.

7. Threading:
   - Eject already calls EndWatching, which joins a watcher thread, on the CPU thread.
   - No caller of MountFromBytes holds m_pendingMutex. ApplyPendingReload acts outside the lock (2778-2784), and the other guards (2832, 2881, 2945, 3156, 3387) are short scoped blocks.
   - So the join cannot deadlock against NoteExternalChange's lock (2694).

8. Overlap with two other open defects that edit the same lines:
   - failed-mount-dangling-disk-image: if its fix trial-loads before retiring, so a failed load keeps the old disk, EndWatching has to move with RetireBay and run only once the new image is known good.
   - remount-discards-dirty: if its fix bails out of the occupied-bay branch on a failed flush, the same applies.

   The rule to keep: EndWatching sits immediately before RetireBay, wherever RetireBay ends up.

**Proposed fix:** In DiskImageStore::MountFromBytes (CassoEmuCore/Devices/Disk/DiskImageStore.cpp:263-269), call EndWatching between the flush and RetireBay, in the same order Eject uses (2304, 2319, 2322):

        if (entry.mounted)
        {
            hr = FlushEntry (entry, FlushMoment::Running);
            IGNORE_RETURN_VALUE (hr, S_OK);

            //  Before the path goes: the watch is keyed by the directory the
            //  old image sits in, and there is no finding that once RetireBay
            //  has cleared the path. Mount watches the new one afterward.
            EndWatching (slot, drive);

            RetireBay (entry);
        }

Why this placement:

1. It covers every way a bay gets emptied by a mount: Mount's success path, Mount's load-failure path (bay left empty at 282-287), and direct MountFromBytes callers such as ScratchHeatReplayer.cpp:563.

2. EndWatching keeps a directory that another bay still uses (3661-3674), so two disks from one folder stay covered.

3. When the old and new files share a directory (a power-cycle remount through RemountSlot6Disks, a machine switch, or a same-folder swap), the watch is dropped and taken again by BeginWatching at 630. That is the same sequence RepointBayToFile uses (3581-3589). The cost is one thread join and one new armed thread, and no change can be missed in between: sharedState.Mount at 626 re-records the identity and clears anything pending either way.

4. Do not try to skip EndWatching when the two directories match. A same-directory load that fails would then leak the watch again.

5. Leave MountRestored, SeatMedia, Eject, RepointBayToFile and EjectLostImage as they are.

**Regression test:** Extend UnitTest/EmuTests/SharedImageTests.cpp, which already uses FakeImageWatcher through Rig (Rig at 78-274, SetImageWatcher at 124). Add the tests after AWatchIsKeptWhileAnotherBayStillNeedsItsDirectory (374-393).

1. TEST_METHOD (MountingOverADiskDropsTheWatchOnTheFolderItLeft). This one fails before the fix and passes after.
   - Setup: rig.WriteImage (kImagePath, 0x11); rig.WriteImage ("C:\\games\\Game.dsk", 0x22); Mount (kSlot, kDrive, kImagePath); then Mount (kSlot, kDrive, "C:\\games\\Game.dsk") with no Eject in between.
   - Assert: watcher.watched.size() == 1 and watched[0] == "C:\\games"; watcher.unwatched.size() == 1 and unwatched[0] == kDirectory ("C:\\work"); GetSharedState (kSlot, kDrive)->IsWatching() is true.
   - Before the fix: watched holds both "C:\\work" and "C:\\games", and unwatched is empty.

2. TEST_METHOD (AFailedMountOverADiskDoesNotLeaveItsFolderWatched). This one also fails before the fix.
   - Setup: mount kImagePath; set rig.files["C:\\games\\Bad.dsk"] = vector<Byte> (37, 0xAB) and Stamp it; Mount (kSlot, kDrive, "C:\\games\\Bad.dsk").
   - Assert: the Mount fails, IsMounted (kSlot, kDrive) is false, watcher.watched is empty, and unwatched == { "C:\\work" }.
   - Before the fix: "C:\\work" is still watched with no bay using it.
   - If the failed-mount-dangling-disk-image fix lands first and keeps the old disk on a failed load, flip the assertions: "C:\\work" still watched, IsWatching true, unwatched empty. The invariant both versions check is that the watched set equals the directories of the mounted bays.

3. TEST_METHOD (MountingOverADiskKeepsAFolderAnotherDriveStillNeeds). This guards against a fix that calls Unwatch directly instead of EndWatching.
   - Setup: drive 1 holds "C:\\work\\Data.dsk", drive 0 holds kImagePath; then mount "C:\\games\\Game.dsk" into drive 0.
   - Assert: watched holds "C:\\work" and "C:\\games", and unwatched is empty.
   - This passes both before and after the fix.

4. Extend AReMountOfTheSameFileIntoItsOwnDriveStillWorks (640-652). After the second Mount, assert rig.watcher.watched.size() == 1 and GetSharedState (kSlot, kDrive)->IsWatching() is true, so a same-file remount still ends with the bay watched.

**Sites:** CassoEmuCore/Devices/Disk/DiskImageStore.cpp:263-269 (MountFromBytes occupied-bay branch: FlushEntry 265, RetireBay 268, no EndWatching), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:272 (entry.path overwritten with the new file), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:584-644 (Mount: IsFileInAnotherBay 606, MountFromBytes 619, BeginWatching 630 for the new directory only), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3651-3684 (EndWatching, sole caller of Unwatch at 3678; directory taken from entry.path at 3654), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2197-2221 (RetireBay clears entry.path at 2217), CassoEmuCore/Devices/Disk/Win32ImageWatcher.cpp:52-121, 135-149 (one thread and directory handle per watched directory, released only by Unwatch or the destructor), CassoEmuCore/Shell/DiskManager.cpp:466 (MountDiskInSlot6 -> m_diskStore.Mount with no eject first)

## softreset-second-write-path [changed-but-present]

**Status on this branch:** fixed in `e32b2b68c` (tasks.md T066, T067). Reported to 035 as 041 audit defect c on 2026-10-08. 035's tip `d3c15b55c` holds no cherry-pick of the commit, only a comment (`fe0427676`) that gives the reset as the exception to the flushes held behind live, so 035 reaches `master` with the defect and 041's merge removes it; tasks.md T008 corrects that comment once both are merged. The evidence below is as found at `811a6f727`.

**Evidence:** The unguarded write is still there, unchanged. 035 has made it reachable in more ways, and in those ways it is worse.

1. The write itself (unchanged). Disk2Controller::SoftReset calls Reset() and then m_activeDisk[drive]->Flush() on every loaded drive (CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:931-948, the call is at 944). m_activeDisk is the store-owned image that SetExternalDisk hands over (Disk2Controller.cpp:806-813). MachineHost::BindDiskDrives does this at MachineHost.cpp:1821-1824, and so does DiskManager.cpp:564. DiskImage::Flush (CassoEmuCore/Devices/Disk/DiskImage.cpp:981-1009) checks only the dirty bit (991). It then calls DiskImageStore::WriteFileAtomically(m_filePath) directly (1000). That write skips everything FlushEntry has:
- the replay gate (DiskImageStore.cpp:1090)
- the write-protect gate (1099-1103)
- the identity check and the preserve-and-repoint step (1131-1207)
- the recovery image (1214)
- the test flush sink (1220-1224)
- the identity refresh (1253-1256)

It also ignores the flush hold, which is checked only in FlushAllUnlessHeld (DiskImageStore.cpp:1893-1906). m_filePath is set only in LoadFromBytes and Load (DiskImage.cpp:803, 881) and cleared in Eject (934). RepointBayToFile still changes only entry.path (DiskImageStore.cpp:3583), and the identity and watch at 3581-3589. So after a "keep my version" repoint, m_filePath is still the original file.

2. The reset path in the worktree. IDM_MACHINE_RESET is journaled first (EmulatorShellCpuThread.cpp:352-357). It then runs RemountDisks() followed by SoftReset() (CpuCommandDispatcher.cpp:62-71). SoftReset goes EmulatorShell.cpp:1350-1353, then MachineManager.cpp:645-647, then MachineHost::SoftReset (MachineHost.cpp:919-952), then MemoryBus::SoftResetAll (MemoryBus.cpp:793-798), then Disk2Controller::SoftReset. MachineHost::SoftReset never calls the store. DiskImageStore::SoftReset (DiskImageStore.cpp:2345-2352), which is FlushAllUnlessHeld, has no production caller; only tests call it (DiskFlushHoldTests.cpp:58,114; DiskImageStoreTests.cpp:518). The comment at DiskImageStore.cpp:1886-1889 says "a reset ... flush[es] through here", so the design intends that route, but nothing takes it. MachineHost::PowerCycle does follow the 035 pattern (FlushAllUnlessHeld at MachineHost.cpp:984). Disk2Controller::PowerCycle touches only the controller's internal m_disks (Disk2Controller.cpp:973-978). That leaves SoftReset as the one reset that bypasses the store.

3. The master trigger still works (live, failed remount). RemountSlot6Disks (DiskManager.cpp:650-682) calls DiskImageStore::Mount through MountDiskInSlot6 (DiskManager.cpp:466). Mount exits before MountFromBytes, the only place that flushes the old image (FlushEntry at DiskImageStore.cpp:263-269), in three cases:
- the file is in another bay (606-609)
- the extension is unknown (611-612)
- ReadImageFile fails (616-617)

In each case the old dirty image stays in the drive, and Disk2Controller::SoftReset writes it raw to m_filePath.

4. New in 035: under the flush hold, the remount no longer protects anything. RemountSlot6Disks returns at once while the store holds flushes (DiskManager.cpp:661-664). LeaveLive sets the hold (ReverseController.cpp:2005). After that early return, SoftReset still reaches DiskImage::Flush. The existing hold test (DiskResetRemountHoldTests.cpp:59-92) covers only the remount half of the Reset command. A user Reset from behind live normally diverges first: AllowCommand raises the divergence question and posts IDM_DEBUG_DIVERGE, and Diverge then calls OnMachineChanged, which calls BecomeLive and clears the hold (EmulatorShellReverse.cpp:315-330, ReverseHost.cpp:210-218, ReverseController.cpp:259, 2070). But the gate's check reads IsBehindLiveForUi() on the UI thread. A Reset posted just behind a queued step back may therefore run with the hold on. This is plausible but not verified.

5. New in 035, deterministic: replays and running forward from the past. A recorded Reset is applied by Replayer::ApplyInput, which calls m_machine.SoftReset() (Replayer.cpp:493-496). It is reached two ways:
- Replayer::RunTo, with the store marked replaying (Replayer.cpp:162, 175)
- Replayer::PrepareStepHere, when the machine runs forward from the past under the hold (Replayer.cpp:226-249)

Both reach Disk2Controller::SoftReset and DiskImage::Flush, which ignores both m_isReplaying and m_isFlushHeld. A keyframe restore makes the image dirty. DiskImage::LoadState marks every track that differs from the current image dirty (DiskImage.cpp:1241-1245, 1306-1309), and the guest's replayed writes dirty it too.

So stepping back across a recorded Reset writes the disk as it stood at that past reset over the user's file. The store's identity is not refreshed (DiskImage::Flush skips DiskImageStore.cpp:1253-1256). The watcher then reports Casso's own write as an outside change (ApplyPendingReloadToBay depends on the identity for this, 2803-2807). The next live FlushEntry treats the file as changed by another program. It moves the guest's current disk to a timestamped copy and repoints the bay to it (1147-1206), leaving the original file at the old version Casso itself wrote. DiskHistoryTests has ReplayingAPowerCycleWritesNothing (DiskHistoryTests.cpp:182-221) but no test for a replayed reset.

6. Tests miss it. Every disk test installs a flush sink. DiskImage::Flush does not go through that sink, so its writes go to the real filesystem without being counted. In DiskHistoryTests the mount path is the relative "history.nib" (DiskHistoryTests.cpp:457), so such a write lands in the test's working directory.

**035 impact:** 035 makes the master fix direction insufficient. Making RepointBayToFile update DiskImage::m_filePath, or hardening DiskImage::Flush, would aim the write at the right file. It would still write at the wrong time. The 035 bypasses write to the correct path, just at moments the store has said nothing may write:
- during a replay (m_isReplaying, Replayer.cpp:162)
- while the machine runs forward from the past under the hold (m_isFlushHeld, ReverseController.cpp:2005)
- after RemountSlot6Disks returns early under the hold (DiskManager.cpp:661-664)

So the reset flush has to go through DiskImageStore::FlushAllUnlessHeld, exactly as 035 already did for the power cycle (MachineHost.cpp:978-985) and for the motor-off flush (MachineBuilder.cpp:1519-1521). The device-level flush in Disk2Controller has to go. The remount can no longer be treated as the guard that normally makes the device flush a no-op, because under the hold it does not run.

Doing this also gives DiskImageStore::SoftReset (DiskImageStore.cpp:2345-2352) its first production caller. The hold tests already assume it behaves this way (DiskFlushHoldTests.cpp:46-68, 101-118). Once nothing calls DiskImage::Flush on a store-owned image, the stale m_filePath after RepointBayToFile becomes harmless: the store writes entry.path through FlushEntry. It needs no separate fix.

**Proposed fix:** 1. CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:931-948. Make Disk2Controller::SoftReset just call Reset(). Delete the hrFlush and drive locals and the loop that calls m_activeDisk[drive]->Flush(). Rewrite the banner (925-927) along these lines: "//e soft reset clears the controller's hardware state and keeps the disks in the drives. Writing dirty images back belongs to the disk store, which MachineHost::SoftReset calls first, so every safeguard on a write, the reverse-execution hold and the replay flag apply."

2. CassoEmuCore/Shell/MachineHost.cpp:919. At the top of MachineHost::SoftReset, before m_memoryBus->SoftResetAll(), add `m_diskStore->SoftReset();` with a comment matching PowerCycle's (978-983): flush dirty disks through the store before the devices reset, but not while reverse execution holds the disks, nor during a replay of a recorded reset. m_diskStore is always constructed (MachineHost.cpp:41), so the empty-machine test (MachineHostLifecycleTests.cpp:262-273) stays safe. Flushing before the device resets matches PowerCycle. The order cannot lose a write either way: FlushEntry commits a pending flux write (DiskImageStore.cpp:1095), and so does Disk2NibbleEngine::Reset (Disk2NibbleEngine.cpp:500). An equivalent alternative is `hrFlush = m_diskStore->FlushAllUnlessHeld(); IGNORE_RETURN_VALUE (hrFlush, S_OK);` as PowerCycle does it.

3. What the user's Reset does afterward:
- Live: the remount flushes and re-reads as before, and the image is clean when the store flush runs.
- Failed remount (the master case): FlushEntry runs with all of its safeguards. A write-protected image is not written. A repointed bay writes to entry.path, with the identity check. A lost file whose question is outstanding gets the copy saved under the question's own path, with no repoint (1180-1190). The original file is not touched.
- Under the hold, or during replay or running forward from the past: nothing is written.

4. Nothing else in production loses its flush. Disk2Controller::MountDisk (Disk2Controller.cpp:717) has no production caller; the shell and the CLI mount through the store (DiskManager.cpp:466, DebugBatchRunner.cpp:273). After the change, DiskImage::Flush is called only from DiskImage::Eject on the controller's internal m_disks (Disk2Controller.cpp:760, 975). RepointBayToFile does not need to update m_filePath. Adding the write-protect test to DiskImage::Flush (as DiskImage::Eject does at DiskImage.cpp:928) is optional hardening.

5. Fix up the test whose name becomes false: ResetSemanticsTests::SoftResetPreservesDiskMountsAndFlushesDirty (UnitTest/EmuTests/ResetSemanticsTests.cpp:266-279). See the regression tests.

Not this defect, but seen on the same path: when MountFromBytes rejects the re-read bytes, RetireBay has already reset entry.image (DiskImageStore.cpp:2216) unless reverse retention is on (2203-2214). No bay change is emitted on that failure (Mount exits at 620, before 635), so the controller is left pointing at freed memory.

**Regression test:** (Not taken as written: the temp-path, `std::filesystem::exists` and working-directory checks below would read and write real files, which the constitution's test-isolation rule forbids in unit tests. The tests that shipped in `e32b2b68c` use disks with no file behind them and take the dirty bit as the witness: a write around the store has nowhere to go but still clears the bit, so a disk still dirty where nothing may write it shows that nothing wrote it; tasks.md T066. The two store tests mount with `MountFromBytes` and an empty path, `ReplayingAResetWritesNothing` through a new private `MountWithoutFile` helper in place of the scratch path and `PrepareRigLoop` path parameter below, and the reset-semantics test marks a bare controller's disks loaded, the first dirty, with `SetLoadedForTest`, with no store at all. The replay test also checks two things the steps below do not: the keyframe the forward seek starts from is before the reset, and `ReverseSessionRig::Checksum` at the end of the replay equals the live end's. There is no heat-rebuild case, because a heat rebuild reaches the same `Disk2Controller::SoftReset` through the same `Replayer::RunTo`.) Primary: extend UnitTest/EmuTests/DiskHistoryTests.cpp with TEST_METHOD (ReplayingAResetWritesNothing), modeled on ReplayingAPowerCycleWritesNothing (DiskHistoryTests.cpp:182-221).

Setup: give PrepareRigLoop a path parameter (existing callers keep "history.nib", DiskHistoryTests.cpp:457). Mount this test's disk at a scratch path, (std::filesystem::temp_directory_path() / "casso_reset_replay.nib").string(), following the ScratchPath precedent at DiskImageStoreTests.cpp:765-768. Remove the scratch file at the start and again before the asserts. The image reader and identity reader are seams, so the file need not exist, and with the counting flush sink installed, nothing the store writes reaches it.

Steps:
- controller.Start, then RunCycles (s_kDiskWarmupCycles).
- Assert the image is dirty.
- machine.RecordInput (InputKind::Reset, 0, 0, {}), then machine.SoftReset(), then RunCycles (s_kDiskAfterCycles).
- Record liveEnd and liveCount.
- SeekToPosition (GetOldestPosition), then SeekToPosition (liveEnd).

Asserts:
- `Assert::AreEqual<size_t> (1, liveCount, L"the live reset wrote once, through the store")`
- `Assert::AreEqual<size_t> (liveCount, log.count, L"the replayed reset wrote nothing to the file")`
- `Assert::IsFalse (std::filesystem::exists (scratch), L"no write went around the store")`
- result.outcome == ReverseOutcome::Moved, and the position is back at liveEnd

Before the fix:
- liveCount is 0, because Disk2Controller::SoftReset's DiskImage::Flush wrote the real file instead of the sink.
- The scratch file exists, written by both the live reset and the replayed one, since LoadState re-dirties the tracks (DiskImage.cpp:1241-1245).

After the fix:
- The live reset reaches FlushAllUnlessHeld with the hold off and writes once through the sink.
- The replayed reset stops at m_isReplaying.
- No real file appears.

Second, the hold path: extend UnitTest/EmuTests/DiskResetRemountHoldTests.cpp with ResetUnderHoldWritesNothingAndKeepsTheWrites.
- Mount the disk at a scratch path with the same seams as RemountUnderHoldWritesNothingAndKeepsTheDisk (lines 69-73).
- Point machine.GetRefs().diskController->SetExternalDisk (0, image) at it.
- Flip a bit, then store.SetFlushHold (true).
- Run manager.RemountSlot6Disks(), then machine.SoftReset(). This is the IDM_MACHINE_RESET order from CpuCommandDispatcher.cpp:69-70.
- Assert writes == 0, image->IsDirty(), and that the scratch file does not exist.

Before the fix, DiskImage::Flush writes the file and clears the dirty bit, so the test fails. After the fix it passes.

Third, no filesystem: change UnitTest/EmuTests/ResetSemanticsTests.cpp:266-279. Rename SoftResetPreservesDiskMountsAndFlushesDirty to SoftResetPreservesDiskMountsAndLeavesTheFlushToTheStore, and after ctrl->SoftReset() add `Assert::IsTrue (ctrl->GetDisk (0)->IsDirty(), L"the controller does not write disks; the store does")`. Before the fix, DiskImage::Flush with no path clears the dirty bit (DiskImage.cpp:993-1005), so the test fails. After the fix it passes.

The existing tests DiskFlushHoldTests.cpp:46-68 and 101-118 (store->SoftReset under the hold and under replay) and DiskImageStoreTests.cpp:503-523 should stay green unchanged.

**Sites:** CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:931-948 (flush loop 940-947, DiskImage::Flush call at 944), CassoEmuCore/Devices/Disk/DiskImage.cpp:981-1009 (Flush: dirty-only gate 991, raw WriteFileAtomically(m_filePath) at 1000), CassoEmuCore/Shell/MachineHost.cpp:919-952 (SoftReset never calls the disk store; compare PowerCycle at 978-985), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2345-2352 (DiskImageStore::SoftReset = FlushAllUnlessHeld, no production caller), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3565-3594 (RepointBayToFile updates entry.path only, 3583; DiskImage::m_filePath stays stale), CassoEmuCore/Debugger/Reverse/Replayer.cpp:493-496 (replayed Reset calls MachineHost::SoftReset during RunTo and PrepareStepHere), CassoEmuCore/Shell/DiskManager.cpp:661-664 (remount skipped under the flush hold, so the reset's SoftReset runs with the held dirty image), CassoEmuCore/Shell/CpuCommandDispatcher.cpp:62-71 (RemountDisks then SoftReset), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:584-644 (Mount exits at 606-617 before MountFromBytes' FlushEntry at 263-269)

## head-clamp-139 [still-present]

**Evidence:** The defect is unchanged in the worktree at 811a6f727. Only the line numbers moved, and 035 added three new sites that read the same constant.

1. The stop is still a fixed constant: Disk2Controller.h:43 `static constexpr int kMaxQuarterTrack = 139;`.
2. The only movement path still clamps to it. In Disk2Controller.cpp:423-434, HandlePhase adds the stepper delta, then `if (m_quarterTrack > kMaxQuarterTrack) m_quarterTrack = kMaxQuarterTrack;`, then pushes the value into the engine at :436. UpdateEngineSelection (:500) also forwards m_quarterTrack. The bump checks at :447 and :465 test the raw target against the same 139.
3. 035 added a state restore that writes m_quarterTrack (LoadState, Disk2Controller.cpp:1143-1218). It cannot get past the stop: :1198 `CBREx (quarterTrack <= kMaxQuarterTrack, ...)` rejects anything above 139 before :1204 assigns it. SaveState (:1101) stores the head as one Byte. GetDiagnostics (:1077) reports kMaxQuarterTrack as the debugger head view's range, and DiskHeadView.h:73 sizes its ruler from it (max + 1 = 140 positions). The only other write is the reset to 0 at :902.
4. The layers below the controller allow more. The engine clamps at 159 (Disk2NibbleEngine.h:55 `kMaxTrack = 159`; Disk2NibbleEngine.cpp:134-159 says 0..159; its own LoadState check is at :982). DiskImage has `kMaxTracks = 40` and `kQuarterTrackCount = 160` (DiskImage.h:102, 106), and its comment at :187-189 says the head steps over 0..159. The default map sends every quarter track to qt / 4 (DiskImage.cpp:59-72). The WOZ loader walks all 160 TMAP entries (WozLoader.h:54; WozLoader.cpp:701-806) and calls SetQuarterTrackSlot for each one, FLUX entries included (:751-773). Data at quarter tracks 140-159 loads, but the head never reaches it.
5. Parity side effect, confirmed. Every stepper delta is even (Disk2Controller.cpp:394-400, :419). From 138, a +2 step clamps to the odd 139. Every later inward step then lands on an odd quarter track (137, 135, ...) until the head hits the clamp at 0. On a WOZ whose TMAP leaves odd positions unmapped, the guest reads unformatted noise there.
6. Existing tests pin only `<= kMaxQuarterTrack` (Disk2Tests.cpp:377-399, comment at :384 "clamp at quarter-track 139"). Nothing tests reaching track 35-39, and nothing tests parity at the stop. The bump tests in Disk2ControllerEventTests.cpp:269 and Disk2ControllerAudioTests.cpp:160 cover track 0 only.
7. DriveWidget has its own rail constant, DriveWidget.h:231 `kMaxQuarterTrack = 139`. GetHeadCoreCenterX (DriveWidget.cpp:330-342) clamps to it and was written expecting positions up to the engine's 159 (comment at :317-322; tested over the engine's whole range in DriveWidgetHitTests.cpp:332-378). The 2D rail is therefore safe either way.

**035 impact:** 035 changes nothing in the clamp or the stepper; the fix is still one constant in Disk2Controller.h. 035 does add three readers of that constant, and each one is safe or adjusts by itself once the stop is raised:

- **LoadState check (Disk2Controller.cpp:1198):** it follows the constant automatically. Saves made before the fix (head at 139 or less) still load, and a head at 158 still fits SaveState's one Byte (:1101), so kStateVersion (Disk2Controller.h:195) does not need a bump.
- **Debugger head view:** the diagnostics snapshot (:1077) and DiskHeadView (DiskHeadView.h:73, DiskHeadView.cpp:344/363/432) scale their ruler and head width from maxQuarterTrack. They go from 140 to 159 positions with no code change. The view tests (DiskHeadSweepTests, DiagnosticsVisualsTests, ColorLegendTests, DebuggerViewStateTests) build their own DiagnosticsDiskHead with a literal 139, so they are unaffected. DiagnosticsProviderTests.cpp:153 compares against the constant, so it still passes.

None of the flush hold, the journaling of guest writes, or the new pause/step machinery touches head travel, so the fix does not interact with them.

Do not raise the stop to the engine's 159. That value is odd, and because every stepper delta is even, an odd stop brings back the off-detent problem described in evidence item 5. The stop has to be even.

The 2D DriveWidget rail (DriveWidget.h:231) is a separate 35-track scale and already parks positions past 139 at its end. It needs no change for correctness; whether the rail should widen to 40 tracks is a visual call for the owner.

The drive-signature packing (EmulatorShellPresent.cpp:672-681) uses 8 bits, so 158 still fits; only its "runs to 139" comment goes stale.

**Proposed fix:** In CassoEmuCore/Machines/Apple2/Common/Disk2Controller.h:43, take the stop from the image geometry and make it the last half-track detent the 160-entry quarter-track map covers, not 139:

    // The head's outer travel stop. The quarter-track map covers 0..159
    // (40 tracks), and every stepper move is an even number of quarter tracks,
    // so the stop is the last EVEN position: clamping to an odd one would leave
    // the head between detents after a bump until it recalibrated at track 0.
    static constexpr int    kMaxQuarterTrack = DiskImage::kQuarterTrackCount - 2;   // 158, track 39.5

and, below the class or next to the constant:

    static_assert (Disk2Controller::kMaxQuarterTrack <= Disk2NibbleEngine::kMaxTrack, "the head stop must lie inside the engine's range");
    static_assert ((Disk2Controller::kMaxQuarterTrack & 1) == 0,                      "the head stop must be a half-track detent");

DiskImage.h and Disk2NibbleEngine.h are already included at Disk2Controller.h:10-11. HandlePhase, the two bump checks, LoadState and GetDiagnostics all read the constant, so the change needs nothing else in Disk2Controller.cpp.

Comment updates only, no behavior change:
- EmulatorShellPresent.cpp:672: "runs to 139" becomes "runs to 158".
- DriveWidgetState.h:66: say 0 to Disk2Controller::kMaxQuarterTrack, not 0 to 139.
- Disk2Tests.cpp:384: the comment says 139.
- DriveWidget.cpp:317-322: the positions past the rail now come from the controller's stop at 158, no longer from the engine's 159.

Leave DriveWidget::kMaxQuarterTrack (DriveWidget.h:231) at 139 unless the owner wants the 2D rail widened. Its clamp already keeps the core on the rail.

Not in scope:
- Sector images (.dsk/.po/.nib) still have only 35 slots, so tracks 35-39 resolve to -1 (DiskImage.cpp:119-122) and read as blank surface. That is the right physical result for a 35-track disk.
- Making the stop depend on the mounted image would be wrong: the stop belongs to the drive, not the media.

**Regression test:** Extend UnitTest/EmuTests/Disk2Tests.cpp, next to HeadStepWrapsAtTrackBoundaries (:377), with two tests that use the file's existing helpers (s_kSlot6Base, WriteNibbleToImage, the LSS_ReadsKnownNibblePattern sequence at :401-438).

1. **TEST_METHOD (HeadReachesTrack39AndReadsIt)**
   - Setup: `disk = make_unique<Disk2Controller> (6); img = disk->GetDisk (0);`, then `img->EnsureTrackSlots (DiskImage::kMaxTracks); img->ResizeTrack (39, s_kSyntheticTrackBytes * s_kBitsPerNibble);`. The default whole-track map already sends qt 156 to slot 39.
   - Seek: motor on, then step outward 78 half-steps from track 0 using the PhaseSeekAlwaysRestsOnEvenQuarterTrack loop (:162-176, `phase = (step + 1) & 3`, on then off).
   - Assert: `disk->GetQuarterTrack() == 156`, `disk->GetEngine (0).GetCurrentTrack() == 156`, and `img->ResolveQuarterTrack (156) == 39`.
   - Then: Q7 off, Q6 off, `Tick (kMotorSpinupCycles)`, write D5 AA 96 at the engine's bit position on track 39, tick 9 bit cells, and assert `disk->Read (0xC0EC) == 0xD5`.
   - Before the fix, the head stops at 139 (qt / 4 = slot 34, which is empty), so the position assertion fails, and the read would return noise rather than 0xD5. After the fix, both pass.

2. **TEST_METHOD (HeadStaysOnADetentAfterBumpingTheOuterStop)**
   - Walk outward 300 phase steps, as HeadStepWrapsAtTrackBoundaries does.
   - Assert `GetQuarterTrack() == Disk2Controller::kMaxQuarterTrack` and `(GetQuarterTrack() & 1) == 0`.
   - Step inward one half-track by energizing the phase one below the last one energized (last phase = 299 & 3 = 3, so read 0xC0E5 then 0xC0E4). Assert the head is at kMaxQuarterTrack - 2 and still even.
   - Before the fix, the stop is 139 and the step lands on 137, so both parity assertions fail. After the fix, 158 then 156.

Optionally, in UnitTest/Devices/Disk2ControllerEventTests.cpp next to PhaseChange_pastTrack0_firesHeadBumpNotHeadStep (:269), add an outer-stop counterpart: walk to the stop, energize one more outward phase, and assert exactly one HeadBump with atQt == Disk2Controller::kMaxQuarterTrack and no HeadStep.

Gate: the full suite, which includes the boot and readback gates (GameBootTests, DiskReadbackTests). A title that relied on the old stop at track 35 would show up there.

**Sites:** CassoEmuCore/Machines/Apple2/Common/Disk2Controller.h:43, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:423-436, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:447, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:465, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:1077, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:1101, CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:1198, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.h:55, CassoEmuCore/Devices/Disk/DiskImage.h:102, CassoEmuCore/Devices/Disk/DiskImage.h:106, CassoEmuCore/Machines/Apple2/Common/WozLoader.cpp:701-806, CassoEmuCore/Ui/Chrome/DriveWidget.h:231, CassoEmuCore/Shell/EmulatorShellPresent.cpp:672, CassoEmuCore/Ui/DriveWidgetState.h:66, UnitTest/EmuTests/Disk2Tests.cpp:377-399

## commit-not-durable [still-present]

**Evidence:** The function is byte-for-byte the same as on master. I compared the two bodies line by line and found no differences. Only the line numbers moved: 1208 on master, 1279 in the worktree.

Emulator commit, DiskImageStore::WriteFileAtomically (CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1279-1372):
- It picks a sibling temporary (1306-1317).
- CreateFileW opens it with CREATE_ALWAYS and FILE_ATTRIBUTE_NORMAL (1337-1338).
- One WriteFile writes the bytes (1343-1344). CloseHandle follows (1348). No FlushFileBuffers anywhere.
- fs::rename(tempPath, path, ec) replaces the target (1357).
- In the installed STL, fs::rename is MoveFileExW(MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING), with no MOVEFILE_WRITE_THROUGH (VC/Tools/MSVC/14.51.36231/crt/src/stl/filesystem.cpp:707-709).
- The comment at 1354-1356 states only "old or new" for a running system.

Callers in the worktree:
- FlushEntry (DiskImageStore.cpp:1235)
- SetImageWriteProtect (1474)
- SalvageToFile (1731)
- WritePreserved (3791)
- DiskImage::Flush (CassoEmuCore/Devices/Disk/DiskImage.cpp:1000), reached from Disk2Controller::SoftReset (CassoEmuCore/Machines/Apple2/Common/Disk2Controller.cpp:944)

The recovery copy is also written without a flush: TryWriteRecoveryImage, CreateFileW/WriteFile/CloseHandle at DiskImageStore.cpp:1033-1041.

CLI and Cassque commit, DiskImageSession::Commit (CassoEmuCore/Devices/Disk/DiskImageSession.cpp:629-634):
- It writes the temporary through Win32DiskFileIo::WriteAllBytes (CassoEmuCore/Seams/Win32DiskFileIo.cpp:78-123). That function never calls FlushFileBuffers. The only flush in the file is at :466, inside the Debug-only CASSO_DIAG_DISK_ABORT hook.
- It then calls ReplaceAtomically (:232-233) with MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH. That flag guarantees a flush only for a copy-and-delete move, not for a rename within one directory. So the CLI has the same power-loss gap, and the asymmetry claimed on master is only nominal.
- The comment at Win32DiskFileIo.cpp:479-482 says the temporary is "complete and flushed" before the replace. That is not true.

The same pattern appears in Win32FileSystem::WriteAllText (CassoEmuCore/Config/Win32FileSystem.cpp:126-155) for settings files.

Metadata: every path creates a fresh FILE_ATTRIBUTE_NORMAL temporary and moves it over the target. The original's explicit ACL, attributes, alternate data streams and creation time are lost on every successful commit.

What 035 added does not touch the commit primitive. The flush hold (DiskImageStore.h:133-148; FlushAllUnlessHeld at DiskImageStore.cpp:1893-1906) only gates when FlushAll runs. CommitHeldWrites (1970-1973) still goes FlushAll -> FlushEntry -> WriteFileAtomically.

**035 impact:** The flush hold must be left alone, and the fix goes below it. The hold is:
- declared at DiskImageStore.h:133-148
- checked in FlushAllUnlessHeld at DiskImageStore.cpp:1899
- turned on at ReverseController.cpp:2005 and turned off at ReverseController.cpp:2070, :98 and :142

It only controls whether a spin-down, reset or power-cycle flush happens. Every commit that does happen ends in WriteFileAtomically. That includes CommitHeldWrites (1970-1973), eject, FlushAllForShutdown (EmulatorShell.cpp:299) and the write-protect toggle. So durability belongs in the commit primitive, and the fix must not add a new flush trigger that would get around the hold.

The hold makes this defect more costly. While reverse execution holds the disks, a whole session of guest writes reaches the file in one commit, when the hold is released or at exit. Losing that one commit loses all of them.

Thread cost: the spin-down commit runs on the CPU thread inside Tick (MachineBuilder.cpp:1513-1521). FlushFileBuffers forces a device cache flush. On an SSD that is a few milliseconds. On a hard disk, USB stick or network share it can be tens to hundreds of milliseconds, which shows up as an emulation and audio hitch at each spin-down after a write. The write was already synchronous there; the flush adds device latency on top. Measure it. If it is too long, move the spin-down commit off the CPU thread rather than dropping the flush.

Disk2Controller::SoftReset (Disk2Controller.cpp:931-947) calls DiskImage::Flush, which calls the static WriteFileAtomically (DiskImage.cpp:1000). That path goes around the store, its IDiskFileIo seam and the hold; it is the separate defect softreset-second-write-path. So the static WriteFileAtomically must itself become durable, not only a member path that goes through the seam.

The removed StepInstructionWhilePaused and the new pause and step code are not involved.

**Proposed fix:** Give both commit paths one durable sequence: write the temporary, flush it to stable storage, then replace with write-through.

1. Seam. In IDiskFileIo (CassoEmuCore/Devices/Disk/IDiskFileIo.h, after WriteAllBytes at :68), add `virtual HRESULT FlushToStorage (const std::string & path) = 0;`. Document that it returns only once the file's bytes and size are on stable storage, and that a commit calls it on the temporary before ReplaceAtomically, because the rename can reach the disk before the data.
   - Implement it in Win32DiskFileIo: CreateFileW(GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, OPEN_EXISTING), then FlushFileBuffers. Capture GetLastError before CloseHandle, then CWR. FlushFileBuffers flushes the file's cached data whichever handle wrote it.
   - Keep ReplaceAtomically as MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH).
   - Add the method to FakeDiskFileIo (see the test) and to the RigFileIo stub in SharedImageTests.cpp:53-70, returning E_NOTIMPL there; it is never reached because the rig installs a flush sink.

2. CLI and Cassque. In DiskImageSession::Commit (DiskImageSession.cpp:629-634), between WriteAllBytes and ReplaceAtomically, add `hr = m_fileIo.FlushToStorage (tempPath); CHRF (hr, RefuseCommit (opened.imagePath, DescribeTemporaryWriteFailure (hr), result));`. progress.furthestAttempted is still WriteTemporary at that point, so CommitPlan::ShouldRemoveTemporary already removes the temporary on a failed flush. No CommitPlan change is needed.

3. Emulator. Replace the private CreateFileW/WriteFile/CloseHandle block (DiskImageStore.cpp:1329-1352) and fs::rename (1357) with the same sequence over an IDiskFileIo.
   - Add a static `CommitThroughFileIo (IDiskFileIo & io, const string & path, const vector<Byte> & bytes)`. It tries names from GetCommitTemporaryPath with io.Exists until one is free, then calls io.WriteAllBytes(temp), io.FlushToStorage(temp) and io.ReplaceAtomically(temp, path), and calls io.Remove(temp) on any failure.
   - WriteFileAtomically keeps its signature, for DiskImage::Flush and the existing tests, and calls CommitThroughFileIo with a local Win32DiskFileIo.
   - FlushEntry (1235), SetImageWriteProtect (1474), SalvageToFile (1731) and WritePreserved (3791) call it with *m_fileIo when one is installed. Production installs Win32DiskFileIo at DiskManager.cpp:967-970. Otherwise they fall back to WriteFileAtomically.
   - This drops MOVEFILE_COPY_ALLOWED, which could turn a cross-volume replace into a non-atomic copy, and keeps the real Win32 code traveling, since both CWR.

4. In TryWriteRecoveryImage (DiskImageStore.cpp:1037-1041), call FlushFileBuffers before CloseHandle and capture its error the same way. That file may be the session's only lossless copy.

5. Correct the comments that state more than the code does: DiskImageStore.cpp:1354-1356, Win32DiskFileIo.cpp:209-211 and :479-482, and IDiskFileIo.h:76-77.

Leave for separate follow-ups:
- Keeping ACLs, attributes and streams with ReplaceFileW. It requires an existing target, so salvage and preserved copies need a MoveFileExW fallback, and the gap is identical on the CLI.
- Win32FileSystem::WriteAllText (Config/Win32FileSystem.cpp:126-155).

**Regression test:** A real power cut cannot be tested. The cache manager serves reads of data that was never flushed, and the project rules forbid system calls in unit tests. So the tests model the power loss behind the seam.

1. Extend UnitTest/EmuTests/FakeDiskFileIo.h:
   - Add a `durable` map and an ordered `operations` log of (verb, path) pairs.
   - WriteAllBytes updates `files` and sets durable[path] to a zero-filled vector of the same size. NTFS logs the size but not the data.
   - FlushToStorage copies files[path] into durable[path].
   - ReplaceAtomically moves both entries. The rename is logged metadata, so it survives.
   - SimulatePowerLoss() sets files = durable.

2. UnitTest/EmuTests/DiskFailureModeTests.cpp, beside AWriteProtectedImage_RefusesThePutAndLeavesAMountableImageUnchanged (:506): add APowerCutRightAfterThePut_LeavesTheImageWhollyOldOrWhollyNew.
   - SeedFile kBlankDsk, run MakePutOptions(kBlankDsk, "PROG") through DiskCommandRunner, then call io.SimulatePowerLoss().
   - Assert that io.files[kBlankDsk] equals either the seed or the committed image, never zeros.
   - Assert that `operations` holds Write(temp), Flush(temp), Replace(temp -> kBlankDsk), in that order.
   - Before the fix, Commit never flushes, so the image comes back zero-filled and the test fails.

3. UnitTest/EmuTests/DiskImageStoreTests.cpp, beside WriteFileAtomically_ReplacesTargetAndLeavesNoTempBehind (:782) and Flush_ToRealFile_WritesThroughAtomicPath (:834): add Flush_ThroughFileIo_FlushesTheTemporaryBeforeTheReplace.
   - Use a DiskImageStore with store.SetFileIo(&fakeIo) and no flush sink. MountFromBytes at a path that does not exist, such as Z:/casso-fake/flush.dsk, with MakeDsk(0x24). Flip a bit as at :846-847, call store.Flush(kSlot, kDrive), then fakeIo.SimulatePowerLoss().
   - Assert that fakeIo.files holds the full-size image (NibblizationLayer::kImageByteSize), that the op log is Write, Flush, Replace on GetCommitTemporaryPath(target, 0), and fakeIo.HasNoTemporaryFiles().
   - Before the fix, the store goes around the seam and writes the real filesystem directly. The fake records nothing, and the missing folder fails the flush with ERROR_PATH_NOT_FOUND, so the test fails. After the fix it passes.
   - (Not taken as written: the run against the code before the fix calls CreateFileW and fs::rename on the real path, which the constitution's test-isolation rule forbids in unit tests, and its result depends on whether that root exists on the machine, a mapped Z: drive for one. The plan writes the test with the fix and checks it against R3's mutations of the fixed code instead, each of which fails it, all on the fake, and lists it among the SC-001 exceptions; tasks.md T059, and T065 for the recovery-copy tests that reach TryWriteRecoveryImage's fs::exists and CreateFileW the same way.)

4. Win32DiskFileIoTests.cpp, which already uses real scratch files: add FlushToStorage_OfAMissingFile_ReportsFileNotFound_NotEFail and a round trip after FlushToStorage. These cover the Win32 method's error code. They are not a durability check. (The class moves from UnitTest to ScenarioTests with these cases, because unit tests may not touch real files; tasks.md T057.)

**Sites:** CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1337-1348 (CreateFileW/WriteFile/CloseHandle, no FlushFileBuffers), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1357 (fs::rename = MoveFileExW COPY_ALLOWED|REPLACE_EXISTING, no WRITE_THROUGH), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1354-1356 (comment promising old-or-new only), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1235 (FlushEntry caller), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1474 (SetImageWriteProtect caller), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1731 (SalvageToFile caller), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3791 (WritePreserved caller), CassoEmuCore/Devices/Disk/DiskImage.cpp:1000 (DiskImage::Flush caller, reached from Disk2Controller.cpp:944), CassoEmuCore/Devices/Disk/DiskImageStore.cpp:1033-1041 (TryWriteRecoveryImage, no flush), CassoEmuCore/Seams/Win32DiskFileIo.cpp:94-120 (WriteAllBytes, no FlushFileBuffers), CassoEmuCore/Seams/Win32DiskFileIo.cpp:232-233 (ReplaceAtomically, WRITE_THROUGH does not flush a same-volume rename), CassoEmuCore/Seams/Win32DiskFileIo.cpp:479-482 (comment wrongly says the temporary is flushed), CassoEmuCore/Devices/Disk/DiskImageSession.cpp:629-634 (CLI/Cassque commit: write then replace, no flush), CassoEmuCore/Devices/Disk/IDiskFileIo.h:68,76-79 (seam has no durability step), CassoEmuCore/Config/Win32FileSystem.cpp:126-155 (same pattern for settings, out of scope)

## relative-disk1-no-watch [still-present]

**Evidence:** The defect is still present in the worktree at 811a6f727. Most line numbers have moved since master. No code in Gui, Shell, Devices/Disk, Seams or Cli makes a command-line disk path absolute: the only fs::absolute calls in CassoEmuCore are at Cli/ArtifactWriter.cpp:645-646, and they are unrelated. Nothing calls SetCurrentDirectory either.

How the path flows:
- CassoCore/CommandLineParser.cpp:4678-4679 copies the argv string into parsed.disk1/disk2 unchanged.
- GuiMain.cpp:704-705 copies it again. GuiMain.cpp:429-442 only runs fs::exists on it. GuiMain.cpp:813-815 passes fs::path(disk1Path).string() to EmulatorShell::Initialize.
- EmulatorShell.cpp:626 calls DiskManager::MountCommandLineDisks. DiskManager.cpp:321-322 copies the path into resolvedDisk1/2. DiskManager.cpp:402-412 calls MountDiskInSlot6, which reaches m_diskStore.Mount at DiskManager.cpp:466.
- DiskImageStore::Mount (DiskImageStore.cpp:584-644) calls MountFromBytes at :619. MountFromBytes stores `entry.path = virtualPath` unchanged (:272). Mount then calls BeginWatching at :630.
- BeginWatching (DiskImageStore.cpp:3613-3633) calls MountedImageState::GetDirectory(entry.path) at :3616. For "game.dsk", find_last_of returns npos, because the path has no separator, so GetDirectory returns "" (MountedImageState.cpp:224-237). The `!directory.empty()` guard at DiskImageStore.cpp:3621 skips m_watcher->Watch, and :3630 records SetWatching(false).
- Nothing in production reads IsWatching (MountedImageState.h:81), so there is no fallback.
- The comment at MountedImageState.cpp:218-221 is still wrong for this case: for a bare name, the working directory IS where the image is.
- A relative path that contains a separator (for example "disks\game.dsk" or ".\game.dsk") IS watched. Win32ImageWatcher.cpp:314-316 reports `fs::path(directory) / name`, and IsSamePath matches that form. So the miss is specific to a bare filename.

Intent channel (the part verified as overstated holds the same way here). CassoCli does not send an absolute path: it sends the path exactly as typed.
- DiskCommandRunner.cpp:3828-3832 and ImageArtifactSink.cpp:258-261 pass options.*.imagePath to StateIntent. Win32IntentChannel::StateIntent and Encode (Win32IntentChannel.cpp:427-450, 238-248) do not normalize it.
- The receiver passes payload.imagePath straight to NoteExternalChange (EmulatorWindow.cpp:2786-2789).
- NoteExternalChange matches with IsSamePath (DiskImageStore.cpp:2707), which folds only case and '/' (MountedImageState.cpp:187-206).
- Result: a relative path on both sides from the same working directory matches only by accident. Any other written form is dropped. That includes absolute vs relative, ".\x" vs "x", and a build run from another directory.
- The reply tracker has the same dependency: TryTakeReload compares paths at EmulatorShellDisks.cpp:366 and IntentReplyTracker.cpp:127-147.

Further consequences found in the worktree:
- **Next launch looks in the wrong folder.** The relative path is saved to prefs (DiskManager.cpp:473-477 -> DiskSettings.cpp:223). MakeExeRelativePath leaves a non-absolute input unchanged (PathResolver.cpp:277). On the next launch, ReadSavedDiskPath (DiskSettings.cpp:182) calls ResolveExeRelativePath, which joins it to the EXE directory (PathResolver.cpp:318-320), not the original working directory. The remembered disk is then cleared as missing (DiskManager.cpp:334-344, GuiMain.cpp:318-324). If a file with that name sits next to Casso.exe, a different disk is mounted silently.
- **Recent-disks list.** EmulatorShellDisks.cpp:230 adds the relative path to the MRU, although GlobalUserPrefs.cpp:1252 documents recentDisks as absolute.
- **One file can go into both drives.** `--disk1 a.dsk --disk2 .\a.dsk` passes both duplicate checks: DiskManager.cpp:384-385 and IsFileInAnotherBay at DiskImageStore.cpp:606/558 each compare only case and separators. The same file then lands in both bays, which is the double-flush hazard the comment at DiskImageStore.cpp:596-601 describes.
- **Data is still protected.** The pre-write identity check at DiskImageStore.cpp:1131-1147 still catches the conflict before an overwrite, so the user gets a stale disk and later a conflict or rescue prompt instead of an automatic reload.

**035 impact:** **What 035 does not change.** The flush hold plays no part. The fix sits upstream of the store: the path is made absolute before DiskImageStore::Mount ever receives it. The hold checks are left as they are:
- RemountSlot6Disks still returns early under IsFlushHeld (DiskManager.cpp:661-664).
- Mount still flushes an occupied bay through FlushEntry (DiskImageStore.cpp:265).

The new step and pause machinery is not involved.

**What 035 adds that makes the fix more important and shows where it goes.**
- **Machine-state files keep the relative path.** 035's state file records the bay path with GetSourcePath (MachineStateFile.cpp:75). MountRestored replays it (MachineStateFile.cpp:562 -> DiskImageStore.cpp:664-705), and MountRestored's watch at :692-698 is skipped for a bare name the same way. A state saved from a relative --disk1 and loaded from another working directory points the bay, and its later flushes, at the wrong file. The fix therefore has to make the path absolute before the store records it. A watch-only fallback (watching cwd when GetDirectory is empty) would fix the watch and leave every persisted form relative, so it is the wrong fix.
- **Store-level absolutizing breaks the test seams.** MountFromBytes treats the path as an opaque round-trip identifier (DiskImageStore.cpp:207-209), and the read, identity and flush seams in tests are keyed by that string. Rewriting the path inside the store would break those seams. The shell edge (DiskManager) is the right layer.
- **Paths kept between mounts follow automatically.** MachineManager.cpp:417-418/606 and RemountSlot6Disks (DiskManager.cpp:668) re-feed GetSourcePath. Once the first mount is absolute, these stay absolute, and applying the conversion again is harmless.
- **A precedent exists.** 035's DebugSession::ResolvePath (DebugSession.cpp:882-918) already resolves relative debugger paths against the working directory. It is a session member with its own CD state, so it is not the helper to reuse.
- **The sender must change in the same commit.** Today a relative path on both sides from one directory matches by accident (DiskImageStore.cpp:2707). Making only --disk1 absolute would break that case, because CassoCli would still send "game.dsk" against an absolute bay path. DiskCommandRunner::AnnounceIntent and ImageArtifactSink must send the absolute path too.

**Proposed fix:** 1. **Add a helper.** In CassoEmuCore/Core/PathResolver.h/.cpp, add a class static beside MakeExeRelativePath, with EHM-exempt pure style:
   `static std::string MakeAbsolutePath (const std::string & path);`
   - The plan takes this with an explicit base, `MakeAbsolutePath (path, baseDirectory)`, so the unit tests do not depend on the test process's working directory; the shell reads the real working directory in `EmulatorShell::InitAssetPathsAndStores`, and the CLI senders read it through the `IDiskFileIo` seam (`GetWorkingDirectory`, whose `Win32DiskFileIo` version calls `PathResolver::GetWorkingDirectory()`), so `CassoCli` stays code-free and no unit test reads it (contracts/internal-interfaces.md, Paths; tasks.md T073).
   - Empty input returns empty.
   - Otherwise it returns `fs::absolute (fs::path (path), ec).string()`, or the input unchanged if ec is set.
   - On MSVC, fs::absolute goes through GetFullPathNameW. That collapses "." and "..", and resolves drive-relative "C:x" and rooted "\x" correctly, which a hand join to current_path does not.
   - (Not taken: `fs::absolute` and `GetFullPathNameW` resolve against the process's working directory, which the explicit base replaces. The plan's helper resolves against `baseDirectory`, rooted forms and drive-relative forms on the base's drive against that drive, and normalizes the result; a drive-relative path on another drive comes back unchanged, because only that drive's working directory, process state, could resolve it; per the contract; tasks.md T073.)

2. **Use it at the shell edge.** In DiskManager::MountCommandLineDisks (DiskManager.cpp:321-322), change the two copies to:
   `std::string  resolvedDisk1 = PathResolver::MakeAbsolutePath (disk1Path);`
   `std::string  resolvedDisk2 = PathResolver::MakeAbsolutePath (disk2Path);`
   This covers the --disk1/--disk2 launch (EmulatorShell.cpp:626) and the machine switch's remount (MachineManager.cpp:606). The saved-prefs branch at :326-368 already yields absolute paths. After this one change, everything downstream gets the absolute form:
   - the bay path (DiskImageStore.cpp:272)
   - the watch directory (:3616)
   - the duplicate checks (DiskManager.cpp:384-385, DiskImageStore.cpp:606)
   - the saved prefs (DiskManager.cpp:473-477, so MakeExeRelativePath now works as designed)
   - the MRU (EmulatorShellDisks.cpp:230)
   - the machine-state file (MachineStateFile.cpp:75)

3. **Send the absolute path from the CLI.** At DiskCommandRunner.cpp:3831 and ImageArtifactSink.cpp:260, pass `PathResolver::MakeAbsolutePath (options.disk.imagePath)` and `PathResolver::MakeAbsolutePath (options.imagePath)` to StateIntent. Only the stated path changes; the runner's own file I/O keeps using the path as typed. CassqueWindow and DiskOperations already send browser paths, which are absolute. Doing this in the callers rather than inside Win32IntentChannel::StateIntent keeps it reachable through FakeIntentChannel. An extra call in StateIntent before Encode (Win32IntentChannel.cpp:429) is harmless if wanted.

4. **Rewrite the comment at MountedImageState.cpp:218-221.** It should say that every production mount arrives absolute, because DiskManager makes the command line's paths absolute. A path with no separator is therefore only a test's virtual path, with no directory to watch.

5. **Optional.** Apply the same helper at GuiMain.cpp:704-705, so the "Disk image not found" dialog at GuiMain.cpp:434-435 shows the full path. It is not needed for correctness.

Keep to the copilot-instructions rules for new code: the helper is a class static, no std <> includes in the .cpp, and insert it before the `////` banner of the next function.

**Regression test:** (Not taken as written: every `fs::absolute (...)` and `fs::current_path()` below would tie the tests to the test process's working directory. The plan's tests pass a synthetic base directory, `"C:\\work"`, and compare against paths built from it, for example `"C:\\work\\relative.nib"`; tasks.md T072. The cases themselves are kept.)

**(A) Emulator side.** Extend UnitTest/EmuTests/DiskResetRemountHoldTests.cpp, the only existing rig that builds a real DiskManager, and add `#include "FakeImageWatcher.h"`.

New `TEST_METHOD (ARelativeCommandLineDiskIsMountedByItsFullPathAndWatched)`:
- Declare `FakeImageWatcher watcher;` BEFORE `TestMachine machine ("Apple2e");`. The store keeps a raw pointer, so the watcher must outlive the machine's teardown ejects.
- Install the seams as the existing test does: SetImageReader returns MakeImage(), SetIdentityReader returns ImageIdentity(), and SetFlushSink counts writes. Then call `store.SetImageWatcher (&watcher)`.
- Build the DiskManager with the same arguments, and call `manager.MountCommandLineDisks ("relative.nib", "")`.
- Assert:
  - `MountedImageState::IsSamePath (store.GetSourcePath (6, 0), fs::absolute ("relative.nib").string())`
  - `watcher.watched.size() == 1`, and `IsSamePath (watcher.watched[0], fs::absolute ("relative.nib").parent_path().string())`
  - `store.GetSharedState (6, 0)->IsWatching()`
- Then call `watcher.Fire (watcher.watched[0], fs::absolute ("relative.nib").string())` and assert `store.GetSharedState (6, 0)->GetPending().seen`.

Before the fix: the source path is "relative.nib", `watched` is empty, and IsWatching is false. After the fix, all pass.

Second method, `TwoWrittenFormsOfOneFileGoInDriveOneOnly`: call `MountCommandLineDisks ("dup.nib", ".\\dup.nib")` and assert that `GetImage (6, 1) == nullptr` and that drive 0 is mounted. Before the fix both bays mount the same file.

**(B) Sender side.** Extend UnitTest/EmuTests/IntentChannelTests.cpp. New `TEST_METHOD (ARelativeImagePathIsAnnouncedByItsFullPath)`:
- Build MakePut(ExternalChangeIntent::Restart) and set `options.disk.imagePath = "Loader.dsk"`.
- Seed the FakeDiskFileIo with the files and stamps keyed "Loader.dsk", plus kHostFile.
- Run with a FakeIntentChannel.
- Assert `channel.stated.size() == 1` and `channel.stated[0].imagePath == fs::absolute ("Loader.dsk").string()`.

This fails before the fix, because the stated path is "Loader.dsk". The existing AStatedIntentIsAnnouncedAfterAWriteSucceeds stays green, since kImagePath is already absolute. A matching case for the assembler path can be added to UnitTest/AssemblerToDiskTests.cpp beside its changeIntent tests at :196/:239.

**(C) Helper.** Extend UnitTest/EmuTests/PathResolverTests.cpp with MakeAbsolutePath cases:
- empty input stays empty
- "C:\\x\\a.dsk" comes back unchanged
- "a.dsk" becomes fs::current_path() / "a.dsk"
- ".\\a.dsk" and "sub\\..\\a.dsk" collapse to the same result as "a.dsk"

**Sites:** CassoEmuCore/Gui/GuiMain.cpp:704-705, CassoEmuCore/Gui/GuiMain.cpp:429-442, CassoEmuCore/Gui/GuiMain.cpp:813-815, CassoEmuCore/Shell/EmulatorShell.cpp:626, CassoEmuCore/Shell/DiskManager.cpp:321-322, CassoEmuCore/Shell/DiskManager.cpp:384-385, CassoEmuCore/Shell/DiskManager.cpp:466, CassoEmuCore/Shell/DiskManager.cpp:473-477, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:272, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:606, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:630, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:3613-3633, CassoEmuCore/Devices/Disk/DiskImageStore.cpp:2707, CassoEmuCore/Devices/Disk/MountedImageState.cpp:218-237, CassoEmuCore/Devices/Disk/DiskCommandRunner.cpp:3831, CassoEmuCore/Cli/ImageArtifactSink.cpp:260, CassoEmuCore/Seams/Win32IntentChannel.cpp:427-450, CassoEmuCore/Shell/Window/EmulatorWindow.cpp:2786-2789, CassoEmuCore/Config/DiskSettings.cpp:182, CassoEmuCore/Config/DiskSettings.cpp:223, CassoEmuCore/Core/PathResolver.cpp:277, CassoEmuCore/Core/PathResolver.cpp:318-320, CassoEmuCore/Shell/EmulatorShellDisks.cpp:230, CassoEmuCore/Shell/MachineStateFile.cpp:75, CassoEmuCore/Shell/MachineStateFile.cpp:562

## unmapped-qt-writes-discarded [still-present]

**Evidence:** The defect is still present in the worktree. 035 moved the line numbers but did not change the logic. Paths are relative to C:\Users\relmer\source\repos\relmer\Casso-worktrees\041-disk-integrity.

(1) The loader leaves blank tracks with no data. WozLoader::Load clears the map to -1 (CassoEmuCore/Machines/Apple2/Common/WozLoader.cpp:696, which calls CassoEmuCore/Devices/Disk/DiskImage.cpp:309-313). It then skips every $FF entry before SetQuarterTrackSlot (WozLoader.cpp:727-730 for v2, 754-757 for FLUX, 782-785 for v1). A zero-bit record bails out in ParseV2Track (WozLoader.cpp:288) or ParseV1Track (235-238), but its slot is still mapped (745 / 805) and keeps 0 bits.

(2) ResolveQuarterTrack turns both cases into -1 (DiskImage.cpp:119-126): a negative or out-of-range map entry, or a Bits slot with a zero bit count.

(3) The engine caches that -1. ResolveSlot sets m_slot = -1 and m_isFluxSlot = false (Disk2NibbleEngine.cpp:198-203), and StepLss sets hasTrack = (m_slot >= 0) (601).

(4) The write is dropped. `writing = m_writeMode && hasTrack && !IsWriteProtected()` comes out false (Disk2NibbleEngine.cpp:672). The only WriteBit call (692) is inside the `else if (hasTrack)` branch (688-701). Control falls through to the blank branch, which only does `m_bitPos = (m_bitPos + 1) % kUnformattedTrackBits` (702-707).

(5) Nothing else could store the write:
- TryLocateBit rejects slot -1 and 0-bit slots (DiskImage.cpp:400-422).
- WriteBit's only caller is Disk2NibbleEngine.cpp:692.
- ResizeTrack is called only by the loaders and builders (WozLoader.cpp:240/305, NibblizationLayer.cpp:354/452, NibbleImageCodec.cpp:138, BlankDiskBuilder.cpp:449/478).
- ClearQuarterTrackMap and SetQuarterTrackSlot are called only from WozLoader.cpp:696/745/772/805.
- 035 adds one EnsureTrackSlots caller (Shell/ScratchHeatReplayer.cpp:569). It grows a scratch replay image and maps nothing.

The image is never marked dirty. Serialize rebuilds the TMAP from ResolveQuarterTrack (WozLoader.cpp:1496-1504), so these positions are written back as $FF with zero TRK records. Casso's own blank WOZ avoids the problem only because it sizes all 35 tracks (BlankDiskBuilder.cpp:331-335, 444-450).

The new 035 flux write path (Disk2NibbleEngine.cpp:675-687) does not help. A flux slot counts as data because of its kind (DiskImage.cpp:123), but a $FF or zero-bit bit slot is never a flux slot.

Existing tests do not cover the case. UnitTest/EmuTests/Disk2NibbleEngineTests.cpp:124-135 only checks that the disk keeps turning. Every LSS write test sizes its track first with ResizeTrack (Disk2NibbleEngineTests.cpp:75-93, UnitTest/EmuTests/DiskWritePathTests.cpp:205-248).

**035 impact:** 035 puts three constraints on the fix.

(a) The slot count must not change after mount.
- Reverse keyframes save every mounted DiskImage: MachineHost::GetStateParts (Shell/MachineHost.cpp:1423-1434) feeds DiskImage::SaveState (DiskImage.cpp:1092-1153).
- DiskImage::LoadState rejects a state whose trackCount differs from m_trackBits.size() (DiskImage.cpp:1276). DiskTrackSnapshot::Restore does the same (DiskTrackSnapshot.cpp:81).
- The quarter-track map is wiring and is not saved (DiskImage.h:252-256).
- So the obvious fix, giving the head position a new slot with EnsureTrackSlots and SetQuarterTrackSlot at write time, would break reverse execution. Every keyframe captured before the first write to a blank track would fail to load with ERROR_INVALID_DATA, so stepping back across a guest INIT would fail.
- The storage has to be reserved at load. At write time only a slot's bits and bit count may change, and SaveState/LoadState already save and restore both.
- LoadState bumps m_layoutGeneration (DiskImage.cpp:1311), so the engine's RefreshSlot (Disk2NibbleEngine.cpp:595-598) resolves the position back to -1 after a step back past the write. CommitStateTrack marks that track dirty (DiskImage.cpp:1404-1408).

(b) The slot count must survive a Serialize-then-Load round trip.
- MachineStateFile::Build serializes each disk (Shell/MachineStateFile.cpp:77).
- Apply reloads it into a fresh image (511, then MountRestored and MountFromBytes), and LoadStateOverMountedMedia then runs DiskImage::LoadState with the same count check.
- HeatHistory caches the serialized bytes plus the live GetTrackCount (Debugger/HeatHistory.cpp:1366-1386). ScratchHeatReplayer::MountDisks can only grow the slot count to match, never shrink it (ScratchHeatReplayer.cpp:569).
- So the reservation must be a deterministic function of the TMAP/FLUX that Serialize writes back. Reserve whole tracks in ascending order; use index N for track N when no map entry uses N, otherwise the lowest unused TRK index. A slot the guest has written keeps its index, so a reload makes the same choices for every other track.
- WozLoader::Load can run on a reused DiskImage. LoadFromBytes resets slot kinds and flux but not bit buffers (DiskImage.cpp:810-812), and UnitTest/EmuTests/WozFluxLoaderTests.cpp:194-205 loads twice into one object. A reserved slot must therefore be emptied explicitly, or a reload could expose stale bits from an earlier load.

(c) The flush hold must be respected.
- The write must reach the image only through WriteBit (DiskImage.cpp:480-508), which sets m_dirty. FlushAllUnlessHeld, SetFlushHold, CommitHeldWrites, DiscardHeldWrites and the replay gate (DiskImageStore.h:133-154) then handle it like any other guest write.
- The ResizeTrack that creates the track must not set m_dirty, and it does not (DiskImage.cpp:715-736). The fix adds no flush of its own.
- DiscardHeldWrites reloads through MountExternallyModifiedDisk into a fresh image (DiskImageStore.cpp:2014, 3511-3525), so a discarded format leaves no created track behind.
- ResizeTrack reallocates the track buffer, but only on the thread that runs the machine. That thread already replaces whole track vectors on every keyframe load (DiskImage.cpp:1300), so this adds no new kind of threading hazard.

The removal of StepInstructionWhilePaused does not matter here. The change sits inside StepLss, which run, step and replay all reach through Disk2Controller's catch-up.

**Proposed fix:** 1. WozLoader.cpp, in Load, after the v2 TMAP and FLUX loops and the v1 loop, just before `out.ClearDirty()` at 809: reserve storage for blank whole tracks. Put this in a static helper.
- Build a used[kV2TrkRecordCount] table from every tmap[] and flux[] entry below kV2TrkRecordCount.
- For each whole track N from 0 to 39 (kTmapChunkSize / 4) where out.GetMappedSlot(4*N) < 0:
  - Pick r = N if used[N] is false; otherwise pick the lowest r below kV2TrkRecordCount with used[r] false. Then set used[r] = true. A free index always exists: each used index takes at least one mapped quarter track, so free indices are never fewer than unmapped positions.
  - Call out.EnsureTrackSlots(r + 1) and then out.ResizeTrack(r, 0), so the slot is an empty Bits slot even on a reused image.
  - Call out.SetQuarterTrackSlot(4*N, r), and the same for 4*N-1 and 4*N+1 when each is in range and still unmapped. Leave 4*N+2 alone; this follows the usual WOZ layout for a standard track.
- Because the slot holds 0 bits, ResolveQuarterTrack still returns -1 (DiskImage.cpp:123-126). Reads, TrackWritability (TrackWritability.cpp:46-53), DamagedMountReport and Serialize's TMAP rebuild (WozLoader.cpp:1496-1504, which still writes $FF and a zero record) behave as before until the guest writes.
- A mapped zero-bit record needs no reservation: its slot already exists. (Not taken as written: R4 unmaps an empty bit slot and reserves it like an unmapped track, on an image with no damage. Under spec 040's track-record rule (its FR-053, built in 040's `51ad867e8`, which this branch takes), every record with a zero bit count is such a slot, whatever its start block, the one with start block 3 or more included (owner confirmed 2026-10-09); a record with a count above zero and a zero start block or block count, a start block below 3, or a count larger than its blocks hold, and a map entry from 160 to 254, are damage and write-protect the image, and the reservation does nothing for an image with any damage. research.md R4; tasks.md T111, T115, T136.)

2. DiskImage: add `int MakeBlankTrackWritable (int quarterTrack, size_t bitCount)`.
- slot = GetMappedSlot(quarterTrack).
- The slot qualifies only if all of these hold: !IsWriteProtected(), so damaged and protected images are excluded; 0 <= slot < m_trackBitCounts.size(); m_slotKind[slot] == TrackKind::Bits; m_trackBitCounts[slot] == 0.
- If it qualifies, call ResizeTrack(slot, bitCount). That gives zero bits, a new layout generation and a TouchTrack, but no dirty flag.
- Return slot, or -1 if the slot does not qualify.
- Check write protection before resizing, as WriteBit does, so a protected disk never gains a 51,200-bit all-zero track that a later flush would write out.

3. Disk2NibbleEngine::StepLss: inside `if (readClock)`, before `writing` is computed at line 672, add:
```cpp
if (!hasTrack && m_writeMode && m_disk != nullptr && !m_disk->IsWriteProtected()
    && m_disk->MakeBlankTrackWritable (m_currentTrack, kUnformattedTrackBits) >= 0)
{
    ResolveSlot();
    hasTrack = (m_slot >= 0);
}
```
- m_bitPos is already below kUnformattedTrackBits, because the blank branch (702-707) and PlaceHead (289-302) keep it modulo that length. So the first bit lands at the same rotational position, and every later bit takes the normal WriteBit path (692), which marks the image dirty.
- ResolveSlot records the new layout generation, so the next clock does not call RefreshSlot.
- Add `static_assert (kUnformattedTrackBits == NibblizationLayer::kTrackBitCapacity)` in the .cpp, so a created track is exactly the length Casso's own blank WOZ uses (BlankDiskBuilder.cpp:449).

4. Defensive change, because DiskTrackSnapshot is the documented keyframe API even though only tests use it today. In DiskTrackSnapshot::Restore, increment disk.m_layoutGeneration when any track's bit count changes (DiskTrackSnapshot.cpp:92-94), as DiskImage::LoadState does at 1311. Without that, an engine that cached a created slot keeps m_slot >= 0 over a 0-bit slot after a restore. Its bit cursor then stops (Disk2NibbleEngine.cpp:695-700), and it never re-creates the track.

5. Update the comments that describe the old behavior:
- Disk2NibbleEngine.cpp:457-462 and 702-705
- DiskImage.cpp:474-477 and the ResolveQuarterTrack banner
- the WozLoader::Load banner

Known gap left open: a write at an unmapped half-track (4N+2) is still dropped. Formatters never write there.

**Regression test:** Extend UnitTest/EmuTests/DiskWritePathTests.cpp (class DiskWritePathTests). Model the new tests on LssWrite_DirectEngine_FF_RoundTrips (205-248), using its load/shift loop and FrameTrack/FindSubsequence (147-198).

(a) LssWrite_OverATrackTheWozMapsAsBlank_LandsAndReadsBack
- Setup: WozLoader::BuildSyntheticV2(1, false, bits, 4096, woz). It maps only quarter tracks 0, 1 and 3 (WozLoader.cpp:1128-1135), so quarter track 4 is $FF. Load with img.LoadFromBytes(DiskFormat::Woz, woz, "").
- Preconditions: img.ResolveQuarterTrack(4) == -1. Record before = img.GetTrackCount().
- Write: SetDiskImage, SetMotorOn(true), SetCurrentTrack(4), SetWriteMode(true), then six $FF bytes through the load/shift loop.
- Asserts after the write:
  - img.IsDirty()
  - slot = img.ResolveQuarterTrack(4) >= 0
  - img.GetTrackBitCount(slot) == Disk2NibbleEngine::kUnformattedTrackBits
  - img.GetTrackCount() == before
- Read back: Reset, reattach, SetCurrentTrack(4), and require at least six leading 0xFF nibbles.
- Round trip: WozLoader::Serialize and Load into a fresh DiskImage; require ResolveQuarterTrack(4) >= 0 and the FF run present in FrameTrack.
- Before the fix the first assert fails: the image is never dirty and the position stays at -1.

(b) The same test over a zero-bit record. Use BuildSyntheticV2(1, false, {}, 0, woz) and write at quarter track 0, whose TRK record has bitCount 0 and bails at WozLoader.cpp:288. (Not taken as written: that record has start block 3 and block count 1 (WozLoader.cpp:1102-1105, :1147-1149); under spec 040's rule it is empty, not damage (owner confirmed 2026-10-09), so T112's load tests use it beside an all-zero record, the form the WOZ format gives an unused record, and the write test uses the all-zero record. research.md R4; tasks.md T112, T113.)

(c) LssWrite_OverABlankTrackOfAProtectedWoz_LeavesItBlank. Build with writeProtected = true. After the same write, require:
- ResolveQuarterTrack(4) == -1
- !IsDirty()
- GetTrackBitCount(GetMappedSlot(4)) == 0

This passes before and after the fix, but it fails if a fix resizes the track before checking write protection.

(d) Store-level check in the same file. Follow the SetFlushSink pattern of NibbleFlush_WritesOnlyTheTrackTheGuestTouched (392-435): MountFromBytes the blank-track WOZ, write over quarter track 4 with an engine attached to store.GetImage, call store.Eject, run WozLoader::Load on the sink's bytes, and require ResolveQuarterTrack(4) >= 0. This proves the file no longer gets $FF there.

Extend UnitTest/Devices/Disk2StateTests.cpp (Disk2StateTests, using SavePart/LoadFrom at 46-76) with DiskStepBackAcrossTheFirstWriteToABlankTrack:
- Load the blank-track WOZ and take blob = SavePart(disk). Write over quarter track 4 as in (a).
- Require LoadFrom(disk, blob) == S_OK. If a fix grows the slot vector at write time, this returns kInvalidData (DiskImage.cpp:1276).
- Then require disk.ResolveQuarterTrack(4) == -1 and disk.IsDirty(), the latter from CommitStateTrack.
- Repeat the write. Serialize the image, LoadFromBytes into a fresh DiskImage, and require its GetTrackCount() to equal the live one and LoadFrom(fresh, SavePart(live)) == S_OK. This is the MachineStateFile restore path (MachineStateFile.cpp:77, 511).

**Sites:** CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:601, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:672, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:688-707, CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.cpp:198-203, CassoEmuCore/Devices/Disk/DiskImage.cpp:119-126, CassoEmuCore/Devices/Disk/DiskImage.cpp:400-422, CassoEmuCore/Devices/Disk/DiskImage.cpp:309-313, CassoEmuCore/Machines/Apple2/Common/WozLoader.cpp:696, CassoEmuCore/Machines/Apple2/Common/WozLoader.cpp:727-730, CassoEmuCore/Machines/Apple2/Common/WozLoader.cpp:754-757, CassoEmuCore/Machines/Apple2/Common/WozLoader.cpp:782-785, CassoEmuCore/Machines/Apple2/Common/WozLoader.cpp:288, CassoEmuCore/Machines/Apple2/Common/WozLoader.cpp:235-238, CassoEmuCore/Machines/Apple2/Common/WozLoader.cpp:809, CassoEmuCore/Machines/Apple2/Common/WozLoader.cpp:1496-1504, CassoEmuCore/Devices/Disk/DiskTrackSnapshot.cpp:83-95

