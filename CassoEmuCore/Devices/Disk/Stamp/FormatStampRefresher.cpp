#include "Pch.h"

#include "Devices/Disk/Stamp/FormatStampRefresher.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Inspector/DecodeSettings.h"
#include "Devices/Disk/Inspector/DiskCopy.h"
#include "Devices/Disk/Inspector/TrackCopy.h"
#include "Devices/Disk/Stamp/FormatStamp.h"
#include "Devices/Disk/Stamp/FormatStampBuilder.h"
#include "Devices/Disk/Stamp/FormatStampChunk.h"
#include "Machines/Apple2/Common/WozMetadata.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampRefresher::GetMetadataForSave
//
////////////////////////////////////////////////////////////////////////////////

void FormatStampRefresher::GetMetadataForSave (const DiskImage & image, WozMetadata & outMeta)
{
    HRESULT      hr    = S_OK;
    FormatStamp  stamp;
    StampState   state = StampState::Absent;



    outMeta = image.GetWozMetadata();
    state   = FormatStampChunk::GetStamp (outMeta, stamp);

    if (state == StampState::Unreadable && HasDirtyTrack (image))
    {
        FormatStampChunk::RemoveStamp (outMeta);
    }

    if (state != StampState::Readable || !TryRefresh (image, stamp))
    {
        return;
    }

    //  A refreshed stamp is about the size of the one it replaces, so it
    //  passes the size limit only in theory; if it does, the stale stamp is
    //  removed rather than written.
    hr = FormatStampChunk::SetStamp (outMeta, stamp);

    if (FAILED (hr))
    {
        FormatStampChunk::RemoveStamp (outMeta);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampRefresher::HasDirtyTrack
//
////////////////////////////////////////////////////////////////////////////////

bool FormatStampRefresher::HasDirtyTrack (const DiskImage & image)
{
    bool  isDirty = false;



    for (int slot = 0; !isDirty && slot < image.GetTrackCount(); slot++)
    {
        isDirty = image.IsTrackDirty (slot);
    }

    return isDirty;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampRefresher::TryRefresh
//
//  Replaces each changed entry and adds an entry for each position that
//  gained a record, with the stamp's decode settings. False when nothing
//  changed.
//
////////////////////////////////////////////////////////////////////////////////

bool FormatStampRefresher::TryRefresh (const DiskImage & image, FormatStamp & ioStamp)
{
    std::shared_ptr<const DiskCopy>  copy      = DiskCopy::MakeFromImage (image, 0, std::string(), 0, false);
    DecodeSettings                   settings;
    bool                             isChanged = false;



    for (const DecodeRange & range : ioStamp.decodeRanges)
    {
        settings.AddRange (range);
    }

    for (StampTrack & entry : ioStamp.tracks)
    {
        int   slot     = copy->playedSlot[(size_t) entry.position];
        bool  hasTrack = slot >= 0 && slot < (int) copy->tracks.size() && copy->tracks[(size_t) slot] != nullptr;
        bool  isStale  = false;

        if (entry.hasBitsCrc)
        {
            isStale = !hasTrack || FormatStampBuilder::ComputeTrackCrc (*copy->tracks[(size_t) slot]) != entry.bitsCrc;
        }
        else
        {
            isStale = hasTrack && image.IsTrackDirty (slot);
        }

        if (!isStale)
        {
            continue;
        }

        FormatStampBuilder::BuildTrack (*copy, entry.position, settings, entry);
        entry.isChangedAfterCapture = true;
        isChanged                   = true;
    }

    for (int position : FormatStampBuilder::GetStampPositions (*copy))
    {
        StampTrack  entry;

        if (std::ranges::any_of (ioStamp.tracks, [position] (const StampTrack & existing) { return existing.position == position; }))
        {
            continue;
        }

        FormatStampBuilder::BuildTrack (*copy, position, settings, entry);
        entry.isChangedAfterCapture = true;
        isChanged                   = true;

        ioStamp.tracks.insert (std::ranges::upper_bound (ioStamp.tracks, position, {}, &StampTrack::position), std::move (entry));
    }

    return isChanged;
}
