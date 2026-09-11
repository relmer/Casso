#include "Pch.h"

#include "ComponentRegistry.h"
#include "MachineConfig.h"
#include "../Devices/RamDevice.h"
#include "../Devices/RomDevice.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "../Devices/Acia6551.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Machines/Apple2/Common/MockingboardCard.h"





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
    auto  it = s_factories.find (typeName);



    // Null for an unregistered type name; the caller reports it against the
    // machine config that named it.
    return (it != s_factories.end()) ? it->second (config, bus)
                                     : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsRegistered
//
////////////////////////////////////////////////////////////////////////////////

bool ComponentRegistry::IsRegistered (const string & typeName) const
{
    return s_factories.find (typeName) != s_factories.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRegisteredTypes
//
////////////////////////////////////////////////////////////////////////////////

vector<string> ComponentRegistry::GetRegisteredTypes() const
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
    registry.Register ("apple2-family-keyboard",      AppleKeyboard::Create);
    registry.Register ("apple2e-family-keyboard",     Apple2eKeyboard::Create);
    registry.Register ("apple2-family-speaker",       AppleSpeaker::Create);
    registry.Register ("apple2-family-softswitches",  AppleSoftSwitchBank::Create);
    registry.Register ("apple2-family-gameport",      AppleGamePort::Create);
    registry.Register ("apple2e-family-softswitches", Apple2eSoftSwitchBank::Create);
    registry.Register ("language-card",        LanguageCard::Create);
    registry.Register ("disk-ii",              Disk2Controller::Create);
    registry.Register ("acia-6551",            Acia6551::Create);
    registry.Register ("parallel-printer",     PrinterCard::Create);
    registry.Register ("mockingboard",         MockingboardCard::Create);
    registry.Register ("mockingboard-c",       MockingboardCard::CreateSpeech);
}
