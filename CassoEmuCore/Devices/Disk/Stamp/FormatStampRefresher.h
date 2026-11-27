#pragma once

#include "Pch.h"

class DiskImage;
struct FormatStamp;
struct WozMetadata;





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampRefresher
//
//  The WOZ metadata a save writes, with the format stamp brought up to date
//  with the tracks. An entry whose CRC no longer matches its track's bits,
//  or, without a CRC, whose track is dirty, is analyzed again from the
//  current bits; its capture facts are removed and it is marked as changed
//  after capture. A position that gained a record gets an entry. When no
//  entry changed, the stamp chunk is written back byte for byte. A stamp
//  this build can't read is removed once a track changed, since it can't
//  be brought up to date.
//
////////////////////////////////////////////////////////////////////////////////

class FormatStampRefresher
{
public:
    static void  GetMetadataForSave (const DiskImage & image, WozMetadata & outMeta);

private:
    static bool  HasDirtyTrack      (const DiskImage & image);
    static bool  TryRefresh         (const DiskImage & image, FormatStamp & ioStamp);
};
