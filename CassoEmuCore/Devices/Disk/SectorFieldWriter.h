#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/SectorWrite.h"
#include "Devices/Disk/Inspector/FieldLocator.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SectorFieldWriter
//
//  The one way Casso's tools change an existing sector. It rewrites only the
//  nibbles of a data field's body and checksum inside the existing track and
//  keeps everything else: the address field with its volume, the marks, every
//  sync run and gap, every other field, the track's length and each sector's
//  place.
//
//  On a bit track each new nibble takes the cells of the old one at the same
//  place and keeps the extra zero cells after it. On a flux track each cell
//  keeps the time it was recorded with, so the field's total time, the time of
//  one turn and every transition outside the field are unchanged. A record is
//  found through the image's map, never by taking slot N as track N, and a
//  record several quarter tracks read is changed once.
//
//  All or nothing: every write is planned and checked on a copy first, by
//  decoding the new track again, and the image changes only when all of them
//  pass. A failure leaves the image exactly as it was and lists every write
//  that could not be made.
//
////////////////////////////////////////////////////////////////////////////////

class SectorFieldWriter
{
public:
    static HRESULT      Write    (DiskImage                    &  image,
                                  std::span<const SectorWrite>    writes,
                                  SectorWritePolicy               policy,
                                  vector<SectorWriteError>     &  outErrors);

    static std::string  Describe (const SectorWriteError & error);

private:
    struct SlotPlan
    {
        int                               slot = -1;
        vector<const SectorWrite *>       writes;
        std::shared_ptr<const TrackCopy>  before;
        std::shared_ptr<TrackCopy>        after;
    };

    static int   ResolveSlot      (const DiskImage & image, const SectorWrite & write);
    static bool  IsSlotDamaged    (const DiskImage & image, int slot);
    static bool  IsTrackComplete  (const vector<LocatedField> & fields);
    static int   FindDataField    (const vector<LocatedField> & fields, const SectorWrite & write, SectorWriteFailure & outFailure);
    static void  PlanSlot         (const DiskImage & image, SectorWritePolicy policy, SlotPlan & inOutPlan, vector<SectorWriteError> & inOutErrors);
    static void  WriteNibble      (const FramedTrack & track, int nibbleIndex, Byte value, vector<Byte> & inOutCells);
    static void  BuildAfter       (const FramedTrack & track, const vector<Byte> & cells, SlotPlan & inOutPlan);
    static bool  IsVerified       (const vector<LocatedField> & before, const vector<LocatedField> & after, const vector<int> & written, const SlotPlan & plan);
    static void  AddError         (const SectorWrite & write, int slot, SectorWriteFailure reason, vector<SectorWriteError> & inOutErrors);
};
