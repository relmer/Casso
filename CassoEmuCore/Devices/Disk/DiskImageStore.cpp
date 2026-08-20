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

wstring DiskImageStore::FormatCrcLaunderMessage (const string & path)
{
    wstring  widePath = fs::path (path).wstring();



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    return L"Casso is about to save a disk image whose stored checksum did not "
           L"match its contents when it was loaded:\n\n" + widePath +
           L"\n\nThe saved file gets a newly computed checksum, so after this "
           L"save the existing damage can no longer be detected. Keep a copy of "
           L"the original first if its condition matters.";
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

wstring DiskImageStore::FormatFlushLossMessage (const string & path)
{
    wstring  widePath = fs::path (path).wstring();



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    return L"Casso could not save changes to the disk image:\n\n" + widePath +
           L"\n\nYour recent writes were NOT persisted. The file on disk is "
           L"unchanged. If this is a .dsk, try a .woz image -- WOZ round-trips "
           L"writes reliably.";
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
    HRESULT       hr     = S_OK;
    vector<Byte>  bytes;



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
    // Said before the write, not after: once the file is replaced its stored
    // CRC is valid again, so this is the last moment the mismatch is true.
    if (entry.image->HasSourceCrcMismatch())
    {
        EhmNotifyUser (FormatCrcLaunderMessage (entry.path).c_str());
    }

    hr = entry.image->Serialize (bytes);
    CHRN (hr, FormatFlushLossMessage (entry.path).c_str());

    if (m_flushSink)
    {
        hr = m_flushSink (entry.path, bytes);
        CHRN (hr, FormatFlushLossMessage (entry.path).c_str());
    }
    else if (!entry.path.empty())
    {
        // Never write in place: a flush that fails midway would otherwise
        // have already truncated the user's image, trading a stale file for
        // no file at all. The dirty bit survives a failure, so a later flush
        // retries.
        hr = WriteFileAtomically (entry.path, bytes);
        CHRN (hr, FormatFlushLossMessage (entry.path).c_str());
    }

    // The file now carries a freshly computed CRC that matches it, so the
    // mismatch is no longer true of what is on disk -- and the warning above
    // must not repeat on every later eject or power cycle.
    entry.image->SetSourceCrcMismatch (false);
    entry.image->ClearDirty();

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
