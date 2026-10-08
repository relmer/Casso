#include "Pch.h"

#include "Machines/Apple2/Common/WozCompatibility.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Machines/MachineDefinitions.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HardwareLabel
//
//  How the tooltip writes each model. The first one listed takes the "Apple"
//  in front of it, so the list reads the way the machines were sold.
//
////////////////////////////////////////////////////////////////////////////////

struct WozCompatibility::HardwareLabel
{
    uint16_t  flag  = 0;
    LPCWSTR   label = nullptr;
};


static constexpr WozCompatibility::HardwareLabel  s_kHardwareLabels[] =
{
    { WozCompatibility::kApple2,          L"][" },
    { WozCompatibility::kApple2Plus,      L"][+" },
    { WozCompatibility::kApple2e,         L"//e" },
    { WozCompatibility::kApple2c,         L"//c" },
    { WozCompatibility::kApple2eEnhanced, L"//e Enhanced" },
    { WozCompatibility::kApple2gs,        L"IIgs" },
    { WozCompatibility::kApple2cPlus,     L"//c+" },
    { WozCompatibility::kApple3,          L"///" },
    { WozCompatibility::kApple3Plus,      L"///+" },
};





////////////////////////////////////////////////////////////////////////////////
//
//  WozCompatibility::ReadRequirements
//
//  INFO version 2 added both fields. An older INFO, or none at all -- a disk
//  Casso built, any non-WOZ image -- declares nothing.
//
////////////////////////////////////////////////////////////////////////////////

WozRequirements WozCompatibility::ReadRequirements (const WozMetadata & meta)
{
    constexpr Byte  kFirstVersionWithFields = 2;
    constexpr int   kHighByteShift          = 8;



    const vector<Byte> &  info      = meta.infoPayload;
    bool                  hasFields = false;
    WozRequirements       declared;



    hasFields = info.size() >= WozLoader::kInfoChunkSize
             && info[WozLoader::kInfoOffsetVersion] >= kFirstVersionWithFields;

    if (hasFields)
    {
        declared.compatibleHardware = static_cast<uint16_t> (info[WozLoader::kInfoOffsetCompatibleHw]
                                                           | (info[WozLoader::kInfoOffsetCompatibleHw + 1] << kHighByteShift));
        declared.requiredRamK       = static_cast<uint16_t> (info[WozLoader::kInfoOffsetRequiredRam]
                                                           | (info[WozLoader::kInfoOffsetRequiredRam + 1] << kHighByteShift));
    }

    return declared;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozCompatibility::GetMachineFacts
//
//  A machine the definitions do not know -- a user's own copy under a new id
//  -- has no hardware flag, so only its RAM can be checked.
//
////////////////////////////////////////////////////////////////////////////////

WozMachineFacts WozCompatibility::GetMachineFacts (const MachineConfig & config)
{
    const MachineDefinition  * definition = MachineDefinitions::Find (config.machineId);
    WozMachineFacts            facts;



    facts.hardwareFlag = (definition != nullptr) ? definition->wozHardwareFlag : 0;
    facts.ramK         = static_cast<uint16_t> (config.GetTotalRamK());

    return facts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozCompatibility::HasHardwareConflict
//
//  The image lists the models it runs on and this machine is not one of them.
//  Either side not saying is no conflict.
//
////////////////////////////////////////////////////////////////////////////////

bool WozCompatibility::HasHardwareConflict (const WozRequirements & declared, const WozMachineFacts & machine)
{
    return declared.compatibleHardware != 0
        && machine.hardwareFlag        != 0
        && (declared.compatibleHardware & machine.hardwareFlag) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozCompatibility::HasRamConflict
//
////////////////////////////////////////////////////////////////////////////////

bool WozCompatibility::HasRamConflict (const WozRequirements & declared, const WozMachineFacts & machine)
{
    return declared.requiredRamK != 0
        && machine.ramK          != 0
        && machine.ramK < declared.requiredRamK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozCompatibility::HasConflict
//
////////////////////////////////////////////////////////////////////////////////

bool WozCompatibility::HasConflict (const WozRequirements & declared, const WozMachineFacts & machine)
{
    return HasHardwareConflict (declared, machine) || HasRamConflict (declared, machine);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozCompatibility::FormatHardwareList
//
//  Bits the format has no model for -- reserved today -- are given in hex
//  at the end rather than dropped, since a list that silently lost an entry
//  would misstate what the image says.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WozCompatibility::FormatHardwareList (uint16_t compatibleHardware)
{
    constexpr size_t  kHexBufferChars = 16;



    std::wstring  list;
    uint16_t      unknown              = compatibleHardware;
    wchar_t       hex[kHexBufferChars] = {};



    for (const HardwareLabel & entry : s_kHardwareLabels)
    {
        if ((compatibleHardware & entry.flag) == 0)
        {
            continue;
        }

        list    += list.empty() ? L"Apple " : L", ";
        list    += entry.label;
        unknown  = static_cast<uint16_t> (unknown & ~entry.flag);
    }

    if (unknown != 0)
    {
        swprintf_s (hex, L"0x%04X", unknown);
        list += list.empty() ? L"" : L", ";
        list += hex;
    }

    return list;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozCompatibility::ComposeTooltip
//
//  The first block quotes only what the image declares and the second lists
//  only what conflicts. The wording stays neutral: whoever imaged the disk set
//  these fields, often from the box, and they are often conservative.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WozCompatibility::ComposeTooltip (
    const WozRequirements  & declared,
    const WozMachineFacts  & machine,
    const std::wstring     & machineName)
{
    bool          hardwareConflict = HasHardwareConflict (declared, machine);
    bool          ramConflict      = HasRamConflict      (declared, machine);
    std::wstring  text;



    if (!hardwareConflict && !ramConflict)
    {
        return text;
    }

    text = L"This .woz image specifies:";

    if (declared.compatibleHardware != 0)
    {
        text += L"\n  Compatible with: " + FormatHardwareList (declared.compatibleHardware);
    }

    if (declared.requiredRamK != 0)
    {
        text += L"\n  Minimum RAM: " + std::to_wstring (declared.requiredRamK) + L"K";
    }

    text += L"\n\nThis machine: " + machineName;

    if (machine.ramK != 0)
    {
        text += L", " + std::to_wstring (machine.ramK) + L"K";
    }

    if (hardwareConflict)
    {
        text += L"\n  " + std::wstring (s_kpszWarningSign) + L" Not in the image's compatible list";
    }

    if (ramConflict)
    {
        text += L"\n  " + std::wstring (s_kpszWarningSign) + L" Less RAM than the image's minimum";
    }

    return text;
}
