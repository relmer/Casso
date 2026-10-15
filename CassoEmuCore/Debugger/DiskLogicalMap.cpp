#include "Pch.h"

#include "Debugger/DiskLogicalMap.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"





static constexpr size_t  kSectorBytes = 256;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogicalMap::GetDosPhysical
//
//  The inverse of the nibblizer's own table of each physical sector's
//  DOS-order file index.
//
////////////////////////////////////////////////////////////////////////////////

int DiskLogicalMap::GetDosPhysical (int logical)
{
    int  found = -1;



    for (int physical = 0; physical < kSectorsPerTrack; physical++)
    {
        found = (NibblizationLayer::GetDosFileIndexForPhysicalSector (physical) == logical) ? physical : found;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogicalMap::GetBlockPhysical
//
//  ProDosSkeleton places a block's bytes in a DOS-order image; each half's
//  place there is a track and a DOS logical sector, which GetDosPhysical
//  turns into the physical sector.
//
////////////////////////////////////////////////////////////////////////////////

void DiskLogicalMap::GetBlockPhysical (Word block, int & outTrack, int & outFirst, int & outSecond)
{
    size_t  first  = ProDosSkeleton::GetBlockByteOffset (block, 0) / kSectorBytes;
    size_t  second = ProDosSkeleton::GetBlockByteOffset (block, kSectorBytes) / kSectorBytes;



    outTrack  = (int) (first / kSectorsPerTrack);
    outFirst  = GetDosPhysical ((int) (first  % kSectorsPerTrack));
    outSecond = GetDosPhysical ((int) (second % kSectorsPerTrack));
}
