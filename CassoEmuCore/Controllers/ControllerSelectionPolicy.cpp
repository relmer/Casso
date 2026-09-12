#include "Pch.h"

#include "Controllers/ControllerSelectionPolicy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Evaluate
//
//  The whole selection rule in one pass over what is attached.
//
//  Three cases, in order. With nothing selected, the first controller found
//  takes the game port and the arrow keys and paddle give it up, so plugging a
//  controller in is all a user has to do (FR-032). With the selected
//  controller attached, nothing changes -- it simply resumes. With the
//  selected controller absent, the only thing that may change the selection is
//  adoption: a DirectInput unit that moved to another port comes back with a
//  different identity, so exactly one attached unit of the same model is taken
//  to be it. Two of that model attached and nothing is adopted, because
//  guessing which one the user meant would be a coin flip.
//
////////////////////////////////////////////////////////////////////////////////

ControllerSelectionPolicy::Decision ControllerSelectionPolicy::Evaluate (
    const std::optional<ControllerUnitKey> &   current,
    const std::vector<ControllerDeviceInfo> &  devices,
    bool                                       hasGamePort)
{
    Decision                      decision;
    const ControllerDeviceInfo *  found = nullptr;



    decision.selection = current;

    // A machine with no game port keeps the selection and does nothing with
    // it, so switching back to one that has a port finds the choice intact.
    if (!hasGamePort)
    {
        return decision;
    }

    if (!current.has_value())
    {
        if (devices.empty())
        {
            return decision;
        }

        decision.selection             = devices.front().unit;
        decision.description           = devices.front().description;
        decision.reason                = SelectionChangeReason::AutomaticSelection;
        decision.hasChanged            = true;
        decision.clearsOtherInputModes = true;

        return decision;
    }

    if (FindUnit (devices, current.value()) != nullptr)
    {
        return decision;
    }

    // Xbox-class controllers are recognized by model alone (FR-018a), so a
    // selection of one already matched above against whichever unit is
    // attached. Only DirectInput units can go missing while their replacement
    // is present under another identity.
    if (current.value().model.kind != ControllerKind::DirectInput)
    {
        return decision;
    }

    found = FindSoleSameModel (devices, current.value().model);

    if (found == nullptr)
    {
        return decision;
    }

    decision.selection   = found->unit;
    decision.description = found->description;
    decision.reason      = SelectionChangeReason::Adoption;
    decision.hasChanged  = true;

    return decision;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSelectedAttached
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerSelectionPolicy::IsSelectedAttached (
    const std::optional<ControllerUnitKey> &   selection,
    const std::vector<ControllerDeviceInfo> &  devices)
{
    if (!selection.has_value())
    {
        return false;
    }

    return FindUnit (devices, selection.value()) != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindUnit
//
////////////////////////////////////////////////////////////////////////////////

const ControllerDeviceInfo * ControllerSelectionPolicy::FindUnit (
    const std::vector<ControllerDeviceInfo> &  devices,
    const ControllerUnitKey &                  unit)
{
    for (const ControllerDeviceInfo & device : devices)
    {
        if (device.unit == unit)
        {
            return &device;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSoleSameModel
//
//  The one attached unit of this model, or null when there is none or more
//  than one.
//
////////////////////////////////////////////////////////////////////////////////

const ControllerDeviceInfo * ControllerSelectionPolicy::FindSoleSameModel (
    const std::vector<ControllerDeviceInfo> &  devices,
    const ControllerModelKey &                 model)
{
    const ControllerDeviceInfo *  found = nullptr;



    for (const ControllerDeviceInfo & device : devices)
    {
        if (!(device.unit.model == model))
        {
            continue;
        }

        if (found != nullptr)
        {
            return nullptr;
        }

        found = &device;
    }

    return found;
}
