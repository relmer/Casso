#include "Pch.h"

#include "DiskImageStore.h"
#include "NibblizationLayer.h"
#include "WozLoader.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::DiskImageStore
//
////////////////////////////////////////////////////////////////////////////////

DiskImageStore::DiskImageStore()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  At
//
////////////////////////////////////////////////////////////////////////////////

DiskImageStore::Entry & DiskImageStore::At (int slot, int drive)
{
    return m_entries[slot][drive];
}


const DiskImageStore::Entry & DiskImageStore::At (int slot, int drive) const
{
    return m_entries[slot][drive];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DetectFormatByExtension
//
//  Lower-cased ASCII extension match. Anything unknown defaults to E_FAIL
//  so callers can route unsupported types explicitly.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::DetectFormatByExtension (const string & path, DiskFormat & outFmt)
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



    hr = DetectFormatByExtension (path, fmt);

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
        Entry &   entry = At (slot, drive);

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

    hr = DetectFormatByExtension (path, fmt);
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
    At (slot, drive).sharedState.Mount (ReadIdentity (path));

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
    size_t          size      = bytes.size();
    bool            isSized   = size == (size_t) NibblizationLayer::kImageByteSize;



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
                                                const string & recoveryPath)
{
    wstring  widePath     = fs::path (path).wstring();
    wstring  wideRecovery = fs::path (recoveryPath).wstring();
    wstring  message;



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    message = L"Casso could not save changes to the disk image:\n\n" + widePath +
              L"\n\nThe file on disk is unchanged.";

    // A refusal that leaves the user with no way back to their work is only
    // half a fix, so say where the work went rather than only what failed.
    if (!wideRecovery.empty())
    {
        message += L" Your session was preserved here instead:\n\n" + wideRecovery +
                   L"\n\nThat copy is complete. It keeps the track that could not "
                   L"be written back. Mount it to carry on from where you were.";
    }
    else
    {
        message += L" Your recent writes were NOT persisted. If this is a .dsk, "
                   L"try a .woz image. WOZ round-trips writes reliably.";
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
    bool          fileOk    = false;
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

    CBR (haveName);

    if (m_flushSink)
    {
        hr = m_flushSink (candidate, wozBytes);
        CHR (hr);
    }
    else
    {
        ofstream  file (candidate, ios::binary);

        fileOk = file.good();
        CBR (fileOk);

        file.write (reinterpret_cast<const char *> (wozBytes.data()),
                    static_cast<streamsize> (wozBytes.size()));
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
    HRESULT        hr         = S_OK;
    HRESULT        hrRecovery = S_OK;
    bool           unchanged  = true;
    string         recoveryPath;
    vector<Byte>   bytes;
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
    if (entry.sharedState.Identity().recorded)
    {
        current   = ReadIdentity (entry.path);
        unchanged = entry.sharedState.Identity().Matches (current);

        //  The image KEEPS ITS DIRTY BIT, which is the whole difference between
        //  refusing and losing: the guest's writes are still in memory and
        //  still flushable once the conflict is settled. STG_E_NOTCURRENT says
        //  precisely this and nothing else -- the object changed since it was
        //  last read.
        CBRFEx (unchanged, STG_E_NOTCURRENT,
                EhmNotifyUser (FormatExternalChangeMessage (entry.path).c_str()));
    }

    hr = entry.image->Serialize (bytes);

    if (FAILED (hr))
    {
        hrRecovery = TryWriteRecoveryImage (entry, recoveryPath);
        IGNORE_RETURN_VALUE (hrRecovery, S_OK);
    }

    CHRN (hr, FormatFlushLossMessage (entry.path, recoveryPath).c_str());

    if (m_flushSink)
    {
        hr = m_flushSink (entry.path, bytes);
        CHRN (hr, FormatFlushLossMessage (entry.path, recoveryPath).c_str());
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
        CHRN (hr, FormatFlushLossMessage (entry.path, recoveryPath).c_str());
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
    if (entry.sharedState.Identity().recorded)
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
    constexpr const char *  kTempSuffix = ".casso-tmp";
    HRESULT                 hr          = S_OK;
    string                  tempPath    = path + kTempSuffix;
    bool                    hasPath     = !path.empty();
    bool                    wroteOk     = false;
    std::error_code         ec;



    CBRAEx (hasPath, E_INVALIDARG);

    {
        ofstream  file (tempPath, ios::binary | ios::trunc);

        wroteOk = file.good();

        if (wroteOk && !bytes.empty())
        {
            file.write (reinterpret_cast<const char *> (bytes.data()),
                        static_cast<streamsize> (bytes.size()));
        }

        file.close();

        wroteOk = wroteOk && file.good();
    }

    CBR (wroteOk);

    // Rename replaces an existing target, so the swap is one filesystem
    // operation: readers see either the old file or the new one.
    fs::rename (tempPath, path, ec);
    CBR (!ec);

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

    hr = FlushEntry (At (slot, drive));

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
        Entry &  entry = At (slot, drive);

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
        CHRN (hr, FormatFlushLossMessage (entry.path).c_str());

        hr = WozLoader::SetWriteProtectFlag (bytes, writeProtected);
        CHRN (hr, FormatFlushLossMessage (entry.path).c_str());

        if (m_flushSink)
        {
            hr = m_flushSink (entry.path, bytes);
        }
        else
        {
            hr = WriteFileAtomically (entry.path, bytes);
        }

        CHRN (hr, FormatFlushLossMessage (entry.path).c_str());

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

    return At (slot, drive).salvageOffered;
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
        Entry &  entry = At (slot, drive);

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
        Entry &  entry = At (slot, drive);

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
    if (IsValidBay (slot, drive) && At (slot, drive).mounted)
    {
        Entry &   entry = At (slot, drive);

        // Flush failures are reported to the user by FlushEntry itself; the
        // eject proceeds either way, because refusing to unmount would leave
        // the user with no way to get the disk out.
        hr = FlushEntry (entry);
        IGNORE_RETURN_VALUE (hr, S_OK);

        entry.image.reset();
        entry.path.clear();
        entry.mounted = false;
        entry.sharedState.Eject();
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
    return IsValidBay (slot, drive) ? At (slot, drive).image.get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsMounted
//
////////////////////////////////////////////////////////////////////////////////

bool DiskImageStore::IsMounted (int slot, int drive) const
{
    return IsValidBay (slot, drive) && At (slot, drive).mounted;
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
    return IsValidBay (slot, drive) ? At (slot, drive).path : m_emptyPath;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountedSourcePaths
//
//  Every mounted entry's backing path with its bay. Entries mounted from
//  bytes with an empty virtual path are skipped -- there is no host file to
//  collide with.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DiskImageStore::MountedSource> DiskImageStore::MountedSourcePaths() const
{
    std::vector<MountedSource>  result;
    int                         slot   = 0;
    int                         drive  = 0;



    for (slot = 0; slot < kSlotCount; slot++)
    {
        for (drive = 0; drive < kDriveCount; drive++)
        {
            const Entry &  entry = At (slot, drive);

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
//  DiskImageStore::SharedState
//
//  What a bay knows about its image beyond the bytes.
//
//  NULL FOR AN OUT-OF-RANGE BAY rather than a reference to a shared empty, so a
//  caller cannot write into a placeholder and believe it recorded something.
//
////////////////////////////////////////////////////////////////////////////////

MountedImageState * DiskImageStore::SharedState (int slot, int drive)
{
    MountedImageState *  state = nullptr;



    if (IsValidBay (slot, drive))
    {
        state = &At (slot, drive).sharedState;
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::SharedState  (const)
//
//  The same answer for a caller that only reads it.
//
////////////////////////////////////////////////////////////////////////////////

const MountedImageState * DiskImageStore::SharedState (int slot, int drive) const
{
    const MountedImageState *  state = nullptr;



    if (IsValidBay (slot, drive))
    {
        state = &At (slot, drive).sharedState;
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
