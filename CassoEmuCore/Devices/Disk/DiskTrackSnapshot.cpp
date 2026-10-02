#include "Pch.h"

#include "DiskTrackSnapshot.h"
#include "DiskImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Capture
//
//  A track is shared with `previous` only when both come from the same
//  medium and the track's generation has not moved, which DiskImage
//  guarantees means the bits and length are identical. Everything else is
//  copied.
//
////////////////////////////////////////////////////////////////////////////////

void DiskTrackSnapshot::Capture (const DiskImage & disk, const DiskTrackSnapshot * previous)
{
    size_t  trackCount = disk.m_trackBits.size();
    size_t  track      = 0;
    bool    sameMedium = (previous != nullptr && previous->m_imageId == disk.m_imageId);
    bool    canShare   = false;



    m_imageId             = disk.m_imageId;
    m_loaded              = disk.m_loaded;
    m_dirty               = disk.m_dirty;
    m_imageWriteProtected = disk.m_imageWriteProtected;
    m_userWriteProtected  = disk.m_userWriteProtected;

    m_tracks.assign (trackCount, Track());

    for (track = 0; track < trackCount; track++)
    {
        canShare = sameMedium
                   && track < previous->m_tracks.size()
                   && previous->m_tracks[track].generation == disk.m_trackGeneration[track];

        m_tracks[track].bitCount   = disk.m_trackBitCounts[track];
        m_tracks[track].generation = disk.m_trackGeneration[track];
        m_tracks[track].dirty      = disk.m_trackDirty[track];
        m_tracks[track].bits       = canShare
                                     ? previous->m_tracks[track].bits
                                     : make_shared<const vector<Byte>> (disk.m_trackBits[track]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Restore
//
//  Fails with ERROR_INVALID_DATA, changing nothing, when the disk now in the
//  drive is not the same shape of medium (another loaded state or track
//  count). A track whose generation still matches is already identical and
//  is left alone. On the same medium a restored track takes back its saved
//  generation, so the next Capture can share it again; on another medium it
//  takes a fresh one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskTrackSnapshot::Restore (DiskImage & disk) const
{
    HRESULT  hr         = S_OK;
    size_t   trackCount = m_tracks.size();
    size_t   diskTracks = disk.m_trackBits.size();
    size_t   track      = 0;
    bool     sameMedium = (m_imageId == disk.m_imageId);



    CBREx (m_loaded   == disk.m_loaded, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (trackCount == diskTracks,    HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (track = 0; track < trackCount; track++)
    {
        disk.m_trackDirty[track] = m_tracks[track].dirty;

        if (sameMedium && disk.m_trackGeneration[track] == m_tracks[track].generation)
        {
            continue;
        }

        disk.m_trackBits[track]       = *m_tracks[track].bits;
        disk.m_trackBitCounts[track]  = m_tracks[track].bitCount;
        disk.m_trackGeneration[track] = sameMedium ? m_tracks[track].generation : ++disk.m_lastGeneration;
    }

    disk.m_dirty               = m_dirty;
    disk.m_imageWriteProtected = m_imageWriteProtected;
    disk.m_userWriteProtected  = m_userWriteProtected;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsTrackSharedWith
//
////////////////////////////////////////////////////////////////////////////////

bool DiskTrackSnapshot::IsTrackSharedWith (int track, const DiskTrackSnapshot & other) const
{
    bool  inRange = track >= 0
                    && track < static_cast<int> (m_tracks.size())
                    && track < static_cast<int> (other.m_tracks.size());



    return inRange && m_tracks[track].bits == other.m_tracks[track].bits;
}



