#include "Pch.h"

#include "Devices/Disk/Inspector/DiskCopy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCopy::MakeFromImage
//
//  Every record, both maps of quarter tracks to records (what each quarter
//  track plays, and what the map gives it even when that holds nothing), and
//  everything about the file the inspector shows. The caller makes it where
//  nothing can change the image meanwhile.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DiskCopy> DiskCopy::MakeFromImage (const DiskImage & image, uint64_t mediaId, const std::string & fileName, uint64_t fileSize, bool isReadOnly)
{
    auto  copy = std::make_shared<DiskCopy>();
    int   slot = 0;
    int   qt   = 0;



    copy->mediaId              = mediaId;
    copy->format               = image.GetSourceFormat();
    copy->fileName             = fileName;
    copy->fileSize             = fileSize;
    copy->isReadOnly           = isReadOnly;
    copy->damagedTracks        = image.GetDamagedTracks();
    copy->damagedQuarterTracks = image.GetDamagedQuarterTrackEntries();
    copy->hasCrcMismatch       = image.HasSourceCrcMismatch();
    copy->woz                  = image.GetWozMetadata();
    copy->writeProtect         = image.GetWriteProtectInfo();

    for (slot = 0; slot < image.GetTrackCount(); slot++)
    {
        copy->tracks.push_back (TrackCopy::MakeFromImage (image, slot));
    }

    for (qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
    {
        copy->playedSlot[qt] = image.ResolveQuarterTrack (qt);
        copy->mappedSlot[qt] = image.GetMappedSlot (qt);
    }

    return copy;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCopy::IsSlotDamaged
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCopy::IsSlotDamaged (int slot) const
{
    return std::any_of (damagedTracks.begin(), damagedTracks.end(), [slot] (const DamagedTrack & d) { return d.trkIndex == slot; });
}
