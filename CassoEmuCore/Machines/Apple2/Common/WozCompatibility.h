#pragma once

#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Machines/Apple2/Common/WozMetadata.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WozRequirements
//
//  What a WOZ image's INFO chunk says about the machine it wants: the models
//  it runs on, as a bitfield, and the least RAM it needs, in K. Zero in either
//  field means the image does not say.
//
////////////////////////////////////////////////////////////////////////////////

struct WozRequirements
{
    uint16_t  compatibleHardware = 0;
    uint16_t  requiredRamK       = 0;

    bool  operator== (const WozRequirements & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  WozMachineFacts
//
//  The running machine, as the compatibility check sees it. A hardware flag
//  of zero means the format has no bit for this machine, and a RAM total of
//  zero means it is not known.
//
////////////////////////////////////////////////////////////////////////////////

struct WozMachineFacts
{
    uint16_t  hardwareFlag = 0;
    uint16_t  ramK         = 0;

    bool  operator== (const WozMachineFacts & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  WozCompatibility
//
//  Compares what a WOZ image declares against the machine running it, and
//  writes the text the drive's info icon shows.
//
////////////////////////////////////////////////////////////////////////////////

class WozCompatibility
{
public:
    //  The bits of INFO's compatible-hardware field, one per model.
    static constexpr uint16_t  kApple2          = 0x0001;
    static constexpr uint16_t  kApple2Plus      = 0x0002;
    static constexpr uint16_t  kApple2e         = 0x0004;
    static constexpr uint16_t  kApple2c         = 0x0008;
    static constexpr uint16_t  kApple2eEnhanced = 0x0010;
    static constexpr uint16_t  kApple2gs        = 0x0020;
    static constexpr uint16_t  kApple2cPlus     = 0x0040;
    static constexpr uint16_t  kApple3          = 0x0080;
    static constexpr uint16_t  kApple3Plus      = 0x0100;

    static WozRequirements  ReadRequirements    (const WozMetadata & meta);
    static WozMachineFacts  GetMachineFacts     (const MachineConfig & config);
    static bool             HasHardwareConflict (const WozRequirements & declared, const WozMachineFacts & machine);
    static bool             HasRamConflict      (const WozRequirements & declared, const WozMachineFacts & machine);
    static bool             HasConflict         (const WozRequirements & declared, const WozMachineFacts & machine);

    //  "Apple ][, ][+" -- the models a compatible-hardware field gives.
    static std::wstring     FormatHardwareList  (uint16_t compatibleHardware);

    //  The info icon's tooltip, or an empty string when nothing conflicts.
    static std::wstring     ComposeTooltip      (const WozRequirements  & declared,
                                                 const WozMachineFacts  & machine,
                                                 const std::wstring     & machineName);

    //  One model's bit and how the tooltip writes it.
    struct HardwareLabel;

private:
    static std::wstring     GetListSeparator    (const std::wstring & list);
    static LPCWSTR          GetVerdictMark      (bool isKnown, bool hasConflict);
};
