#include "Pch.h"

#include "ComponentRegistry.h"
#include "MachineConfig.h"
#include "../Devices/RamDevice.h"
#include "../Devices/RomDevice.h"
#include "../Devices/AppleKeyboard.h"
#include "../Devices/AppleSoftSwitchBank.h"
#include "../Devices/AppleSpeaker.h"
#include "../Devices/LanguageCard.h"
#include "../Devices/DiskIIController.h"
#include "../Devices/AppleIIeKeyboard.h"
#include "../Devices/AuxRamCard.h"
#include "../Devices/AppleIIeSoftSwitchBank.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Private data
//
////////////////////////////////////////////////////////////////////////////////

static unordered_map<string, FactoryFunc> s_factories;





////////////////////////////////////////////////////////////////////////////////
//
//  Register
//
////////////////////////////////////////////////////////////////////////////////

void ComponentRegistry::Register (const string & typeName, FactoryFunc factory)
{
    s_factories[typeName] = move (factory);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> ComponentRegistry::Create (
    const string & typeName,
    const DeviceConfig & config,
    MemoryBus & bus) const
{
    auto it = s_factories.find (typeName);

    if (it == s_factories.end ())
    {
        return nullptr;
    }

    return it->second (config, bus);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsRegistered
//
////////////////////////////////////////////////////////////////////////////////

bool ComponentRegistry::IsRegistered (const string & typeName) const
{
    return s_factories.find (typeName) != s_factories.end ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRegisteredTypes
//
////////////////////////////////////////////////////////////////////////////////

vector<string> ComponentRegistry::GetRegisteredTypes () const
{
    vector<string> types;

    for (const auto & pair : s_factories)
    {
        types.push_back (pair.first);
    }

    return types;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterBuiltinDevices
//
//  Placeholder — populated in later phases as device classes are implemented.
//
////////////////////////////////////////////////////////////////////////////////

void ComponentRegistry::RegisterBuiltinDevices (ComponentRegistry & registry)
{
    registry.Register ("apple2-keyboard",      AppleKeyboard::Create);
    registry.Register ("apple2e-keyboard",     AppleIIeKeyboard::Create);
    registry.Register ("apple2-speaker",       AppleSpeaker::Create);
    registry.Register ("apple2-softswitches",  AppleSoftSwitchBank::Create);
    registry.Register ("apple2e-softswitches", AppleIIeSoftSwitchBank::Create);
    registry.Register ("aux-ram-card",         AuxRamCard::Create);
    registry.Register ("language-card",        LanguageCard::Create);
    registry.Register ("disk-ii",              DiskIIController::Create);
}
