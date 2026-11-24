#include "Pch.h"

#include "Machines/Apple2/Common/SectorInterleave.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DOS 3.3 physical-to-file interleave (used when nibblizing .dsk/.do)
//
//  INDEXED BY PHYSICAL SECTOR. The sixteen address fields go down in
//  sequential order, so the address-field number IS the physical position,
//  and the 256 data bytes emitted under address mark P come from file offset
//  (track * 16 + kDsk_PhysicalToFile[P]) * 256.
//
//  A .dsk stores its sectors in DOS LOGICAL order, so the file index here is
//  also the DOS logical sector number: this table is physical-to-logical, the
//  exact inverse of the logical-to-physical skew DOS itself boots through at
//  $084D -- which DirectBootTests asserts against a persisted copy of that
//  table. This constant wore the name kDsk_LtoP for a long time, and half
//  the code reading it believed the name; the belief was measurably backwards.
//
////////////////////////////////////////////////////////////////////////////////

const int SectorInterleave::kDsk_PhysicalToFile[NibbleFormat::kSectorsPerTrack] =
{
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};





////////////////////////////////////////////////////////////////////////////////
//
//  ProDOS physical-to-file interleave for .po
//
//  INDEXED BY PHYSICAL SECTOR, exactly like its DOS neighbor above. A .po
//  arranges its 16 sectors per track in ProDOS-block order rather than DOS
//  logical order, so the same physical positions map to different file
//  offsets: the 256 data bytes emitted under address mark P come from file
//  offset (track * 16 + kPo_PhysicalToFile[P]) * 256.
//
////////////////////////////////////////////////////////////////////////////////

const int SectorInterleave::kPo_PhysicalToFile[NibbleFormat::kSectorsPerTrack] =
{
    0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15
};





////////////////////////////////////////////////////////////////////////////////
//
//  SectorInterleave::GetPoFileIndexForDosLogicalSector
//
//  Both interleave tables answer the same question -- which file offset holds
//  the sector the drive will see at physical position P -- so the mapping
//  between the two FILE layouts is their composition, and is derived here
//  rather than written down a third time.
//
//  Writing it down again is the specific hazard. A reorder table restated by
//  hand can be wrong in a way nothing notices: the file is written through it
//  and read back through it, so every round trip agrees with itself and the
//  image is unreadable only on a real machine.
//
////////////////////////////////////////////////////////////////////////////////

int SectorInterleave::GetPoFileIndexForDosLogicalSector (int logicalSector)
{
    HRESULT  hr       = S_OK;
    int      physical = 0;
    int      found    = 0;
    bool     inRange  = logicalSector >= 0 && logicalSector < NibbleFormat::kSectorsPerTrack;



    CBRAEx (inRange, E_INVALIDARG);

    for (physical = 0; physical < NibbleFormat::kSectorsPerTrack; physical++)
    {
        if (kDsk_PhysicalToFile[physical] == logicalSector)
        {
            found = kPo_PhysicalToFile[physical];
        }
    }

Error:
    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorInterleave::GetDosFileIndexForPhysicalSector
//
//  The same table the nibblizer reads when it decides which of a buffer's
//  sixteen sectors to encode under each address-field number, answered for
//  callers that have to lay bytes down in the order a drive will hand them
//  back rather than in the order a filesystem stores them.
//
//  This is the table itself rather than a composition of it, because the
//  table IS indexed by physical sector. It used to have a twin named for a
//  logical-sector argument, reading the same table with the same index; the
//  two answers were byte-identical because they were the same lookup, and
//  the twin's name was the half that lied.
//
////////////////////////////////////////////////////////////////////////////////

int SectorInterleave::GetDosFileIndexForPhysicalSector (int physicalSector)
{
    HRESULT  hr      = S_OK;
    int      found   = 0;
    bool     inRange = physicalSector >= 0 && physicalSector < NibbleFormat::kSectorsPerTrack;



    CBRAEx (inRange, E_INVALIDARG);

    found = kDsk_PhysicalToFile[physicalSector];

Error:
    return found;
}





