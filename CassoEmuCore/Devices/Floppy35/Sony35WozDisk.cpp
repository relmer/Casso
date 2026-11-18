#include "Pch.h"

#include "Devices/Floppy35/Sony35WozDisk.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/BlockDevice/IReplayMedia.h"
#include "Devices/Disk/CommitPlan.h"
#include "Devices/Disk/DurableCommit.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Floppy35/Gcr35Track.h"
#include "Machines/Apple2/Common/WozLoader.h"





static constexpr int  s_kSides = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35WozDisk::Load
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35WozDisk::Load (IDiskFileIo * fileIo, const std::string & path, const std::vector<Byte> & bytes, const WriteProtectInfo & hostProtect, std::wstring & outError)
{
    HRESULT                 hr       = S_OK;
    WozLoader::Description  about;
    bool                    isLoaded = false;



    WozLoader::Describe (bytes, about);

    if (!about.isWoz || about.diskType != WozLoader::kDiskType35)
    {
        outError = L"The WOZ image isn't a 3.5-inch disk.";
        CBREx (false, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));
    }

    m_image.LoadFromBytes (DiskFormat::Woz, bytes, path);
    isLoaded = m_image.IsLoaded();

    if (!isLoaded)
    {
        outError = L"The WOZ image's tracks don't load.";
        CBREx (false, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));
    }

    m_sides       = (about.info.sides == 1) ? 1 : s_kSides;
    m_isProtected = about.writeProtected || hostProtect.Any();
    m_fileIo      = fileIo;
    m_path        = path;
    m_bytes       = std::make_shared<const std::vector<Byte>> (bytes);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35WozDisk::ReadTrack
//
//  The WOZ track at TMAP entry track * 2 + side, or a blank track of the
//  zone's length where the entry is empty.
//
////////////////////////////////////////////////////////////////////////////////

void Sony35WozDisk::ReadTrack (int track, int side, std::vector<Byte> & outBits)
{
    int     slot  = m_image.GetMappedSlot (track * s_kSides + side);
    size_t  count = (slot >= 0) ? m_image.GetTrackBitCount (slot) : 0;



    outBits.assign ((count != 0) ? count : Gcr35Track::GetTrackBits (track), 0);

    for (size_t i = 0; i < count; i++)
    {
        outBits[i] = m_image.ReadBit (slot, i);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35WozDisk::Flush
//
//  Each written track's bits replace the WOZ track at TMAP entry track * 2 +
//  side, a new track where the entry is empty, and the image replaces its
//  file in one step: after an interruption the file is the old one or the
//  new one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35WozDisk::Flush (const Tracks & tracks, std::set<int> & dirty)
{
    HRESULT               hr       = S_OK;
    bool                  hasDirty = !dirty.empty();
    std::vector<Byte>     bytes;
    CommitPlan::Progress  progress = {};



    BAIL_OUT_IF (!hasDirty, S_OK);

    for (int key : dirty)
    {
        const std::vector<Byte>  & bits = tracks.at (key);
        int                        slot = m_image.GetMappedSlot (key);

        if (slot < 0)
        {
            slot = m_image.GetTrackCount();
            m_image.EnsureTrackSlots (slot + 1);
            m_image.SetQuarterTrackSlot (key, slot);
        }

        m_image.ResizeTrack (slot, bits.size());

        for (size_t i = 0; i < bits.size(); i++)
        {
            m_image.WriteBit (slot, i, bits[i]);
        }
    }

    dirty.clear();

    hr = m_image.Serialize (bytes);
    CHR (hr);

    //  A replay's copy doesn't write a file.
    BAIL_OUT_IF (m_fileIo == nullptr, S_OK);

    hr = DurableCommit::Commit (*m_fileIo, m_path, bytes, 0, CommitMode::Replace, progress);
    CHR (hr);

    m_bytes = std::make_shared<const std::vector<Byte>> (std::move (bytes));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35WozDisk::SaveState
//
//  The image's path, then the image.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35WozDisk::SaveState (StateWriter & writer) const
{
    writer.WriteUInt32 (static_cast<uint32_t> (m_path.size()));
    writer.WriteBytes  (reinterpret_cast<const Byte *> (m_path.data()), m_path.size());

    return m_image.SaveState (writer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35WozDisk::LoadState
//
//  A state saved with another image is bad data.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35WozDisk::LoadState (StateReader & reader)
{
    HRESULT      hr         = S_OK;
    uint32_t     pathLength = 0;
    std::string  path;



    reader.ReadUInt32 (pathLength);
    CBREx (pathLength <= MAX_PATH * 4, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    path.resize (pathLength);
    reader.ReadBytes (reinterpret_cast<Byte *> (path.data()), pathLength);
    CBREx (path == m_path, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = m_image.LoadState (reader);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35WozDisk::CopyForReplay
//
//  The WOZ file as last read or written.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35WozDisk::CopyForReplay (ReplayBlockMedia & outCopy)
{
    outCopy                   = {};
    outCopy.path              = m_path;
    outCopy.protect.imageFlag = m_isProtected;
    outCopy.isWoz             = true;
    outCopy.content           = m_bytes;

    return S_OK;
}
