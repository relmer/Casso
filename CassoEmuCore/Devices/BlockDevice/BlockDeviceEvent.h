#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDeviceKind
//
////////////////////////////////////////////////////////////////////////////////

enum class BlockDeviceKind : uint8_t
{
    Scsi,
    ProFile,
    SmartPort,
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDeviceOp
//
////////////////////////////////////////////////////////////////////////////////

enum class BlockDeviceOp : uint8_t
{
    Status,
    Read,
    Write,
    Format,
    Control,
    Init,
    SpinUp,
    Seek,
    Attach,
    Eject,
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDeviceEvent
//
//  One completed operation of a hard disk, ProFile or SmartPort device. The
//  result is the device's error code, 0 for success.
//
////////////////////////////////////////////////////////////////////////////////

struct BlockDeviceEvent
{
    uint64_t         cycle      = 0;
    BlockDeviceKind  kind       = BlockDeviceKind::Scsi;
    Byte             slot       = 0;
    Byte             unit       = 0;
    BlockDeviceOp    op         = BlockDeviceOp::Status;
    uint32_t         firstBlock = 0;
    uint32_t         blockCount = 0;
    Byte             result     = 0;
};
