#include "Pch.h"

#include "Machines/MachineDefinitions.h"

#include "Machines/Apple2/Apple2/Apple2Definition.h"
#include "Machines/Apple2/Apple2Plus/Apple2PlusDefinition.h"
#include "Machines/Apple2/Apple2e/Apple2eDefinition.h"
#include "Machines/Apple2/Apple2eEnhanced/Apple2eEnhancedDefinition.h"
#include "Machines/Apple2/Apple2c/Apple2cDefinition.h"





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

    static const MachineDefinition * const  s_kpAll[] =
    {
        &Apple2Definition::Get(),
        &Apple2PlusDefinition::Get(),
        &Apple2eDefinition::Get(),
        &Apple2eEnhancedDefinition::Get(),
        &Apple2cDefinition::Get()
    };

    for (const MachineDefinition * candidate : s_kpAll)
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
    std::vector<std::string>  ids = { Apple2Definition::Get().id,
                                      Apple2PlusDefinition::Get().id,
                                      Apple2eDefinition::Get().id,
                                      Apple2eEnhancedDefinition::Get().id,
                                      Apple2cDefinition::Get().id };

    return (ids);
}
