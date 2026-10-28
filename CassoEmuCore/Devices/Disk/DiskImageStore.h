#pragma once

#include "Pch.h"

#include "Core/ThreadOwnership.h"

#include "DiskImage.h"
#include "MountedImageState.h"
#include "ChangePrompt.h"
#include "PreservedCopy.h"
#include "IDiskFileIo.h"
#include "DurableCommit.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "BayChange.h"
#include "BayStatus.h"
#include "IImageWatcher.h"
#include "DiskStoreTypes.h"
#include "MountDiagnosis.h"
#include "DiskFileOps.h"
#include "DiskIoQueue.h"
#include "DiskStoreText.h"
#include "DiskStoreReload.h"
#include "DiskStoreSave.h"
#include "DiskStoreSalvage.h"
#include "DiskStoreMedia.h"
#include "DiskStoreMount.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore
//
//  Mount/Eject coordinator. Owns the DiskImage instances mounted across
//  every (slot, drive) pair. Routes Mount() requests by file extension:
//      .dsk → NibblizationLayer (DOS 3.3 sector order)
//      .do  → NibblizationLayer (DOS 3.3 sector order)
//      .po  → NibblizationLayer (ProDOS sector order)
//      .woz → WozLoader (native bit-stream)
//
//  Auto-flush invariants:
//      Eject(slot, drive) — flush dirty image, then release.
//      FlushAll()         — flush every dirty mount; called on machine
//                           switch and on emulator exit / PowerCycle.
//      SoftReset()        — keep mounts; flush every dirty image unless
//                           the reverse-execution hold or a replay is in
//                           force.
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
//  One thread uses the store at a time: the thread holding its
//  ThreadOwnership token, which starts as the constructing thread and passes
//  to the CPU thread while it runs. Every public instance member checks it,
//  except NoteExternalChange, which any thread may call, and
//  GetThreadOwnership, which returns the token itself.
//
////////////////////////////////////////////////////////////////////////////////


class DiskImageStore
{
public:
    using FlushSink      = std::function<HRESULT (const string &, const vector<Byte> &)>;
    using ImageReader    = std::function<HRESULT (const string &, vector<Byte> &)>;

    //  How the store learns what a file looks like right now.
    //
    //  A THIRD SEAM BESIDE THE OTHER TWO, and for their reason: a test that has
    //  redirected reads and writes into memory has no file to stat, so without
    //  it the staleness gate below would compare a recorded identity against a
    //  missing one and refuse every flush.
    using IdentityReader = std::function<ImageIdentity (const string &)>;

    static constexpr int   kSlotCount  = 8;
    static constexpr int   kDriveCount = 2;

    DiskImageStore ();

    //  The token that says which thread may use the store, shared with every
    //  drive wired to it.
    ThreadOwnership       & GetThreadOwnership ()       { return m_ownership; }
    const ThreadOwnership & GetThreadOwnership () const { return m_ownership; }

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
    void          Eject             (int slot, int drive);

    //  Where an asynchronous mount's result goes, and an asynchronous eject's:
    //  whether the bay is empty. Both run on the store's thread.
    using MountDone = DiskStoreMount::MountDone;
    using EjectDone = std::function<void (bool isEjected)>;

    //  Mount and Eject with their file work on the I/O queue: done runs when
    //  it is over. The overloads above wait for it.
    void          Mount             (int slot, int drive, const string & path, MountDone done);
    void          Eject             (int slot, int drive, EjectDone done);

    //  A machine state's disk back in its bay, built from the bytes the state
    //  carries rather than read from path (see the .cpp).
    HRESULT       MountRestored     (int slot, int drive, const string & path,
                                     DiskFormat fmt, const vector<Byte> & bytes);
    HRESULT       Flush             (int slot, int drive);
    void          Flush             (int slot, int drive, std::function<void (HRESULT)> done);
    HRESULT       FlushAll          ();

    //  The same, for the last flush of the process, which can only ask through
    //  a blocking dialog. Separate from FlushAll so the moment is explicit at
    //  the call site rather than inferred from where it happens to be called.
    HRESULT       FlushAllForShutdown ();

    //  The flushes nobody asked for: the motor spinning down, a reset and a
    //  power cycle. They do nothing while the flush hold is on or a replay is
    //  running; every other flush (eject, machine switch, exit, an explicit
    //  save, a mount over a disk) writes, and writes the disk as it is now.
    HRESULT       FlushAllUnlessHeld ();

