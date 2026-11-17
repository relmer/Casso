#pragma once

#include "Pch.h"

#include "Devices/Disk/IDiskImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayBlockMedia
//
//  A hard disk, ProFile or 3.5-inch disk as a replay on a second machine
//  needs it: its path, its write protection and its content as it was when
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
//  memory, and the file isn't claimed, opened or written.
//
////////////////////////////////////////////////////////////////////////////////

class IReplayMedia
{
public:
    virtual ~IReplayMedia() = default;

    virtual HRESULT  CopyForReplay   (ReplayBlockMedia & outCopy)      = 0;
    virtual HRESULT  MountReplayCopy (const ReplayBlockMedia & copy)   = 0;
};
