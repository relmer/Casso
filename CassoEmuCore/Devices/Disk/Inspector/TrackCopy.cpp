#include "Pch.h"

#include "Devices/Disk/Inspector/TrackCopy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackCopy::MakeFromImage
//
//  Copies one slot of an image. The caller is responsible for doing this
//  where nothing else can change the image at the same time: the emulation
//  thread between guest accesses, or a thread that owns an image read from a
//  file.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const TrackCopy> TrackCopy::MakeFromImage (const DiskImage & image, int slot)
{
    auto  copy = std::make_shared<TrackCopy>();



    copy->slot = slot;
    copy->kind = image.GetTrackKind (slot);

    if (copy->kind == TrackKind::Flux)
    {
        copy->fluxBytes = image.GetFluxTrack (slot).GetBytes();
    }
    else if (slot >= 0 && slot < image.GetTrackCount())
    {
        copy->bits     = image.GetTrackBits (slot);
        copy->bitCount = image.GetTrackBitCount (slot);
    }

    return copy;
}
