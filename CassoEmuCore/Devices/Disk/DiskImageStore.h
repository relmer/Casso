#pragma once

#include "Pch.h"

#include "DiskImage.h"
#include "MountedImageState.h"
#include "ChangePrompt.h"
#include "PreservedCopy.h"
#include "IImageWatcher.h"
#include "IDiskFileIo.h"
#include "MountDiagnosis.h"
#include "NibblizationLayer.h"
#include "BayChange.h"





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

//
//  What salvaging a bay would produce, and whether it is worth offering.
//  Read-only: producing one writes nothing and touches nothing.
//
//      isOffered      the disk is damaged AND has ordinary 16-sector
//                     structure. Both halves matter. A disk that is not
//                     damaged is not write-protected, so there is nothing to
//                     escape and a lossy copy would only lose data; a
//                     copy-protected disk has no standard sectors to salvage,
//                     and rebuilding it from sectors would destroy the very
//                     tracks that make it work.
//      totalSectors   sectors on the tracks this disk actually has -- 320 on a
//                     20-track disk, not a flat 560, or the count is a lie.
//      suggestedPath  "<name>.salvaged.woz" beside the original.
//
struct SalvageAssessment
{
    bool              isOffered     = false;
    int               totalSectors  = 0;
    DenibblizeReport  report;
    string            suggestedPath;
};





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
    HRESULT       Flush             (int slot, int drive);
    HRESULT       FlushAll          ();

    //  The same, for the last flush of the process, which can only ask through
    //  a blocking dialog. Separate from FlushAll so the moment is explicit at
    //  the call site rather than inferred from where it happens to be called.
    HRESULT       FlushAllForShutdown ();

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
    HRESULT       SalvageToFile (int slot, int drive, const string & path,
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

    void          SetFlushSink      (FlushSink sink) { m_flushSink = std::move (sink); }

    //  Read counterpart to SetFlushSink: redirects every backing-file read
    //  (Mount, and the write-protect patch's read-modify-write) to the
    //  caller's buffer, so a test can pair the two and exercise a genuine
    //  read-modify-write cycle without a real file.
    void          SetImageReader    (ImageReader reader) { m_imageReader = std::move (reader); }

    //  Replaces the filesystem stat behind the mount-time record and the
    //  pre-commit re-check.
    void          SetIdentityReader (IdentityReader reader) { m_identityReader = std::move (reader); }

    //  Where notification comes from. The shell builds the platform watcher and
    //  hands it over; REGISTERING AND DROPPING WATCHES IS THIS CLASS'S JOB,
    //  because mount-registers-a-watch is orchestration and orchestration is
    //  testable. Caller-owned and may be null, which is a session with no
    //  notification -- the check before every write still holds.
    void          SetImageWatcher (IImageWatcher * watcher) { m_watcher = watcher; }

    //  Where "is somebody else writing this right now" is answered. Optional:
    //  without it a pick-up cannot be deferred for a third-party writer, and
    //  the quiet period is the only debounce.
    void          SetFileIo (IDiskFileIo * fileIo) { m_fileIo = fileIo; }

    //  The machine as the user knows it, for the notices that mention it.
    //  "Apple //e" rather than "the Apple", which is not what is in front of
    //  them. Empty is allowed and the notices fall back to "the machine".
    void          SetMachineName (const string & name) { m_machineName = name; }

    //  Restarting the machine.
    //
    //  A CALLBACK RATHER THAN A CALL. A device-layer image store reaching
    //  machine lifecycle directly is a layering inversion; the decision stays
    //  here and the action belongs to the shell.
    void          SetMachineRestartCallback (std::function<void ()> cb) { m_restartCallback = std::move (cb); }

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

    void          SetChangeReportSink (ReportSink sink) { m_reportSink = std::move (sink); }

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

    void          SetAskSink (AskSink sink) { m_askSink = std::move (sink); }

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

    void          SetRescueSink (RescueSink sink) { m_rescueSink = std::move (sink); }

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
    using BayChangeSink = std::function<void (int slot, int drive, BayChange change)>;

    void          SetBayChangeSink (BayChangeSink sink) { m_bayChangeSink = std::move (sink); }

    //  The user answered a question this store asked.
    //
    //  ON THE THREAD THAT OWNS DISK WRITES, like every other entry point that
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
    void          SetClock (std::function<int64_t ()> clock) { m_clock = std::move (clock); }

    //  Where the wall-clock time in a preserved copy's NAME comes from.
    //
    //  A SECOND SEAM RATHER THAN A CONVERSION OF THE FIRST. The quiet period
    //  needs a monotonic count of milliseconds and a filename needs a calendar
    //  date; deriving one from the other would tie a timer to the user's clock
    //  changing under it.
    void          SetTimestampSource (std::function<time_t ()> source) { m_timestamp = std::move (source); }

    //  A change was noticed, from a watcher or stated by a writer.
    //
    //  CALLED FROM ANY THREAD and does no work beyond recording: the watcher
    //  runs on its own thread and the message channel on the UI thread, while
    //  acting on a change belongs to the thread that owns disk writes.
    void          NoteExternalChange (const string & path, ExternalChangeIntent intent);

    //  Act on whatever has settled. Called on the CPU thread at a moment with
    //  no disk operation in flight.
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

    //  Replaces `path` with `bytes` without ever leaving the target in a
    //  partially written state: the bytes land in a sibling temporary file
    //  that is verified in full, then renamed over the target. A failure at
    //  any step leaves the ORIGINAL file exactly as it was and reports it,
    //  which is what a flush needs -- opening the target directly truncates
    //  it before the first byte is written, so a write that then fails
    //  (a full volume, a disconnected share) destroys the only copy.
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

    //  Why a mount failed, in the user's terms: the file named, then the
    //  diagnosis worded as a sentence about it. Pure -- no filesystem access
    //  -- and public because the wording is the observable half of a failed
    //  mount, and the only half worth testing on its own.
    //
    //  It takes the diagnosis rather than re-deriving one from the path. The
    //  path can only ever answer "is this an extension we read", which left
    //  every other refusal sharing one sentence that named none of them.
    static wstring  FormatMountFailureMessage (const string & path,
                                               const MountDiagnosis & diagnosis);

    //  Why a load of these bytes was refused, from what the bytes and the
    //  format alone can settle. Static and pure, so a test can ask it about a
    //  buffer without mounting anything.
    static MountDiagnosis  ClassifyLoadFailure (DiskFormat fmt, const vector<Byte> & bytes);
private:
    struct Entry
    {
        unique_ptr<DiskImage>  image;
        string                 path;
        DiskFormat             format  = DiskFormat::Dsk;
        bool                   mounted = false;

        //  Whether salvage applies to this image, decided once at mount.
        //  Answering it needs a full decode for a damaged disk, which is far
        //  too much work to repeat every time a menu is drawn.
        bool                   salvageOffered = false;

        //  What this bay knows about the file behind it. Set at mount, cleared
        //  at eject, refreshed after every commit this store makes.
        MountedImageState      sharedState;

        //  Which bay this is.
        //
        //  CARRIED ON THE ENTRY BECAUSE FlushEntry NEEDS IT AND HAS ONLY THIS.
        //  Every message about a disk names the drive it is in, and threading
        //  a pair of ints through five call sites to reach one function is a
        //  worse answer than the entry knowing where it lives.
        int                    slot    = 0;
        int                    drive   = 0;
    };

    // Every public accessor takes a caller-supplied slot/drive pair, so each
    // one range-checks before GetEntry() indexes the fixed array.
    static bool   IsValidBay        (int slot, int drive);

    //  Moves a mounted bay onto a different file without disturbing the disk
    //  in it. The image, and everything the guest can observe, is untouched;
    //  what changes is which file the bay reads and writes from here on.
    //
    //  THIS IS A SAVE-AS, NOT A MOUNT. Nothing is loaded, so a guest mid-write
    //  sees nothing at all.
    HRESULT       RepointBayToFile  (int slot, int drive, const string & newPath);

    //  How a flush failure can be put to the user, which differs by what is
    //  still standing when it happens.
    //
    //  THE DISCRIMINATOR IS NOT WHICH FUNCTION FLUSHED. What matters is
    //  whether there is a bay still holding the disk, a pump still running,
    //  and a thread that can raise a file dialog -- and the three moments
    //  below have different answers to those.
    enum class FlushMoment
    {
        //  The machine is running and the bay stays: a spindown, an explicit
        //  flush, a write-protect change, a machine switch, a soft reset. The
        //  question goes to the shell and is answered later, and nothing is
        //  lost while it stands.
        Running,

        //  The disk is coming out. The question goes the same way, but the
        //  eject waits for it: emptying the bay is what would destroy a disk
        //  whose copy could not be written, so it does not happen until the
        //  user has said which way they want it.
        Ejecting,

        //  The process is on its way out. There is no pump left to deliver a
        //  posted question and no CPU thread to act on the answer -- but this
        //  runs on the UI thread with its apartment still up, so a blocking
        //  file dialog still works. That is the one route left, and asking
        //  through it is synchronous.
        ShuttingDown,
    };

    Entry &       GetEntry          (int slot, int drive);
    const Entry & GetEntry          (int slot, int drive) const;
    HRESULT       FlushEntry        (Entry & entry, FlushMoment moment);
    HRESULT       FlushEveryBay     (FlushMoment moment);

    //  Tells the user the guest's version could not be written beside the
    //  file that displaced it -- as a question where one can still be
    //  answered, and as a notice where it cannot.
    void          ReportPreserveFailure (Entry & entry, FlushMoment moment,
                                         const string & attemptedPath, HRESULT hrKeep,
                                         const string & original);

    //  Asks the shell where to put a disk on the way out and writes it there,
    //  without returning until it is done. Only ever called at ShuttingDown.
    bool          RescueOnTheWayOut (Entry & entry, const string & original);

    //  Why a disk's changes are going, said where claiming they are safe would
    //  be false.
    static wstring FormatDiscardedWritesMessage (const string & path,
                                                 const string & attemptedPath,
                                                 HRESULT        reason);

    //  Whether some other bay already reads and writes this file, and which
    //  drive has it. The bay being mounted into is excluded, because putting
    //  the same file back into the same drive is how a re-mount works.
    bool          IsFileInAnotherBay (const string & path, int exceptSlot, int exceptDrive,
                                      int & outDrive);

    // Routes through m_imageReader when a test has installed one, so the
    // read and write seams stay symmetric.
    HRESULT       ReadImageFile     (const string & path, vector<Byte> & bytes) const;

    // Recover what can be recovered. Decode only -- this is all the counts
    // need, and it is the half AssessSalvage can afford to run.
    HRESULT       DecodeForSalvage (Entry & entry, vector<Byte> & outSectors,
                                    DenibblizeReport & report);

    // Decode, then rebuild the result as a WOZ. Only the write path needs the
    // rebuild, so only the write path pays for it.
    HRESULT       BuildSalvagedImage (Entry & entry, vector<Byte> & outBytes,
                                      DenibblizeReport & report);

    // Builds the user-facing "could not save" message from the mount path;
    // handed to CHRN/CBRN in FlushEntry on a genuine persist failure. When a
    // recovery image was written, its path is named so the user can retrieve
    // the session rather than only being told what was lost.
    //
    //  `reason` IS PRINTED, code and system text both. The notice used to name
    //  no cause at all and then advise switching to .woz, which is wrong
    //  advice for a folder that refused the write; the system's own words for
    //  the failure are the useful part.
    //
    //  recoveryPath is optional: the write-protect path has no recovery copy
    //  to name, and the message says something useful either way.
    static wstring FormatFlushLossMessage (const string & path,
                                           HRESULT        reason,
                                           const string & recoveryPath = string());

    // Preserves an image that could not be serialized to its own format,
    // beside the original and losslessly. Leaves the original untouched.
    HRESULT        TryWriteRecoveryImage  (Entry & entry, string & outPath);

    // Enough room to step past collisions without ever spinning.
    static constexpr int  kMaxRecoveryNameAttempts = 64;

    // Why a damaged image will not be written to. A checksum mismatch
    // write-protects the image for the session, so the file is never
    // rewritten and the damage it carries stays detectable.
    static wstring FormatDamagedImageMessage (const string & path);

    // Why a salvaged copy could not be written. Names the copy, and says the
    // original is untouched -- which is the thing the user will worry about.
    static wstring FormatSalvageFailedMessage (const string & path);

    //  The identity `path` has right now: through the seam when one is
    //  installed, and from the filesystem when none is.
    ImageIdentity  ReadIdentity (const string & path) const;

    //  Why a commit was refused because the file changed underneath it. Names
    //  the image, and says the writes are still held rather than lost, which is
    //  the part the user will worry about.
    static wstring FormatExternalChangeMessage (const string & path);

    //  Everything one bay's settled change leads to.
    void           ApplyPendingReloadToBay (int slot, int drive);

    //  Carries out what was decided. Split from deciding so the decision has
    //  one shape whether it came from the policy or from the user.
    //
    //  `author` COMES FROM THE CALLER BECAUSE ONLY THE CALLER STILL KNOWS. A
    //  bay's pending record holds the newest change and nothing older, so by
    //  the time this runs it can describe a write that landed after the one
    //  being acted on. Reading it here named whoever wrote last.
    void           CarryOutChangeAction (int slot, int drive, ChangeAction action,
                                         const vector<Byte> & bytes, ChangeAuthor author);

    //  Writes what the bay currently holds to a preserved copy beside the
    //  original, and reports where it went.
    //
    //  SERIALIZED FROM THE MOUNTED IMAGE rather than copied from the file: the
    //  point of preserving it is that the file no longer holds this version.
    HRESULT        SaveLoadedImage (Entry & entry, string & outPath);

    //  A preserved-copy path nothing is sitting at yet.
    //
    //  THE COLLISION LOOP IS NOT OPTIONAL. Two conflicts on one image inside a
    //  second is exactly what a build loop produces, and a one-second stamp
    //  cannot keep the accumulate-rather-than-overwrite promise on its own.
    HRESULT        FindFreePreservedPath (const string & imagePath, string & outPath) const;

    //  Whether something is already at `path`, through the file seam when one
    //  is installed and through the filesystem when none is.
    bool           DoesPathExist (const string & path) const;

    //  Writes bytes to a path, through the flush sink when one is installed so
    //  a test captures preserved copies the same way it captures flushes.
    HRESULT        WritePreserved (const string & path, const vector<Byte> & bytes);

    //  Empties a bay whose file is gone, WITHOUT trying to flush it. The
    //  ordinary eject flushes first, which here would either fail or write the
    //  disk back to a path the user has just been told no longer exists.
    void           EjectLostImage (int slot, int drive);

    //  Replaces a mounted image's contents WITHOUT flushing what it held.
    //
    //  A FLUSH HERE WOULD WRITE THE OLD DISK OVER THE NEW FILE, which is the
    //  precise loss this feature exists to prevent. The new bytes are loaded
    //  into a fresh image first, so a load that fails leaves the mounted disk
    //  exactly as it was.
    HRESULT        MountExternallyModifiedDisk (int slot, int drive, const vector<Byte> & bytes);

    //  Registers and drops the watch on a bay's directory.
    //
    //  A DIRECTORY IS DROPPED ONLY WHEN NO OTHER BAY STILL NEEDS IT. Two disks
    //  out of one folder are ordinary, and ejecting the first must not blind
    //  the second.
    void           BeginWatching (int slot, int drive);
    void           EndWatching   (int slot, int drive);

    //  Milliseconds now, through the injected clock when one is installed.
    int64_t        GetNowMs () const;

    //  Tells the shell a bay's disk changed, when a sink is installed. The one
    //  chokepoint every mutation path calls, so the door and its sounds cannot
    //  be lit from a path that forgot to.
    void           EmitBayChange (int slot, int drive, BayChange change);

    Entry                    m_entries[kSlotCount][kDriveCount];
    FlushSink                m_flushSink;
    ImageReader              m_imageReader;
    IdentityReader           m_identityReader;
    string                   m_emptyPath;

    IImageWatcher *          m_watcher  = nullptr;
    IDiskFileIo *            m_fileIo   = nullptr;

    std::function<void ()>   m_restartCallback;
    string                   m_machineName;
    ReportSink               m_reportSink;
    AskSink                  m_askSink;
    RescueSink               m_rescueSink;
    BayChangeSink            m_bayChangeSink;
    std::function<int64_t ()>  m_clock;
    std::function<time_t ()>   m_timestamp;

    //  Guards the pending records alone. A watcher thread records a change
    //  while the CPU thread reads it, and those two fields are the whole of
    //  what crosses between them -- the image, the path and the identity are
    //  touched only by the thread that owns disk writes.
    mutable std::mutex       m_pendingMutex;
};
