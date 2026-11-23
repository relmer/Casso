#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "DiskImage.h"
#include "Machines/Apple2/Common/Denibblizer.h"
#include "MountedImageState.h"
#include "ChangePrompt.h"
#include "PreservedCopy.h"
#include "IImageWatcher.h"
#include "IDiskFileIo.h"
#include "MountDiagnosis.h"
#include "BayChange.h"
#include "Devices/Disk/Store/BayFileAccess.h"
#include "Devices/Disk/Store/BayWatchRegistry.h"
#include "Devices/Disk/Store/DiskBayTable.h"
#include "Devices/Disk/Store/DiskFlusher.h"
#include "Devices/Disk/Store/DiskSalvager.h"
#include "Devices/Disk/Store/DiskStoreHooks.h"
#include "Devices/Disk/Store/DiskStoreInterfaces.h"
#include "Devices/Disk/Store/ExternalChangeActions.h"
#include "Devices/Disk/Store/ExternalChangeMonitor.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore
//
//  Mount/Eject coordinator. Owns the DiskImage instances mounted across
//  every (slot, drive) pair. Routes Mount() requests by file extension:
//      .dsk → Nibblizer (DOS 3.3 sector order)
//      .do  → Nibblizer (DOS 3.3 sector order)
//      .po  → Nibblizer (ProDOS sector order)
//      .woz → WozLoader (native bit-stream)
//
//  Auto-flush invariants (FR-025):
//      Eject(slot, drive) — flush dirty image, then release.
//      FlushAll()         — flush every dirty mount; called on machine
//                           switch and on emulator exit / PowerCycle.
//      SoftReset()        — keep mounts; flush dirty (Phase 4 contract).
//      PowerCycle()       — unmount everything (auto-flush each).
//
//  Test hooks: SetFlushSink redirects serialized bytes to an in-memory
//  capture buffer instead of the host filesystem, and SetImageReader does
//  the same for reads, so the IFixtureProvider isolation contract is
//  preserved even for a read-modify-write.
//
//  There is no way to force a flush past the dirty and write-protect gates.
//  The one caller that wanted that was changing a WOZ's write-protect flag,
//  which lives inside the file; it now goes through SetImageWriteProtect,
//  which patches the single byte instead of rebuilding the image around it.
//
//  Flush-error reporting: a flush that was supposed to persist a dirty
//  image but couldn't (serialize failure, or the file/sink write failed)
//  used to vanish -- every caller drops FlushEntry's HRESULT (Eject and
//  PowerCycle are void; the shell/SoftReset IGNORE_RETURN_VALUE it), so a
//  user's writes could be silently lost. FlushEntry now surfaces the loss
//  itself through the shared EHM notifier (CHRN/CBRN -> EhmNotifyUser),
//  which routes to whatever handler the host registered (a MessageBox in
//  the GUI, stderr headless), so the report reaches the user regardless of
//  what the caller does with the return.
//
////////////////////////////////////////////////////////////////////////////////

class DiskImageStore : private IBayRepoint, private IBayLifecycle
{
public:
    HRESULT       Mount             (int slot, int drive, const string & path);
    HRESULT       MountFromBytes    (int slot, int drive, const string & virtualPath,
                                     DiskFormat fmt, const vector<Byte> & bytes);

    //  The same two mounts, reporting WHY a refusal happened as well as that
    //  it did. A caller that has a user to answer to wants these; one that
    //  only needs to know whether it worked keeps the shorter forms above.
    //  The diagnosis is written on success too, as None, so a caller cannot
    //  read a stale reason off a mount that worked.
    HRESULT       Mount             (int slot, int drive, const string & path,
                                     MountDiagnosis & outDiagnosis);
    HRESULT       MountFromBytes    (int slot, int drive, const string & virtualPath,
                                     DiskFormat fmt, const vector<Byte> & bytes,
                                     MountDiagnosis & outDiagnosis);
    void          Eject             (int slot, int drive) override;

    //  Writing dirty images back to their files, and a WOZ's write-protect
    //  flag into its file.
    DiskFlusher & GetFlusher        () { return m_flusher; }

    //  The verdict reached at mount. Free to call: no decoding, no allocation.
    //  Use this to decide whether to OFFER salvage; use AssessSalvage when the
    //  user has asked for it and the counts have to be current.
    bool          IsSalvageOffered (int slot, int drive) const;

    //  Assessing and writing a salvaged copy of a damaged disk.
    DiskSalvager & GetSalvager      () { return m_salvager; }

    void          SoftReset         ();
    void          PowerCycle        ();

    DiskImage *   GetImage          (int slot, int drive);
    uint64_t      GetMediaId        (int slot, int drive) const;

    //  A machine state was restored and a mounted disk's tracks may have
    //  changed under it (FR-069): the bay gets a new media id and the sink
    //  gets BayChange::Restored, so the drive's counts start over and a viewer
    //  analyzes the disk as after a reload. The file's bytes as inserted are
    //  left as they are, so "As inserted" is the disk as first inserted
    //  (FR-117).
    void          NoteStateRestored (int slot, int drive);
    bool          IsMounted         (int slot, int drive) const;
    const string &GetSourcePath     (int slot, int drive) const;

