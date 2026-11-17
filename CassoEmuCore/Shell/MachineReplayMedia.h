#pragma once

#include "Pch.h"

#include "Devices/BlockDevice/IReplayMedia.h"

struct MachineRefs;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineReplayMedia
//
//  The hard disks, ProFiles and 3.5-inch drives of a machine, in an order
//  two machines built from one configuration share: each SCSI card's
//  targets, each ProFile, each 3.5 controller card's drives, each Liron
//  card's chain, then the //c's SmartPort chain and the //c Plus's drives.
//  A replay on a second machine copies the first machine's disks and mounts
//  them in the second's matching drives.
//
////////////////////////////////////////////////////////////////////////////////

class MachineReplayMedia
{
public:
    static void     Get   (MachineRefs & refs, std::vector<IReplayMedia *> & outDrives);

    //  On the thread that runs the machine.
    static HRESULT  Copy  (MachineRefs & refs, std::vector<ReplayBlockMedia> & outCopies);

    //  Mounts each copy in its drive, except where `mounted` already has
    //  that copy, and updates `mounted`. Copies from a machine with other
    //  drives are bad data.
    static HRESULT  Mount (MachineRefs & refs, const std::vector<ReplayBlockMedia> & copies, std::vector<ReplayBlockMedia> & mounted);
};
