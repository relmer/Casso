#include "Pch.h"

#include "Machines/MachineDefinitions.h"

#include "Machines/Apple2/Apple2/Apple2.h"
#include "Machines/Apple2/Apple2Plus/Apple2Plus.h"
#include "Machines/Apple2/Apple2e/Apple2e.h"
#include "Machines/Apple2/Apple2eEnhanced/Apple2eEnhanced.h"
#include "Machines/Apple2/Apple2c/Apple2c.h"


//
//  Every machine this build can be, in the order they were built.
//
//  Held as the concrete types rather than as IMachine pointers so the chain is
//  visible here: Apple2Plus is an Apple2, Apple2e is an Apple2Plus, and the
//  //c and the Enhanced //e are siblings off the //e.
//
static const Apple2           s_kApple2;
static const Apple2Plus       s_kApple2Plus;
static const Apple2e          s_kApple2e;
static const Apple2c          s_kApple2c;
static const Apple2eEnhanced  s_kApple2eEnhanced;

static const IMachine * const  s_kpAllMachines[] =
{
    &s_kApple2,
    &s_kApple2Plus,
    &s_kApple2e,
    &s_kApple2c,
    &s_kApple2eEnhanced
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDefinitions::FindMachine
//
//  Looks a model up by the id its resource directory uses. Returns nullptr
//  rather than a default for an unknown id: a machine this program does not
//  know how to be is an error the caller has to face, not a ][ in disguise.
//
////////////////////////////////////////////////////////////////////////////////

const IMachine * MachineDefinitions::FindMachine (const std::string & machineId)
{
    const IMachine *  found = nullptr;



    for (const IMachine * candidate : s_kpAllMachines)
    {
        if (candidate->GetId() == machineId)
        {
            found = candidate;
            break;
        }
    }

    return (found);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDefinitions::Find
//
//  The machine's answers, flattened into the record the config loader and the
//  settings pages consume.
//
//  Flattened for every machine on the first lookup and never again, because
//  callers hold the pointer past the call, a machine's answers never change,
//  and the CPU and UI threads both look up here: a table built once under
//  the static initializer is read-only from then on.
//
////////////////////////////////////////////////////////////////////////////////

const MachineDefinition * MachineDefinitions::Find (const std::string & machineId)
{
    static const std::map<std::string, MachineDefinition>  s_table = BuildTable();
    auto                                                   known   = s_table.find (machineId);



    if (known == s_table.end())
    {
        return (nullptr);
    }

    return (&known->second);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDefinitions::BuildTable
//
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, MachineDefinition> MachineDefinitions::BuildTable()
{
    std::map<std::string, MachineDefinition>  table;



    for (const IMachine * machine : s_kpAllMachines)
    {
        MachineDefinition  definition;

        definition.id              = machine->GetId();
        definition.cpu             = machine->GetCpu();
        definition.cpuManufacturer = machine->GetCpuManufacturer();
        definition.ram             = machine->GetRam();
        definition.internalDevices = machine->GetInternalDevices();
        definition.videoModes      = machine->GetVideoModes();
        definition.keyboardType    = machine->GetKeyboardLayout();
        definition.slotCount       = machine->GetSlotCount();
        definition.hasGamePort     = machine->HasGamePortDevice();
        definition.hasCaseSwitches = machine->HasCaseSwitches();
        definition.hasBuiltInDrive = machine->HasBuiltInDrive();

        table.emplace (definition.id, std::move (definition));
    }

    return (table);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDefinitions::GetKnownIds
//
//  Every machine this build can be. Used by tests to assert that each shipped
//  resource directory has a machine behind it, so a machine cannot ship as
//  JSON alone.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> MachineDefinitions::GetKnownIds()
{
    std::vector<std::string>  ids;



    for (const IMachine * machine : s_kpAllMachines)
    {
        ids.push_back (machine->GetId());
    }

    return (ids);
}
