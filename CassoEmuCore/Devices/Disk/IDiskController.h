#pragma once

#include "Pch.h"

class IDiskImage;
class IDiskImageStore;
class RomDevice;





////////////////////////////////////////////////////////////////////////////////
//
//  IDiskController
//
//  Nibble-level Disk II controller per slot. Soft-switch surface at
//  $C0E0+slot*16 .. $C0EF+slot*16; ROM at $C600+slot*$100 (one page).
//  Reads return the current data latch when in read mode (Q6=0, Q7=0);
//  writes to the appropriate switch enter write mode and stream bits
//  through the nibble engine.
//
////////////////////////////////////////////////////////////////////////////////

class IDiskController
{
public:
    virtual              ~IDiskController () = default;

    virtual HRESULT       Initialize (int slot, IDiskImageStore * store) = 0;
    virtual HRESULT       MountDrive (int drive, IDiskImage * image) = 0;
    virtual HRESULT       EjectDrive (int drive) = 0;

    virtual uint8_t       ReadSoftSwitch  (uint8_t addrLow) = 0;
    virtual void          WriteSoftSwitch (uint8_t addrLow, uint8_t value) = 0;

    virtual void          MapRom (RomDevice * slotRom) = 0;

    virtual void          Tick (uint32_t cpuCycles) = 0;
};
