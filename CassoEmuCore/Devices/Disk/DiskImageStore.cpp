#include "Pch.h"

#include "DiskImageStore.h"
#include "NibblizationLayer.h"
#include "WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore::DiskImageStore
//
////////////////////////////////////////////////////////////////////////////////

DiskImageStore::DiskImageStore ()
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
    HRESULT   hr   = S_OK;
    size_t    pos  = 0;
    string    ext;
    size_t    i    = 0;

    pos = path.find_last_of ('.');

    if (pos == string::npos || pos + 1 >= path.size ())
    {
        hr = E_FAIL;
        goto Error;
    }

    ext = path.substr (pos + 1);

    for (i = 0; i < ext.size (); i++)
    {
        ext[i] = static_cast<char> (tolower (static_cast<unsigned char> (ext[i])));
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

    if (slot < 0 || slot >= kSlotCount || drive < 0 || drive >= kDriveCount)
    {
        hr = E_INVALIDARG;
        goto Error;
    }

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

        if (!entry.image->IsLoaded ())
        {
            entry.image.reset ();
            entry.path.clear ();
            entry.mounted = false;
            hr = E_FAIL;
            goto Error;
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
        ifstream  file (path, ios::binary | ios::ate);
        streamsize  size = 0;

        fileOk = file.good ();
        CBREx (fileOk, E_FAIL);

        size = file.tellg ();
        file.seekg (0, ios::beg);

        bytes.resize (static_cast<size_t> (size));

        if (size > 0)
        {
            file.read (reinterpret_cast<char *> (bytes.data ()),
                       static_cast<streamsize> (size));
        }
    }

    hr = MountFromBytes (slot, drive, path, fmt, bytes);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushEntry
//
//  Centralized flush helper. Dispatches through SetFlushSink when the
//  test hook is installed; otherwise writes to the host filesystem.
//  Does nothing if the image is clean or no source path is recorded.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageStore::FlushEntry (Entry & entry)
{
    HRESULT       hr     = S_OK;
    vector<Byte>  bytes;
    bool          fileOk = false;

    if (!entry.mounted || entry.image == nullptr)
    {
        goto Error;
    }

    if (!entry.image->IsDirty ())
    {
        goto Error;
    }

    if (entry.image->IsWriteProtected ())
    {
        entry.image->ClearDirty ();
        goto Error;
    }

    hr = entry.image->Serialize (bytes);

    if (FAILED (hr))
    {
        goto Error;
    }

    if (m_flushSink)
    {
        hr = m_flushSink (entry.path, bytes);
        CHR (hr);
    }
    else if (!entry.path.empty ())
    {
        ofstream  file (entry.path, ios::binary);

        fileOk = file.good ();
        CBREx (fileOk, E_FAIL);

        file.write (reinterpret_cast<const char *> (bytes.data ()),
                    static_cast<streamsize> (bytes.size ()));
    }

    entry.image->ClearDirty ();

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

    if (slot < 0 || slot >= kSlotCount || drive < 0 || drive >= kDriveCount)
    {
        hr = E_INVALIDARG;
        goto Error;
    }

    hr = FlushEntry (At (slot, drive));

Error:
    return hr;
}


HRESULT DiskImageStore::FlushAll ()
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

    if (slot < 0 || slot >= kSlotCount || drive < 0 || drive >= kDriveCount)
    {
        return;
    }

    {
        Entry &   entry = At (slot, drive);

        if (!entry.mounted)
        {
            return;
        }

        hr = FlushEntry (entry);
        IGNORE_RETURN_VALUE (hr, S_OK);

        entry.image.reset ();
        entry.path.clear ();
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

void DiskImageStore::SoftReset ()
{
    HRESULT   hr = FlushAll ();

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  FR-035 / data-model.md: unmount everything, flushing dirty as we go.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageStore::PowerCycle ()
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
//  GetImage / IsMounted / GetSourcePath
//
////////////////////////////////////////////////////////////////////////////////

DiskImage * DiskImageStore::GetImage (int slot, int drive)
{
    if (slot < 0 || slot >= kSlotCount || drive < 0 || drive >= kDriveCount)
    {
        return nullptr;
    }

    return At (slot, drive).image.get ();
}


bool DiskImageStore::IsMounted (int slot, int drive) const
{
    if (slot < 0 || slot >= kSlotCount || drive < 0 || drive >= kDriveCount)
    {
        return false;
    }

    return At (slot, drive).mounted;
}


const string & DiskImageStore::GetSourcePath (int slot, int drive) const
{
    if (slot < 0 || slot >= kSlotCount || drive < 0 || drive >= kDriveCount)
    {
        return m_emptyPath;
    }

    return At (slot, drive).path;
}