    //  One mounted image's backing path with its bay, for consumers that need
    //  the full mounted set (the create dialog's mounted-target refusal).
    struct MountedSource
    {
        string  path;
        int     slot  = 0;
        int     drive = 0;
    };

    std::vector<MountedSource>  GetMountedSourcePaths() const;

    void          SetFlushSink      (DiskStoreHooks::FlushSink sink) { m_hooks.flushSink = std::move (sink); }

    //  Read counterpart to SetFlushSink: redirects every backing-file read
    //  (Mount, and the write-protect patch's read-modify-write) to the
    //  caller's buffer, so a test can pair the two and exercise a genuine
    //  read-modify-write cycle without a real file.
    void          SetImageReader    (DiskStoreHooks::ImageReader reader) { m_hooks.imageReader = std::move (reader); }

    //  Replaces the filesystem stat behind the mount-time record and the
    //  pre-commit re-check.
    void          SetIdentityReader (DiskStoreHooks::IdentityReader reader) { m_hooks.identityReader = std::move (reader); }

    //  Where notification comes from. The shell builds the platform watcher and
    //  hands it over; REGISTERING AND DROPPING WATCHES IS THIS CLASS'S JOB,
    //  because mount-registers-a-watch is orchestration and orchestration is
    //  testable. Caller-owned and may be null, which is a session with no
    //  notification -- the check before every write still holds.
    void          SetImageWatcher (IImageWatcher * watcher) { m_hooks.watcher = watcher; }

    //  Where "is somebody else writing this right now" is answered. Optional:
    //  without it a pick-up cannot be deferred for a third-party writer, and
    //  the quiet period is the only debounce.
    void          SetFileIo (IDiskFileIo * fileIo) { m_hooks.fileIo = fileIo; }

    //  The machine as the user knows it, for the notices that mention it.
    //  "Apple //e" rather than "the Apple", which is not what is in front of
    //  them. Empty is allowed and the notices fall back to "the machine".
    void          SetMachineName (const string & name) { m_hooks.machineName = name; }

    //  Restarting the machine.
    //
    //  A CALLBACK RATHER THAN A CALL. A device-layer image store reaching
    //  machine lifecycle directly is a layering inversion; the decision stays
    //  here and the action belongs to the shell.
    void          SetMachineRestartCallback (std::function<void ()> cb) { m_hooks.restartCallback = std::move (cb); }

    //  Showing a report that does not block the machine. Given the bay it is
    //  about and everything to draw.
    //
    //  THE SINK REPLACES, IT DOES NOT APPEND. A report is emitted for every
    //  change acted on, and a notice already up for that bay is re-worded
    //  rather than joined by a second one. Emitting only the first would keep
    //  one notice at the cost of it going stale: measured, a `reload` followed
    //  by a `reboot` left the bar still advising a reboot that had already
    //  happened.
    void          SetChangeReportSink (DiskStoreHooks::ReportSink sink) { m_hooks.reportSink = std::move (sink); }

    //  Putting a question to the user.
    //
    //  IT DOES NOT RETURN THE ANSWER. Asking happens on the thread that owns
    //  disk writes and answering on the one that owns the screen, so a sink
    //  that returned the answer would have to block the machine while the user
    //  read it. The answer comes back through ResolvePendingChange instead.
    //
    //  IT RETURNS WHETHER THE QUESTION REACHED SOMEWHERE IT CAN BE ANSWERED,
    //  which is a different fact and one only the sink knows. The shell posts
    //  the question to its own window and cannot do that before the window
    //  exists or when the queue is full; a bay marked as having a question
    //  outstanding that nobody ever saw is a bay nothing acts on again until
    //  the disk is ejected.
    //
    //  Null means nothing can answer, and a change that needs an answer stays
    //  pending rather than resolving itself by default.
    void          SetAskSink (DiskStoreHooks::AskSink sink) { m_hooks.askSink = std::move (sink); }

    //  Asking where to put a disk, and not returning until it is answered.
    //
    //  THE ONE ROUTE LEFT AT SHUTDOWN, and the reason it is separate from
    //  AskSink. That one posts a message and reads the answer later, which
    //  needs a pump and a CPU thread; by the time the last flush runs there is
    //  neither. A file dialog is a blocking modal with a pump of its own, so
    //  it still works -- but only from a thread whose apartment suits it,
    //  which is why the store never calls this anywhere but ShuttingDown.
    //
    //  Returns false when the user declined or nothing could ask.
    void          SetRescueSink (DiskStoreHooks::RescueSink sink) { m_hooks.rescueSink = std::move (sink); }

