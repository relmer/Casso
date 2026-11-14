#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ScsiStatus
//
////////////////////////////////////////////////////////////////////////////////

enum class ScsiStatus : Byte
{
    Good           = 0x00,
    CheckCondition = 0x02,
    Busy           = 0x08,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ScsiCommandResult
//
//  What a target does with a command: the bytes it returns in the data in
//  phase, or the number it takes in the data out phase, the status it
//  returns when there is no data out, and how long the command takes before
//  its first byte or its status, in CPU cycles.
//
////////////////////////////////////////////////////////////////////////////////

struct ScsiCommandResult
{
    ScsiStatus         status        = ScsiStatus::Good;
    std::vector<Byte>  dataIn;
    size_t             dataOutLength = 0;
    uint64_t           delayCycles   = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IScsiTarget
//
//  A SCSI target as the bus sees it. The bus runs the phases and the
//  handshake; the target executes commands.
//
////////////////////////////////////////////////////////////////////////////////

class IScsiTarget
{
public:
    virtual ~IScsiTarget() = default;

    virtual int   GetScsiId    () const = 0;

    //  False for a target that does not answer selection, such as a drive
    //  with no image.
    virtual bool  IsSelectable () const = 0;

    virtual void  BeginCommand  (std::span<const Byte> cdb, uint64_t now, ScsiCommandResult & outResult) = 0;

    //  The bytes of the data out phase, once all have arrived. Returns the
    //  status, and the delay before it in `outDelayCycles`.
    virtual ScsiStatus  FinishDataOut (std::span<const Byte> data, uint64_t now, uint64_t & outDelayCycles) = 0;

    //  The bus's RST line, or a power cycle.
    virtual void  ResetTarget () = 0;
};
