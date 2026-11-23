#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/NibbleFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SectorInterleave
//
//  The DOS 3.3 and ProDOS sector skews of a 16-sector image: which sector of
//  a .dsk / .do or .po file is under each physical address field.
//
////////////////////////////////////////////////////////////////////////////////

class SectorInterleave
{
public:
    //  Indexed by physical sector; each entry is a file sector within the
    //  track.
    static const int  kDsk_PhysicalToFile[NibbleFormat::kSectorsPerTrack];
    static const int  kPo_PhysicalToFile[NibbleFormat::kSectorsPerTrack];

    //  Where DOS logical sector L sits within a ProDOS-ordered file's track.
    //
    //  COMPOSED from the two interleave tables above rather than restated. Both
    //  of those are indexed by PHYSICAL sector, and a hand-written third table
    //  indexed by logical sector is how a file reorder comes to disagree with
    //  the layout the drive would actually see -- an image that reads back
    //  perfectly through the same wrong table and is garbage on real hardware.
    static int  GetPoFileIndexForDosLogicalSector (int logicalSector);

    //  Which sector of a DOS-ordered buffer the drive presents at physical
    //  position P -- equivalently, which one answers to the address field
    //  numbered P, since the sixteen address fields are laid down in order.
    //
    //  ANYONE READING SECTORS THE WAY THE BOOT ROM DOES NEEDS THIS. The ROM
    //  asks for consecutive address-field numbers, which is physical order,
    //  and a buffer is in logical order; the two differ by the skew this file
    //  owns. Writing that skew down a second time is what the composition
    //  above exists to prevent, so it is answered here instead.
    //
    //  NOT for addressing an image's records by sector number: a DOS-ordered
    //  image keeps logical sector S at record (T * 16 + S), the identity, and
    //  the sector commands take that path. This function once had a twin
    //  under a logical-sector name performing the identical lookup, which the
    //  sector commands routed their numbers through -- the twin's name
    //  claimed the inverse of what the shared table does, and following the
    //  name put user bytes on the wrong sector. The twin is gone.
    static int  GetDosFileIndexForPhysicalSector  (int physicalSector);
};
