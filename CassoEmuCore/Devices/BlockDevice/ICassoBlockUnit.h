#pragma once

#include "Pch.h"

#include "Devices/BlockDevice/BlockDeviceEvent.h"
#include "Devices/BlockDevice/BlockDeviceMedia.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ICassoBlockUnit
//
//  A drive that Casso's block firmware reads and writes directly: its image,
//  and a record of each access for the drive's lamp, sounds and event log.
//
////////////////////////////////////////////////////////////////////////////////

class ICassoBlockUnit
{
public:
    virtual ~ICassoBlockUnit() = default;

    virtual BlockDeviceMedia  & GetMedia         () = 0;
    virtual void                RecordFastAccess (BlockDeviceOp op, uint32_t block, Byte result, uint64_t now) = 0;
};
