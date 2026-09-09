#include "Pch.h"

#include "Machines/MachineDefinitions.h"

#include "Machines/Apple2/Apple2/Apple2Definition.h"
#include "Machines/Apple2/Apple2Plus/Apple2PlusDefinition.h"
#include "Machines/Apple2/Apple2e/Apple2eDefinition.h"
#include "Machines/Apple2/Apple2eEnhanced/Apple2eEnhancedDefinition.h"
#include "Machines/Apple2/Apple2c/Apple2cDefinition.h"


//
//  Every machine this build can be. Each Get() hands back a function-local
//  static, so taking its address here is safe whatever order translation units
//  initialize in.
//
static const MachineDefinition * const  s_kpAllMachines[] =
{
    &Apple2Definition::Get(),
    &Apple2PlusDefinition::Get(),
    &Apple2eDefinition::Get(),
    &Apple2eEnhancedDefinition::Get(),
    &Apple2cDefinition::Get()
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDefinitions::Find
//
//  Looks a model up by the id its resource directory uses. Returns nullptr
//  rather than a default for an unknown id: a machine this program does not
//  know how to be is an error the caller has to face, not a ][ in disguise.
//
////////////////////////////////////////////////////////////////////////////////

const MachineDefinition * MachineDefinitions::Find (const std::string & machineId)
{
    const MachineDefinition *  found = nullptr;



    for (const MachineDefinition * candidate : s_kpAllMachines)
    {
        if (candidate->id == machineId)
        {
            found = candidate;
            break;
        }
    }

    return (found);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDefinitions::GetKnownIds
//
//  Every machine this build can be. Used by tests to assert that each shipped
//  resource directory has a definition behind it, so a machine cannot ship as
//  JSON alone.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> MachineDefinitions::GetKnownIds()
{
    std::vector<std::string>  ids;



    ids = { Apple2Definition::Get().id,
            Apple2PlusDefinition::Get().id,
            Apple2eDefinition::Get().id,
            Apple2eEnhancedDefinition::Get().id,
            Apple2cDefinition::Get().id };

    return (ids);
}
