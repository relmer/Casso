#pragma once

#include "Pch.h"

#include "Machines/IMachine.h"
#include "Machines/Apple2/Common/WozCompatibility.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3
//
//  The Apple /// of 1980: a 6502B at 2 MHz with extended addressing, 128K
//  or 256K of banked RAM behind the memory map, four peripheral slots, the
//  built-in disk drive and up to three external drives, the RAM character
//  set and seven video modes. Not an Apple II, so not on the Apple II chain.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3 : public IMachine
{
public:
    std::string  GetId              () const override { return ("Apple3"); }
    std::string  GetCpu             () const override { return ("6502"); }
    std::string  GetCpuManufacturer () const override { return ("Synertek"); }
    std::string  GetKeyboardLayout  () const override { return ("apple3-layout"); }

    std::vector<RamRegion>       GetRam             () const override { return {}; }
    std::vector<InternalDevice>  GetInternalDevices () const override { return {}; }
    std::vector<std::string>     GetVideoModes      () const override;
    std::vector<std::string>     GetMemoryOptions   () const override { return { "128K", "256K" }; }

    int   GetSlotCount            () const override { return (4); }
    int   GetGamePortAxisCount    () const override { return (4); }
    bool  HasGamePortDevice       () const override { return (false); }
    bool  HasAnnunciators         () const override { return (false); }
    bool  HasCassettePort         () const override { return (false); }
    bool  HasCaseSwitches         () const override { return (false); }
    bool  HasBuiltInDrive         () const override { return (true); }
    bool  HasBankedMemory         () const override { return (true); }
    int   GetBuiltInDriveCount    () const override { return (1); }
    int   GetMaxExternalDrives    () const override { return (3); }
    int   GetDiskPortFloppyCount  () const override { return (0); }
    int   GetDiskPortApple35Count () const override { return (0); }

    uint16_t  GetWozHardwareFlag () const override { return (WozCompatibility::kApple3); }
};
