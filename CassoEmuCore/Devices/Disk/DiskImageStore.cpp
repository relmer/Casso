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
//  SaveAllUnlessHeld
//
//  The motor spinning down. The same moment and the same rules as
//  FlushAllUnlessHeld, but the file work is posted and this returns at once,
//  so the CPU thread does not wait on the disk's file.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::SaveAllUnlessHeld()
{
    int  slot  = 0;
    int  drive = 0;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    if (!m_isFlushHeld && !m_isReplaying)
    {
        for (slot = 0; slot < kSlotCount; slot++)
        {
            for (drive = 0; drive < kDriveCount; drive++)
            {
                m_save.Save (m_entries[slot][drive], FlushMoment::Background, nullptr);
            }
        }
    }
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
//  The save runs on the I/O queue, and done runs when the eject is over, with
//  whether the bay is empty.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::Eject (int slot, int drive, EjectDone done)
{
    bool      isMounted = false;
    uint64_t  imageId   = 0;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    // An out-of-range bay and an empty one are both nothing to eject.
    isMounted = IsValidBay (slot, drive) && GetEntry (slot, drive).mounted;

    if (!isMounted)
    {
        if (done)
        {
            done (true);
        }
    }
    else
    {
        imageId = (GetEntry (slot, drive).image != nullptr) ? GetEntry (slot, drive).image->GetImageId() : 0;

        m_save.Save (GetEntry (slot, drive), FlushMoment::Ejecting,
                     [this, slot, drive, imageId, done] (HRESULT hr, const FlushOutcome & outcome)
        {
            UNREFERENCED_PARAMETER (outcome);

            FinishEject (slot, drive, imageId, hr);

            if (done)
            {
                done (!GetEntry (slot, drive).mounted);
            }
        });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FinishEject
//
//  After the eject's save: the bay is emptied when the writes are in a
//  file, and otherwise the question's answer finishes it, or the notice
//  states why the disk is in the drive. Nothing is done when another disk is
//  in the bay by then.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::FinishEject (int slot, int drive, uint64_t imageId, HRESULT hr)
{
    Entry  & entry     = GetEntry (slot, drive);
    bool     isSame    = entry.mounted && (entry.image == nullptr || entry.image->GetImageId() == imageId);
    bool     isWaiting = false;
    bool     isSaved   = false;



    if (isSame)
    {
        isWaiting = entry.sharedState.IsEjectWhenAnswered();
        isSaved   = m_save.AreWritesSaved (entry);
    }

    if (isSame && !isWaiting && !isSaved && SUCCEEDED (hr))
    {
        EhmNotifyUser (DiskStoreText::FormatBehindLiveEjectMessage (drive).c_str());
    }

    if (isSame && !isWaiting && isSaved)
    {
        //  Before the path goes: the watch is keyed by the directory the
        //  image sits in, and there is no finding that from a cleared path.
        m_reload.EndWatching (slot, drive);

        //  In memory while reverse execution may put it back.
        m_media.RetireBay (entry);

        //  The disk left. Emitted after the bay is empty, so the handler
        //  that detaches the controller runs in the post-eject state.
        EmitBayChange (slot, drive, BayChange::Ejected);

        NotifyMediaChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Eject, waiting
//
//  The eject above, for a caller that uses the bay's state when it returns.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::Eject (int slot, int drive)
{
    shared_ptr<bool>  isDone = std::make_shared<bool> (false);
    HRESULT           hr     = S_OK;



    Eject (slot, drive, [isDone] (bool isEjected) { UNREFERENCED_PARAMETER (isEjected); *isDone = true; });

    hr = WaitForIo (*isDone);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  Every disk stays mounted and every dirty image is saved, so the guest's
//  writes survive a soft reset, unless the reverse-execution hold or a
//  replay is in force. Then the writes are machine state a step can change,
//  and the files are left alone. The saves are posted: the reset does not
//  use their result, and a failure is reported from its completion.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::SoftReset()
{
    ASSERT_THREAD_OWNERSHIP (m_ownership);

    SaveAllUnlessHeld();
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
//  WaitForIo
//
//  For a caller that uses a result before it returns. With the inline queue
//  the work is done; with a threaded one the store's thread drains until it
//  is, for at most s_kSyncIoTimeoutMs.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::WaitForIo (const bool & isDone)
{
    HRESULT  hr = S_OK;



    if (!isDone)
    {
        hr = m_ioQueue->WaitUntilIdle (s_kSyncIoTimeoutMs);
        CHR (hr);
    }

    CBREx (isDone, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));

Error:
    return hr;
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





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::Mount
//
//  Forwards to DiskStoreMount.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::Mount (int slot, int drive, const string & path)
{
    return m_mount.Mount (slot, drive, path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::Mount, with its diagnosis
//
//  Forwards to DiskStoreMount.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::Mount (int slot, int drive, const string & path,
                               MountDiagnosis & outDiagnosis)
{
    return m_mount.Mount (slot, drive, path, outDiagnosis);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::Mount, through the I/O queue
//
//  Forwards to DiskStoreMount.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::Mount (int slot, int drive, const string & path, MountDone done)
{
    m_mount.Mount (slot, drive, path, std::move (done));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::MountFromBytes
//
//  Forwards to DiskStoreMount.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::MountFromBytes (int slot, int drive, const string & virtualPath,
                                       DiskFormat fmt, const vector<Byte> & bytes)
{
    return m_mount.MountFromBytes (slot, drive, virtualPath, fmt, bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::MountFromBytes, with its diagnosis
//
//  Forwards to DiskStoreMount.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::MountFromBytes (int slot, int drive, const string & virtualPath,
                                       DiskFormat fmt, const vector<Byte> & bytes,
                                       MountDiagnosis & outDiagnosis)
{
    return m_mount.MountFromBytes (slot, drive, virtualPath, fmt, bytes, outDiagnosis);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::MountRestored
//
//  Forwards to DiskStoreMount.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::MountRestored (int slot, int drive, const string & path,
                                      DiskFormat fmt, const vector<Byte> & bytes)
{
    return m_mount.MountRestored (slot, drive, path, fmt, bytes);
}
