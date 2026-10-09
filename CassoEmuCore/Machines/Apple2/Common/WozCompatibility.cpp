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
    WozInfo          info;
    WozRequirements  declared;



    WozLoader::ReadInfo (meta.infoPayload, info);

    if (info.hasVersion2Fields)
    {
        declared.compatibleHardware = info.compatibleHardware;
        declared.requiredRamK       = info.requiredRamK;
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

        list    += list.empty() ? std::wstring (L"Apple ") : GetListSeparator (list);
        list    += entry.label;
        unknown  = static_cast<uint16_t> (unknown & ~entry.flag);
    }

    if (unknown != 0)
    {
        swprintf_s (hex, L"0x%04X", unknown);
        list += list.empty() ? std::wstring() : GetListSeparator (list);
        list += hex;
    }

    return list;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozCompatibility::GetListSeparator
//
//  A comma straight after "][" tucks under the bracket's foot, which then looks
//  bent up, so a thin space holds the comma off.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WozCompatibility::GetListSeparator (const std::wstring & list)
{
    std::wstring  separator = L", ";



    if (!list.empty() && list.back() == L'[')
    {
        separator.insert (separator.begin(), s_kchThinSpace);
    }

    return separator;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozCompatibility::ComposeTooltip
//
//  The first block quotes only what the image declares. The second gives this
//  machine's value for each of those requirements, marked as meeting it or
//  not. The wording stays neutral: whoever imaged the disk set these fields,
//  often from the box, and they are often conservative.
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

    text += L"\n\nThis machine:";

    // The name is always at hand, but a machine the format has no bit for
    // cannot be judged against the list.
    if (declared.compatibleHardware != 0)
    {
        text += L"\n  " + std::wstring (GetVerdictMark (machine.hardwareFlag != 0, hardwareConflict)) + L" " + machineName;
    }

    // A RAM total that is not known has no value to show.
    if (declared.requiredRamK != 0 && machine.ramK != 0)
    {
        text += L"\n  " + std::wstring (GetVerdictMark (true, ramConflict)) + L" RAM " + std::to_wstring (machine.ramK) + L"K";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WozCompatibility::GetVerdictMark
//
//  A green check where the machine meets the requirement, a red X where it
//  does not, and a question mark where the machine's side is not known.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR WozCompatibility::GetVerdictMark (bool isKnown, bool hasConflict)
{
    LPCWSTR  mark = s_kpszGreenCheck;



    if (!isKnown)
    {
        mark = s_kpszGrayQuestion;
    }
    else if (hasConflict)
    {
        mark = s_kpszRedCross;
    }

    return mark;
}
