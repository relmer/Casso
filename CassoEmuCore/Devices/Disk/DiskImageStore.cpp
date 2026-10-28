#include "Pch.h"

#include "DiskImageStore.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Core/TextEncoding.h"
#include "ChangePrompt.h"
#include "CommitPlan.h"
#include "Seams/Win32DiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::DiskImageStore
//
////////////////////////////////////////////////////////////////////////////////

DiskImageStore::DiskImageStore()
{
    int  slot  = 0;
    int  drive = 0;



    //  Each entry learns where it lives, once, so anything holding one can
    //  name the drive without being told.
    for (slot = 0; slot < kSlotCount; slot++)
    {
        for (drive = 0; drive < kDriveCount; drive++)
        {
            m_entries[slot][drive].slot  = slot;
            m_entries[slot][drive].drive = drive;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  At
//
////////////////////////////////////////////////////////////////////////////////

DiskImageStore::Entry & DiskImageStore::GetEntry (int slot, int drive)
{
    return m_entries[slot][drive];
}


const DiskImageStore::Entry & DiskImageStore::GetEntry (int slot, int drive) const
{
    return m_entries[slot][drive];
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSourceFormatByExtension
//
//  Which loader reads a file with this name. Lower-cased ASCII extension
//  match; anything unknown answers E_FAIL so callers can route unsupported
//  types explicitly.
//
//  IT ANSWERS FOR FILES THAT ALREADY EXIST, which is what "source" means here
//  and why the name changed from DetectFormatByExtension. What a NEW image may
//  be written as is a different and shorter list, held beside the blank-disk
//  builder; reaching for this one to answer that question offers containers
//  this tool can read and cannot produce.
//
//  IT ANSWERS THE CONTAINER FAMILY AND NOTHING MORE. For the sector formats
//  the extension settled the geometry too, which made the old name fair. It
//  does not for nibble images: .nib and .nb2 share one enumerator and the track
//  size comes from the file's length. A caller that has a DiskFormat does NOT
//  thereby know how the file is laid out, and must not infer it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::GetSourceFormatByExtension (const string & path, DiskFormat & outFmt)
{
    HRESULT   hr       = S_OK;
    size_t    pos      = 0;
    string    ext;
    size_t    pathLen  = 0;



    pos     = path.find_last_of ('.');
    pathLen = path.size();

    CBR (pos != string::npos && pos + 1 < pathLen);

    ext = path.substr (pos + 1);

    for (char & ch : ext)
    {
        ch = static_cast<char> (tolower (static_cast<unsigned char> (ch)));
    }

    if (ext == "dsk")
    {
        outFmt = DiskFormat::Dsk;
    }
    else if (ext == "do")
    {
        outFmt = DiskFormat::Do;
    }
    else if (ext == "po")
    {
        outFmt = DiskFormat::Po;
    }
    else if (ext == "woz")
    {
        outFmt = DiskFormat::Woz;
    }
    else if (ext == "nib" || ext == "nb2")
    {
        //  Both names, one format. Which of the two track sizes the file holds
        //  is decided by its LENGTH at load, not here -- either size circulates
        //  under either name, so the extension cannot be trusted to say.
        outFmt = DiskFormat::Nib;
    }
    else
    {
        hr = E_FAIL;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsMountableImageExtension
//
//  The question every file filter in the product should be asking, answered
//  by the same code that routes the mount.
//
//  There used to be a second list. `Casso/Ui/DriveWidgetState.h` carried its
//  own array of extensions for the drag-and-drop filter and the disk picker,
//  and it held one -- `.nib` -- that the routing below has never handled. A
//  file that passed the filter and then failed to load produced no message at
//  all: the mount runs on the CPU thread and its result is dropped, so the
//  disk simply never appeared. Asking the router directly is what makes that
//  class of disagreement unrepresentable rather than merely fixed.
//
//  A rejected extension is an ordinary answer here, not a failure, so the
//  detector's E_FAIL is read as `false` and goes no further.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::IsMountableImageExtension (const string & path)
{
    HRESULT     hr  = S_OK;
    DiskFormat  fmt = DiskFormat::Dsk;



    hr = GetSourceFormatByExtension (path, fmt);

    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsMountableImageExtension  (wide)
//
//  The overload the interface actually calls. Narrowing happens here, once,
//  rather than at each filter -- a per-call-site conversion is the seam the
//  two answers would drift apart through next.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::IsMountableImageExtension (const wstring & path)
{
    string  narrowed = fs::path (path).string();



    return IsMountableImageExtension (narrowed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountFromBytes
//
//  Test-friendly mount path that bypasses the host filesystem. The
//  virtualPath is used purely as the round-trip identifier; production
//  code uses Mount() which reads from disk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::MountFromBytes (
    int                    slot,
    int                    drive,
    const string        &  virtualPath,
    DiskFormat             fmt,
    const vector<Byte>  &  bytes)
{
    MountDiagnosis  ignored;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    return MountFromBytes (slot, drive, virtualPath, fmt, bytes, ignored);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountFromBytes
//
//  The same mount, saying why it refused. A load that fails is classified from
//  the bytes it was given, which is where the answer is: the store knows how
//  many bytes arrived and which container the name promised, and the WOZ
//  loader knows whether its header was ever there.
//
//  A mount that succeeds calls the bay-change sink with Inserted, as Mount
//  does, so a controller attached by that sink reads the new image.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::MountFromBytes (
    int                    slot,
    int                    drive,
    const string        &  virtualPath,
    DiskFormat             fmt,
    const vector<Byte>  &  bytes,
    MountDiagnosis      &  outDiagnosis)
{
    HRESULT       hr = S_OK;
    FlushOutcome  outcome;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    hr = ReplaceBay (slot, drive, virtualPath, fmt, bytes, outDiagnosis, outcome);
    ReportReplaceOutcome (slot, drive, virtualPath, hr, outcome);
    CHR (hr);

    EmitBayChange (slot, drive, BayChange::Inserted);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::LoadImage
//
//  The image the bytes make, built apart from every bay, so the bay is as it
//  was when the file does not load. On failure, outDiagnosis indicates why.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::LoadImage (DiskFormat fmt, const vector<Byte> & bytes, const string & path,
                                   unique_ptr<DiskImage> & outImage, MountDiagnosis & outDiagnosis)
{
    HRESULT  hr       = S_OK;
    bool     isLoaded = false;



    outImage = make_unique<DiskImage>();
    outImage->LoadFromBytes (fmt, bytes, path);

    isLoaded = outImage->IsLoaded();

    if (!isLoaded)
    {
        outImage.reset();
        outDiagnosis = ClassifyLoadFailure (fmt, bytes);
    }

    CBR (isLoaded);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::SaveBeforeReplace
//
//  Saves the disk in the bay before a mount over it, and declines the mount
//  when a guest write is in no file afterward. A recovery copy that is
//  current lets another file in, because the copy has every write; it does
//  not let the same file be read again, which would put the file's older
//  contents in place of the session.
//
//  THE DECLINE TESTS THE DATA, NOT THE SAVE'S RESULT. A save that writes
//  nothing, as during a replay, returns success with the image dirty, and a
//  test of the result would retire that disk from its bay.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::SaveBeforeReplace (Entry & entry, const string & newPath,
                                           MountDiagnosis & outDiagnosis, FlushOutcome & outOutcome)
{
    HRESULT  hr         = S_OK;
    HRESULT  hrSave     = S_OK;
    HRESULT  hrDecline  = S_OK;
    bool     isSameFile = false;
    bool     canReplace = false;



    BAIL_OUT_IF (!entry.mounted || entry.image == nullptr, S_OK);

    hrSave = m_save.FlushEntry (entry, FlushMoment::Replacing, outOutcome);
    IGNORE_RETURN_VALUE (hrSave, S_OK);

    isSameFile = IsSameFile (entry.path, newPath);
    canReplace = !entry.image->IsDirty() || (!isSameFile && m_save.IsRecoveryCurrent (entry));
    hrDecline  = FAILED (outOutcome.saveError) ? outOutcome.saveError : E_PENDING;

    if (!canReplace)
    {
        outDiagnosis.failure       = MountFailure::UnsavedWrites;
        outDiagnosis.saveError     = outOutcome.saveError;
        outDiagnosis.keptPath      = entry.path;
        outDiagnosis.isSameFile    = isSameFile;
        outDiagnosis.occupiedDrive = entry.drive;
        outDiagnosis.wasReported   = entry.sharedState.IsSaveFailureReported();

        entry.sharedState.MarkSaveFailureReported (string());
    }

    CBREx (canReplace, hrDecline);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::ReplaceBay
//
//  A mount over a bay: the new image is loaded first, then the disk in the
//  bay is saved, and the two are swapped only when the save allows it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::ReplaceBay (int slot, int drive, const string & path, DiskFormat fmt,
                                    const vector<Byte> & bytes, MountDiagnosis & outDiagnosis,
                                    FlushOutcome & outOutcome)
{
    HRESULT                hr      = S_OK;
    bool                   isValid = IsValidBay (slot, drive);
    string                 target  = path;
    unique_ptr<DiskImage>  loaded;



    outDiagnosis              = MountDiagnosis();
    outDiagnosis.format       = fmt;
    outDiagnosis.fileByteSize = bytes.size();

    CBRAEx (isValid, E_INVALIDARG);

    hr = LoadImage (fmt, bytes, target, loaded, outDiagnosis);
    CHR (hr);

    hr = SaveBeforeReplace (GetEntry (slot, drive), target, outDiagnosis, outOutcome);
    CHR (hr);

    SwapInImage (GetEntry (slot, drive), std::move (loaded), target, fmt);

Error:
    //  A disk that stays in the drive is stated in the decline's text.
    if (FAILED (hr) && isValid && GetEntry (slot, drive).mounted && outDiagnosis.keptPath.empty())
    {
        outDiagnosis.keptPath = GetEntry (slot, drive).path;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::SwapInImage
//
//  Puts a loaded image in a bay. The disk that was there is retired, its
//  watch dropped first because the watch is keyed by its directory, and the
//  salvage question is settled once, here, where the cost is paid by a mount
//  the user already asked for. Undamaged images settle it for free; a damaged
//  one is decoded once instead of on every menu draw.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::SwapInImage (Entry & entry, unique_ptr<DiskImage> loaded,
                                  const string & path, DiskFormat fmt)
{
    SalvageAssessment  assessment;
    HRESULT            hrAssess = S_OK;



    if (entry.mounted)
    {
        m_reload.EndWatching (entry.slot, entry.drive);
        m_media.RetireBay (entry);
    }

    entry.image   = std::move (loaded);
    entry.path    = path;
    entry.format  = fmt;
    entry.mounted = true;

    hrAssess             = m_salvage.AssessSalvage (entry.slot, entry.drive, assessment);
    entry.salvageOffered = SUCCEEDED (hrAssess) && assessment.isOffered;

    MarkStatusChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::ReportReplaceOutcome
//
//  Reports a guest copy the save before a mount moved off a file another
//  program changed. The text depends on how the mount ended: the same file
//  reloaded, another file inserted, or no swap at all, with the bay on the
//  copy.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::ReportReplaceOutcome (int slot, int drive, const string & newPath, HRESULT hr,
                                           const FlushOutcome & outcome)
{
    ChangePrompt  prompt;



    if (outcome.movedTo.empty() || !m_reportSink)
    {
        return;
    }

    if (FAILED (hr))
    {
        prompt = ChangePrompt::ComposeConflictReport (outcome.movedFrom, drive, outcome.movedTo);
    }
    else if (IsSameFile (outcome.movedFrom, newPath))
    {
        prompt = ChangePrompt::ComposeReloadReport (outcome.movedFrom, drive, false, m_machineName,
                                                    ChangeAuthor::AnotherProgram, outcome.movedTo, true);
    }
    else
    {
        prompt = ChangePrompt::ComposeReplacedReport (outcome.movedFrom, drive, outcome.movedTo, newPath);
    }

    m_reportSink (slot, drive, prompt);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::IsSameFile
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::IsSameFile (const string & left, const string & right)
{
    return MountedImageState::IsSamePath (fs::path (left).lexically_normal().string(),
                                          fs::path (right).lexically_normal().string());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::AreAllWritesSaved
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::AreAllWritesSaved()
{
    int   slot    = 0;
    int   drive   = 0;
    bool  isSaved = true;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    for (slot = 0; slot < kSlotCount; slot++)
    {
        for (drive = 0; drive < kDriveCount; drive++)
        {
            isSaved = isSaved && m_save.AreWritesSaved (m_entries[slot][drive]);
        }
    }

    return isSaved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Mount
//
//  Production mount path. Reads the file from the host filesystem then
//  routes through the appropriate loader by extension.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::Mount (int slot, int drive, const string & path)
{
    MountDiagnosis  ignored;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    return Mount (slot, drive, path, ignored);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::IsFileInAnotherBay
//
//  Whether some OTHER bay is already reading and writing this file.
//
//  EVERY SLOT, NOT JUST THIS ONE. Two controllers are as capable of holding one
//  file between them as two drives on one controller are, and the damage is
//  identical.
//
//  A BAY WITH NO PATH IS NOT A MATCH. An empty bay and a bay built from bytes
//  that never came off disk both carry nothing to compare, and an empty string
//  matches another empty string.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::IsFileInAnotherBay (const string & path, int exceptSlot, int exceptDrive,
                                         int & outDrive)
{
    int   slot  = 0;
    int   drive = 0;
    bool  found = false;



    outDrive = -1;

    if (path.empty())
    {
        return false;
    }

    for (slot = 0; slot < kSlotCount && !found; slot++)
    {
        for (drive = 0; drive < kDriveCount && !found; drive++)
        {
            const Entry &  other = m_entries[slot][drive];

            if (slot == exceptSlot && drive == exceptDrive)
            {
                continue;
            }

            if (other.mounted && MountedImageState::IsSamePath (other.path, path))
            {
                outDrive = drive;
                found    = true;
            }
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Mount
//
//  The same mount, saying why it refused. The two failures BEFORE any loader
//  runs are settled here and nowhere else: a name no loader claims, and bytes
//  that never arrived. Neither is visible further in -- the loaders are handed
//  a buffer and a format, and by then the file name and the read are history.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::Mount (int slot, int drive, const string & path,
                               MountDiagnosis & outDiagnosis)
{
    HRESULT       hr          = S_OK;
    DiskFormat    fmt         = DiskFormat::Dsk;
    bool          elsewhere   = false;
    bool          isSameFile  = false;
    bool          isUnchanged = false;
    bool          isValid     = IsValidBay (slot, drive);
    FlushOutcome  outcome;
    vector<Byte>  bytes;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    outDiagnosis = MountDiagnosis();

    CBRAEx (isValid, E_INVALIDARG);

    //  ONE FILE, ONE DRIVE, and refused before a byte is read -- the bytes are
    //  not what is wrong with it. Two bays on one file each hold their own
    //  DiskImage, and a flush writes the whole image, so from the guest's
    //  first write each drive overwrites whatever the other saved. One
    //  external change then raises the conflict twice, writes two rescue
    //  copies and puts two dialogs up in a row.
    //
    //  THE SAME FILE BACK INTO THE SAME DRIVE IS NOT THIS. Re-mounting is how
    //  a machine switch and a reload put the disk back, so the bay being
    //  mounted into is the one bay this does not look at.
    elsewhere = IsFileInAnotherBay (path, slot, drive, outDiagnosis.occupiedDrive);

    CBRFEx (!elsewhere, E_INVALIDARG,
            outDiagnosis.failure = MountFailure::AlreadyMounted);

    hr = GetSourceFormatByExtension (path, fmt);
    CHRF (hr, outDiagnosis.failure = MountFailure::UnknownExtension);

    outDiagnosis.format = fmt;

    //  THE FILE IN THE BAY IS SAVED BEFORE IT IS READ. Reading first would load
    //  the file without the writes the save is about to put in it, and the next
    //  save would write that older disk back over them. When nothing is unsaved
    //  after the save and the file is as the bay last read or wrote it, the
    //  mounted image is the file, so it stays: nothing is read or swapped, and
    //  the history gets no media change.
    {
        Entry &  entry = GetEntry (slot, drive);

        isSameFile = entry.mounted && IsSameFile (entry.path, path);

        if (isSameFile)
        {
            hr = SaveBeforeReplace (entry, path, outDiagnosis, outcome);
            CHR (hr);

            isUnchanged = !entry.image->IsDirty()
                       && entry.sharedState.GetIdentity().recorded
                       && entry.sharedState.GetIdentity().Matches (ReadIdentity (path));
        }
    }

    BAIL_OUT_IF (isUnchanged, S_OK);

    hr = ReadImageFile (path, bytes);
    CHRF (hr, outDiagnosis.failure = MountFailure::FileUnreadable);

    hr = ReplaceBay (slot, drive, path, fmt, bytes, outDiagnosis, outcome);
    CHR (hr);

    //  Recorded AFTER the read and only on success, matching the command
    //  line's read-then-stamp order. A stamp taken before the read could
    //  describe a file the loaded bytes did not come from, and a stamp
    //  recorded for a mount that failed would sit on an empty bay.
    GetEntry (slot, drive).sharedState.Mount (ReadIdentity (path));

    //  Mount registers the watch. It is orchestration rather than platform
    //  work, so it lives here and not in whatever built the watcher.
    m_reload.BeginWatching (slot, drive);

    //  A disk went in. The shell wires the controller and lights the door from
    //  here rather than from the path that called Mount, so a command-line
    //  mount and a picker mount and a machine-switch remount all react alike.
    EmitBayChange (slot, drive, BayChange::Inserted);

    //  After the bay change, which points the drive at the new disk: reverse
    //  execution snapshots the machine here, and the drive's head state must
    //  already be what the guest will run on.
    NotifyMediaChanged();

Error:
    if (isValid)
    {
        //  A disk that stays in the drive is stated in the decline's text.
        if (FAILED (hr) && GetEntry (slot, drive).mounted && outDiagnosis.keptPath.empty())
        {
            outDiagnosis.keptPath = GetEntry (slot, drive).path;
        }

        ReportReplaceOutcome (slot, drive, path, hr, outcome);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountRestored
//
//  Puts a disk saved in a machine state back in its bay: the image is built
//  from the bytes the state carries, never read from the file, so the guest
//  sees what it saw at the save even if the file has changed since. The
//  bay still answers to the file's path, which is where a later guest write
//  is flushed, and the file is watched from here like any mounted image.
//
//  THE IMAGE IS LOADED FIRST, so the bay is as it was when the bytes do not
//  load. A disk in the bay is retired without a save, so the caller saves
//  first, and the swap does not happen while a guest write is in no file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::MountRestored (
    int                    slot,
    int                    drive,
    const string        &  path,
    DiskFormat             fmt,
    const vector<Byte>  &  bytes)
{
    HRESULT                hr           = S_OK;
    bool                   isValid      = IsValidBay (slot, drive);
    bool                   isSafeToSwap = false;
    unique_ptr<DiskImage>  loaded;
    MountDiagnosis         diagnosis;
    ImageIdentity          identity;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    CBRAEx (isValid, E_INVALIDARG);

    hr = LoadImage (fmt, bytes, path, loaded, diagnosis);
    CHR (hr);

    {
        Entry &  entry = GetEntry (slot, drive);

        isSafeToSwap = m_save.AreWritesSaved (entry);
        CBREx (isSafeToSwap, E_UNEXPECTED);

        SwapInImage (entry, std::move (loaded), path, fmt);
    }

    identity = ReadIdentity (path);

    if (identity.recorded)
    {
        GetEntry (slot, drive).sharedState.Mount (identity);
        m_reload.BeginWatching (slot, drive);
    }

    EmitBayChange (slot, drive, BayChange::Inserted);
    NotifyMediaChanged();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClassifyLoadFailure
//
//  What a refused load was, from the bytes and the format alone.
//
//  THE FALLBACK IS Unrecognized RATHER THAN None. A load that failed for a
//  reason none of the tests below names must not report "nothing went wrong":
//  that is the degraded state that reads as a healthy one, and it would surface
//  as a mount failure whose message says no failure occurred. Today nothing
//  reaches it -- the sector path fails only on length and the WOZ path is
//  classified by its own loader -- and it is here so that a future loader with
//  a new refusal is reported generically instead of falsely.
//
////////////////////////////////////////////////////////////////////////////////

MountDiagnosis DiskImageStore::ClassifyLoadFailure (DiskFormat fmt, const vector<Byte> & bytes)
{
    MountDiagnosis  diagnosis;
    HRESULT         hrGeometry = S_OK;
    size_t          size       = bytes.size();
    size_t          trackSize  = 0;
    bool            hasNibble  = false;
    bool            isSized    = size == (size_t) NibblizationLayer::kImageByteSize;



    diagnosis.format       = fmt;
    diagnosis.fileByteSize = size;
    diagnosis.failure      = MountFailure::Unrecognized;

    // Emptiness outranks everything: a zero-byte file is the wrong size for
    // every container, and "there is nothing in it" is the more useful thing
    // to say than an arithmetic comparison against 143,360.
    if (size == 0)
    {
        diagnosis.failure = MountFailure::EmptyFile;
    }
    else if (fmt == DiskFormat::Woz)
    {
        diagnosis.failure = WozLoader::ClassifyLoadFailure (bytes);
    }
    else if (fmt == DiskFormat::Nib)
    {
        //  Two valid lengths rather than one, and a content check that is the
        //  only one this format allows: a file carrying no high bit anywhere
        //  cannot be read by any drive. Anything past that is indistinguishable
        //  from a real image without booting it.
        hasNibble  = NibbleImageCodec::HasAnyNibble (bytes);
        hrGeometry = NibbleImageCodec::ResolveGeometry (size, trackSize);
        isSized    = SUCCEEDED (hrGeometry);

        if (!isSized)
        {
            diagnosis.failure = MountFailure::WrongSizeForNibble;
        }
        else if (!hasNibble)
        {
            diagnosis.failure = MountFailure::NotANibbleStream;
        }
    }
    else if (!isSized)
    {
        diagnosis.failure = MountFailure::WrongSizeForFormat;
    }

    return diagnosis;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::MakeRecoveryPath
//
//  Recovery images sit beside the original under its own name, so the pairing
//  is obvious in a folder listing. WOZ because it is the only format that can
//  hold what the sector formats could not -- including the very track whose
//  content caused the refusal.
//
//  The attempt index exists so another file at a recovery name is never
//  overwritten; losing a previous rescue to a later one would repeat the
//  mistake being fixed. The one copy replaced is the copy this mount wrote, by
//  TryWriteRecoveryImage, and the replacement is always a later state of the
//  same session.
//
////////////////////////////////////////////////////////////////////////////////

string DiskImageStore::MakeRecoveryPath (const string & imagePath, int attempt)
{
    fs::path  base = fs::path (imagePath);
    string    stem;



    base.replace_extension();
    stem = base.string();

    if (attempt <= 0)
    {
        return stem + ".recovered.woz";
    }

    return stem + ".recovered." + std::to_string (attempt) + ".woz";
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteFileAtomically
//
//  DurableCommit over the real file system, for a caller with no store: the
//  bytes go to a sibling temporary, which is flushed to storage and then
//  replaces the target in one step.
//
//  THE TEMPORARY'S NAME HAS THIS PROCESS'S INVOCATION TAG, and an existing
//  name is stepped past. With a fixed suffix, two emulators with one image
//  mounted wrote into each other's temporary, and one renamed the other's
//  bytes over the target. Reusing the temporary of a writer killed mid-commit
//  would append this image to the remains of that one.
//
//  EACH FAILURE RETURNS ITS WIN32 CODE. The save-failure notice prints the code
//  and the system's text for it, so a folder without write access, a read-only
//  target and a full disk each report a distinct code, never E_FAIL.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::WriteFileAtomically (const string & path, const vector<Byte> & bytes)
{
    HRESULT               hr       = S_OK;
    bool                  hasPath  = !path.empty();
    Win32DiskFileIo       fileIo;
    CommitPlan::Progress  progress = {};



    CBRAEx (hasPath, E_INVALIDARG);

    hr = DurableCommit::Commit (fileIo, path, bytes, GetInvocationTag(), CommitMode::Replace, progress);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Flush / FlushAll
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::Flush (int slot, int drive)
{
    HRESULT   hr = S_OK;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    CBRAEx (slot >= 0 && slot < kSlotCount && drive >= 0 && drive < kDriveCount, E_INVALIDARG);

    hr = m_save.FlushEntry (GetEntry (slot, drive), FlushMoment::Running);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetImageWriteProtect
//
//  Change a mounted WOZ's write-protect flag in its backing file, by patching
//  the one byte that holds it. The flag lives inside the file, so the change
//  has to be written; the question is how much of the file gets rewritten to
//  carry it. The answer here is one byte plus the header CRC.
//
//  The path this replaces sent the flag through DiskImage::Serialize, the full
//  rebuild-from-model writer. Everything that writer could not reproduce was
//  lost on a menu click -- no guest write, no emulation, just a click -- and
//  it fired in both directions, so un-protecting a preservation dump before
//  writing to it rewrote it too. Now the guarantee does not depend on the
//  writer reproducing every field: the bytes are never parsed, so they cannot
//  be damaged.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::SetImageWriteProtect (int slot, int drive, bool writeProtected)
{
    HRESULT       hr        = S_OK;
    bool          bayOk     = false;
    bool          isWoz     = false;
    bool          hasPath   = false;
    bool          hasImage  = false;
    bool          isDamaged = false;
    vector<Byte>  bytes;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    bayOk = IsValidBay (slot, drive);
    CBRAEx (bayOk, E_INVALIDARG);

    {
        Entry &  entry = GetEntry (slot, drive);

        hasImage = (entry.mounted && entry.image != nullptr);
        CBREx (hasImage, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

        isWoz = (entry.format == DiskFormat::Woz);
        CBRAEx (isWoz, E_INVALIDARG);

        hasPath = !entry.path.empty();
        CBR (hasPath);

        // A damaged image is refused outright. Patching the flag byte
        // recomputes the header checksum, and that checksum failing to match
        // IS the damage report -- so the one write that is otherwise harmless
        // is the one write that would destroy the evidence.
        isDamaged = entry.image->IsDamaged();
        CBRN (!isDamaged, DiskStoreText::FormatDamagedImageMessage (entry.path, entry.image->HasDamagedTracks()).c_str());

        // Guest writes go out FIRST, while the image still accepts a flush.
        // Patching the flag byte afterwards edits a file that already holds
        // them; doing it the other way round would strand them behind the
        // gate this call is about to close.
        hr = m_save.FlushEntry (entry, FlushMoment::Running);
        CHR (hr);

        hr = ReadImageFile (entry.path, bytes);
        CHRN (hr, DiskStoreText::FormatFlushLossMessage (entry.path, hr).c_str());

        hr = WozLoader::SetWriteProtectFlag (bytes, writeProtected);
        CHRN (hr, DiskStoreText::FormatFlushLossMessage (entry.path, hr).c_str());

        hr = m_save.CommitFile (entry.path, bytes, CommitMode::Replace);
        CHRN (hr, DiskStoreText::FormatFlushLossMessage (entry.path, hr).c_str());

        // The live image follows the file, and only once the file has
        // actually changed -- so a failed write leaves the two agreeing.
        entry.image->SetImageWriteProtected (writeProtected);
        MarkStatusChanged();

        //  THIS STORE JUST WROTE THE FILE, so the identity it watches has to
        //  move with it, exactly as FlushEntry does after its own commit.
        //  Without this the next poll finds bytes that do not match the
        //  mount-time identity and reports the disk as changed outside Casso,
        //  over a change the user made from the Disk menu.
        //
        //  Only for a bay that recorded one, so a mount from bytes does not
        //  acquire an identity by being write-protected.
        if (entry.sharedState.GetIdentity().recorded)
        {
            entry.sharedState.SetIdentity (ReadIdentity (entry.path));
        }

        NotifyMediaChanged();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadImageFile
//
//  Whole-file read for a mounted image's backing file, through the test
//  reader hook when one is installed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::ReadImageFile (const string & path, vector<Byte> & bytes) const
{
    return GetFileOps().Read (path, bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadFileBytes
//
//  Reads a whole file. Reports failure rather than returning a short buffer:
//  a caller that cannot tell a truncated read from a small file will happily
//  write the truncation back.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::ReadFileBytes (const string & path, vector<Byte> & bytes)
{
    HRESULT     hr      = S_OK;
    bool        hasPath = !path.empty();
    bool        fileOk  = false;
    bool        readOk  = false;
    streamsize  size    = 0;



    CBRAEx (hasPath, E_INVALIDARG);

    {
        ifstream  file (path, ios::binary | ios::ate);

        fileOk = file.good();
        CBR (fileOk);

        size = file.tellg();
        CBR (size >= 0);

        file.seekg (0, ios::beg);
        bytes.resize (static_cast<size_t> (size));

        if (size > 0)
        {
            file.read (reinterpret_cast<char *> (bytes.data()),
                       static_cast<streamsize> (size));

            readOk = (file.gcount() == size);
            CBR (readOk);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushAll / FlushAllForShutdown
//
//  MOST FLUSHES HAPPEN WITH THE MACHINE RUNNING, AND NOBODY ASKED FOR THEM.
//  After a spindown, a machine switch or a soft reset, every bay and every
//  pump is in place, so a failure among them is a question like any other.
//  Only the last flush of the process is different, which is why it goes
//  through a separate call.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::FlushAll()
{
    ASSERT_THREAD_OWNERSHIP (m_ownership);

    return FlushEveryBay (FlushMoment::Background);
}



HRESULT DiskImageStore::FlushAllForShutdown()
{
    ASSERT_THREAD_OWNERSHIP (m_ownership);

    return FlushEveryBay (FlushMoment::ShuttingDown);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushAllUnlessHeld
//
//  The motor spinning down, a reset and a power cycle flush through here.
//  While the machine is behind live in its recorded history, or a replay is
//  running, the guest's writes are machine state that a step can still
//  change, so these moments leave the file alone.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::FlushAllUnlessHeld()
{
    HRESULT  hr = S_OK;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    BAIL_OUT_IF (m_isFlushHeld || m_isReplaying, S_OK);

    hr = FlushAll();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasUnsavedWrites
//
//  Whether any mounted disk holds writes its file does not.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::HasUnsavedWrites() const
{
    ASSERT_THREAD_OWNERSHIP (m_ownership);

    return CountUnsavedDisks() > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CountUnsavedDisks
//
//  How many mounted disks hold writes their files do not.
//
////////////////////////////////////////////////////////////////////////////////

int DiskImageStore::CountUnsavedDisks() const
{
    int  count = 0;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    for (const auto & row : m_entries)
    {
        for (const Entry & entry : row)
        {
            if (entry.mounted && entry.image != nullptr && entry.image->IsDirty())
            {
                count++;
            }
        }
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommitHeldWrites
//
//  Writes every bay as it stands now, through the ordinary flush, which
//  replaces each file atomically.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::CommitHeldWrites()
{
    ASSERT_THREAD_OWNERSHIP (m_ownership);

    return FlushAll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiscardHeldWrites
//
//  Reloads every bay holding unsaved writes from its file, through the same
//  swap a changed file takes, so nothing is flushed on the way. A bay whose
//  file cannot be read keeps its writes and the first failure is returned.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::DiscardHeldWrites()
{
    HRESULT       hr      = S_OK;
    HRESULT       hrFirst = S_OK;
    int           slot    = 0;
    int           drive   = 0;
    vector<Byte>  bytes;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    for (slot = 0; slot < kSlotCount; slot++)
    {
        for (drive = 0; drive < kDriveCount; drive++)
        {
            Entry &  entry = GetEntry (slot, drive);

            if (!entry.mounted || entry.image == nullptr || entry.path.empty())
            {
                continue;
            }

            //  An open flux write is part of what is discarded.
            entry.image->CommitPendingWrite();

            if (!entry.image->IsDirty())
            {
                continue;
            }

            hr = ReadImageFile (entry.path, bytes);

            if (SUCCEEDED (hr))
            {
                hr = m_reload.MountExternallyModifiedDisk (slot, drive, bytes);
            }

            if (FAILED (hr) && SUCCEEDED (hrFirst))
            {
                hrFirst = hr;
            }
        }
    }

    return hrFirst;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMediaId
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DiskImageStore::GetMediaId (int slot, int drive) const
{
    ASSERT_THREAD_OWNERSHIP (m_ownership);

    if (!IsValidBay (slot, drive))
    {
        return 0;
    }

    {
        const Entry &  entry = GetEntry (slot, drive);

        return (entry.mounted && entry.image != nullptr) ? entry.image->GetImageId() : 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NotifyMediaChanged
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::NotifyMediaChanged()
{
    if (m_mediaChangeListener && !m_isReplaying)
    {
        m_mediaChangeListener();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushEveryBay
//
//  What both of the above are, once the moment is decided.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::FlushEveryBay (FlushMoment moment)
{
    HRESULT   hr      = S_OK;
    HRESULT   hrFirst = S_OK;
    int       slot    = 0;
    int       drive   = 0;



    for (slot = 0; slot < kSlotCount; slot++)
    {
        for (drive = 0; drive < kDriveCount; drive++)
        {
            hr = m_save.FlushEntry (m_entries[slot][drive], moment);

            if (FAILED (hr) && SUCCEEDED (hrFirst))
            {
                hrFirst = hr;
            }
        }
    }

    return hrFirst;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Eject
//
//  Saves the disk, then empties the bay.
//
//  THE BAY IS EMPTIED ONLY WHEN EVERY GUEST WRITE IS IN A FILE: the disk's
//  file, or a recovery copy that is current. After a failed save, either a
//  question is up, and its answer finishes the eject, or a notice states why
//  the disk is in the drive. During a replay nothing is saved or reported, so
//  the decline is reported here.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::Eject (int slot, int drive)
{
    HRESULT   hr        = S_OK;
    bool      isMounted = false;
    bool      isWaiting = false;
    bool      isSaved   = false;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    // An out-of-range bay and an empty one are both nothing to eject.
    isMounted = IsValidBay (slot, drive) && GetEntry (slot, drive).mounted;

    if (isMounted)
    {
        Entry &   entry = GetEntry (slot, drive);

        hr = m_save.FlushEntry (entry, FlushMoment::Ejecting);

        isWaiting = entry.sharedState.IsEjectWhenAnswered();
        isSaved   = m_save.AreWritesSaved (entry);

        if (!isWaiting && !isSaved && SUCCEEDED (hr))
        {
            EhmNotifyUser (DiskStoreText::FormatBehindLiveEjectMessage (drive).c_str());
        }

        if (!isWaiting && isSaved)
        {
            //  Before the path goes: the watch is keyed by the directory the
            //  image sits in, and there is no finding that from a cleared path.
            m_reload.EndWatching (slot, drive);

            //  Kept in memory while reverse execution may still put it back.
            m_media.RetireBay (entry);

            //  The disk left. Emitted after the bay is empty, so the handler
            //  that detaches the controller sees the post-eject state.
            EmitBayChange (slot, drive, BayChange::Ejected);

            NotifyMediaChanged();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  Keeps every disk mounted and flushes every dirty image, so a soft reset
//  does not lose the guest's writes, unless the reverse-execution hold or a
//  replay is in force. Then the writes are machine state a step can still
//  change, and the files are left alone.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::SoftReset()
{
    HRESULT   hr = FlushAllUnlessHeld();



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  FR-035 / data-model.md: unmount everything, flushing dirty as we go.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::PowerCycle()
{
    int   slot  = 0;
    int   drive = 0;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    for (slot = 0; slot < kSlotCount; slot++)
    {
        for (drive = 0; drive < kDriveCount; drive++)
        {
            Eject (slot, drive);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsValidBay
//
//  Whether (slot, drive) names a real bay. The one place the fixed array's
//  bounds are stated, so an accessor cannot get the check subtly wrong.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::IsValidBay (int slot, int drive)
{
    return slot  >= 0 && slot  < kSlotCount
        && drive >= 0 && drive < kDriveCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetImage / IsMounted / GetSourcePath
//
////////////////////////////////////////////////////////////////////////////////

DiskImage * DiskImageStore::GetImage (int slot, int drive)
{
    ASSERT_THREAD_OWNERSHIP (m_ownership);

    // Null for a bad bay is the same answer as for an empty one: no image.
    return IsValidBay (slot, drive) ? GetEntry (slot, drive).image.get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsMounted
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::IsMounted (int slot, int drive) const
{
    ASSERT_THREAD_OWNERSHIP (m_ownership);

    return IsValidBay (slot, drive) && GetEntry (slot, drive).mounted;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSourcePath
//
////////////////////////////////////////////////////////////////////////////////

const string & DiskImageStore::GetSourcePath (int slot, int drive) const
{
    ASSERT_THREAD_OWNERSHIP (m_ownership);

    // Returns a reference, so a bad bay yields the member empty string rather
    // than a temporary.
    return IsValidBay (slot, drive) ? GetEntry (slot, drive).path : m_emptyPath;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMountedSourcePaths
//
//  Every mounted entry's backing path with its bay. Entries mounted from
//  bytes with an empty virtual path are skipped -- there is no host file to
//  collide with.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DiskImageStore::MountedSource> DiskImageStore::GetMountedSourcePaths() const
{
    std::vector<MountedSource>  result;
    int                         slot   = 0;
    int                         drive  = 0;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    for (slot = 0; slot < kSlotCount; slot++)
    {
        for (drive = 0; drive < kDriveCount; drive++)
        {
            const Entry &  entry = GetEntry (slot, drive);

            if (entry.mounted && !entry.path.empty())
            {
                result.push_back (MountedSource{ entry.path, slot, drive });
            }
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBayStatus
//
//  A copy of the bay, with nothing in it for an empty or invalid bay.
//
////////////////////////////////////////////////////////////////////////////////

BayStatus DiskImageStore::GetBayStatus (int slot, int drive) const
{
    BayStatus  status;
    bool       isValid = IsValidBay (slot, drive);



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    if (isValid)
    {
        const Entry &  entry = GetEntry (slot, drive);

        if (entry.mounted)
        {
            status.isMounted        = true;
            status.path             = entry.path;
            status.format           = entry.format;
            status.writeProtect     = (entry.image != nullptr) ? entry.image->GetWriteProtectInfo() : WriteProtectInfo();
            status.isSalvageOffered = entry.salvageOffered;
            status.wozRequirements  = (entry.image != nullptr) ? WozCompatibility::ReadRequirements (entry.image->GetWozMetadata()) : WozRequirements();
        }
    }

    return status;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetUserWriteProtectFlag
//
//  The user's write-protect setting on the mounted image. Nothing is saved:
//  unsaved writes remain in the image when the setting turns on.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::SetUserWriteProtectFlag (int slot, int drive, bool isProtected)
{
    bool  isValid = IsValidBay (slot, drive);



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    if (isValid && GetEntry (slot, drive).image != nullptr)
    {
        GetEntry (slot, drive).image->SetUserWriteProtected (isProtected);
        MarkStatusChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetUserWriteProtect
//
//  The user's write-protect setting, from a command. A protected disk is
//  never saved, so protecting saves the disk first, at Running, as an
//  explicit save, which writes under the flush hold too.
//
//  AFTER A FAILED SAVE THE DISK IS WRITABLE, with its writes. The media change
//  it reports makes the boundary keyframe at this position record the drive
//  unprotected, so a replay of the journaled protect ends as the live machine
//  did.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::SetUserWriteProtect (int slot, int drive, bool isProtected)
{
    HRESULT  hr        = S_OK;
    bool     isValid   = IsValidBay (slot, drive);
    bool     isMounted = false;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    CBRAEx (isValid, E_INVALIDARG);

    isMounted = GetEntry (slot, drive).mounted && GetEntry (slot, drive).image != nullptr;
    BAIL_OUT_IF (!isMounted, S_OK);

    if (isProtected)
    {
        hr = m_save.FlushEntry (GetEntry (slot, drive), FlushMoment::Running);
        CHRF (hr, NotifyMediaChanged());
    }

    SetUserWriteProtectFlag (slot, drive, isProtected);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFileWriteProtect
//
//  The backing file's read-only and permission state on the mounted image.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::SetFileWriteProtect (int slot, int drive, bool isReadOnly, bool hasNoPermission)
{
    bool  isValid = IsValidBay (slot, drive);



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    if (isValid && GetEntry (slot, drive).image != nullptr)
    {
        GetEntry (slot, drive).image->SetFileWriteProtect (isReadOnly, hasNoPermission);
        MarkStatusChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::ReadIdentity
//
//  What the file at `path` looks like right now.
//
//  THROUGH THE SEAM WHEN ONE IS INSTALLED. A test that has redirected reads and
//  writes into memory has no file to stat, and reaching past the seam to the
//  filesystem here would have the store checking one world and writing another.
//
////////////////////////////////////////////////////////////////////////////////

ImageIdentity DiskImageStore::ReadIdentity (const string & path) const
{
    return GetFileOps().ReadIdentity (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::GetSharedState
//
//  What a bay knows about its image beyond the bytes.
//
//  NULL FOR AN OUT-OF-RANGE BAY rather than a reference to a shared empty, so a
//  caller cannot write into a placeholder and believe it recorded something.
//
////////////////////////////////////////////////////////////////////////////////

MountedImageState * DiskImageStore::GetSharedState (int slot, int drive)
{
    MountedImageState *  state = nullptr;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    if (IsValidBay (slot, drive))
    {
        state = &GetEntry (slot, drive).sharedState;
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::GetSharedState  (const)
//
//  The same answer for a caller that only reads it.
//
////////////////////////////////////////////////////////////////////////////////

const MountedImageState * DiskImageStore::GetSharedState (int slot, int drive) const
{
    const MountedImageState *  state = nullptr;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    if (IsValidBay (slot, drive))
    {
        state = &GetEntry (slot, drive).sharedState;
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::GetNowMs
//
//  Milliseconds now.
//
//  INJECTED SO A TEST NEED NOT WAIT. The quiet period is a second, and a suite
//  that spent one proving every coalescing rule would be a suite nobody runs.
//
////////////////////////////////////////////////////////////////////////////////

int64_t DiskImageStore::GetNowMs() const
{
    int64_t  now = 0;



    if (m_clock)
    {
        now = m_clock();
    }
    else
    {
        now = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                  std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    return now;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::EmitBayChange
//
//  Tells the shell a bay's disk changed.
//
//  EVERY MUTATION PATH ENDS HERE. Mount, user eject, pick-up, lost file -- the
//  door and its sounds are lit from this one call, so a new path that moves a
//  disk cannot forget to. A null sink is a headless or test session with no
//  drive on screen, and does nothing.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::EmitBayChange (int slot, int drive, BayChange change)
{
    if (m_bayChangeSink)
    {
        m_bayChangeSink (slot, drive, change);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::GetCommitTemporaryPath
//
//  Where a commit of `path` writes before it renames.
//
////////////////////////////////////////////////////////////////////////////////

string DiskImageStore::GetCommitTemporaryPath (const string & path, unsigned attempt)
{
    return CommitPlan::GetTemporaryPath (path, GetInvocationTag(), attempt);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::GetInvocationTag
//
//  ONE INVOCATION TAG FOR THE WHOLE PROCESS, taken once. Two emulators get
//  different tags and therefore different names; two flushes inside one
//  emulator get the same tag and are separated by the attempt counter and the
//  existence check, which is enough because they cannot overlap -- every flush
//  runs on the one thread that owns disk writes.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DiskImageStore::GetInvocationTag()
{
    static const uint64_t  s_kInvocationTag = CommitPlan::NextInvocationTag();



    return s_kInvocationTag;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::GetFileOps
//
////////////////////////////////////////////////////////////////////////////////

DiskFileOps DiskImageStore::GetFileOps() const
{
    DiskFileOps  ops;



    ops.flushSink      = m_flushSink;
    ops.imageReader    = m_imageReader;
    ops.identityReader = m_identityReader;
    ops.fileIo         = m_fileIo;

    return ops;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::NoteExternalChange
//
//  Forwards to DiskStoreReload.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::NoteExternalChange (const string & path, ExternalChangeIntent intent)
{
    m_reload.NoteExternalChange (path, intent);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::ApplyPendingReload
//
//  Forwards to DiskStoreReload.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::ApplyPendingReload()
{
    m_reload.ApplyPendingReload();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::ResolvePendingChange
//
//  Forwards to DiskStoreReload.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::ResolvePendingChange (int slot, int drive, ChangeAction chosen, const string & savePath)
{
    m_reload.ResolvePendingChange (slot, drive, chosen, savePath);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::AssessSalvage
//
//  Forwards to DiskStoreSalvage.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::AssessSalvage (int slot, int drive, SalvageAssessment & out)
{
    return m_salvage.AssessSalvage (slot, drive, out);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::IsSalvageOffered
//
//  Forwards to DiskStoreSalvage.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::IsSalvageOffered (int slot, int drive) const
{
    return m_salvage.IsSalvageOffered (slot, drive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::SalvageToFile
//
//  Forwards to DiskStoreSalvage.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::SalvageToFile (int slot, int drive, uint64_t mediaId, const string & path, DenibblizeReport & report)
{
    return m_salvage.SalvageToFile (slot, drive, mediaId, path, report);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::SetMediaRetention
//
//  Forwards to DiskStoreMedia.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::SetMediaRetention (bool isOn)
{
    m_media.SetMediaRetention (isOn);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::CanSeatMedia
//
//  Forwards to DiskStoreMedia.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::CanSeatMedia (int slot, int drive, uint64_t mediaId) const
{
    return m_media.CanSeatMedia (slot, drive, mediaId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::SeatMedia
//
//  Forwards to DiskStoreMedia.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::SeatMedia (int slot, int drive, uint64_t mediaId, bool & outChanged)
{
    return m_media.SeatMedia (slot, drive, mediaId, outChanged);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::ReportSeatedMedia
//
//  Forwards to DiskStoreMedia.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::ReportSeatedMedia (int slot, int drive, uint64_t previousMediaId)
{
    m_media.ReportSeatedMedia (slot, drive, previousMediaId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::PruneRetainedMedia
//
//  Forwards to DiskStoreMedia.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::PruneRetainedMedia (uint64_t oldestPosition)
{
    m_media.PruneRetainedMedia (oldestPosition);
}
