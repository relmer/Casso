#pragma once

#include "Pch.h"

class IDebugTarget;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskValueSymbols
//
//  The Disk II controller's state as expression symbols: DISK.DRIVE,
//  DISK.QTRACK, DISK.QTRACK1, DISK.QTRACK2, DISK.MOTOR, DISK.WRITING,
//  DISK.PROTECTED, DISK.INSERTED, and DISK.ATRACK, DISK.ASECTOR and
//  DISK.AVOLUME from the selected drive's last address field with a good
//  checksum in the nibble record, $FFFF when there is none. Everything is
//  read from the controller's getters and its record, never through the
//  bus.
//
////////////////////////////////////////////////////////////////////////////////

class DiskValueSymbols
{
public:
    static constexpr Word  kNoField = 0xFFFF;

    //  Whether the name is one of the values, in any case.
    static bool  IsDiskValue (const std::string & name);

    //  The value, or false on a machine with no controller or for a name
    //  that is not a disk value.
    static bool  TryResolve  (const std::string & name, const IDebugTarget & target, Word & value);

private:
    struct AddressField
    {
        Word  volume = kNoField;
        Word  track  = kNoField;
        Word  sector = kNoField;
    };

    static AddressField  FindLastAddressField (const IDebugTarget & target, int drive);
    static std::string   ToUpper              (const std::string & name);
};
