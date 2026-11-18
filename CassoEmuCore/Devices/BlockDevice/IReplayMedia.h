#pragma once

#include "Pch.h"

#include "Devices/Disk/IDiskImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayBlockMedia
//
//  A hard disk, ProFile or 3.5-inch disk for a replay on a second machine:
//  its path, its write protection and its content as it was when
//  it was attached, shared by every copy and never written. A machine state
//  has the blocks written since then. A 3.5-inch WOZ image's content is the
//  image's file; its state has every track. An empty drive has no content.
//
////////////////////////////////////////////////////////////////////////////////

struct ReplayBlockMedia
{
    std::string                                path;
    WriteProtectInfo                           protect;
    bool                                       isWoz    = false;
    std::shared_ptr<const std::vector<Byte>>   content;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IReplayMedia
//
//  A drive whose disk a replay on a second machine copies. CopyForReplay is
//  called on the thread that runs the first machine. MountReplayCopy is
//  called on the second machine's matching drive: the copy's writes stay in
//  memory, and the file isn't attached, opened or written.
//
////////////////////////////////////////////////////////////////////////////////

class IReplayMedia
{
public:
    virtual ~IReplayMedia() = default;

    virtual HRESULT  CopyForReplay   (ReplayBlockMedia & outCopy)      = 0;
    virtual HRESULT  MountReplayCopy (const ReplayBlockMedia & copy)   = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IReplayMediaSource
//
//  A card or port whose drives have disks a replay copies. GetReplayMedia
//  appends the drives, in an order two cards built from one configuration
//  share. The state of a part that implements it is passed over by a load
//  for a picture, which has no disks.
//
////////////////////////////////////////////////////////////////////////////////

class IReplayMediaSource
{
public:
    virtual ~IReplayMediaSource() = default;

    virtual void  GetReplayMedia (std::vector<IReplayMedia *> & outDrives) = 0;
};
