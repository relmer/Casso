#include "Pch.h"

#include "DiskImageStore.h"
#include "NibblizationLayer.h"
#include "WozLoader.h"





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
    HRESULT   hr = S_OK;

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
            entry.mounted = false;
            hr = E_FAIL;
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
    HRESULT       hr        = S_OK;
    DiskFormat    fmt       = DiskFormat::Dsk;
    vector<Byte>  bytes;
    bool          fileOk    = false;



    hr = DetectFormatByExtension (path, fmt);
    CHR (hr);

    {
        streamsize  size = 0;

        ifstream  file (path, ios::binary | ios::ate);

        fileOk = file.good();
        CBR (fileOk);

        size = file.tellg();
        file.seekg (0, ios::beg);

        bytes.resize (static_cast<size_t> (size));

        if (size > 0)
        {
            file.read (reinterpret_cast<char *> (bytes.data()),
                       static_cast<streamsize> (size));
        }
    }

    hr = MountFromBytes (slot, drive, path, fmt, bytes);

Error:
    return hr;
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

wstring DiskImageStore::FormatFlushLossMessage (const string & path, const string & recoveryPath)
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
                   L"\n\nThat copy is complete -- it keeps the track that could not "
                   L"be written back. Mount it to carry on from where you were.";
    }
    else
    {
        message += L" Your recent writes were NOT persisted. If this is a .dsk, "
                   L"try a .woz image -- WOZ round-trips writes reliably.";
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

HRESULT DiskImageStore::FlushEntry (Entry & entry, bool force)
{
    HRESULT       hr         = S_OK;
    HRESULT       hrRecovery = S_OK;
    string        recoveryPath;
    vector<Byte>  bytes;
    bool          fileOk     = false;



    // No-op cases -- nothing dirty to persist -- succeed silently. A
    // forced flush persists the current state regardless: the dirty bit
    // and the write-protect gate govern guest writes, not a host-side
    // persist of (say) a flipped write-protect flag.
    BAIL_OUT_IF (!entry.mounted || entry.image == nullptr, S_OK);
    BAIL_OUT_IF (!force && !entry.image->IsDirty(), S_OK);

    if (!force && entry.image->IsWriteProtected())
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
        ofstream  file (entry.path, ios::binary);

        fileOk = file.good();
        CBRN (fileOk, FormatFlushLossMessage (entry.path, recoveryPath).c_str());

        file.write (reinterpret_cast<const char *> (bytes.data()),
                    static_cast<streamsize> (bytes.size()));
    }

    entry.image->ClearDirty();

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



    CBRAEx (slot >= 0 && slot < kSlotCount && drive >= 0 && drive < kDriveCount, E_INVALIDARG);

    hr = FlushEntry (At (slot, drive));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ForceFlush
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::ForceFlush (int slot, int drive)
{
    HRESULT   hr = S_OK;



    CBRAEx (slot >= 0 && slot < kSlotCount && drive >= 0 && drive < kDriveCount, E_INVALIDARG);

    hr = FlushEntry (At (slot, drive), true);

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