    //  The motor spinning down: FlushAllUnlessHeld's saves, posted to the I/O
    //  queue without waiting for them. A failure is reported from its save's
    //  completion.
    void          SaveAllUnlessHeld  ();

    //  Reverse execution keeps guest writes in memory while it records: the
    //  disk is machine state then, and the file is written only at the moments
    //  above. Commit writes every bay as it stands now; discard reloads every
    //  bay holding unsaved writes from its file.
    void          SetFlushHold      (bool isHeld) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_isFlushHeld = isHeld; }
    bool          IsFlushHeld       () const      { ASSERT_THREAD_OWNERSHIP (m_ownership); return m_isFlushHeld; }
    bool          HasUnsavedWrites  () const;

    //  Whether every bay's guest writes are in a file: the disk is clean, or
    //  its recovery copy is current.
    bool          AreAllWritesSaved ();
    int           CountUnsavedDisks () const;
    HRESULT       CommitHeldWrites  ();
    HRESULT       DiscardHeldWrites ();

    //  Set while a replay re-runs history. No flush of any kind and no reload
    //  of a changed file happens then: the replay is recomputing a past the
    //  host already saw, and the host file is never written from it.
    void          SetReplaying      (bool isReplaying) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_isReplaying = isReplaying; }
    bool          IsReplaying       () const           { ASSERT_THREAD_OWNERSHIP (m_ownership); return m_isReplaying; }

    //  Which medium a bay holds, as DiskImage::GetImageId gives it, or zero
    //  for an empty bay. A machine snapshot holds these, and loading one puts
    //  the same media back in the same bays.
    uint64_t      GetMediaId        (int slot, int drive) const;

    //  With retention on, a disk that leaves a bay is kept in memory, with
    //  any unsaved writes, so a snapshot taken while it was in the drive can
    //  put it back. Turning retention off releases every kept disk.
    void          SetMediaRetention     (bool isOn);
    bool          IsRetainingMedia      () const { ASSERT_THREAD_OWNERSHIP (m_ownership); return m_media.IsRetaining(); }
    size_t        GetRetainedMediaCount () const { ASSERT_THREAD_OWNERSHIP (m_ownership); return m_media.GetRetainedCount(); }

    //  Whether SeatMedia could put mediaId in the bay, and putting it there:
    //  the bay's current disk is kept (or dropped, without retention) and the
    //  kept disk with that identity takes its place. Zero empties the bay.
    //  Nothing is flushed and nothing is reported as a bay change.
    bool          CanSeatMedia      (int slot, int drive, uint64_t mediaId) const;
    HRESULT       SeatMedia         (int slot, int drive, uint64_t mediaId, bool & outChanged);

    //  Emits the bay change that took a bay from previousMediaId to the disk
    //  it holds now, after SeatMedia, which emits none. A bay holding the same
    //  disk emits nothing.
    void          ReportSeatedMedia (int slot, int drive, uint64_t previousMediaId);

    //  Drops kept disks that left their bay before oldestPosition, which no
    //  snapshot still in history can hold.
    void          PruneRetainedMedia (uint64_t oldestPosition);

    //  The machine's instruction count, which stamps when a disk left its bay.
    void          SetPositionSource (const uint64_t * source) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_media.SetPositionSource (source); }

    //  Told after a disk is mounted, ejected, swapped for a changed file, or
    //  has its write protection changed in its file: the change reverse
    //  execution records as a boundary in history. Not told while replaying,
    //  nor for SeatMedia.
    void          SetMediaChangeListener (std::function<void ()> listener) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_mediaChangeListener = std::move (listener); }

    //  Sets a mounted WOZ's write-protect flag in its backing file by patching
    //  the single byte that carries it -- read the file, set INFO's flag byte,
    //  recompute the header CRC, write it back atomically -- rather than
    //  re-serializing the image.
    //
    //  This replaced a ForceFlush that pushed the flag through the full
    //  rebuild-from-model writer. That writer is correct for a flush, where
    //  the track model IS the newer truth, and wrong for this, where the file
    //  is: one menu click relaid out an entire image to carry one bit, and on
    //  a preservation dump that was pure loss. Patching one byte cannot lose
    //  what it never reads, which is a guarantee by construction rather than
    //  one that depends on the writer retaining every field correctly.
    //
    //  Flushes pending guest writes first, so the patch lands on a file that
    //  already holds them. That ordering used to be the caller's to get right.
    HRESULT       SetImageWriteProtect (int slot, int drive, bool writeProtected);
    void          SetImageWriteProtect (int slot, int drive, bool writeProtected, std::function<void (HRESULT)> done);

    //  Whether salvage is worth offering for a bay, and what it would cost.
    //  Writes nothing -- this exists so the user sees the counts BEFORE
    //  committing to a lossy copy, rather than learning them afterwards.
    HRESULT       AssessSalvage (int slot, int drive, SalvageAssessment & out);

    //  The verdict reached at mount. Free to call: no decoding, no allocation.
    //  Use this to decide whether to OFFER salvage; use AssessSalvage when the
    //  user has asked for it and the counts have to be current.
    bool          IsSalvageOffered (int slot, int drive) const;

    //  Writes the salvaged copy to `path`. The ORIGINAL IS NEVER TOUCHED --
    //  that is the whole shape of this feature: the damaged file keeps its
    //  damage, detectably, and the user gets a separate disk they can work on.
    //
    //  The copy keeps the source's META (what the disk *is* -- title,
    //  publisher, provenance) but is stamped with Casso as creator, because
    //  Casso did write this particular file and claiming otherwise would put
    //  a preservation tool's name on a lossy reconstruction.
    //
    //  `mediaId` is the medium the user saw assessed. If the bay holds another
    //  one now, nothing is written and the result is ERROR_MEDIA_CHANGED.
    HRESULT       SalvageToFile (int slot, int drive, uint64_t mediaId, const string & path,
                                 DenibblizeReport & report);
    void          SoftReset         ();
    void          PowerCycle        ();

    DiskImage *   GetImage          (int slot, int drive);
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

    //  One bay's status for display, built on the thread that has the store's
    //  token. The generation moves on every write to a bay's path, mount
    //  state, format, salvage verdict, image or write-protect cause, so an
    //  unmoved generation means no bay's status changed.
    BayStatus     GetBayStatus        (int slot, int drive) const;
    uint64_t      GetStatusGeneration () const { ASSERT_THREAD_OWNERSHIP (m_ownership); return m_statusGeneration; }
    void          MarkStatusChanged   ()       { ASSERT_THREAD_OWNERSHIP (m_ownership); m_statusGeneration++; }

    //  Write-protect flags set on the mounted image without saving it: the
    //  user's setting, and the file's read-only and permission state.
    void          SetUserWriteProtectFlag (int slot, int drive, bool isProtected);
    void          SetFileWriteProtect     (int slot, int drive, bool isReadOnly, bool hasNoPermission);

    //  The user's write-protect setting, from a command. Protecting saves the
    //  disk first, because a protected disk is never saved; after a failed
    //  save the disk is writable, with its writes.
    HRESULT       SetUserWriteProtect     (int slot, int drive, bool isProtected);
    void          SetUserWriteProtect     (int slot, int drive, bool isProtected, std::function<void (HRESULT)> done);

    void          SetFlushSink      (FlushSink sink) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_flushSink = std::move (sink); }

    //  Read counterpart to SetFlushSink: redirects every backing-file read
    //  (Mount, and the write-protect patch's read-modify-write) to the
    //  caller's buffer, so a test can pair the two and exercise a genuine
    //  read-modify-write cycle without a real file.
    void          SetImageReader    (ImageReader reader) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_imageReader = std::move (reader); }

    //  Replaces the filesystem stat behind the mount-time record and the
    //  pre-commit re-check.
    void          SetIdentityReader (IdentityReader reader) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_identityReader = std::move (reader); }

    //  Where notification comes from. The shell builds the platform watcher and
    //  hands it over; REGISTERING AND DROPPING WATCHES IS THIS CLASS'S JOB,
    //  because mount-registers-a-watch is orchestration and orchestration is
    //  testable. Caller-owned and may be null, which is a session with no
    //  notification -- the check before every write still holds.
    void          SetImageWatcher (IImageWatcher * watcher) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_watcher = watcher; }

    //  Where "is somebody else writing this right now" is answered. Optional:
    //  without it a pick-up cannot be deferred for a third-party writer, and
    //  the quiet period is the only debounce.
    void          SetFileIo (IDiskFileIo * fileIo) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_fileIo = fileIo; }

    //  Where the store's file I/O runs, the inline queue when null. The queue
    //  is the caller's and must outlive the store's use of it.
    void          SetIoQueue (IDiskIoQueue * queue) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_ioQueue = (queue != nullptr) ? queue : &m_inlineQueue; }

    //  Applies the results of finished file I/O, on the thread with the
    //  store's token.
    void          DrainIo () { ASSERT_THREAD_OWNERSHIP (m_ownership); m_ioQueue->DrainCompletions(); }

    //  The queue the store's file work runs on, for a caller whose file work
    //  goes with a disk operation and is to run in order with it.
    IDiskIoQueue & GetIoQueue () { ASSERT_THREAD_OWNERSHIP (m_ownership); return *m_ioQueue; }

    //  False for a store whose disks have no file to save to, such as a
    //  scratch machine's: its saves write nothing and clear the dirty bits.
    void          SetFileBacked (bool isFileBacked) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_isFileBacked = isFileBacked; }

    //  The machine as the user knows it, for the notices that mention it.
    //  "Apple //e" rather than "the Apple", which is not what is in front of
    //  them. Empty is allowed and the notices fall back to "the machine".
    void          SetMachineName (const string & name) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_machineName = name; }

    //  Restarting the machine.
    //
    //  A CALLBACK RATHER THAN A CALL. A device-layer image store reaching
    //  machine lifecycle directly is a layering inversion; the decision stays
    //  here and the action belongs to the shell.
    void          SetMachineRestartCallback (std::function<void ()> cb) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_restartCallback = std::move (cb); }

    //  Showing a report that does not block the machine. Given the bay it is
    //  about and everything to draw.
    //
    //  THE SINK REPLACES, IT DOES NOT APPEND. A report is emitted for every
    //  change acted on, and a notice already up for that bay is re-worded
    //  rather than joined by a second one. Emitting only the first would keep
    //  one notice at the cost of it going stale: measured, a `reload` followed
    //  by a `reboot` left the bar still advising a reboot that had already
    //  happened.
    using ReportSink = std::function<void (int slot, int drive, const ChangePrompt &)>;

    void          SetChangeReportSink (ReportSink sink) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_reportSink = std::move (sink); }

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
    using AskSink = std::function<bool (int slot, int drive, const ChangePrompt &)>;

    void          SetAskSink (AskSink sink) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_askSink = std::move (sink); }

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
    using RescueSink = std::function<bool (const string & imagePath, string & outPath)>;

    void          SetRescueSink (RescueSink sink) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_rescueSink = std::move (sink); }

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
    //  FIRES ON THE THREAD HOLDING THE STORE, where every path that can
    //  change a bay already runs. The handler does its controller and audio
    //  work there, exactly as the mount path did before this was central.
    using BayChangeSink = std::function<void (int slot, int drive, BayChange change)>;

    void          SetBayChangeSink (BayChangeSink sink) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_bayChangeSink = std::move (sink); }

    //  What a settled change to a mounted image was decided to be, reported
    //  once per decision so a tool that stated the intent can be told.
    //
    //  `guestCopyPreserved` says the guest's unsaved writes were moved to
    //  `preservedPath` before the decision was taken again as an ordinary
    //  change. A decision that puts a question to the user is not reported;
    //  nothing has been decided yet.
    //
    //  FIRES ON THE THREAD HOLDING THE STORE, where the decision is taken.
    using DecisionSink = std::function<void (const string & path, ChangeAction action,
                                             bool guestCopyPreserved, const string & preservedPath)>;

    void          SetDecisionSink (DecisionSink sink) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_decisionSink = std::move (sink); }

    //  The user answered a question this store asked.
    //
    //  ON THE THREAD HOLDING THE STORE, like every other entry point that
    //  can swap an image. The shell routes it there rather than acting on the
    //  UI thread where the answer arrived.
    //
    //  `savePath` IS WHERE THE USER CHOSE TO PUT THE IN-MEMORY COPY, and is
    //  meaningful only for an answer of PreserveCopy. Choosing it is a file
    //  dialog, which only the shell can raise, so the path arrives with the
    //  answer rather than being asked for from here.
    void          ResolvePendingChange (int slot, int drive, ChangeAction chosen,
                                        const string & savePath = string());

    //  Where "now" comes from, in milliseconds, so the quiet period can be
    //  swept in a test without waiting for one.
    void          SetClock (std::function<int64_t ()> clock) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_clock = std::move (clock); }

    //  Where the wall-clock time in a preserved copy's NAME comes from.
    //
    //  A SECOND SEAM RATHER THAN A CONVERSION OF THE FIRST. The quiet period
    //  needs a monotonic count of milliseconds and a filename needs a calendar
    //  date; deriving one from the other would tie a timer to the user's clock
    //  changing under it.
    void          SetTimestampSource (std::function<time_t ()> source) { ASSERT_THREAD_OWNERSHIP (m_ownership); m_timestamp = std::move (source); }

    //  A change was noticed, from a watcher or stated by a writer.
    //
    //  CALLED FROM ANY THREAD. The change goes into an inbox, and nothing else
    //  happens: the watcher calls this from the watch thread and the
    //  message channel from the UI thread, and ApplyPendingReload moves the
    //  inbox into the bays on the thread that has the store's token.
    void          NoteExternalChange (const string & path, ExternalChangeIntent intent);

    //  Act on whatever has settled. On the thread holding the store, at a
    //  moment with no disk operation in flight.
    void          ApplyPendingReload ();

    //  What a bay knows about its image beyond the bytes: the identity read at
    //  mount, any change noticed since, and whether a report stands. Null for
    //  an out-of-range bay.
    //
    //  Exposed rather than wrapped, because everything built above the store --
    //  the watch wiring, the banner, the prompt -- asks these questions of a
    //  bay, and a dozen forwarding accessors would put one piece of state
    //  behind two names.
    MountedImageState *        GetSharedState (int slot, int drive);
    const MountedImageState *  GetSharedState (int slot, int drive) const;

    static HRESULT  GetSourceFormatByExtension (const string & path, DiskFormat & outFmt);

    //  Whether `path`'s extension names a container this build can actually
    //  mount. Answered BY GetSourceFormatByExtension rather than by a second list
    //  of extensions, which is the point: the interface offering a file and
    //  the loader accepting it must not be able to disagree. They did, over
    //  `.nib` -- the drop filter said yes, the loader said no, and the mount
    //  failed silently.
    //
    //  The wide overload exists because the drag-and-drop and picker filters
    //  work in wchar_t, and narrowing at each call site is how the two answers
    //  drift apart again.
    static bool     IsMountableImageExtension (const string  & path);
    static bool     IsMountableImageExtension (const wstring & path);

    //  Where a recovery image goes for the Nth attempt, when an image cannot be
    //  written back to its own format without loss. Public because the naming
    //  and never-overwrite policy are part of the observable contract, not an
    //  implementation detail -- and pure, so both are testable without a disk.
    static string   MakeRecoveryPath        (const string & imagePath, int attempt);

    //  Replaces `path` with `bytes` through DurableCommit on the real file
    //  system: the bytes go to a sibling temporary, which is flushed to
    //  storage and then replaces the target in one step. After a failure at
    //  any step the ORIGINAL file is exactly as it was and the failure is
    //  reported, and after a power cut the target is the old file or the new
    //  one, whole.
    //  Opening the target directly would truncate it before the first byte is
    //  written, so a write that then failed (a full volume, a disconnected
    //  share) would destroy the only copy.
    //
    //  Public because it is the unit of behavior worth testing on its own:
    //  the failure paths are filesystem states, not emulator states.
    static HRESULT  WriteFileAtomically (const string & path, const vector<Byte> & bytes);

    //  The temporary a commit of `path` would write through, for a test that
    //  needs to know a commit left nothing behind -- or that two of them chose
    //  different names.
    //
    //  PUBLIC BECAUSE THE NAME IS PART OF THE CONTRACT NOW. It used to be a
    //  fixed suffix three tests could spell for themselves; it cannot be, so
    //  asking is the only way to stay right about it.
    static string   GetCommitTemporaryPath (const string & path, unsigned attempt);

    //  Reads a whole file into `bytes`. The read counterpart of the write
    //  above, and public for the same reason.
    static HRESULT  ReadFileBytes (const string & path, vector<Byte> & bytes);


    //  Why a load of these bytes was refused, from what the bytes and the
    //  format alone can settle. Static and pure, so a test can ask it about a
    //  buffer without mounting anything.
    static MountDiagnosis  ClassifyLoadFailure (DiskFormat fmt, const vector<Byte> & bytes);
