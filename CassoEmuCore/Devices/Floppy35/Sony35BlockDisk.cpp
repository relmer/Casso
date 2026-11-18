#include "Pch.h"

#include "Devices/Floppy35/Sony35BlockDisk.h"

#include "Devices/Floppy35/Gcr35Track.h"





static constexpr int  s_kSides = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35BlockDisk::Attach
//
//  An 800K image is two-sided, a 400K image one-sided, and any other size
//  fails.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35BlockDisk::Attach (IDiskFileIo & fileIo, const std::string & path, const WriteProtectInfo & hostProtect, std::wstring & outError)
{
    HRESULT   hr     = S_OK;
    uint32_t  blocks = 0;
    bool      isFit  = false;



    hr = m_media.Attach (fileIo, path, Gcr35Track::kBlocksTwoSides, hostProtect, outError);
    CHR (hr);

    blocks = m_media.GetBlockCount();
    isFit  = blocks == Gcr35Track::kBlocksTwoSides || blocks == Gcr35Track::kBlocksOneSide;

    if (!isFit)
    {
        hr = m_media.Detach();
        IGNORE_RETURN_VALUE (hr, S_OK);

        outError = std::format (L"A 3.5-inch disk is 800 or 1,600 blocks, and the image has {}.", blocks);
        CBREx (false, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));
    }

    m_sides = (blocks == Gcr35Track::kBlocksTwoSides) ? s_kSides : 1;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35BlockDisk::MountReplayCopy
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35BlockDisk::MountReplayCopy (const ReplayBlockMedia & copy)
{
    HRESULT  hr = S_OK;



    hr = m_media.MountReplayCopy (copy);
    CHR (hr);

    m_sides = (m_media.GetBlockCount() == Gcr35Track::kBlocksTwoSides) ? s_kSides : 1;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35BlockDisk::ReadTrack
//
//  The track's sectors read from the image and GCR encoded.
//
////////////////////////////////////////////////////////////////////////////////

void Sony35BlockDisk::ReadTrack (int track, int side, std::vector<Byte> & outBits)
{
    int                count  = Gcr35Track::GetSectorsPerTrack (track);
    uint32_t           first  = Gcr35Track::GetFirstBlock (track, side, m_sides);
    Byte               format = (m_sides == s_kSides) ? Gcr35Track::kFormatTwoSides : Gcr35Track::kFormatOneSide;
    std::vector<Byte>  data ((size_t) count * Gcr35Track::kDataBytes, 0);



    for (int sector = 0; sector < count; sector++)
    {
        HRESULT  hr = m_media.ReadBlock (first + (uint32_t) sector, std::span<Byte> (data).subspan ((size_t) sector * Gcr35Track::kDataBytes, Gcr35Track::kDataBytes));

        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    Gcr35Track::BuildTrack (track, side, format, data, outBits);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35BlockDisk::Flush
//
//  Each written track decoded back to its sectors, which go to the image.
//  A sector that no longer decodes is left as the image has it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35BlockDisk::Flush (const Tracks & tracks, std::set<int> & dirty)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  data;
    std::vector<bool>  found;



    for (int key : dirty)
    {
        int       track = key / s_kSides;
        int       side  = key % s_kSides;
        uint32_t  first = Gcr35Track::GetFirstBlock (track, side, m_sides);

        Gcr35Track::DecodeTrack (track, side, tracks.at (key), data, found);

        for (size_t sector = 0; sector < found.size(); sector++)
        {
            if (!found[sector])
            {
                continue;
            }

            hr = m_media.WriteBlock (first + (uint32_t) sector, std::span<const Byte> (data).subspan (sector * Gcr35Track::kDataBytes, Gcr35Track::kDataBytes));
            CHR (hr);
        }
    }

    dirty.clear();

    hr = m_media.Flush();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35BlockDisk::SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Sony35BlockDisk::SaveState (StateWriter & writer) const
{
    m_media.SaveState (writer);

    return S_OK;
}