    //  A bay's disk changed, and the shell should react: re-point the
    //  controller, re-apply write protection, log the debug event, and drive
    //  the door and its sounds.
    //
    //  ONE SIGNAL FOR EVERY PATH. A mount, a user eject, a pick-up, a file that
    //  vanished -- each ends here, so the door and the speaker are lit from one
    //  place rather than from every path that can move a disk. The store knows
    //  when a bay changed; what to do about it on screen is the shell's, and a
    //  fake sink lets a test assert the store fired without a drive on screen.
    //
    //  FIRES ON THE THREAD THAT OWNS DISK WRITES, where every path that can
    //  change a bay already runs. The handler does its controller and audio
    //  work there, exactly as the mount path did before this was central.
    void          SetBayChangeSink (DiskStoreHooks::BayChangeSink sink) { m_hooks.bayChangeSink = std::move (sink); }

    //  What a settled change to a mounted image was decided to be, reported
    //  once per decision so a tool that stated the intent can be told.
    //
    //  `guestCopyPreserved` says the guest's unsaved writes were moved to
    //  `preservedPath` before the decision was taken again as an ordinary
    //  change. A decision that puts a question to the user is not reported;
    //  nothing has been decided yet.
    //
    //  FIRES ON THE THREAD THAT OWNS DISK WRITES, where the decision is taken.
    void          SetDecisionSink (DiskStoreHooks::DecisionSink sink) { m_hooks.decisionSink = std::move (sink); }

    //  Applies a ChangeAction to a bay, and the user's answer to a prompt.
    ExternalChangeActions & GetChangeActions () { return m_changeActions; }

    //  Recording changes made to mounted images' files outside Casso, and
    //  acting on them once they settle.
    ExternalChangeMonitor & GetChangeMonitor () { return m_changeMonitor; }

    //  Where "now" comes from, in milliseconds, so the quiet period can be
    //  swept in a test without waiting for one.
    void          SetClock (std::function<int64_t ()> clock) { m_hooks.clock = std::move (clock); }

    //  Where the wall-clock time in a preserved copy's NAME comes from.
    //
    //  A SECOND SEAM RATHER THAN A CONVERSION OF THE FIRST. The quiet period
    //  needs a monotonic count of milliseconds and a filename needs a calendar
    //  date; deriving one from the other would tie a timer to the user's clock
    //  changing under it.
    void          SetTimestampSource (std::function<time_t ()> source) { m_hooks.timestamp = std::move (source); }

    //  What a bay knows about its image beyond the bytes: the identity read at
    //  mount, any change noticed since, and whether a report stands. Null for
    //  an out-of-range bay.
    //
    //  Exposed rather than wrapped, because everything built above the store --
    //  the watch wiring, the banner, the prompt -- asks these questions of a
    //  bay, and a dozen forwarding accessors would put one piece of state
    //  behind two names.
    MountedImageState        * GetSharedState (int slot, int drive);
    const MountedImageState  * GetSharedState (int slot, int drive) const;

private:
    //  Moves a mounted bay onto a different file without disturbing the disk
    //  in it. The image, and everything the guest can observe, is untouched;
    //  what changes is which file the bay reads and writes from here on.
    //
    //  THIS IS A SAVE-AS, NOT A MOUNT. Nothing is loaded, so a guest mid-write
    //  sees nothing at all.
    HRESULT       RepointBayToFile  (int slot, int drive, const string & newPath) override;

    //  Empties a bay whose file is gone, WITHOUT trying to flush it. The
    //  ordinary eject flushes first, which here would either fail or write the
    //  disk back to a path the user has just been told no longer exists.
    void           EjectLostImage (int slot, int drive) override;

    //  Replaces a mounted image's contents WITHOUT flushing what it held.
    //
    //  A FLUSH HERE WOULD WRITE THE OLD DISK OVER THE NEW FILE, which is the
    //  precise loss this feature exists to prevent. The new bytes are loaded
    //  into a fresh image first, so a load that fails leaves the mounted disk
    //  exactly as it was.
    HRESULT        MountExternallyModifiedDisk (int slot, int drive, const vector<Byte> & bytes) override;

    //  Loads bytes into a bay for MountFromBytes and Mount, which each call the
    //  bay-change sink after a successful load, in their own order.
    HRESULT        LoadIntoBay (int slot, int drive, const string & virtualPath,
                                DiskFormat fmt, const vector<Byte> & bytes,
                                MountDiagnosis & outDiagnosis);

    //  Tells the shell a bay's disk changed, when a sink is installed. The one
    //  chokepoint every mutation path calls, so the door and its sounds cannot
    //  be lit from a path that forgot to.
    void           EmitBayChange (int slot, int drive, BayChange change);

    DiskStoreHooks         m_hooks;
    DiskBayTable           m_bays;
    BayFileAccess          m_files         { m_hooks };
    DiskSalvager           m_salvager      { m_bays, m_hooks };
    DiskFlusher            m_flusher       { m_bays, m_hooks, m_files, *this };
    ExternalChangeActions  m_changeActions { m_bays, m_hooks, m_files, *this, *this };
    ExternalChangeMonitor  m_changeMonitor { m_bays, m_hooks, m_files, m_changeActions };
    BayWatchRegistry       m_watches       { m_bays, m_hooks, m_changeMonitor };
    string                 m_emptyPath;
};
