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





////////////////////////////////////////////////////////////////////////////////
//
//  Private data
//
////////////////////////////////////////////////////////////////////////////////

static std::unordered_map<std::string, FactoryFunc> s_factories;





////////////////////////////////////////////////////////////////////////////////
//
//  Register
//
////////////////////////////////////////////////////////////////////////////////

void ComponentRegistry::Register (const std::string & typeName, FactoryFunc factory)
{
    s_factories[typeName] = std::move (factory);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<MemoryDevice> ComponentRegistry::Create (
    const std::string & typeName,
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

bool ComponentRegistry::IsRegistered (const std::string & typeName) const
{
    return s_factories.find (typeName) != s_factories.end ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRegisteredTypes
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ComponentRegistry::GetRegisteredTypes () const
{
    std::vector<std::string> types;

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
    registry.Register ("apple2-keyboard",     AppleKeyboard::Create);
    registry.Register ("apple2-speaker",      AppleSpeaker::Create);
    registry.Register ("apple2-softswitches", AppleSoftSwitchBank::Create);
    registry.Register ("language-card",       LanguageCard::Create);
    registry.Register ("disk-ii",             DiskIIController::Create);
}