private:
    friend class DiskStoreReload;
    friend class DiskStoreSave;
    friend class DiskStoreSalvage;
    friend class DiskStoreMedia;
    friend class DiskFileOps;
    friend class DiskStoreMount;

    using Entry = DiskBayEntry;

    //  Tells the media change listener, unless a replay is running.
    void           NotifyMediaChanged ();

    // Every public accessor takes a caller-supplied slot/drive pair, so each
    // one range-checks before GetEntry() indexes the fixed array.
    static bool   IsValidBay        (int slot, int drive);

    Entry &       GetEntry          (int slot, int drive);
    const Entry & GetEntry          (int slot, int drive) const;
    HRESULT       FlushEveryBay     (FlushMoment moment);

    //  Whether some other bay already reads and writes this file, and which
    //  drive has it. The bay being mounted into is excluded, because putting
    //  the same file back into the same drive is how a re-mount works.
    bool          IsFileInAnotherBay (const string & path, int exceptSlot, int exceptDrive,
                                      int & outDrive);

    // Routes through m_imageReader when a test has installed one, so the
    // read and write seams stay symmetric.
    HRESULT       ReadImageFile     (const string & path, vector<Byte> & bytes) const;

    // Enough room to step past collisions without ever spinning.
    static constexpr int  kMaxRecoveryNameAttempts = 64;

    //  The identity `path` has right now: through the seam when one is
    //  installed, and from the filesystem when none is.
    ImageIdentity  ReadIdentity (const string & path) const;

    //  The seams set on the store, as the value a job copies.
    DiskFileOps    GetFileOps () const;

    //  The tag in this process's commit temporaries, taken once.
    static uint64_t  GetInvocationTag();

    //  Milliseconds now, through the injected clock when one is installed.
    int64_t        GetNowMs () const;

    //  How long a caller that uses a result before it returns waits for it.
    static constexpr DWORD  s_kSyncIoTimeoutMs = 30000;


    //  SetImageWriteProtect's step after the save.
    void            PatchWriteProtectFlag (int slot, int drive, uint64_t imageId, const string & path,
                                           bool writeProtected, std::function<void (HRESULT)> done);

    //  Eject's step after the save.
    void            FinishEject        (int slot, int drive, uint64_t imageId, HRESULT hr);

    //  Waits, for a caller that uses the result, until isDone is set.
    HRESULT         WaitForIo          (const bool & isDone);

    //  Whether two paths are one file, after `..` and `.\` are resolved.
    static bool     IsSameFile         (const string & left, const string & right);

    //  Tells the shell a bay's disk changed, when a sink is installed. The one
    //  chokepoint every mutation path calls, so the door and its sounds cannot
    //  be lit from a path that forgot to.
    void           EmitBayChange (int slot, int drive, BayChange change);

    Entry                    m_entries[kSlotCount][kDriveCount];
    FlushSink                m_flushSink;
    ImageReader              m_imageReader;
    IdentityReader           m_identityReader;
    string                   m_emptyPath;

    IImageWatcher *          m_watcher      = nullptr;
    IDiskFileIo *            m_fileIo       = nullptr;
    bool                     m_isFileBacked = true;

    std::function<void ()>   m_restartCallback;
    string                   m_machineName;
    ReportSink               m_reportSink;
    AskSink                  m_askSink;
    RescueSink               m_rescueSink;
    BayChangeSink            m_bayChangeSink;
    DecisionSink             m_decisionSink;
    std::function<int64_t ()>  m_clock;
    std::function<time_t ()>   m_timestamp;
    std::function<void ()>     m_mediaChangeListener;

    bool                     m_isFlushHeld    = false;
    bool                     m_isReplaying    = false;
    ThreadOwnership          m_ownership;

    //  Starts at 1, so the first publish builds the bay table.
    uint64_t                 m_statusGeneration = 1;

    InlineDiskIoQueue        m_inlineQueue;
    IDiskIoQueue *           m_ioQueue = &m_inlineQueue;

    DiskStoreReload          m_reload  { *this };
    DiskStoreSave            m_save    { *this };
    DiskStoreSalvage         m_salvage { *this };
    DiskStoreMedia           m_media   { *this };
    DiskStoreMount           m_mount   { *this };
};