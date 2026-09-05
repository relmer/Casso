#include "Pch.h"

#include "DiskImageStore.h"
#include "NibblizationLayer.h"
#include "NibbleImageCodec.h"
#include "WozLoader.h"
#include "Core/TextEncoding.h"
#include "ChangePrompt.h"
#include "CommitPlan.h"





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
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::MountFromBytes (
    int                    slot,
    int                    drive,
    const string        &  virtualPath,
    DiskFormat             fmt,
    const vector<Byte>  &  bytes,
    MountDiagnosis      &  outDiagnosis)
{
    HRESULT   hr = S_OK;



    outDiagnosis              = MountDiagnosis();
    outDiagnosis.format       = fmt;
    outDiagnosis.fileByteSize = bytes.size();

    CBRAEx (slot >= 0 && slot < kSlotCount && drive >= 0 && drive < kDriveCount, E_INVALIDARG);

    {
        Entry &   entry = GetEntry (slot, drive);

        if (entry.mounted)
        {
            hr = FlushEntry (entry);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        entry.image   = make_unique<DiskImage> ();
        entry.path    = virtualPath;
        entry.format  = fmt;
        entry.mounted = true;

        entry.image->LoadFromBytes (fmt, bytes, virtualPath);

        // A format the loader rejects leaves the slot empty rather than
        // half-mounted.
        if (!entry.image->IsLoaded())
        {
            entry.image.reset();
            entry.path.clear();
            entry.mounted        = false;
            entry.salvageOffered = false;
            outDiagnosis         = ClassifyLoadFailure (fmt, bytes);
            hr = E_FAIL;
        }
        else
        {
            // Decide the salvage question once, here, where the cost is paid
            // by a mount the user already asked for. Undamaged images settle
            // it for free; a damaged one is decoded once instead of on every
            // menu draw.
            SalvageAssessment  assessment;
            HRESULT            hrAssess = AssessSalvage (slot, drive, assessment);

            entry.salvageOffered = SUCCEEDED (hrAssess) && assessment.isOffered;
        }
    }

Error:
    return hr;
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



    return Mount (slot, drive, path, ignored);
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
    HRESULT       hr   = S_OK;
    DiskFormat    fmt  = DiskFormat::Dsk;
    vector<Byte>  bytes;



    outDiagnosis = MountDiagnosis();

    hr = GetSourceFormatByExtension (path, fmt);
    CHRF (hr, outDiagnosis.failure = MountFailure::UnknownExtension);

    outDiagnosis.format = fmt;

    hr = ReadImageFile (path, bytes);
    CHRF (hr, outDiagnosis.failure = MountFailure::FileUnreadable);

    hr = MountFromBytes (slot, drive, path, fmt, bytes, outDiagnosis);
    CHR (hr);

    //  Recorded AFTER the read and only on success, matching the command
    //  line's read-then-stamp order. A stamp taken before the read could
    //  describe a file the loaded bytes did not come from, and a stamp
    //  recorded for a mount that failed would sit on an empty bay.
    GetEntry (slot, drive).sharedState.Mount (ReadIdentity (path));

    //  Mount registers the watch. It is orchestration rather than platform
    //  work, so it lives here and not in whatever built the watcher.
    BeginWatching (slot, drive);

    //  A disk went in. The shell wires the controller and lights the door from
    //  here rather than from the path that called Mount, so a command-line
    //  mount and a picker mount and a machine-switch remount all react alike.
    EmitBayChange (slot, drive, BayChange::Inserted);

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
//  FormatMountFailureMessage
//
//  User-facing message for a mount that did not happen: the file named, then
//  the diagnosis worded as a sentence about it.
//
//  IT USED TO GUESS. The mount reported every refusal as one generic HRESULT,
//  so this re-derived what it could from the path -- an extension no loader
//  claims, versus everything else -- and told everyone in the second group
//  that their file "could not be read, or its contents are not a disk image
//  this loader accepts", which is four different problems in one sentence and
//  actionable for none of them. The reason now travels here, and the wording
//  is the diagnosis's to give.
//
//  Deliberately says nothing about what happened to the drive. A rejected
//  file leaves the bay empty, while a file that could not be read at all
//  leaves the previous disk in place, and a message that guessed wrong about
//  that is worse than one that stays quiet on it.
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskImageStore::FormatMountFailureMessage (const string & path,
                                                   const MountDiagnosis & diagnosis)
{
    wstring  widePath = fs::path (path).wstring();
    wstring  reason   = TextEncoding::NarrowToWide (diagnosis.Describe());



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    return L"Casso could not open this file as a disk image:\n\n" + widePath +
           L"\n\nThis file " + reason + L".";
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatDamagedImageMessage
//
//  User-facing message for a disk this tool will not write back, because
//  rewriting it would give damaged data a freshly computed checksum and
//  leave nothing able to detect the damage again.
//
//  THE BANNER ABOVE THIS USED TO NAME THE FUNCTION BELOW IT, which was the
//  next one down: this function was spliced in after the banner rather than
//  before it, so both carried the same title and neither described what it
//  sat on.
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskImageStore::FormatDamagedImageMessage (const string & path)
{
    wstring  widePath = fs::path (path).wstring();



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    return L"This disk is damaged, so Casso will not write to it:\n\n" + widePath +
           L"\n\nRewriting it would give the file a newly computed checksum, "
           L"leaving nothing able to detect the damage it already carries. The "
           L"disk stays readable and the emulated machine sees it as "
           L"write-protected. Work on a copy if you need to write to it.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatFlushLossMessage
//
//  User-facing message for a flush that failed to persist a dirty image,
//  built from the mount path (widened) the store already holds. Handed to
//  the CHRN/CBRN notifications in FlushEntry.
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskImageStore::FormatFlushLossMessage (const string & path,
                                                HRESULT        reason,
                                                const string & recoveryPath)
{
    wstring  widePath     = fs::path (path).wstring();
    wstring  wideRecovery = fs::path (recoveryPath).wstring();
    wstring  message;



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    //  The code and the system's own words for it, the same way every other
    //  disk notice reports a failure. A notice that names no cause sends the
    //  reader guessing, and this one used to guess for them -- wrongly.
    message = L"Casso could not save changes to the disk image:\n\n" + widePath
            + L"\n\nError: " + ChangePrompt::DescribeError (reason)
            + L"\n\nThe file on disk is unchanged.";

    // A refusal that leaves the user with no way back to their work is only
    // half a fix, so say where the work went rather than only what failed.
    if (!wideRecovery.empty())
    {
        message += L" Your session was preserved here instead:\n\n" + wideRecovery +
                   L"\n\nThat copy is complete. It keeps the track that could not "
                   L"be written back. Mount it to pick up where you left off.";
    }
    else
    {
        //  True and actionable, where the old text advised a format change
        //  that cannot help a permission or disk-space failure.
        message += L" Your recent writes have not been saved. The disk in the drive "
                   L"still has them.";
    }

    return message;
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
//  The attempt index exists so an earlier recovery is never overwritten; losing
//  a previous rescue to a later one would repeat the mistake being fixed.
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
//  DiskImageStore::TryWriteRecoveryImage
//
//  Serializes the live image to WOZ and lands it beside the original. WOZ is
//  chosen deliberately over the denibblized sector buffer: that buffer holds
//  zeros exactly where the unreadable track should be, so writing it would
//  discard the one thing the refusal was protecting. WozLoader::Serialize takes
//  the per-track bit streams verbatim and is format-agnostic, so a .dsk-sourced
//  mount round-trips whole.
//
//  Never overwrites an existing file, and never touches the original or what is
//  currently mounted -- preserving the work and adopting it are different
//  operations, and only the first belongs here.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::TryWriteRecoveryImage (Entry & entry, string & outPath)
{
    HRESULT       hr        = S_OK;
    string        candidate;
    int           attempt   = 0;
    bool          haveImage = entry.image != nullptr;
    bool          havePath  = !entry.path.empty();
    bool          haveName  = false;
    vector<Byte>  wozBytes;



    outPath.clear();

    CBR (haveImage);
    CBR (havePath);

    hr = WozLoader::Serialize (*entry.image, wozBytes);
    CHR (hr);

    for (attempt = 0; !haveName && attempt < kMaxRecoveryNameAttempts; attempt++)
    {
        candidate = MakeRecoveryPath (entry.path, attempt);

        // With a sink installed there is no host file to collide with -- the
        // sink is the filesystem, and it decides what to do with the name.
        if (m_flushSink)
        {
            haveName = true;
        }
        else
        {
            bool  taken = fs::exists (fs::path (candidate));

            haveName = !taken;
        }
    }

    CBREx (haveName, HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS));

    if (m_flushSink)
    {
        hr = m_flushSink (candidate, wozBytes);
        CHR (hr);
    }
    else
    {
        //  Same rule as WriteFileAtomically: the filesystem's own code for a
        //  refusal travels out, not E_FAIL. The name was just verified free,
        //  so CREATE_NEW is faithful and refuses a race for it rather than
        //  overwriting whoever won.
        std::wstring  wide     = fs::path (candidate).wstring();
        HANDLE        file     = INVALID_HANDLE_VALUE;
        DWORD         written  = 0;
        DWORD         lastErr  = ERROR_SUCCESS;
        DWORD         expected = static_cast<DWORD> (wozBytes.size());
        BOOL          ok       = FALSE;

        file = CreateFileW (wide.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        CWR (file != INVALID_HANDLE_VALUE);

        ok      = WriteFile (file, wozBytes.data(), static_cast<DWORD> (wozBytes.size()),
                             &written, nullptr);
        lastErr = ok ? ERROR_SUCCESS : GetLastError();

        CloseHandle (file);

        CBREx (ok, HRESULT_FROM_WIN32 (lastErr));
        CBREx (written == expected, HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT));
    }

    outPath = candidate;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushEntry
//
//  Centralized flush helper. Dispatches through SetFlushSink when the test
//  hook is installed; otherwise writes to the host filesystem. Does nothing
//  if the image is clean or no source path is recorded. A genuine failure
//  to persist a dirty image is surfaced to the user via the shared EHM
//  notifier (CHRN/CBRN), because every caller drops the return value.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::FlushEntry (Entry & entry)
{
    HRESULT        hr           = S_OK;
    HRESULT        hrRecovery   = S_OK;
    bool           unchanged    = true;
    bool           keptExternal = false;
    string         recoveryPath;
    string         preservedPath;
    vector<Byte>   bytes;
    vector<Byte>   external;
    ImageIdentity  current;



    // No-op cases -- nothing dirty to persist -- succeed silently. There is
    // deliberately no way to force a flush past these gates: the only caller
    // that wanted one was changing a write-protect flag, and rebuilding a
    // whole image to carry one bit is what SetImageWriteProtect exists to
    // avoid. An API that cannot be asked to do that cannot be misused into it.
    BAIL_OUT_IF (!entry.mounted || entry.image == nullptr, S_OK);
    BAIL_OUT_IF (!entry.image->IsDirty(), S_OK);

    if (entry.image->IsWriteProtected())
    {
        entry.image->ClearDirty();
        BAIL_OUT_IF (true, S_OK);
    }

    // A genuine failure to persist a DIRTY image must not vanish: every
    // caller drops FlushEntry's HRESULT (Eject / PowerCycle are void; the
    // shell / SoftReset IGNORE_RETURN_VALUE it), so the loss is surfaced
    // here through the shared EHM notifier rather than a return nobody
    // checks. The image keeps its dirty bit on failure so a later flush
    // can retry.
    // Serialization now refuses rather than quietly emitting a buffer with
    // zeros where a track could not be read. That protects the file on disk,
    // but it strands the session unless the session goes somewhere -- so
    // preserve it losslessly beside the original before reporting the loss.
    //
    // There is deliberately no warning here about overwriting an image whose
    // stored checksum did not validate at load. There used to be, and it is
    // no longer reachable: a checksum mismatch now write-protects the image,
    // so the gate above returns before this point and the file is never
    // rewritten. The user is told at mount instead, which is earlier and is
    // where the decision actually gets made.
    //  Immediately before committing, and whatever any watcher has or has not
    //  reported. THIS IS THE GUARANTEE AND NOTIFICATION IS ONLY PROMPTNESS: a
    //  missed notification, an unwatchable share, a change that arrived while
    //  the machine was busy -- all of them end here, one stat before the write.
    //
    //  A BAY THAT NEVER RECORDED AN IDENTITY IS NOT CHECKED. MountFromBytes has
    //  no file behind it, and a test that redirects reads into memory has none
    //  either; comparing against a stat that never ran would refuse writes that
    //  nothing has endangered.
    if (entry.sharedState.GetIdentity().recorded)
    {
        current   = ReadIdentity (entry.path);
        unchanged = entry.sharedState.GetIdentity().Matches (current);

        //  THE SAME CONFLICT THE WATCHER FINDS, discovered at the other end.
        //  Notification failed, or the change landed while this write was
        //  already under way, and the guest's version is about to go over a
        //  version this store never took up.
        //
        //  THE FILE STAYS WITH WHOEVER CHANGED IT. The guest's version moves to
        //  a file of its own and the bay follows it; the original keeps what
        //  the other program wrote and is not touched here at all. The watcher
        //  path applies exactly this rule, so one collision produces the same
        //  two files whichever end found it. It did not use to: which version
        //  kept the original name came down to which side was quicker.
        if (!unchanged)
        {
            string   original = entry.path;
            bool     asked    = entry.sharedState.IsAskOutstanding();
            HRESULT  hrKeep   = S_OK;

            //  Under the name the question already showed, when there is a
            //  question. Reserving happens once, wherever it happens first.
            preservedPath = entry.preservedPath;

            hrKeep = SaveLoadedImage (entry, preservedPath);

            //  A preserve that did not happen stops the write. The image KEEPS
            //  ITS DIRTY BIT, which is the difference between refusing and
            //  losing: the guest's writes are still in memory and still
            //  flushable once there is somewhere to put them.
            //  STG_E_NOTCURRENT says precisely this -- the object changed since
            //  it was last read.
            keptExternal = SUCCEEDED (hrKeep);

            CBRFEx (keptExternal, STG_E_NOTCURRENT,
                    EhmNotifyUser (FormatExternalChangeMessage (original).c_str()));

            //  On disk under its own name, so the bay carries nothing unsaved.
            entry.preservedPath    = preservedPath;
            entry.preservedWritten = true;

            entry.image->SetSourceCrcMismatch (false);
            entry.image->ClearDirty();

            //  A QUESTION ALREADY ON SCREEN OWNS WHAT HAPPENS NEXT. Writing the
            //  copy is not optional -- the guest's work would be gone otherwise
            //  -- but moving the bay and reporting are the ANSWER's job. Doing
            //  them here settled the matter underneath an open dialog, which
            //  left the user being asked about a file the bay no longer had.
            //  The copy is on disk under the name that dialog is showing, so
            //  either answer still does what it says.
            if (asked)
            {
                BAIL_OUT_IF (true, S_OK);
            }

            hrKeep = RepointBayToFile (entry.slot, entry.drive, preservedPath);
            IGNORE_RETURN_VALUE (hrKeep, S_OK);

            //  Composed from the path the bay HAD, since that is the file the
            //  message is about and the repoint above has already moved it.
            if (m_reportSink)
            {
                m_reportSink (entry.slot, entry.drive,
                              ChangePrompt::ComposeConflictReport (original, entry.drive,
                                                                  preservedPath));
            }

            //  Nothing is written over the original. It is not this bay's file
            //  any more.
            BAIL_OUT_IF (true, S_OK);
        }
    }

    hr = entry.image->Serialize (bytes);

    if (FAILED (hr))
    {
        hrRecovery = TryWriteRecoveryImage (entry, recoveryPath);
        IGNORE_RETURN_VALUE (hrRecovery, S_OK);
    }

    CHRN (hr, FormatFlushLossMessage (entry.path, hr, recoveryPath).c_str());

    if (m_flushSink)
    {
        hr = m_flushSink (entry.path, bytes);
        CHRN (hr, FormatFlushLossMessage (entry.path, hr, recoveryPath).c_str());
    }
    else if (!entry.path.empty())
    {
        // Never write in place: a flush that fails midway would otherwise
        // have already truncated the user's image, trading a stale file for
        // no file at all. The dirty bit survives a failure, so a later flush
        // retries.
        //
        // The failure message names the recovery copy written above, because
        // a flush that cannot land is exactly when the session's only
        // lossless copy of the disk is the one sitting beside the original.
        hr = WriteFileAtomically (entry.path, bytes);
        CHRN (hr, FormatFlushLossMessage (entry.path, hr, recoveryPath).c_str());
    }

    // The file now carries a freshly computed CRC that matches it, so the
    // mismatch is no longer true of what is on disk -- and the warning above
    // must not repeat on every later eject or power cycle.
    entry.image->SetSourceCrcMismatch (false);
    entry.image->ClearDirty();

    //  The file this store just wrote is a change to it, and without this the
    //  next flush would find its own commit sitting where the mount-time
    //  identity used to be and refuse itself. NOT SEPARABLE FROM THE CHECK
    //  ABOVE: the two are one mechanism, and the first one alone breaks the
    //  second flush of every ordinary session.
    //
    //  Only for a bay that had an identity to begin with, so a mount from bytes
    //  does not acquire one by being flushed.
    if (entry.sharedState.GetIdentity().recorded)
    {
        entry.sharedState.SetIdentity (ReadIdentity (entry.path));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteFileAtomically
//
//  Write to a sibling temp file, verify every step, then rename it over the
//  target. The verification matters as much as the temp file: an ofstream
//  reports a short or failed write only through its stream state, so a full
//  volume otherwise completes a flush that wrote nothing and looks identical
//  to success. The state is read after close, since bytes can still be
//  buffered when write() returns.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::WriteFileAtomically (const string & path, const vector<Byte> & bytes)
{
    HRESULT           hr        = S_OK;
    bool              hasPath   = !path.empty();
    bool              foundFree = false;
    unsigned          attempt   = 0;
    string            tempPath;
    std::error_code   ec;
    size_t            byteCount = bytes.size();



    CBRAEx (hasPath, E_INVALIDARG);

    //  THE NAME USED TO BE A FIXED SUFFIX AND THAT WAS A DEFECT. Two emulators
    //  holding one image derived the same temporary from it, so each wrote into
    //  the other's file and one of them renamed the other's bytes over the
    //  target as its own.
    //
    //  CommitPlan ALREADY SOLVED THIS FOR THE COMMAND LINE, with a per-process
    //  tag and a step past anything already sitting at the name, and its own
    //  comment names the emulator as the side that still had the bug. A second
    //  scheme here would be a second thing to keep right; this is the same one.
    //
    //  STEPPING PAST AN EXISTING NAME IS WHAT HANDLES AN ABANDONED TEMPORARY.
    //  A writer that was killed mid-commit leaves one behind, and adopting it
    //  would mean appending this image to the remains of somebody else's.
    for (attempt = 0; attempt < CommitPlan::kMaxAttempts; attempt++)
    {
        tempPath  = GetCommitTemporaryPath (path, attempt);
        foundFree = !fs::exists (fs::path (tempPath), ec);

        if (foundFree)
        {
            break;
        }
    }

    CBREx (foundFree, HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS));

    //  A disk image is a few hundred KB; a payload past 4 GB is a caller bug.
    CBRAEx (byteCount <= MAXDWORD, E_INVALIDARG);

    //  THE REAL ERROR TRAVELS. This went through an ofstream and a CBR, which
    //  turned a folder that refused the temporary, a read-only target and a
    //  full disk all into E_FAIL -- and the save-failure notice, built to
    //  print the code and the system's own words for it, said "0x80004005
    //  Unspecified error" for a permission problem. CreateFileW and CWR carry
    //  the Win32 code out whole. The write's error is captured BEFORE the
    //  handle closes, because CloseHandle would overwrite it.
    {
        std::wstring  wideTemp = fs::path (tempPath).wstring();
        HANDLE        file     = INVALID_HANDLE_VALUE;
        DWORD         written  = 0;
        DWORD         lastErr  = ERROR_SUCCESS;
        DWORD         expected = static_cast<DWORD> (byteCount);
        BOOL          ok       = TRUE;

        file = CreateFileW (wideTemp.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        CWR (file != INVALID_HANDLE_VALUE);

        if (!bytes.empty())
        {
            ok      = WriteFile (file, bytes.data(), static_cast<DWORD> (bytes.size()),
                                 &written, nullptr);
            lastErr = ok ? ERROR_SUCCESS : GetLastError();
        }

        CloseHandle (file);

        CBREx (ok, HRESULT_FROM_WIN32 (lastErr));
        CBREx (written == expected, HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT));
    }

    // Rename replaces an existing target, so the swap is one filesystem
    // operation: readers see either the old file or the new one. The code the
    // filesystem gave for a refusal travels with it rather than becoming E_FAIL.
    fs::rename (tempPath, path, ec);
    CBREx (!ec, HRESULT_FROM_WIN32 (ec.value()));

Error:
    if (FAILED (hr))
    {
        std::error_code  cleanupEc;

        // Best effort -- the guarantee is that the TARGET is untouched, not
        // that the temp never lingers, and a temp we cannot remove must not
        // turn into a second error report.
        fs::remove (tempPath, cleanupEc);
    }

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



    CBRAEx (slot >= 0 && slot < kSlotCount && drive >= 0 && drive < kDriveCount, E_INVALIDARG);

    hr = FlushEntry (GetEntry (slot, drive));

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
        isDamaged = entry.image->HasSourceCrcMismatch();
        CBRN (!isDamaged, FormatDamagedImageMessage (entry.path).c_str());

        // Guest writes go out FIRST, while the image still accepts a flush.
        // Patching the flag byte afterwards edits a file that already holds
        // them; doing it the other way round would strand them behind the
        // gate this call is about to close.
        hr = FlushEntry (entry);
        CHR (hr);

        hr = ReadImageFile (entry.path, bytes);
        CHRN (hr, FormatFlushLossMessage (entry.path, hr).c_str());

        hr = WozLoader::SetWriteProtectFlag (bytes, writeProtected);
        CHRN (hr, FormatFlushLossMessage (entry.path, hr).c_str());

        if (m_flushSink)
        {
            hr = m_flushSink (entry.path, bytes);
        }
        else
        {
            hr = WriteFileAtomically (entry.path, bytes);
        }

        CHRN (hr, FormatFlushLossMessage (entry.path, hr).c_str());

        // The live image follows the file, and only once the file has
        // actually changed -- so a failed write leaves the two agreeing.
        entry.image->SetImageWriteProtected (writeProtected);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildSalvagedImage
//
//  Recover every sector that decoded, verified or not, and rebuild the result
//  as a WOZ. Re-nibblizing is what makes a recovered sector readable: it gets
//  a correct checksum by construction, so a sector that would have made DOS
//  report an I/O error becomes an ordinary one holding possibly-wrong bytes.
//
//  The rebuilt image carries the source's META across but not its INFO, so
//  the copy still says which disk it is while the creator field says Casso
//  wrote this file. Putting Applesauce's name on a lossy reconstruction would
//  be the same class of lie the creator policy exists to prevent.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::DecodeForSalvage (
    Entry             &  entry,
    vector<Byte>      &  outSectors,
    DenibblizeReport  &  report)
{
    HRESULT  hr       = S_OK;
    bool     hasImage = false;



    hasImage = (entry.mounted && entry.image != nullptr);
    CBRAEx (hasImage, E_INVALIDARG);

    hr = NibblizationLayer::SalvageSectors (*entry.image, DiskFormat::Dsk, outSectors, report);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildSalvagedImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::BuildSalvagedImage (
    Entry             &  entry,
    vector<Byte>      &  outBytes,
    DenibblizeReport  &  report)
{
    HRESULT       hr       = S_OK;
    DiskImage     rebuilt;
    WozMetadata   carried;
    vector<Byte>  sectors;



    hr = DecodeForSalvage (entry, sectors, report);
    CHR (hr);

    hr = NibblizationLayer::NibblizeDsk (sectors, rebuilt);
    CHR (hr);

    // META travels, INFO does not: the disk is still the same title, but the
    // file is Casso's work now. An empty infoPayload is what tells the writer
    // to stamp its own creator.
    carried.passThrough = entry.image->GetWozMetadata().passThrough;
    rebuilt.SetWozMetadata (carried);
    rebuilt.SetSourceFormat (DiskFormat::Woz);

    hr = WozLoader::Serialize (rebuilt, outBytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSalvageOffered
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::IsSalvageOffered (int slot, int drive) const
{
    if (!IsValidBay (slot, drive))
    {
        return false;
    }

    return GetEntry (slot, drive).salvageOffered;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssessSalvage
//
//  What salvage would cost, without doing it.
//
//  Offered only for a disk that is BOTH damaged and ordinarily formatted.
//  Undamaged disks are writable already, so a lossy copy would be pure loss;
//  copy-protected ones have no standard sectors to recover, and rebuilding
//  them from sectors would destroy the non-standard tracks they depend on.
//  Neither ever reaches the dialog, which is why the dialog never has to
//  explain copy protection.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::AssessSalvage (int slot, int drive, SalvageAssessment & out)
{
    HRESULT       hr         = S_OK;
    bool          bayOk      = false;
    bool          hasImage   = false;
    bool          isDamaged  = false;
    bool          isStandard = false;
    vector<Byte>  sectors;



    out = SalvageAssessment();

    bayOk = IsValidBay (slot, drive);
    CBRAEx (bayOk, E_INVALIDARG);

    {
        Entry &  entry = GetEntry (slot, drive);

        hasImage = (entry.mounted && entry.image != nullptr);
        CBREx (hasImage, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

        // Damage is free to test and decoding is not, so test damage first.
        // This runs from the Disk menu's enable query, which means it runs
        // every time that menu is drawn: decoding both drives unconditionally
        // cost 11 ms an ordinary disk and 154 ms a copy-protected one, where a
        // protected track burns its whole attempt budget before giving up.
        // Salvage is only ever offered for a damaged disk, so an undamaged one
        // never needs the decode at all.
        isDamaged = entry.image->HasSourceCrcMismatch();
        BAIL_OUT_IF (!isDamaged, S_OK);

        hr = DecodeForSalvage (entry, sectors, out.report);
        CHR (hr);

        out.totalSectors = out.report.tracksPresent * NibblizationLayer::kSectorsPerTrack;

        // A track with no standard structure at all is the signature of copy
        // protection, and rebuilding from sectors would destroy it.
        isStandard    = (out.report.tracksPresent > 0) && (out.report.tracksUnformatted == 0);
        out.isOffered = isStandard;

        if (!entry.path.empty())
        {
            fs::path  source = fs::path (entry.path);

            out.suggestedPath = (source.parent_path()
                                 / (source.stem().string() + ".salvaged.woz")).string();
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SalvageToFile
//
//  Writes the salvaged copy. The original is never opened for writing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::SalvageToFile (
    int                 slot,
    int                 drive,
    const string     &  path,
    DenibblizeReport &  report)
{
    HRESULT       hr       = S_OK;
    bool          bayOk    = false;
    bool          hasImage = false;
    bool          hasPath  = !path.empty();
    bool          isSource = false;
    vector<Byte>  bytes;



    bayOk = IsValidBay (slot, drive);
    CBRAEx (bayOk, E_INVALIDARG);
    CBRAEx (hasPath, E_INVALIDARG);

    {
        Entry &  entry = GetEntry (slot, drive);

        hasImage = (entry.mounted && entry.image != nullptr);
        CBREx (hasImage, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

        // Refusing to write over the source is not defensive coding: the
        // entire point of salvage is that the damaged original survives to
        // stay detectably damaged.
        isSource = (path == entry.path);
        CBRAEx (!isSource, E_INVALIDARG);

        hr = BuildSalvagedImage (entry, bytes, report);
        CHRN (hr, FormatSalvageFailedMessage (path).c_str());

        if (m_flushSink)
        {
            hr = m_flushSink (path, bytes);
        }
        else
        {
            hr = WriteFileAtomically (path, bytes);
        }

        CHRN (hr, FormatSalvageFailedMessage (path).c_str());
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatSalvageFailedMessage
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskImageStore::FormatSalvageFailedMessage (const string & path)
{
    wstring  widePath = fs::path (path).wstring();



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    return L"Casso could not write the salvaged copy:\n\n" + widePath +
           L"\n\nThe original disk image was not changed.";
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
    HRESULT   hr = S_OK;



    if (m_imageReader)
    {
        hr = m_imageReader (path, bytes);
        CHR (hr);
        BAIL_OUT_IF (true, S_OK);
    }

    hr = ReadFileBytes (path, bytes);
    CHR (hr);

Error:
    return hr;
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
//  FlushAll
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::FlushAll()
{
    HRESULT   hr      = S_OK;
    HRESULT   hrFirst = S_OK;
    int       slot    = 0;
    int       drive   = 0;



    for (slot = 0; slot < kSlotCount; slot++)
    {
        for (drive = 0; drive < kDriveCount; drive++)
        {
            hr = FlushEntry (m_entries[slot][drive]);

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
//  Auto-flush dirty before releasing. Honors FR-025.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::Eject (int slot, int drive)
{
    HRESULT   hr = S_OK;



    // An out-of-range bay and an empty one are both nothing to eject.
    if (IsValidBay (slot, drive) && GetEntry (slot, drive).mounted)
    {
        Entry &   entry = GetEntry (slot, drive);

        // Flush failures are reported to the user by FlushEntry itself; the
        // eject proceeds either way, because refusing to unmount would leave
        // the user with no way to get the disk out.
        hr = FlushEntry (entry);
        IGNORE_RETURN_VALUE (hr, S_OK);

        //  Before the path goes: the watch is keyed by the directory the
        //  image sits in, and there is no finding that from a cleared path.
        EndWatching (slot, drive);

        entry.image.reset();
        entry.path.clear();
        entry.mounted = false;
        entry.sharedState.Eject();

        //  The disk left. Emitted after the bay is empty, so the handler that
        //  detaches the controller sees the post-eject state.
        EmitBayChange (slot, drive, BayChange::Ejected);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  FR-034 / Phase 4 contract: keep mounts mounted, flush every dirty
//  image so a soft reset never loses user writes.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::SoftReset()
{
    HRESULT   hr = FlushAll();



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
    return IsValidBay (slot, drive) && GetEntry (slot, drive).mounted;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSourcePath
//
////////////////////////////////////////////////////////////////////////////////

const string & DiskImageStore::GetSourcePath (int slot, int drive) const
{
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
    ImageIdentity  identity;



    if (m_identityReader)
    {
        identity = m_identityReader (path);
    }
    else
    {
        identity = ImageIdentity::ReadFromFileSystem (path);
    }

    return identity;
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



    if (IsValidBay (slot, drive))
    {
        state = &GetEntry (slot, drive).sharedState;
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::FormatExternalChangeMessage
//
//  Why a commit was refused because the file moved under it.
//
//  IT SAYS THE WRITES ARE STILL HELD, which is the difference between this and
//  every other flush failure and the only thing the user actually wants to
//  know. The image keeps its dirty bit, so the writes are in memory and a later
//  flush still carries them.
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskImageStore::FormatExternalChangeMessage (const string & path)
{
    wstring  widePath = fs::path (path).wstring();



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    return L"Casso did not save changes to the disk image:\n\n" + widePath +
           L"\n\nThe file was changed by something else since it was mounted, and "
           L"writing now would discard that change. Your writes are still held in "
           L"memory and were not lost.";
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
//  DiskImageStore::NoteExternalChange
//
//  Somebody says this image changed.
//
//  IT RECORDS AND RETURNS. The watcher calls this from its own thread and the
//  message channel from the UI thread, while reading an image and swapping it
//  under a running guest belongs to the thread that owns disk writes. Doing the
//  work here would race the emulator mid-sector.
//
//  A PATH NO BAY HOLDS IS NOT AN ERROR. A directory watch reports every file
//  under it, and an intent may be stated for an image nothing has mounted; both
//  are ordinary and both end here, silently.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::NoteExternalChange (const string & path, ExternalChangeIntent intent)
{
    std::lock_guard<std::mutex>  held (m_pendingMutex);
    int64_t  now   = GetNowMs();
    int      slot  = 0;
    int      drive = 0;



    for (slot = 0; slot < kSlotCount; slot++)
    {
        for (drive = 0; drive < kDriveCount; drive++)
        {
            Entry &  entry = m_entries[slot][drive];

            if (entry.mounted && MountedImageState::IsSamePath (entry.path, path))
            {
                entry.sharedState.NoteChange (now, intent);
            }
        }
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::ApplyPendingReload
//
//  Act on whatever has settled, on the thread that owns disk writes.
//
//  CALLED OFTEN AND DOES NOTHING ALMOST EVERY TIME. It is pumped from the
//  controller's idle callback and from motor spindown, so the common path is a
//  loop over sixteen bays that finds nothing pending and returns.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::ApplyPendingReload()
{
    int      readySlot[kSlotCount * kDriveCount]  = {};
    int      readyDrive[kSlotCount * kDriveCount] = {};
    int      readyCount                           = 0;
    int      slot                                 = 0;
    int      drive                                = 0;
    int      i                                    = 0;
    int64_t  now                                  = GetNowMs();



    //  ONE CLOCK READING AND ONE LOCK FOR THE WHOLE WALK, not one of each per
    //  bay. This runs sixty times a second forever, and asking the clock
    //  sixteen times and taking the lock sixteen times cost 695 ns a call --
    //  measured, and about a hundred times what the work itself is worth. The
    //  snapshot below brings it back to the handful of comparisons it should be.
    {
        std::lock_guard<std::mutex>  guard (m_pendingMutex);

        for (slot = 0; slot < kSlotCount; slot++)
        {
            for (drive = 0; drive < kDriveCount; drive++)
            {
                const Entry &  entry = m_entries[slot][drive];

                if (entry.mounted
                 && entry.image != nullptr
                 && entry.sharedState.IsSettled (now))
                {
                    readySlot[readyCount]  = slot;
                    readyDrive[readyCount] = drive;
                    readyCount++;
                }
            }
        }
    }

    //  ACTED ON OUTSIDE THE LOCK. Reloading an image reads a file and swaps a
    //  disk; holding the pending mutex across that would block the watcher
    //  thread for the length of a read.
    for (i = 0; i < readyCount; i++)
    {
        ApplyPendingReloadToBay (readySlot[i], readyDrive[i]);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::ApplyPendingReloadToBay
//
//  One bay's settled change, from noticing it to acting on it.
//
//  THE READ HAPPENS BEFORE THE DECISION, and that ordering is what lets the
//  policy rank "cannot be used" above everything else: whether the new bytes
//  can be this disk is not knowable without reading them.
//
//  A CHANGE THAT TURNS OUT NOT TO BE ONE IS DROPPED. A directory watch reports
//  a write to any file in the folder and a stated intent is only a hint, so the
//  recorded identity is what settles whether anything actually happened. This is
//  also what stops the store's own commit from coming back as somebody else's
//  change.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::ApplyPendingReloadToBay (int slot, int drive)
{
    HRESULT                            hr        = S_OK;
    Entry                            & entry     = GetEntry (slot, drive);
    bool                               settled   = false;
    bool                               held      = false;
    bool                               usable    = false;
    bool                               unchanged = false;
    ExternalChangeIntent               intent    = ExternalChangeIntent::Unstated;
    ChangeAction                       action    = ChangeAction::Ignore;
    ImageIdentity                      current;
    vector<Byte>                       bytes;
    DiskFormat                         fmt       = DiskFormat::Dsk;
    ExternalChangePolicy::Situation    situation;



    {
        std::lock_guard<std::mutex>  guard (m_pendingMutex);

        settled = entry.mounted
               && entry.image != nullptr
               && entry.sharedState.IsSettled (GetNowMs());
        intent  = entry.sharedState.GetPending().intent;
    }

    if (!settled)
    {
        return;
    }

    //  Deferred rather than refused, indefinitely and silently: the pick-up
    //  simply happens once the hold is released. Both writers in this system
    //  commit atomically, but a text editor or a copy tool need not, and the
    //  quiet period alone does not cover one that takes its time.
    if (m_fileIo != nullptr && m_fileIo->IsHeldByAnotherProcess (entry.path))
    {
        return;
    }

    current   = ReadIdentity (entry.path);
    unchanged = entry.sharedState.GetIdentity().Matches (current);

    if (unchanged)
    {
        std::lock_guard<std::mutex>  guard (m_pendingMutex);

        entry.sharedState.ClearPending();

        return;
    }

    //  Read, and decide whether these bytes can be this disk. A trial load into
    //  a throwaway image is the only honest answer -- the extension and the
    //  length agree with plenty of files that will not load.
    hr = ReadImageFile (entry.path, bytes);

    if (SUCCEEDED (hr))
    {
        hr = GetSourceFormatByExtension (entry.path, fmt);
    }

    if (SUCCEEDED (hr))
    {
        DiskImage  trial;

        trial.LoadFromBytes (fmt, bytes, entry.path);
        usable = trial.IsLoaded();
    }

    situation.changeSeen  = true;
    situation.usable      = usable;
    situation.heldByOther = false;
    situation.intent      = intent;

    //  The guest's unsaved writes are what turns a pick-up into a conflict.
    //  They were deferred outright while there was nowhere to preserve them;
    //  now there is, so the flag goes to the policy and the conflict is
    //  resolved rather than postponed.
    situation.guestDirty  = entry.image->IsDirty();

    action = ExternalChangePolicy::Decide (situation);

    //  A CONFLICT IS RESOLVED INTO AN ORDINARY CHANGE, HERE, BEFORE ANYTHING
    //  ELSE IS DECIDED. The guest's version goes to a file of its own straight
    //  away, which is the whole guarantee: once it is on disk nothing that
    //  follows can lose it, and what follows is then the same decision any
    //  external change gets.
    //
    //  THE FILE ALWAYS STAYS WITH WHOEVER CHANGED IT, and the guest's version
    //  is always the one that moves. The flush path applies the same rule from
    //  the other end, so the outcome no longer depends on which of the two
    //  found the collision first -- it used to, and the winner was whichever
    //  side happened to be quicker.
    if (action == ChangeAction::Conflict)
    {
        hr = SaveLoadedImage (entry, entry.preservedPath);

        if (FAILED (hr))
        {
            //  Nothing is mounted and nothing is overwritten. The pending
            //  change is DROPPED RATHER THAN RETRIED: retrying costs a full
            //  image read and a failed write on every idle tick, sixty times a
            //  second for as long as the folder stays full. The next change to
            //  the file, or the next flush, tries again.
            {
                std::lock_guard<std::mutex>  guard (m_pendingMutex);

                entry.sharedState.ClearPending();
            }

            //  ASKED RATHER THAN REPORTED, because "Save as..." is an answer
            //  and a notice has nowhere to put one. The bay is left with the
            //  question outstanding so a second failure does not stack a
            //  second dialog on the first.
            if (m_askSink && !entry.sharedState.IsAskOutstanding())
            {
                entry.sharedState.SetAskOutstanding (true);
                entry.sharedState.SetAskedAction (ChangeAction::Conflict);

                m_askSink (slot, drive,
                           ChangePrompt::ComposeSaveFailure (entry.path, drive,
                                                             entry.preservedPath, hr,
                                                             SaveFailureCause::ExternalChange));
            }

            return;
        }

        //  It is on disk under its own name now, so the bay carries nothing
        //  unsaved and the change that follows is no longer a conflict.
        entry.preservedWritten = true;

        entry.image->ClearDirty();

        situation.guestDirty = false;
        action               = ExternalChangePolicy::Decide (situation);
    }

    //  A file that is simply gone gets its own sentence rather than sharing
    //  one with a file that is present and unreadable.
    if (action == ChangeAction::Unusable && !DoesPathExist (entry.path))
    {
        action = ChangeAction::Deleted;
    }

    //  A question nothing can answer stays pending rather than resolving itself
    //  by default. A headless host has no way to ask, and picking one of the
    //  answers on the user's behalf is what the fallback exists for.
    //
    //  ONE QUESTION AT A TIME PER BAY. Asking cannot block -- the answer comes
    //  from another thread -- so the change is still pending on the next idle
    //  tick, and without this the user would be asked again sixty times a
    //  second while reading the first one.
    if (ExternalChangePolicy::NeedsAnAnswer (action))
    {
        if (m_askSink && !entry.sharedState.IsAskOutstanding())
        {
            entry.sharedState.SetAskOutstanding (true);
            entry.sharedState.SetAskedAction (action);

            {
                HRESULT  hrName = S_OK;

                //  RESERVED, NOT JUST CALCULATED. The name shown here is the
                //  one the copy will take whoever writes it -- this bay's own
                //  flush may get there first, while this question is still on
                //  screen. Working it out again at that point produced a
                //  different name and left the dialog offering a file nobody
                //  ever created.
                if (entry.preservedPath.empty())
                {
                    hrName = FindFreePreservedPath (entry.path, entry.preservedPath);
                    IGNORE_RETURN_VALUE (hrName, S_OK);
                }

                m_askSink (slot, drive,
                           ChangePrompt::Compose (entry.path, drive, action,
                                                  entry.preservedPath,
                                                  entry.preservedWritten));
            }
        }

        return;
    }

    CarryOutChangeAction (slot, drive, action, bytes);

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::ResolvePendingChange
//
//  The answer to a question this store asked.
//
//  THE BYTES ARE READ AGAIN HERE RATHER THAN KEPT FROM THE QUESTION. The user
//  may have run two more builds while the prompt sat on the screen, and the
//  version they mean is the one they just built -- not the one that was on disk
//  when the question was composed.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::ResolvePendingChange (int slot, int drive, ChangeAction chosen,
                                          const string & savePath)
{
    HRESULT       hr = S_OK;
    vector<Byte>  bytes;



    if (!IsValidBay (slot, drive))
    {
        return;
    }

    {
        Entry &  entry = GetEntry (slot, drive);

        entry.sharedState.SetAskOutstanding (false);

        if (!entry.mounted || entry.image == nullptr)
        {
            return;
        }

        //  "Save as..." after a copy could not be written where this store
        //  chose. The disk stays in the drive and the bay moves onto the file
        //  the user picked, which is the same outcome as keeping it -- only the
        //  folder is theirs rather than ours.
        if (entry.sharedState.GetAskedAction() == ChangeAction::Conflict)
        {
            entry.sharedState.SetAskedAction (ChangeAction::Ignore);

            if (chosen == ChangeAction::PreserveCopy && !savePath.empty())
            {
                vector<Byte>  held;

                hr = entry.image->Serialize (held);

                if (SUCCEEDED (hr))
                {
                    hr = WritePreserved (savePath, held);
                }

                if (FAILED (hr))
                {
                    //  Still nowhere to put it. The disk is untouched and the
                    //  question stands rather than being quietly dropped.
                    entry.sharedState.SetAskOutstanding (true);

                    if (m_askSink)
                    {
                        m_askSink (slot, drive,
                                   ChangePrompt::ComposeSaveFailure (
                                       entry.path, drive, savePath, hr,
                                       SaveFailureCause::ExternalChange));
                    }

                    return;
                }

                entry.image->ClearDirty();
                entry.preservedPath.clear();

                hr = RepointBayToFile (slot, drive, savePath);
                IGNORE_RETURN_VALUE (hr, S_OK);
            }

            {
                std::lock_guard<std::mutex>  guard (m_pendingMutex);

                entry.sharedState.ClearPending();
            }

            return;
        }

        //  An answer about a file that has gone is being given to a question
        //  that no longer applies. Saving what is held and emptying the drive
        //  is the only thing still on offer.
        //
        //  THE SAVE HAPPENS BEFORE THE EJECT, always. The eject is what
        //  discards the in-memory disk, so a failure between the two must
        //  leave the disk in the drive rather than gone from both places.
        if (ExternalChangePolicy::IsFileLost (entry.sharedState.GetAskedAction()))
        {
            entry.sharedState.SetAskedAction (ChangeAction::Ignore);

            if (chosen == ChangeAction::PreserveCopy && !savePath.empty())
            {
                vector<Byte>  held;

                hr = entry.image->Serialize (held);

                if (SUCCEEDED (hr))
                {
                    hr = WritePreserved (savePath, held);
                }

                //  A save the user asked for and did not get must not be
                //  followed by throwing the only copy away.
                if (FAILED (hr))
                {
                    entry.sharedState.SetAskOutstanding (true);

                    if (m_askSink)
                    {
                        m_askSink (slot, drive,
                                   ChangePrompt::ComposeSaveFailure (entry.path, drive,
                                                                     savePath, hr,
                                                                     SaveFailureCause::FileLost));
                    }

                    return;
                }

                //  THE DISK STAYS IN THE DRIVE, ON THE FILE THE USER PICKED.
                //  Serialize writes a whole image, not a fragment, so the drive
                //  now has somewhere to live -- and emptying it would mean
                //  handing back a complete disk and then making them go and
                //  find it. Keeping a version during a conflict already works
                //  this way, and one act should not have two outcomes.
                entry.image->ClearDirty();

                hr = RepointBayToFile (slot, drive, savePath);
                IGNORE_RETURN_VALUE (hr, S_OK);

                return;
            }

            //  Nothing was saved, so there is nothing to point at.
            EjectLostImage (slot, drive);

            return;
        }

        hr = ReadImageFile (entry.path, bytes);

        //  Answering "take it up" about a file that has since gone is not the
        //  same question any more.
        if (FAILED (hr) && chosen != ChangeAction::Ignore)
        {
            chosen = ChangeAction::Deleted;
        }
    }

    CarryOutChangeAction (slot, drive, chosen, bytes);

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::CarryOutChangeAction
//
//  Doing what was decided.
//
//  ONE SHAPE WHETHER THE POLICY DECIDED IT OR THE USER DID. An answer chosen in
//  a dialog arrives here as the same value the policy would have produced, so
//  there is no second implementation of "take it up" that can drift from the
//  first.
//
//  A REPORT IS RAISED ONCE AND ABSORBS WHAT FOLLOWS. Three builds before the
//  developer turns back to the emulator are three pick-ups and one report: the
//  contents reloaded are always the most recent, and three reports about one
//  disk say nothing three times.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::CarryOutChangeAction (int slot, int drive, ChangeAction action,
                                           const vector<Byte> & bytes)
{
    HRESULT  hr           = S_OK;
    Entry &  entry        = GetEntry (slot, drive);
    bool     restarted    = false;
    bool     tookUp       = false;
    bool     preserved    = false;
    bool     preserveFail = false;
    string   preservedPath;
    //  KEEPING MOVES THE BAY ONTO THE COPY, so entry.path is no longer the file
    //  any of these messages are about by the time they are composed. Captured
    //  before anything can move it.
    string   original     = entry.path;



    switch (action)
    {
    case ChangeAction::ReloadInPlace:
    case ChangeAction::Restart:
        hr = MountExternallyModifiedDisk (slot, drive, bytes);

        if (SUCCEEDED (hr))
        {
            tookUp    = true;
            restarted = (action == ChangeAction::Restart);
        }
        else
        {
            //  The bytes passed a trial load and then failed the real one.
            //  Nothing was swapped, so the mounted disk is untouched and the
            //  user is told what they are still holding.
            action = ChangeAction::Unusable;
        }

        break;

    case ChangeAction::KeepHeld:
        //  The user kept the disk that is in the drive, so it gets a file of
        //  its own and the bay moves onto it. The original keeps whatever the
        //  other program wrote.
        //
        //  WRITTEN NOW RATHER THAN AT SOME LATER FLUSH. Deferring it meant the
        //  kept version lived only in memory: a disk the guest had not written
        //  to was not dirty, so quitting or ejecting discarded the very thing
        //  the user had just chosen to keep, without a word. A file on disk is
        //  the only form of "kept" that survives.
        //  The flush may have written it already, under the very name the
        //  question showed. Writing again would make a second copy of the same
        //  disk and leave the first orphaned.
        if (!entry.preservedWritten)
        {
            hr = SaveLoadedImage (entry, entry.preservedPath);

            if (FAILED (hr))
            {
                preserveFail  = true;
                preservedPath = entry.preservedPath;
                break;
            }

            entry.preservedWritten = true;
        }

        preservedPath = entry.preservedPath;
        preserved     = true;

        entry.image->ClearDirty();

        //  The disk in the drive is untouched by this; only the file behind it
        //  changes. Repointing also drops the pending change, which is right:
        //  the file it referred to is not this bay's any more.
        hr = RepointBayToFile (slot, drive, preservedPath);
        IGNORE_RETURN_VALUE (hr, S_OK);

        entry.preservedPath.clear();
        entry.preservedWritten = false;

        break;

    case ChangeAction::Defer:
        //  Leave it pending. Nothing to say and nothing to clear.
        return;

    default:
        break;
    }

    if (!preserveFail)
    {
        std::lock_guard<std::mutex>  guard (m_pendingMutex);

        entry.sharedState.ClearPending();
    }

    //  Whatever was decided, the conflict is over: either the copy was taken
    //  up as the bay's file or the external version went in beside it.
    if (tookUp)
    {
        preservedPath = entry.preservedPath;
        preserved     = preserved || entry.preservedWritten;

        entry.preservedPath.clear();
        entry.preservedWritten = false;
    }

    if (restarted && m_restartCallback)
    {
        m_restartCallback();
    }

    //  A COPY THAT COULD NOT BE WRITTEN IS A QUESTION, not a notice: it offers
    //  somewhere else to put the file, and a notice has nowhere to put an
    //  answer. It leaves the drive exactly as it was either way.
    if (preserveFail)
    {
        if (m_askSink && !entry.sharedState.IsAskOutstanding())
        {
            entry.sharedState.SetAskOutstanding (true);
            entry.sharedState.SetAskedAction (ChangeAction::Conflict);

            m_askSink (slot, drive,
                       ChangePrompt::ComposeSaveFailure (original, drive, preservedPath, hr,
                                                         SaveFailureCause::ExternalChange));
        }

        return;
    }

    //  EVERY TIME, NOT ONLY THE FIRST. The sink re-words a notice already up
    //  for this bay rather than raising a second, so absorbing further changes
    //  costs nothing -- and reporting only the first left the bar saying
    //  something that had stopped being true.
    if (m_reportSink)
    {
        ChangePrompt  report;

        //  THE RELOAD IS REPORTED AHEAD OF PRESERVING, because a conflict that
        //  reloaded did both and the reload is the headline; the copy is a
        //  clause on the end of it.
        if (tookUp)
        {
            report = ChangePrompt::ComposeReloadReport (original, drive, restarted,
                                                        m_machineName, preservedPath);
        }
        else if (preserved)
        {
            report = ChangePrompt::ComposeConflictReport (original, drive, preservedPath);
        }
        else
        {
            report = ChangePrompt::Compose (original, drive, action);
        }

        if (!report.title.empty())
        {
            entry.sharedState.SetReportStanding (true);
            m_reportSink (slot, drive, report);
        }
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::MountExternallyModifiedDisk
//
//  Swaps a mounted image's contents for the ones on disk.
//
//  IT DOES NOT FLUSH, AND THAT IS THE WHOLE POINT. The ordinary remount path
//  flushes what the bay held before loading, which here would write the old
//  disk straight over the change that prompted the swap.
//
//  THE NEW BYTES LOAD INTO A FRESH IMAGE FIRST, so a load that fails leaves the
//  mounted disk exactly as it was. The machine is running and what it holds is
//  known-good; there is no version of this worth half-doing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::MountExternallyModifiedDisk (int slot, int drive, const vector<Byte> & bytes)
{
    HRESULT                  hr         = S_OK;
    Entry                  & entry      = GetEntry (slot, drive);
    SalvageAssessment        assessment;
    HRESULT                  hrAssess   = S_OK;
    bool                     usable     = false;
    unique_ptr<DiskImage>    loaded     = make_unique<DiskImage> ();



    loaded->LoadFromBytes (entry.format, bytes, entry.path);

    usable = loaded->IsLoaded();
    CBR (usable);

    //  THE CONTENTS MOVE, THE OBJECT STAYS. The controller holds a raw pointer
    //  to this DiskImage -- SetExternalDisk hands one over at mount -- so
    //  replacing the unique_ptr would leave the drive reading freed memory the
    //  instant a disk was reloaded. Assigning through keeps the address the
    //  drive was given.
    *entry.image = std::move (*loaded);

    //  The identity is refreshed from the file the bytes came from, so the
    //  swap does not immediately look like another external change.
    entry.sharedState.SetIdentity (ReadIdentity (entry.path));

    hrAssess             = AssessSalvage (slot, drive, assessment);
    entry.salvageOffered = SUCCEEDED (hrAssess) && assessment.isOffered;

    //  The disk in the drive was replaced under the running machine. Swapped,
    //  not Inserted: the door opens and closes rather than only closing,
    //  because from the user's seat a disk came out and another went in.
    EmitBayChange (slot, drive, BayChange::Swapped);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::ClearChangeReport
//
//  The user dismissed the report for a bay.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::ClearChangeReport (int slot, int drive)
{
    if (IsValidBay (slot, drive))
    {
        GetEntry (slot, drive).sharedState.SetReportStanding (false);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::RepointBayToFile
//
//  Moves a bay onto a different file. The disk in the drive does not change.
//
//  A SAVE-AS RATHER THAN A MOUNT. Nothing is loaded and nothing is swapped, so
//  a guest reading the drive sees no interruption; only the file this bay will
//  read and write from here on is different.
//
//  THE WATCH MOVES WITH IT, and the old file stops being this bay's business.
//  Two disks out of one folder share a watch, so the common case of a
//  timestamped copy beside its original costs nothing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::RepointBayToFile (int slot, int drive, const string & newPath)
{
    HRESULT  hr     = S_OK;
    bool     usable = IsValidBay (slot, drive) && !newPath.empty();



    CBR (usable);

    {
        Entry &  entry = GetEntry (slot, drive);

        CBR (entry.mounted && entry.image != nullptr);

        //  Before the path moves, while EndWatching can still tell which
        //  directory this bay was using.
        EndWatching (slot, drive);

        entry.path             = newPath;
        entry.preservedPath.clear();
        entry.preservedWritten = false;

        //  A fresh identity for the new file, and nothing pending against the
        //  old one. Mount is what records both.
        entry.sharedState.Mount (ReadIdentity (newPath));

        BeginWatching (slot, drive);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::BeginWatching
//
//  Starts a watch on the directory holding a bay's image.
//
//  A DIRECTORY THAT CANNOT BE WATCHED IS RECORDED, NOT REPORTED. A network
//  share or a synchronizing folder is exactly the case that produces one, and
//  the guarantee does not live here: the check made before every write is what
//  carries it, and notification only makes it prompt.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::BeginWatching (int slot, int drive)
{
    Entry &  entry     = GetEntry (slot, drive);
    string   directory = MountedImageState::GetDirectory (entry.path);
    bool     watching  = false;



    if (m_watcher != nullptr && !directory.empty())
    {
        watching = m_watcher->Watch (directory,
                                     [this] (const string & path)
                                     {
                                         NoteExternalChange (path, ExternalChangeIntent::Unstated);
                                     });
    }

    entry.sharedState.SetWatching (watching);

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::EndWatching
//
//  Drops a bay's watch, unless another bay still needs the same directory.
//
//  TWO DISKS OUT OF ONE FOLDER IS THE ORDINARY CASE -- a boot disk and a work
//  disk built by the same script -- and ejecting the first must not blind the
//  second.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::EndWatching (int slot, int drive)
{
    Entry   & entry      = GetEntry (slot, drive);
    string    directory  = MountedImageState::GetDirectory (entry.path);
    bool      stillUsed  = false;
    int       otherSlot  = 0;
    int       otherDrive = 0;



    for (otherSlot = 0; otherSlot < kSlotCount; otherSlot++)
    {
        for (otherDrive = 0; otherDrive < kDriveCount; otherDrive++)
        {
            bool  isSelf = (otherSlot == slot && otherDrive == drive);

            if (!isSelf && m_entries[otherSlot][otherDrive].mounted)
            {
                string  other = MountedImageState::GetDirectory (m_entries[otherSlot][otherDrive].path);

                stillUsed = stillUsed || MountedImageState::IsSamePath (other, directory);
            }
        }
    }

    if (m_watcher != nullptr && !directory.empty() && !stillUsed)
    {
        m_watcher->Unwatch (directory);
    }

    entry.sharedState.SetWatching (false);

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::DoesPathExist
//
//  Whether something is already sitting at a path.
//
//  THROUGH THE FILE SEAM WHEN ONE IS INSTALLED, so a test that keeps its files
//  in memory is asked about the world it actually has. Reaching past it to the
//  filesystem would let the collision loop below hand out a name a test already
//  used.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::DoesPathExist (const string & path) const
{
    std::error_code  ec;



    if (m_fileIo != nullptr)
    {
        return m_fileIo->Exists (path);
    }

    return fs::exists (fs::path (path), ec);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::FindFreePreservedPath
//
//  A preserved-copy name nothing is sitting at yet.
//
//  THE LOOP IS THE POINT. A one-second timestamp cannot keep the promise that
//  repeated conflicts accumulate rather than overwrite each other, and two
//  conflicts on one image inside a second is exactly what a build loop makes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::FindFreePreservedPath (const string & imagePath, string & outPath) const
{
    HRESULT  hr      = S_OK;
    string   stamp   = PreservedCopy::MakeStamp (m_timestamp ? m_timestamp() : time (nullptr));
    bool     isFree  = false;
    int      attempt = 0;



    for (attempt = 0; attempt < PreservedCopy::kMaxAttempts; attempt++)
    {
        string  candidate = PreservedCopy::MakePath (imagePath, stamp, attempt);

        if (!DoesPathExist (candidate))
        {
            outPath = candidate;
            isFree  = true;
            break;
        }
    }

    //  Ninety-nine names taken in one second is not a collision any more, it is
    //  something else going wrong, and inventing a hundredth would not help.
    CBR (isFree);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::WritePreserved
//
//  Writes a preserved copy.
//
//  THROUGH THE SAME SINK AS A FLUSH, so a test captures preserved copies the
//  way it captures everything else this class writes, and so the atomic
//  temp-then-rename applies to them too. A preserved copy that could be found
//  half-written would be worth nothing at the moment it is needed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::WritePreserved (const string & path, const vector<Byte> & bytes)
{
    HRESULT  hr = S_OK;



    if (m_flushSink)
    {
        hr = m_flushSink (path, bytes);
    }
    else
    {
        hr = WriteFileAtomically (path, bytes);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::SaveLoadedImage
//
//  Writes what the bay is holding to a file of its own.
//
//  SERIALIZED FROM THE MOUNTED IMAGE, NOT COPIED FROM THE FILE. The entire
//  reason this version needs preserving is that the file no longer holds it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::SaveLoadedImage (Entry & entry, string & outPath)
{
    HRESULT       hr = S_OK;
    vector<Byte>  bytes;



    CBR (entry.image != nullptr);

    hr = entry.image->Serialize (bytes);
    CHR (hr);

    //  A name already reserved is used as it is. Choosing a fresh one here is
    //  what let a question and a flush disagree about where the copy went.
    if (outPath.empty())
    {
        hr = FindFreePreservedPath (entry.path, outPath);
        CHR (hr);
    }

    hr = WritePreserved (outPath, bytes);
    CHR (hr);

Error:
    //  outPath IS LEFT AS THE PATH THAT WAS TRIED, even on failure. The notice
    //  raised for a failed copy prints where it tried to write, and clearing it
    //  here left that notice with nothing to show but an error code.
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::PreserveGivenBytes
//
//  Writes bytes that came off the file to a file of their own.
//
//  THE OTHER DIRECTION. Here the emulator is about to write its own version
//  over an external change it never reloaded, so the version being displaced
//  is the one on disk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::PreserveGivenBytes (const Entry & entry, const vector<Byte> & bytes,
                                            string & outPath)
{
    HRESULT  hr = S_OK;



    hr = FindFreePreservedPath (entry.path, outPath);
    CHR (hr);

    hr = WritePreserved (outPath, bytes);
    CHR (hr);

Error:
    //  outPath IS LEFT AS THE PATH THAT WAS TRIED, even on failure. The notice
    //  raised for a failed copy prints where it tried to write, and clearing it
    //  here left that notice with nothing to show but an error code.
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::EjectLostImage
//
//  Empties a bay whose file has gone.
//
//  IT DOES NOT FLUSH, AND THAT IS THE WHOLE DIFFERENCE FROM Eject. The ordinary
//  eject persists a dirty image first, which here would mean writing the disk
//  back to a path the user has just been told no longer exists -- recreating
//  the file they deleted, or failing and reporting a second problem about the
//  first one.
//
//  WHATEVER THE USER WANTED SAVED IS ALREADY SAVED by the time this runs. The
//  offer comes first and this is what follows it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::EjectLostImage (int slot, int drive)
{
    if (!IsValidBay (slot, drive))
    {
        return;
    }

    {
        Entry &  entry = GetEntry (slot, drive);

        EndWatching (slot, drive);

        entry.image.reset();
        entry.path.clear();
        entry.mounted        = false;
        entry.salvageOffered = false;
        entry.sharedState.Eject();
    }

    //  A file that vanished empties the drive the same as a user eject, and
    //  the door and its sound follow the same way.
    EmitBayChange (slot, drive, BayChange::Ejected);

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::GetCommitTemporaryPath
//
//  Where a commit of `path` writes before it renames.
//
//  ONE INVOCATION TAG FOR THE WHOLE PROCESS, taken once. Two emulators get
//  different tags and therefore different names; two flushes inside one
//  emulator get the same tag and are separated by the attempt counter and the
//  existence check, which is enough because they cannot overlap -- every flush
//  runs on the one thread that owns disk writes.
//
////////////////////////////////////////////////////////////////////////////////

string DiskImageStore::GetCommitTemporaryPath (const string & path, unsigned attempt)
{
    static const uint64_t  s_kInvocationTag = CommitPlan::NextInvocationTag();



    return CommitPlan::GetTemporaryPath (path, s_kInvocationTag, attempt);
}
