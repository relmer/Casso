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

    hrSave = FlushEntry (entry, FlushMoment::Replacing, outOutcome);
    IGNORE_RETURN_VALUE (hrSave, S_OK);

    isSameFile = IsSameFile (entry.path, newPath);
    canReplace = !entry.image->IsDirty() || (!isSameFile && IsRecoveryCurrent (entry));
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
        EndWatching (entry.slot, entry.drive);
        RetireBay (entry);
    }

    entry.image   = std::move (loaded);
    entry.path    = path;
    entry.format  = fmt;
    entry.mounted = true;

    hrAssess             = AssessSalvage (entry.slot, entry.drive, assessment);
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
//  DiskImageStore::AreWritesSaved
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::AreWritesSaved (Entry & entry)
{
    return !entry.mounted || entry.image == nullptr || !entry.image->IsDirty() || IsRecoveryCurrent (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::IsRecoveryCurrent
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::IsRecoveryCurrent (Entry & entry)
{
    return entry.image != nullptr
        && entry.sharedState.IsRecoveryCurrent (entry.image->GetImageId(), entry.image->GetTrackGenerations())
        && IsRecoveryCopyIntact (entry);
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
            isSaved = isSaved && AreWritesSaved (m_entries[slot][drive]);
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
//  DiskImageStore::ReportPreserveFailure
//
//  The guest's version could not be written beside the file that displaced it.
//
//  IT IS THE SAME FAILURE THE WATCHER SIDE ALREADY ASKS ABOUT, so it is asked
//  the same way. Which of the two ends notices a collision first is a matter of
//  timing the user cannot see, and it used to decide whether they were offered
//  somewhere else to put the disk or merely told the write had not happened.
//
//  THE ROUTE DIFFERS BY WHAT IS STILL STANDING. While the machine runs the
//  question is posted and answered later, and nothing is lost while it stands
//  -- the image keeps its dirty bit and a later flush retries. An eject is
//  asked the same way but does not go ahead until the answer comes, because
//  emptying the bay is what would destroy a disk that has nowhere else to be.
//  A shutdown has no pump to deliver a posted question and no CPU thread to act
//  on the answer, but it runs on the UI thread with its apartment still up, so
//  it asks through a blocking dialog instead and writes the copy itself.
//
//  WITH NO QUESTION POSSIBLE, THE DISK STAYS IN THE DRIVE. The guest's version
//  is in no file, so an eject is declined, and the notice states that the disk
//  is in the drive and that another eject raises the question again. At a quit
//  there is no drive for the disk, and the notice states that the changes are
//  lost.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::ReportPreserveFailure (Entry & entry, FlushMoment moment,
                                            const string & attemptedPath, HRESULT hrKeep,
                                            const string & original)
{
    SaveFailureCause  cause     = (moment == FlushMoment::Ejecting)
                                      ? SaveFailureCause::Ejecting
                                      : SaveFailureCause::ExternalChange;
    bool              isAskable = m_askSink && !entry.sharedState.IsAskOutstanding();
    bool              isAsked   = false;



    if (moment == FlushMoment::ShuttingDown)
    {
        //  The last chance there is. A copy written here is written by this
        //  call rather than by an answer that will never arrive.
        if (!RescueOnTheWayOut (entry, original, false))
        {
            EhmNotifyUser (FormatPreserveFailureMessage (original, attemptedPath, hrKeep, entry.drive, moment).c_str());
        }
    }
    else
    {
        if (isAskable)
        {
            entry.sharedState.SetAskedAction (ChangeAction::Conflict);
            entry.sharedState.SetAskedCause  (cause);

            isAsked = m_askSink (entry.slot, entry.drive,
                                 ChangePrompt::ComposeSaveFailure (original, entry.drive, attemptedPath,
                                                                   hrKeep, cause));

            entry.sharedState.SetAskOutstanding (isAsked);
        }

        //  THE EJECT WAITS ON THE ANSWER, but only when the question actually
        //  went up. Without one the eject is declined, because the disk's
        //  writes are in no file, and a notice takes the question's place.
        if (moment == FlushMoment::Ejecting && isAsked)
        {
            entry.sharedState.SetEjectWhenAnswered (true);
        }
        else if (moment == FlushMoment::Ejecting)
        {
            EhmNotifyUser (FormatPreserveFailureMessage (original, attemptedPath, hrKeep, entry.drive, moment).c_str());
        }
        else if (!isAsked)
        {
            EhmNotifyUser (FormatExternalChangeMessage (original).c_str());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::RescueOnTheWayOut
//
//  Asks where to put a disk that is about to go, and puts it there.
//
//  SYNCHRONOUS, BECAUSE IT IS THE LAST THING THAT RUNS. No answer is coming
//  back later at this point -- the pump has gone and so has the thread that
//  would act on one -- so the asking and the writing both happen here, inside
//  the call that found the problem.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::RescueOnTheWayOut (Entry & entry, const string & original, bool isUnserializable)
{
    HRESULT       hr        = S_OK;
    string        offered   = isUnserializable ? MakeRecoveryPath (original, 0) : original;
    string        chosen;
    string        written;
    bool          isChosen  = false;
    bool          isNative  = false;
    vector<Byte>  held;



    isChosen = m_rescueSink && m_rescueSink (offered, chosen) && !chosen.empty();
    CBR (isChosen);

    hr       = entry.image->Serialize (held);
    isNative = SUCCEEDED (hr);

    //  A disk whose format cannot store its writes goes out as WOZ, the one
    //  format that stores them whole.
    if (isNative)
    {
        hr = CommitFile (chosen, held, CommitMode::Replace);
        CHR (hr);

        entry.image->MarkSaved (held);
    }
    else
    {
        hr = WriteWozCopy (entry, chosen, written);
        CHR (hr);
    }

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::AskBeforeEject
//
//  The question an eject raises when its save to the disk's file failed and
//  no recovery copy has the writes. The eject waits for the answer, which
//  finishes it either way.
//
//  A DISK WHOSE FORMAT CANNOT STORE ITS WRITES IS OFFERED A WOZ COPY, at the
//  name the failed recovery copy had, so the picker opens on a .woz name.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::AskBeforeEject (Entry & entry, const FlushOutcome & outcome)
{
    MountedImageState  & state     = entry.sharedState;
    bool                 isAskable = m_askSink && !state.IsAskOutstanding();
    bool                 isAsked   = false;
    SaveFailureCause     cause     = outcome.isUnserializable ? SaveFailureCause::Unserializable : SaveFailureCause::Unwritable;
    ChangePrompt         prompt;



    if (isAskable)
    {
        if (outcome.isUnserializable)
        {
            prompt = ChangePrompt::ComposeSaveFailure (entry.path, entry.drive, outcome.recoveryPath,
                                                       outcome.recoveryError, cause);

            prompt.suggestedSavePath = MakeRecoveryPath (entry.path, 0);
        }
        else
        {
            prompt = ChangePrompt::ComposeSaveFailure (entry.path, entry.drive, entry.path,
                                                       outcome.saveError, cause);
        }

        state.SetAskedAction (ChangeAction::Conflict);
        state.SetAskedCause  (cause);

        isAsked = m_askSink (entry.slot, entry.drive, prompt);

        state.SetAskOutstanding   (isAsked);
        state.SetEjectWhenAnswered (isAsked);
    }

    return isAsked;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::WriteWozCopy
//
//  The bay's image, as WOZ, at the path the user chose with its extension
//  changed to .woz. A sector format cannot store what the guest wrote, so a
//  file with that format's extension would misstate its contents.
//
//  IT BECOMES THE RECOVERY COPY, marked as reported, because the user chose
//  where it went. The bay stays on its file, and at a later save that cannot
//  be stored either, this copy is current and nothing is written.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::WriteWozCopy (Entry & entry, const string & path, string & outPath)
{
    HRESULT       hr = S_OK;
    vector<Byte>  wozBytes;



    outPath = fs::path (path).replace_extension (".woz").string();

    hr = WozLoader::Serialize (*entry.image, wozBytes);
    CHR (hr);

    hr = CommitFile (outPath, wozBytes, CommitMode::Replace);
    CHR (hr);

    entry.sharedState.SetRecovery (outPath, entry.image->GetImageId(), entry.image->GetTrackGenerations(), ReadIdentity (outPath));
    entry.sharedState.MarkSaveFailureReported (outPath);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::SaveAnswerCopy
//
//  Writes the copy for a Save as answer. A native copy moves the bay
//  onto the new file, the same outcome as keeping the disk in any other
//  question. Finishing an eject, an image that does not serialize natively is
//  written as WOZ, with the bay on its file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::SaveAnswerCopy (Entry & entry, const string & savePath, bool isEjecting, string & outPath)
{
    HRESULT       hr        = S_OK;
    HRESULT       hrRepoint = S_OK;
    bool          isWoz     = false;
    vector<Byte>  held;



    outPath = savePath;

    hr    = entry.image->Serialize (held);
    isWoz = FAILED (hr) && isEjecting;

    if (isWoz)
    {
        hr = WriteWozCopy (entry, savePath, outPath);
        CHR (hr);
    }
    else
    {
        CHR (hr);

        hr = CommitFile (savePath, held, CommitMode::Replace);
        CHR (hr);

        entry.image->MarkSaved (held);
        entry.sharedState.SetPreservedPath (string());

        hrRepoint = RepointBayToFile (entry.slot, entry.drive, savePath);
        IGNORE_RETURN_VALUE (hrRepoint, S_OK);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::FormatPreserveFailureMessage
//
//  The notice for a guest's version that could not be written beside a file
//  another program changed, with no question possible. At an eject the disk
//  is in the drive, and another eject raises the question again; at a quit
//  the changes are lost.
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskImageStore::FormatPreserveFailureMessage (const string & path,
                                                      const string & attemptedPath,
                                                      HRESULT        reason,
                                                      int            drive,
                                                      FlushMoment    moment)
{
    wstring  widePath = fs::path (path).wstring();
    wstring  message;



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    message = L"Casso could not save your changes to the disk image:\n\n" + widePath
            + L"\n\nAnother program changed this file after it was inserted, so Casso did not overwrite it. "
              L"Casso tried to save your changes to " + fs::path (attemptedPath).filename().wstring()
            + L" instead, but failed: " + ChangePrompt::DescribeErrorAsSentence (reason);

    if (moment == FlushMoment::ShuttingDown)
    {
        message += L"\n\nCasso is closing, so your changes are lost. The file on disk has the other "
                   L"program's version.";
    }
    else
    {
        message += L"\n\nThe disk is still in drive " + std::to_wstring (drive + 1)
                 + L" with your changes. Eject it again to choose another place to save them. The file "
                   L"on disk has the other program's version.";
    }

    return message;
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
    BeginWatching (slot, drive);

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

        isSafeToSwap = AreWritesSaved (entry);
        CBREx (isSafeToSwap, E_UNEXPECTED);

        SwapInImage (entry, std::move (loaded), path, fmt);
    }

    identity = ReadIdentity (path);

    if (identity.recorded)
    {
        GetEntry (slot, drive).sharedState.Mount (identity);
        BeginWatching (slot, drive);
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



    wstring  message;



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    if (diagnosis.failure == MountFailure::UnsavedWrites)
    {
        message = FormatUnsavedWritesMessage (path, diagnosis);
    }
    else
    {
        message = L"Casso could not open this file as a disk image:\n\n" + widePath +
                  L"\n\nThis file " + reason + L".";

        if (!diagnosis.keptPath.empty())
        {
            message += L"\n\nThe disk already in the drive was not changed.";
        }
    }

    return message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatUnsavedWritesMessage
//
//  The text for a mount declined because the disk in the drive has writes in
//  no file. For another file, the insert did not happen; for the file the disk
//  was read from, the save failed and the file was not read again. With no save
//  attempted, there is no Error paragraph.
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskImageStore::FormatUnsavedWritesMessage (const string & path, const MountDiagnosis & diagnosis)
{
    wstring  widePath  = fs::path (path).wstring();
    wstring  wideKept  = fs::path (diagnosis.keptPath).wstring();
    wstring  errorPara;
    wstring  message;



    if (FAILED (diagnosis.saveError))
    {
        errorPara = L"\n\nError: " + ChangePrompt::DescribeError (diagnosis.saveError);
    }

    if (diagnosis.isSameFile)
    {
        message = L"Casso could not save changes to the disk image:\n\n" + wideKept + errorPara
                + L"\n\nThe file on disk is unchanged, and the disk was not read from it again. "
                  L"The drive still has your changes.";
    }
    else
    {
        message = L"Casso did not insert this disk image:\n\n" + widePath
                + L"\n\nThe disk already in drive " + std::to_wstring (diagnosis.occupiedDrive + 1)
                + (FAILED (diagnosis.saveError) ? L" has changes that could not be saved to its file:\n\n"
                                                : L" has changes that have not been saved to its file yet:\n\n")
                + wideKept + errorPara
                + L"\n\nThat disk is still in the drive with its changes. Eject it to save them to another file.";
    }

    return message;
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

wstring DiskImageStore::FormatDamagedImageMessage (const string & path, bool hasDamagedTracks)
{
    wstring  widePath = fs::path (path).wstring();



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    return L"This disk is damaged, so Casso will not write to it:\n\n" + widePath +
           (hasDamagedTracks
               ? L"\n\nRewriting it would replace the tracks that could not be read "
                 L"with blank ones, hiding the damage. "
               : L"\n\nRewriting it would give the file a newly computed checksum, "
                 L"hiding the damage. ") +
           L"The disk stays readable and the emulated machine sees it as "
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
//  FormatFlushLossMessage, from what the save produced
//
//  A disk whose format cannot store its writes, and whose recovery copy could
//  not be written either, gets a separate tail. The copy's error is in place
//  of the head's Error line, which would otherwise contain the serializer's.
//
//  THE TAIL FOR A DISK THAT STAYS IN THE DRIVE IS FALSE AT AN EJECT OR A QUIT.
//  With no copy, a declined eject states that the disk was not ejected, and a
//  quit states that the changes are gone.
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskImageStore::FormatFlushLossMessage (const string & path, const FlushOutcome & outcome, FlushMoment moment)
{
    bool     isCopyFailed = outcome.isUnserializable && FAILED (outcome.recoveryError);
    bool     hasCopy      = SUCCEEDED (outcome.recoveryError) && !outcome.recoveryPath.empty();
    wstring  widePath     = fs::path (path).wstring();
    wstring  tail;
    wstring  message;



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    if (moment == FlushMoment::Ejecting)
    {
        tail = L"The disk was not ejected, and the drive still has your changes.";
    }
    else if (moment == FlushMoment::ShuttingDown)
    {
        tail = L"Casso is closing, so these changes cannot be kept.";
    }

    if (isCopyFailed)
    {
        message = L"Casso could not save changes to the disk image:\n\n" + widePath
                + L"\n\nThe file on disk is unchanged. Its changes cannot be stored as a "
                + fs::path (path).extension().wstring() + L" image. Casso tried to save them to "
                + fs::path (outcome.recoveryPath).filename().wstring() + L" instead, but failed: "
                + ChangePrompt::DescribeErrorAsSentence (outcome.recoveryError);

        if (!tail.empty())
        {
            message += L" " + tail;
        }
    }
    else if (hasCopy || tail.empty())
    {
        message = FormatFlushLossMessage (path, outcome.saveError, hasCopy ? outcome.recoveryPath : string());
    }
    else
    {
        message = L"Casso could not save changes to the disk image:\n\n" + widePath
                + L"\n\nError: " + ChangePrompt::DescribeError (outcome.saveError)
                + L"\n\nThe file on disk is unchanged. " + tail;
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
//  DiskImageStore::TryWriteRecoveryImage
//
//  Serializes the live image to WOZ and lands it beside the original. WOZ is
//  chosen deliberately over the denibblized sector buffer: that buffer has
//  zeros exactly where the unreadable track should be, so writing it would
//  discard the one thing the refusal was protecting. WozLoader::Serialize takes
//  the per-track bit streams verbatim and is format-agnostic, so a .dsk-sourced
//  mount round-trips whole.
//
//  ONE COPY PER MOUNT. A copy this mount wrote, and that nothing has changed
//  or mounted since, is the session's: when the guest has written nothing
//  since, nothing is written, and otherwise the copy is replaced, the same way
//  an ordinary save replaces the original. Any other file at a recovery name
//  is never overwritten: a new copy takes the first free name.
//
//  outPath is the copy's path, or, when the write failed, the path of the failed write.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::TryWriteRecoveryImage (Entry & entry, string & outPath)
{
    HRESULT              hr         = S_OK;
    MountedImageState  & state      = entry.sharedState;
    bool                 haveImage  = entry.image != nullptr;
    bool                 havePath   = !entry.path.empty();
    bool                 isIntact   = false;
    bool                 isCurrent  = false;
    vector<Byte>         wozBytes;



    outPath.clear();

    CBR (haveImage);
    CBR (havePath);

    isIntact  = !state.GetRecoveryPath().empty() && IsRecoveryCopyIntact (entry);
    isCurrent = isIntact && state.IsRecoveryCurrent (entry.image->GetImageId(), entry.image->GetTrackGenerations());

    if (isCurrent)
    {
        outPath = state.GetRecoveryPath();
        BAIL_OUT_IF (true, S_OK);
    }

    hr = WozLoader::Serialize (*entry.image, wozBytes);
    CHR (hr);

    if (isIntact)
    {
        outPath = state.GetRecoveryPath();

        hr = CommitFile (outPath, wozBytes, CommitMode::Replace);
        CHR (hr);
    }
    else
    {
        hr = WriteNewRecoveryImage (entry, wozBytes, outPath);
        CHR (hr);
    }

    state.SetRecovery (outPath, entry.image->GetImageId(), entry.image->GetTrackGenerations(), ReadIdentity (outPath));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::IsRecoveryCopyIntact
//
//  Whether the bay's recovery copy is as this mount wrote it: its identity
//  matches the identity read after the write, or there is none (a flush sink
//  has no file to stat), and no other bay has it mounted.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::IsRecoveryCopyIntact (const Entry & entry)
{
    const MountedImageState  & state       = entry.sharedState;
    const ImageIdentity      & recorded    = state.GetRecoveryIdentity();
    bool                       isSame      = !recorded.recorded || ReadIdentity (state.GetRecoveryPath()).Matches (recorded);
    int                        otherDrive  = -1;
    bool                       isElsewhere = IsFileInAnotherBay (state.GetRecoveryPath(), entry.slot, entry.drive, otherDrive);



    return isSame && !isElsewhere;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::WriteNewRecoveryImage
//
//  Writes a recovery copy at the first free recovery name, never over a file
//  that is there: a name that is taken, or taken between the check and the
//  write, steps to the next one.
//
//  With a flush sink and no file seam there is no file to collide with, so
//  the first name is used.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::WriteNewRecoveryImage (const Entry & entry, const vector<Byte> & wozBytes, string & outPath)
{
    HRESULT  hr          = S_OK;
    int      attempt     = 0;
    bool     isDone      = false;
    bool     isNameFixed = m_flushSink && m_fileIo == nullptr;
    bool     isFree      = false;
    bool     isTaken     = false;



    for (attempt = 0; !isDone && attempt < kMaxRecoveryNameAttempts; attempt++)
    {
        outPath = MakeRecoveryPath (entry.path, attempt);
        isFree  = isNameFixed || !DoesPathExist (outPath);

        if (isFree)
        {
            hr      = CommitFile (outPath, wozBytes, CommitMode::CreateNew);
            isTaken = hr == HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS) || hr == HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS);
            isDone  = isNameFixed || !isTaken;
        }
    }

    CBREx (isDone, HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS));
    CHR (hr);

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
//  notifier (ReportSaveFailure), because every caller drops the return value.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::FlushEntry (Entry & entry, FlushMoment moment)
{
    HRESULT       hr = S_OK;
    FlushOutcome  outcome;



    hr = FlushEntry (entry, moment, outcome);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushEntry, with what a failed save produced
//
//  The save and its report. outOutcome indicates the save's error and the
//  recovery copy written, if any, for a caller that acts on the result.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::FlushEntry (Entry & entry, FlushMoment moment, FlushOutcome & outOutcome)
{
    HRESULT        hr          = S_OK;
    bool           isPreserved = false;
    vector<Byte>   bytes;



    // No-op cases -- nothing dirty to persist -- succeed silently. There is
    // deliberately no way to force a flush past these gates: the only caller
    // that wanted one was changing a write-protect flag, and rebuilding a
    // whole image to carry one bit is what SetImageWriteProtect exists to
    // avoid. An API that cannot be asked to do that cannot be misused into it.
    //  A replay recomputes a past the host already saw; nothing it does
    //  reaches a file.
    BAIL_OUT_IF (m_isReplaying, S_OK);
    BAIL_OUT_IF (!entry.mounted || entry.image == nullptr, S_OK);

    // A write still open on a flux track has not reached the image yet, so
    // the image is not dirty until it is committed.
    entry.image->CommitPendingWrite();

    //  A store with no files, such as a scratch machine's, saves nothing.
    if (!m_isFileBacked)
    {
        entry.image->ClearDirty();
        BAIL_OUT_IF (true, S_OK);
    }

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
    hr = PreserveConflictingCopy (entry, moment, isPreserved, outOutcome);
    CHR (hr);

    //  Nothing is written over the original. It is not this bay's file any
    //  more.
    BAIL_OUT_IF (isPreserved, S_OK);

    hr = entry.image->Serialize (bytes);

    if (FAILED (hr))
    {
        outOutcome.saveError        = hr;
        outOutcome.isUnserializable = true;
        outOutcome.recoveryError    = TryWriteRecoveryImage (entry, outOutcome.recoveryPath);
    }

    CHRF (hr, ReportSaveFailure (entry, moment, outOutcome));

    // Never write in place: a flush that fails midway would otherwise have
    // already truncated the user's image, trading a stale file for no file at
    // all. The dirty bit survives a failure, so a later flush retries.
    //
    // The failure message includes the recovery copy written above, because a
    // flush that cannot land is exactly when the session's only lossless copy
    // of the disk is the one sitting beside the original.
    if (m_flushSink || !entry.path.empty())
    {
        hr                   = CommitFile (entry.path, bytes, CommitMode::Replace);
        outOutcome.saveError = hr;
        CHRF (hr, ReportSaveFailure (entry, moment, outOutcome));
    }

    RecordCommittedFile (entry, bytes);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreserveConflictingCopy
//
//  Immediately before committing, and whatever any watcher has or has not
//  reported. THIS IS THE GUARANTEE AND NOTIFICATION IS ONLY PROMPTNESS: a
//  missed notification, an unwatchable share, a change that arrived while
//  the machine was busy -- all of them end here, one stat before the write.
//
//  A BAY THAT NEVER RECORDED AN IDENTITY IS NOT CHECKED. MountFromBytes has
//  no file behind it, and a test that redirects reads into memory has none
//  either; comparing against a stat that never ran would refuse writes that
//  nothing has endangered.
//
//  isPreserved is true when the guest's version went to a file of its own,
//  and FlushEntry then writes nothing over the original.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::PreserveConflictingCopy (Entry & entry, FlushMoment moment, bool & isPreserved, FlushOutcome & outOutcome)
{
    HRESULT        hr           = S_OK;
    HRESULT        hrKeep       = S_OK;
    bool           unchanged    = true;
    bool           keptExternal = false;
    bool           asked        = false;
    string         original;
    string         preservedPath;
    ImageIdentity  current;



    isPreserved = false;

    BAIL_OUT_IF (!entry.sharedState.GetIdentity().recorded, S_OK);

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
    BAIL_OUT_IF (unchanged, S_OK);

    original = entry.path;
    asked    = entry.sharedState.IsAskOutstanding();

    //  Under the name the question already showed, when there is a
    //  question. Reserving happens once, wherever it happens first.
    preservedPath = entry.sharedState.GetPreservedPath();

    hrKeep = SaveLoadedImage (entry, preservedPath);

    //  A preserve that did not happen stops the write. The image KEEPS
    //  ITS DIRTY BIT, which is the difference between refusing and
    //  losing: the guest's writes are still in memory and still
    //  flushable once there is somewhere to put them.
    //  STG_E_NOTCURRENT says precisely this -- the object changed since
    //  it was last read.
    keptExternal = SUCCEEDED (hrKeep);

    //  The name it tried is held whether or not the write landed, so a
    //  retry goes to the file the user was told about.
    entry.sharedState.SetPreservedPath (preservedPath);

    CBRFEx (keptExternal, STG_E_NOTCURRENT,
            ReportPreserveFailure (entry, moment, preservedPath, hrKeep, original));

    //  On disk under its own name, so the bay carries nothing unsaved.
    entry.sharedState.SetPreservedWritten (true);

    entry.image->SetSourceCrcMismatch (false);
    entry.image->ClearDirty();
    MarkStatusChanged();

    isPreserved = true;

    //  A QUESTION ALREADY ON SCREEN OWNS WHAT HAPPENS NEXT. Writing the
    //  copy is not optional -- the guest's work would be gone otherwise
    //  -- but moving the bay and reporting are the ANSWER's job. Doing
    //  them here settled the matter underneath an open dialog, which
    //  left the user being asked about a file the bay no longer had.
    //  The copy is on disk under the name that dialog is showing, so
    //  either answer still does what it says.
    BAIL_OUT_IF (asked, S_OK);

    hrKeep = RepointBayToFile (entry.slot, entry.drive, preservedPath);
    IGNORE_RETURN_VALUE (hrKeep, S_OK);

    //  Composed from the path the bay HAD, since that is the file the
    //  message is about and the repoint above has already moved it. Before a
    //  mount, the mount reports the copy, in a text that depends on its end.
    if (moment == FlushMoment::Replacing)
    {
        outOutcome.movedFrom = original;
        outOutcome.movedTo   = preservedPath;
    }
    else if (m_reportSink)
    {
        m_reportSink (entry.slot, entry.drive,
                      ChangePrompt::ComposeConflictReport (original, entry.drive, preservedPath));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReportSaveFailure
//
//  Tells the user a dirty image could not be saved, through the shared EHM
//  notifier. The recovery copy's path is in the text when one was written.
//
//  A FAILED SAVE NOBODY ASKED FOR IS REPORTED ONCE PER MOUNT. Every spindown
//  saves again, and a failure reported at every one buries the user in the
//  same notice. A new recovery copy is news, and so is the first failure with
//  no copy. Every failure of a save the user asked for is reported.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::ReportSaveFailure (Entry & entry, FlushMoment moment, const FlushOutcome & outcome)
{
    MountedImageState  & state    = entry.sharedState;
    string               copyPath = SUCCEEDED (outcome.recoveryError) ? outcome.recoveryPath : string();
    bool                 hasCopy  = !copyPath.empty();
    bool                 isNews   = state.ShouldReportSaveFailure (copyPath);
    bool                 isShown  = true;



    //  Before a mount, only a first recovery copy is news: a failure with
    //  writes in no file declines the mount, and the decline is the report.
    //  With a copy, the eject goes ahead and the quit discards nothing, so the
    //  copy is reported as at a background save. Without one, the eject raises
    //  a question about where the changes go and the quit offers the rescue,
    //  and the notice is for when neither happens.
    if (moment == FlushMoment::Replacing)
    {
        isShown = hasCopy && isNews;
    }
    else if (moment == FlushMoment::Background || (hasCopy && moment != FlushMoment::Running))
    {
        isShown = isNews;
    }
    else if (moment == FlushMoment::Ejecting)
    {
        isShown = !AskBeforeEject (entry, outcome);
    }
    else if (moment == FlushMoment::ShuttingDown)
    {
        isShown = !RescueOnTheWayOut (entry, entry.path, outcome.isUnserializable);
    }

    if (isShown)
    {
        EhmNotifyUser (FormatFlushLossMessage (entry.path, outcome, moment).c_str());
        state.MarkSaveFailureReported (copyPath);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordCommittedFile
//
//  After a save lands. The file now carries a freshly computed CRC that
//  matches it, so the mismatch is no longer true of what is on disk -- and
//  the warning must not repeat on every later eject or power cycle.
//
//  The file this store just wrote is a change to it, and without the
//  identity refresh the next flush would find its own commit sitting where
//  the mount-time identity used to be and refuse itself. NOT SEPARABLE FROM
//  PreserveConflictingCopy's CHECK: the two are one mechanism, and the first
//  one alone breaks the second flush of every ordinary session.
//
//  Only for a bay that had an identity to begin with, so a mount from bytes
//  does not acquire one by being flushed.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::RecordCommittedFile (Entry & entry, const vector<Byte> & savedBytes)
{
    entry.image->SetSourceCrcMismatch (false);
    entry.image->MarkSaved (savedBytes);
    entry.sharedState.ClearRecovery();
    MarkStatusChanged();

    if (entry.sharedState.GetIdentity().recorded)
    {
        entry.sharedState.SetIdentity (ReadIdentity (entry.path));
    }
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

    hr = FlushEntry (GetEntry (slot, drive), FlushMoment::Running);

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
        CBRN (!isDamaged, FormatDamagedImageMessage (entry.path, entry.image->HasDamagedTracks()).c_str());

        // Guest writes go out FIRST, while the image still accepts a flush.
        // Patching the flag byte afterwards edits a file that already holds
        // them; doing it the other way round would strand them behind the
        // gate this call is about to close.
        hr = FlushEntry (entry, FlushMoment::Running);
        CHR (hr);

        hr = ReadImageFile (entry.path, bytes);
        CHRN (hr, FormatFlushLossMessage (entry.path, hr).c_str());

        hr = WozLoader::SetWriteProtectFlag (bytes, writeProtected);
        CHRN (hr, FormatFlushLossMessage (entry.path, hr).c_str());

        hr = CommitFile (entry.path, bytes, CommitMode::Replace);
        CHRN (hr, FormatFlushLossMessage (entry.path, hr).c_str());

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
    ASSERT_THREAD_OWNERSHIP (m_ownership);

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



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    out = SalvageAssessment();

    bayOk = IsValidBay (slot, drive);
    CBRAEx (bayOk, E_INVALIDARG);

    {
        Entry &  entry = GetEntry (slot, drive);

        hasImage = (entry.mounted && entry.image != nullptr);
        CBREx (hasImage, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

        out.mediaId    = GetMediaId (slot, drive);
        out.sourcePath = entry.path;

        // Damage is free to test and decoding is not, so test damage first.
        // This runs from the Disk menu's enable query, which means it runs
        // every time that menu is drawn: decoding both drives unconditionally
        // cost 11 ms an ordinary disk and 154 ms a copy-protected one, where a
        // protected track burns its whole attempt budget before giving up.
        // Salvage is only ever offered for a damaged disk, so an undamaged one
        // never needs the decode at all.
        isDamaged = entry.image->IsDamaged();
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
    uint64_t            mediaId,
    const string     &  path,
    DenibblizeReport &  report)
{
    HRESULT       hr           = S_OK;
    bool          bayOk        = false;
    bool          hasImage     = false;
    bool          hasPath      = !path.empty();
    bool          isSource     = false;
    bool          isSameMedium = false;
    vector<Byte>  bytes;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    bayOk = IsValidBay (slot, drive);
    CBRAEx (bayOk, E_INVALIDARG);
    CBRAEx (hasPath, E_INVALIDARG);

    // The copy is of the disk the user saw assessed. Another disk in the bay
    // since, or the same file put back, is another medium.
    isSameMedium = GetMediaId (slot, drive) == mediaId;
    CBREx (isSameMedium, HRESULT_FROM_WIN32 (ERROR_MEDIA_CHANGED));

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

        hr = CommitFile (path, bytes, CommitMode::Replace);
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
                hr = MountExternallyModifiedDisk (slot, drive, bytes);
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
//  SetMediaRetention
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::SetMediaRetention (bool isOn)
{
    ASSERT_THREAD_OWNERSHIP (m_ownership);

    m_isRetaining = isOn;

    if (!isOn)
    {
        m_retained.clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CanSeatMedia
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::CanSeatMedia (int slot, int drive, uint64_t mediaId) const
{
    bool  canSeat = false;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    if (!IsValidBay (slot, drive))
    {
        return false;
    }

    canSeat = mediaId == 0 || GetMediaId (slot, drive) == mediaId;

    for (const Entry & kept : m_retained)
    {
        canSeat = canSeat || kept.image->GetImageId() == mediaId;
    }

    return canSeat;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SeatMedia
//
//  Puts the kept disk with mediaId in the bay, keeping whatever the bay held,
//  or empties the bay for zero. Used to load a machine snapshot, so it flushes
//  nothing and reports no bay change: the disks move back to where they were,
//  with the writes they had, and nothing about the files changes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::SeatMedia (int slot, int drive, uint64_t mediaId, bool & outChanged)
{
    HRESULT  hr      = S_OK;
    bool     canSeat = CanSeatMedia (slot, drive, mediaId);
    size_t   index   = 0;
    Entry    kept;



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    outChanged = false;

    CBREx (canSeat, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    BAIL_OUT_IF (GetMediaId (slot, drive) == mediaId, S_OK);

    {
        Entry &  entry = GetEntry (slot, drive);

        if (entry.mounted)
        {
            EndWatching (slot, drive);
            RetireBay (entry);
        }

        outChanged = true;

        BAIL_OUT_IF (mediaId == 0, S_OK);

        while (m_retained[index].image->GetImageId() != mediaId)
        {
            index++;
        }

        kept = std::move (m_retained[index]);
        m_retained.erase (m_retained.begin() + static_cast<ptrdiff_t> (index));

        entry.image          = std::move (kept.image);
        entry.path           = kept.path;
        entry.format         = kept.format;
        entry.mounted        = true;
        entry.salvageOffered = kept.salvageOffered;
        entry.sharedState    = kept.sharedState;

        MarkStatusChanged();
        BeginWatching (slot, drive);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReportSeatedMedia
//
//  SeatMedia emits no bay change, which suits a step through history. When
//  the shell's drives have to match the seated disks, as after a state load
//  that failed, the caller emits the change here: an eject for a bay left
//  empty, and an insert for a bay holding a disk it did not hold before, as
//  a mount over an occupied bay emits.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::ReportSeatedMedia (
    int       slot,
    int       drive,
    uint64_t  previousMediaId)
{
    uint64_t  mediaId = GetMediaId (slot, drive);



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    if (!IsValidBay (slot, drive) || mediaId == previousMediaId)
    {
        return;
    }

    EmitBayChange (slot, drive, (mediaId == 0) ? BayChange::Ejected : BayChange::Inserted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PruneRetainedMedia
//
//  A disk that left its bay before the oldest snapshot is in none of them,
//  so nothing can put it back. One that left at the oldest snapshot's
//  position can still be in it: a changed file is taken up from the drive's
//  tick, partway through an instruction and before the position counts that
//  instruction, so the disk it replaces leaves at the position of the
//  snapshot taken at the instruction's start.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::PruneRetainedMedia (uint64_t oldestPosition)
{
    ASSERT_THREAD_OWNERSHIP (m_ownership);

    std::erase_if (m_retained, [oldestPosition] (const Entry & kept) { return kept.retiredAt < oldestPosition; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  RetireBay
//
//  Empties a bay. With retention on, its disk is kept with everything the bay
//  knew about it, stamped with the position it left at.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::RetireBay (Entry & entry)
{
    Entry  kept;



    if (m_isRetaining && entry.image != nullptr)
    {
        kept.image          = std::move (entry.image);
        kept.path           = entry.path;
        kept.format         = entry.format;
        kept.mounted        = true;
        kept.salvageOffered = entry.salvageOffered;
        kept.sharedState    = entry.sharedState;
        kept.retiredAt      = (m_positionSource != nullptr) ? *m_positionSource : 0;

        m_retained.push_back (std::move (kept));
    }

    entry.image.reset();
    entry.path.clear();
    entry.mounted        = false;
    entry.salvageOffered = false;
    entry.sharedState.Eject();

    MarkStatusChanged();
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
            hr = FlushEntry (m_entries[slot][drive], moment);

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

        hr = FlushEntry (entry, FlushMoment::Ejecting);

        isWaiting = entry.sharedState.IsEjectWhenAnswered();
        isSaved   = AreWritesSaved (entry);

        if (!isWaiting && !isSaved && SUCCEEDED (hr))
        {
            EhmNotifyUser (FormatBehindLiveEjectMessage (drive).c_str());
        }

        if (!isWaiting && isSaved)
        {
            //  Before the path goes: the watch is keyed by the directory the
            //  image sits in, and there is no finding that from a cleared path.
            EndWatching (slot, drive);

            //  Kept in memory while reverse execution may still put it back.
            RetireBay (entry);

            //  The disk left. Emitted after the bay is empty, so the handler
            //  that detaches the controller sees the post-eject state.
            EmitBayChange (slot, drive, BayChange::Ejected);

            NotifyMediaChanged();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatBehindLiveEjectMessage
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskImageStore::FormatBehindLiveEjectMessage (int drive)
{
    return L"Casso did not eject the disk in drive " + std::to_wstring (drive + 1)
         + L", because the machine was behind live in its history when the eject ran. Go live and try again.";
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
//  message channel from the UI thread, and only the thread that has the
//  store's token may read an image or swap it under a running guest. So the
//  change goes into an inbox, and ApplyPendingReload matches it to the bays.
//
//  A PATH NO BAY HOLDS IS NOT AN ERROR. A directory watch reports every file
//  under it, and an intent may be stated for an image nothing has mounted; both
//  are ordinary and both end here, silently.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::NoteExternalChange (const string & path, ExternalChangeIntent intent)
{
    int64_t  now = GetNowMs();



    {
        std::lock_guard<std::mutex>  lock (m_pendingMutex);



        m_changeInbox[path] = NotedChange { now, intent };
    }
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



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    //  A replay must not take up a changed file: the disk it swaps in is not
    //  the one history recorded. The change stays pending for the live run.
    if (m_isReplaying)
    {
        return;
    }

    DrainChangeInbox();

    //  ONE CLOCK READING FOR THE WHOLE WALK, not one per bay. This runs sixty
    //  times a second forever, and the walk is a handful of comparisons; the
    //  clock read per bay cost far more than the work itself.
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

    for (i = 0; i < readyCount; i++)
    {
        ApplyPendingReloadToBay (readySlot[i], readyDrive[i]);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::DrainChangeInbox
//
//  Moves the changes noted since the last drain into the bays that have each
//  path, on the thread that has the store's token. Only mounted bays match; a
//  retained disk is not in a bay, so no bay's record changes for it. The lock is
//  taken only to swap the inbox, so a watcher thread never waits on a bay.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::DrainChangeInbox()
{
    int  slot  = 0;
    int  drive = 0;



    {
        std::lock_guard<std::mutex>  lock (m_pendingMutex);



        m_drainedChanges.swap (m_changeInbox);
    }

    for (const auto & [path, change] : m_drainedChanges)
    {
        for (slot = 0; slot < kSlotCount; slot++)
        {
            for (drive = 0; drive < kDriveCount; drive++)
            {
                Entry &  entry = m_entries[slot][drive];

                if (entry.mounted && MountedImageState::IsSamePath (entry.path, path))
                {
                    entry.sharedState.NoteChange (change.atMs, change.intent);
                }
            }
        }
    }

    m_drainedChanges.clear();
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
//  The steps are IsReloadDue, ReadChangedImage, PreserveGuestCopyForConflict
//  and AskAboutChange.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::ApplyPendingReloadToBay (int slot, int drive)
{
    Entry                            & entry     = GetEntry (slot, drive);
    bool                               usable    = false;
    bool                               preserved = false;
    ExternalChangeIntent               intent    = ExternalChangeIntent::Unstated;
    ChangeAuthor                       author    = ChangeAuthor::AnotherProgram;
    ChangeAction                       action    = ChangeAction::Ignore;
    vector<Byte>                       bytes;
    ExternalChangePolicy::Situation    situation;
    string                             savePath;



    if (!IsReloadDue (entry, intent))
    {
        return;
    }

    usable = ReadChangedImage (entry, bytes);

    //  An open write on a flux track is a guest write that is not in the image
    //  yet. It is committed here, as FlushEntry does, so it counts toward the
    //  conflict and goes into the preserved copy rather than reading as clean
    //  and being spliced into the reloaded disk.
    entry.image->CommitPendingWrite();

    situation.changeSeen = true;
    situation.usable     = usable;
    situation.intent     = intent;

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
        if (!PreserveGuestCopyForConflict (slot, drive, entry, savePath))
        {
            return;
        }

        situation.guestDirty = false;
        action               = ExternalChangePolicy::Decide (situation);
        preserved            = true;
    }

    //  A file that is simply gone gets its own sentence rather than sharing
    //  one with a file that is present and unreadable.
    if (action == ChangeAction::Unusable && !DoesPathExist (entry.path))
    {
        action = ChangeAction::Deleted;
    }

    if (ExternalChangePolicy::NeedsAnAnswer (action))
    {
        AskAboutChange (slot, drive, entry, action);
        return;
    }

    if (m_decisionSink)
    {
        m_decisionSink (entry.path, action, preserved, savePath);
    }

    //  THE INTENT THAT DECIDED IS THE INTENT THAT ATTRIBUTES. `intent` was read
    //  when the change was found due, and the record it came from is refreshed
    //  by every later write -- a read taken now would credit this reload to
    //  whoever wrote most recently rather than to whoever asked for it.
    author = (intent == ExternalChangeIntent::Unstated) ? ChangeAuthor::AnotherProgram
                                                        : ChangeAuthor::CassoCli;

    CarryOutChangeAction (slot, drive, action, bytes, author);

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::IsReloadDue
//
//  Whether the bay's noticed change is settled and real, with the intent
//  stated for it.
//
//  WHILE A QUESTION STANDS THE USER OWNS THE BAY, and every later change
//  waits behind their answer -- a stated intent included. Applying one
//  underneath an open dialog swapped the disk out from under it, so
//  "keep your current version" came to mean the other program's version.
//
//  NOTHING IS LOST BY WAITING. `NoteChange` keeps refreshing the pending
//  record, and the answer re-reads the file when it arrives, so what lands is
//  the newest bytes rather than whatever stood when the question went up. One
//  consequence: a restart requested while a question is on screen reloads
//  without restarting, because the answer consumes the record that carried
//  the request.
//
//  IT ALSO STOPS THE READING. The reload reads the whole image and trial-loads
//  it before it reaches the point where an outstanding question is noticed,
//  and the change stays pending by design for as long as the user reads --
//  sixty full reads and nibblizations a second until they answer.
//
//  A file another program has open is deferred rather than refused,
//  indefinitely and silently: the pick-up happens once it is closed. Both
//  writers in this system commit atomically, but a text editor or a copy tool
//  need not, and the quiet period alone does not cover one that takes its
//  time.
//
//  A CHANGE THAT TURNS OUT NOT TO BE ONE IS DROPPED. A directory watch reports
//  a write to any file in the folder and a stated intent is only a hint, so
//  the recorded identity is what settles whether anything actually happened.
//  This is also what stops the store's own commit from coming back as
//  somebody else's change.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::IsReloadDue (Entry & entry, ExternalChangeIntent & outIntent)
{
    bool           settled   = false;
    bool           unchanged = false;
    bool           isDue     = false;
    ImageIdentity  current;



    {
        std::lock_guard<std::mutex>  guard (m_pendingMutex);

        settled   = entry.mounted
                 && entry.image != nullptr
                 && entry.sharedState.IsSettled (GetNowMs());
        outIntent = entry.sharedState.GetPending().intent;
    }

    isDue = settled
         && !entry.sharedState.IsAskOutstanding()
         && !(m_fileIo != nullptr && m_fileIo->IsHeldByAnotherProcess (entry.path));

    if (isDue)
    {
        current   = ReadIdentity (entry.path);
        unchanged = entry.sharedState.GetIdentity().Matches (current);
    }

    if (isDue && unchanged)
    {
        std::lock_guard<std::mutex>  guard (m_pendingMutex);

        entry.sharedState.ClearPending();
        isDue = false;
    }

    return isDue;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::ReadChangedImage
//
//  Reads the bay's file and trial-loads it: true when these bytes can be this disk. A
//  trial load into a throwaway image is the only honest answer -- the
//  extension and the length agree with plenty of files that will not load.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::ReadChangedImage (const Entry & entry, vector<Byte> & outBytes) const
{
    HRESULT     hr     = S_OK;
    DiskFormat  fmt    = DiskFormat::Dsk;
    bool        usable = false;



    hr = ReadImageFile (entry.path, outBytes);

    if (SUCCEEDED (hr))
    {
        hr = GetSourceFormatByExtension (entry.path, fmt);
    }

    if (SUCCEEDED (hr))
    {
        DiskImage  trial;

        trial.LoadFromBytes (fmt, outBytes, entry.path);
        usable = trial.IsLoaded();
    }

    return usable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::PreserveGuestCopyForConflict
//
//  Writes the guest's version to a separate file, so the change that
//  follows is no longer a conflict. False when the copy could not be written,
//  after the failure has been dealt with.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::PreserveGuestCopyForConflict (int slot, int drive, Entry & entry, string & outSavePath)
{
    HRESULT  hr = S_OK;



    outSavePath = entry.sharedState.GetPreservedPath();

    hr = SaveLoadedImage (entry, outSavePath);

    entry.sharedState.SetPreservedPath (outSavePath);

    if (FAILED (hr))
    {
        //  Nothing is mounted and nothing is overwritten. The pending change
        //  is DROPPED RATHER THAN RETRIED: retrying costs a full image read
        //  and a failed write on every idle tick, sixty times a second for as
        //  long as the folder stays full. The next change to the file, or the
        //  next flush, tries again.
        {
            std::lock_guard<std::mutex>  guard (m_pendingMutex);

            entry.sharedState.ClearPending();
        }

        if (m_decisionSink)
        {
            m_decisionSink (entry.path, ChangeAction::Conflict, false, outSavePath);
        }

        //  ASKED RATHER THAN REPORTED, because "Save as..." is an answer and a
        //  notice has nowhere to put one. The bay is left with the question
        //  outstanding so a second failure does not stack a second dialog on
        //  the first.
        if (m_askSink && !entry.sharedState.IsAskOutstanding())
        {
            entry.sharedState.SetAskedAction (ChangeAction::Conflict);

            entry.sharedState.SetAskOutstanding (
                m_askSink (slot, drive,
                           ChangePrompt::ComposeSaveFailure (entry.path, drive,
                                                             outSavePath, hr,
                                                             SaveFailureCause::ExternalChange)));
        }

        return false;
    }

    //  It is on disk under its own name now, so the bay carries nothing
    //  unsaved and the change that follows is no longer a conflict.
    entry.sharedState.SetPreservedWritten (true);

    entry.image->ClearDirty();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::AskAboutChange
//
//  Puts the question about a change to the user.
//
//  A question nothing can answer stays pending rather than resolving itself
//  by default. A headless host has no way to ask, and picking one of the
//  answers on the user's behalf is what the fallback exists for.
//
//  ONE QUESTION AT A TIME PER BAY. Asking cannot block -- the answer comes
//  from another thread -- so the change is still pending on the next idle
//  tick, and without this the user would be asked again sixty times a second
//  while reading the first one.
//
//  THE PRESERVED NAME IS RESERVED, NOT JUST CALCULATED. The name shown here is
//  the one the copy will take whoever writes it -- this bay's own flush may
//  get there first, while this question is still on screen. Working it out
//  again at that point produced a different name and left the dialog offering
//  a file nobody ever created.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::AskAboutChange (int slot, int drive, Entry & entry, ChangeAction action)
{
    HRESULT  hrName = S_OK;
    string   reserved;



    if (!m_askSink || entry.sharedState.IsAskOutstanding())
    {
        return;
    }

    entry.sharedState.SetAskedAction (action);

    reserved = entry.sharedState.GetPreservedPath();

    if (reserved.empty())
    {
        hrName = FindFreePreservedPath (entry.path, reserved);
        IGNORE_RETURN_VALUE (hrName, S_OK);

        entry.sharedState.SetPreservedPath (reserved);
    }

    entry.sharedState.SetAskOutstanding (
        m_askSink (slot, drive,
                   ChangePrompt::Compose (entry.path, drive, action,
                                          reserved,
                                          entry.sharedState.IsPreservedWritten())));
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



    ASSERT_THREAD_OWNERSHIP (m_ownership);

    if (!IsValidBay (slot, drive))
    {
        return;
    }

    {
        Entry &  entry = GetEntry (slot, drive);

        //  AN ANSWER TO A QUESTION NOBODY IS ASKING ANY MORE IS DROPPED. Mount
        //  and eject both clear this flag, so a disk that left the drive while
        //  the dialog stood takes its question with it. Acting anyway carried
        //  the answer onto whatever disk went in next: "keep the one I have"
        //  moved that bay onto the departed disk's rescue copy, which the next
        //  flush then wrote over.
        if (!entry.sharedState.IsAskOutstanding())
        {
            return;
        }

        entry.sharedState.SetAskOutstanding (false);

        if (!entry.mounted || entry.image == nullptr)
        {
            return;
        }

        if (entry.sharedState.GetAskedAction() == ChangeAction::Conflict)
        {
            ResolveConflictAnswer (slot, drive, entry, chosen, savePath);
            return;
        }

        if (ExternalChangePolicy::IsFileLost (entry.sharedState.GetAskedAction()))
        {
            ResolveLostFileAnswer (slot, drive, entry, chosen, savePath);
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

    //  THE USER DID THE INSERTING, so no program is named. A question is only
    //  ever put when nothing stated an intent -- that is what `Ask` means --
    //  and one arriving while the question stood on screen does not turn the
    //  answer into somebody else's reload.
    CarryOutChangeAction (slot, drive, chosen, bytes, ChangeAuthor::AnotherProgram);

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::ResolveConflictAnswer
//
//  "Save as..." after a copy could not be written where this store
//  chose. The disk stays in the drive and the bay moves onto the file
//  the user picked, which is the same outcome as keeping it -- only the
//  folder is theirs rather than ours. Finishing an eject, a disk whose
//  format cannot store its writes is saved as WOZ instead.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::ResolveConflictAnswer (int slot, int drive, Entry & entry, ChangeAction chosen, const string & savePath)
{
    HRESULT           hr          = S_OK;
    bool              finishEject = entry.sharedState.IsEjectWhenAnswered();
    SaveFailureCause  cause       = entry.sharedState.GetAskedCause();
    bool              isAsked     = false;
    string            written;
    ChangePrompt      prompt;



    entry.sharedState.SetAskedAction (ChangeAction::Ignore);

    if (chosen == ChangeAction::PreserveCopy && !savePath.empty())
    {
        hr = SaveAnswerCopy (entry, savePath, finishEject, written);

        if (FAILED (hr))
        {
            //  Still nowhere to put it. The disk is untouched and the
            //  question stands rather than being quietly dropped. The second
            //  question is in the terms of the first.
            if (m_askSink)
            {
                prompt = ChangePrompt::ComposeSaveFailure (entry.path, drive, written, hr, cause);

                if (cause == SaveFailureCause::Unserializable)
                {
                    prompt.suggestedSavePath = MakeRecoveryPath (entry.path, 0);
                }

                entry.sharedState.SetAskedAction (ChangeAction::Conflict);

                isAsked = m_askSink (slot, drive, prompt);
            }

            entry.sharedState.SetAskOutstanding   (isAsked);
            entry.sharedState.SetEjectWhenAnswered (finishEject && isAsked);

            return;
        }
    }
    else if (finishEject)
    {
        //  THE USER CHOSE TO LET IT GO, so the dirty bit goes with it.
        //  Left standing, the eject below would find the same collision
        //  and put the same question up again, and the disk they just
        //  discarded would be undiscardable.
        entry.image->ClearDirty();
    }

    {
        std::lock_guard<std::mutex>  guard (m_pendingMutex);

        entry.sharedState.ClearPending();
    }

    //  Dismissed rather than answered, so the name this store had
    //  picked goes back with the question that offered it.
    entry.sharedState.ReleaseUnwrittenReservation();

    //  The eject that raised this question, now that it has an answer.
    //  Either answer leaves nothing for the flush inside it to do: the
    //  copy landed and cleared the dirty bit, or the discard did.
    if (finishEject)
    {
        entry.sharedState.SetEjectWhenAnswered (false);

        Eject (slot, drive);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::ResolveLostFileAnswer
//
//  An answer about a file that has gone is being given to a question
//  that no longer applies. Saving what is held and emptying the drive
//  is the only thing still on offer.
//
//  THE SAVE HAPPENS BEFORE THE EJECT, always. The eject is what
//  discards the in-memory disk, so a failure between the two must
//  leave the disk in the drive rather than gone from both places.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::ResolveLostFileAnswer (int slot, int drive, Entry & entry, ChangeAction chosen, const string & savePath)
{
    HRESULT       hr    = S_OK;
    ChangeAction  asked = entry.sharedState.GetAskedAction();



    entry.sharedState.SetAskedAction (ChangeAction::Ignore);

    if (chosen == ChangeAction::PreserveCopy && !savePath.empty())
    {
        vector<Byte>  held;

        hr = entry.image->Serialize (held);

        if (SUCCEEDED (hr))
        {
            hr = CommitFile (savePath, held, CommitMode::Replace);
        }

        //  A save the user asked for and did not get must not be
        //  followed by throwing the only copy away. The second question
        //  is the lost-file question, so its answer is read as one.
        if (FAILED (hr))
        {
            if (m_askSink)
            {
                entry.sharedState.SetAskedAction (asked);
                entry.sharedState.SetAskOutstanding (
                    m_askSink (slot, drive,
                               ChangePrompt::ComposeSaveFailure (entry.path, drive,
                                                                 savePath, hr,
                                                                 SaveFailureCause::FileLost)));
            }

            return;
        }

        //  THE DISK STAYS IN THE DRIVE, ON THE FILE THE USER PICKED.
        //  Serialize writes a whole image, not a fragment, so the drive
        //  now has somewhere to live -- and emptying it would mean
        //  handing back a complete disk and then making them go and
        //  find it. Keeping a version during a conflict already works
        //  this way, and one act should not have two outcomes.
        entry.image->MarkSaved (held);

        hr = RepointBayToFile (slot, drive, savePath);
        IGNORE_RETURN_VALUE (hr, S_OK);

        return;
    }

    //  Nothing was saved, so there is nothing to point at.
    //
    //  THE SHELL CAN ONLY SEND BACK AN ANSWER THE PROMPT OFFERED, so a
    //  third value is a coding error rather than a user choice -- and
    //  one that would empty the drive of a disk existing nowhere else.
    //  The disk stays put rather than paying for the mistake.
    ASSERT (chosen == ChangeAction::Discard);

    if (chosen != ChangeAction::Discard)
    {
        return;
    }

    EjectLostImage (slot, drive);

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
                                           const vector<Byte> & bytes, ChangeAuthor author)
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
        preservedPath = entry.sharedState.GetPreservedPath();

        if (!entry.sharedState.IsPreservedWritten())
        {
            hr = SaveLoadedImage (entry, preservedPath);

            entry.sharedState.SetPreservedPath (preservedPath);

            if (FAILED (hr))
            {
                preserveFail = true;
                break;
            }

            entry.sharedState.SetPreservedWritten (true);
        }

        preserved = true;

        entry.image->ClearDirty();

        //  The disk in the drive is untouched by this; only the file behind it
        //  changes. Repointing also drops the pending change, which is right:
        //  the file it referred to is not this bay's any more.
        hr = RepointBayToFile (slot, drive, preservedPath);
        IGNORE_RETURN_VALUE (hr, S_OK);

        entry.sharedState.ClearPreserved();

        break;

    case ChangeAction::Ignore:
        //  THE NAME GOES BACK WHEN THE QUESTION DOES. It was reserved to put
        //  the question, and it carries the moment the question was put, so
        //  holding it after the user waves the question away labels the next
        //  copy with a timestamp from whenever this happened to be -- and
        //  hands it a name another file may have taken since.
        entry.sharedState.ReleaseUnwrittenReservation();

        break;

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
    //
    //  THE PATH AND THE FACT TRAVEL TOGETHER. A bay holds a reserved name from
    //  the moment a question is put, and holds the name it tried after a write
    //  that failed, so the reserved path indicates where a copy would go and
    //  only `IsPreservedWritten` indicates whether one is there.
    if (tookUp)
    {
        preservedPath = entry.sharedState.GetPreservedPath();
        preserved     = preserved || entry.sharedState.IsPreservedWritten();

        entry.sharedState.ClearPreserved();
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

    //  A FILE FOUND MISSING WHILE ACTING IS A QUESTION TOO. The bytes passed a
    //  trial load and failed the real one, or the re-read behind an answer
    //  found the file gone -- either way what is left is the same offer the
    //  watcher would have made: save the disk somewhere, or let it go. Sent to
    //  the notice bar instead, its buttons did nothing at all; the bar routes
    //  no answers.
    //
    //  THE PENDING RECORD IS ALREADY CLEARED ABOVE, so nothing re-asks behind
    //  this.
    if (ExternalChangePolicy::IsFileLost (action) && m_askSink)
    {
        entry.sharedState.SetAskedAction (action);

        entry.sharedState.SetAskOutstanding (
            m_askSink (slot, drive, ChangePrompt::Compose (original, drive, action)));

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
                                                        m_machineName, author,
                                                        preservedPath, preserved);
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
//  With retention on, the outgoing disk is kept as an ejected one is, so
//  stepping back across the reload can put it back in the drive. It is kept
//  without the change this reload deals with and without the name of any copy
//  of the guest's writes made on the way. Put back with them, the change would
//  be acted on a second time, and the disk's next conflict saved over that
//  copy.
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
    Entry                    outgoing;



    loaded->LoadFromBytes (entry.format, bytes, entry.path);

    usable = loaded->IsLoaded();
    CBR (usable);

    //  The drive's open write is a write to the outgoing contents. It is
    //  committed here, so it goes into the disk that is leaving, or into the
    //  copy for history, instead of into the one arriving.
    entry.image->CommitPendingWrite();

    if (m_isRetaining)
    {
        outgoing.image          = make_unique<DiskImage>();
        *outgoing.image         = std::move (*entry.image);
        outgoing.path           = entry.path;
        outgoing.format         = entry.format;
        outgoing.mounted        = true;
        outgoing.salvageOffered = entry.salvageOffered;
        outgoing.sharedState    = entry.sharedState;

        //  The identity stays the one recorded when this disk was loaded, so
        //  a flush of the disk once it is back still finds the file changed.
        outgoing.sharedState.ClearPending();
        outgoing.sharedState.ClearPreserved();

        RetireBay (outgoing);
    }

    //  THE CONTENTS MOVE, THE OBJECT STAYS. The controller holds a raw pointer
    //  to this DiskImage -- SetExternalDisk hands one over at mount -- so
    //  replacing the unique_ptr would leave the drive reading freed memory the
    //  instant a disk was reloaded. Assigning through keeps the address the
    //  drive was given.
    *entry.image = std::move (*loaded);

    //  The identity is refreshed from the file the bytes came from, so the
    //  swap does not immediately look like another external change. The
    //  session a recovery copy was taken from is gone, so its record is too.
    entry.sharedState.SetIdentity (ReadIdentity (entry.path));
    entry.sharedState.ClearRecovery();

    hrAssess             = AssessSalvage (slot, drive, assessment);
    entry.salvageOffered = SUCCEEDED (hrAssess) && assessment.isOffered;

    MarkStatusChanged();

    //  The disk in the drive was replaced under the running machine. Swapped,
    //  not Inserted: the door opens and closes rather than only closing,
    //  because from the user's seat a disk came out and another went in.
    EmitBayChange (slot, drive, BayChange::Swapped);

    NotifyMediaChanged();

Error:
    return hr;
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

        entry.path = newPath;

        //  A fresh identity for the new file, and nothing pending against the
        //  old one. Mount is what records both.
        entry.sharedState.Mount (ReadIdentity (newPath));

        MarkStatusChanged();
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
//  DiskImageStore::CommitFile
//
//  Every write this class makes: an image, a salvaged copy, a preserved copy.
//
//  THROUGH THE FLUSH SINK WHEN ONE IS INSTALLED, so a test captures every
//  write in one place. Otherwise through DurableCommit, over the file seam when
//  the shell installed one and over a local Win32DiskFileIo when not, so a
//  copy or an image is never found half-written. A half-written preserved copy
//  is worthless at the moment the user opens it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::CommitFile (const string & path, const vector<Byte> & bytes, CommitMode mode)
{
    HRESULT               hr       = S_OK;
    Win32DiskFileIo       localIo;
    IDiskFileIo         & fileIo   = (m_fileIo != nullptr) ? *m_fileIo : localIo;
    CommitPlan::Progress  progress = {};



    if (m_flushSink)
    {
        hr = m_flushSink (path, bytes);
        CHR (hr);
    }
    else
    {
        hr = DurableCommit::Commit (fileIo, path, bytes, GetInvocationTag(), mode, progress);
        CHR (hr);
    }

Error:
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

    hr = CommitFile (outPath, bytes, CommitMode::Replace);
    CHR (hr);

    entry.image->MarkSaved (bytes);

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

        RetireBay (entry);
    }

    //  A file that vanished empties the drive the same as a user eject, and
    //  the door and its sound follow the same way.
    EmitBayChange (slot, drive, BayChange::Ejected);

    NotifyMediaChanged();

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
