#include "Pch.h"

#include "Controllers/ControllerSelectionPolicy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Evaluate
//
//  The whole selection rule in one pass over what is attached, which the
//  caller lists LONGEST-ATTACHED FIRST.
//
//  With nothing selected, the first controller listed takes the game port and
//  the arrow keys and paddle give it up, so plugging a controller in is all a
//  user has to do (FR-032). With the selected controller attached, nothing
//  changes: the selection stays on the controller in use, and one arriving
//  does not take it.
//
//  With the selected controller gone, the selection moves (FR-008a). A
//  DirectInput unit that moved to another port comes back under a different
//  identity, so exactly one attached unit of its model is taken to be it;
//  with two of that model, which one moved is a coin flip, and they count as
//  any other controller. Otherwise the longest-attached controller takes over,
//  and with none attached the selection is cleared. A controller returning
//  later is only a controller arriving.
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
    if (current.value().model.kind == ControllerKind::DirectInput)
    {
        found = FindSoleSameModel (devices, current.value().model);
    }

    decision.hasChanged = true;

    if (found != nullptr)
    {
        decision.selection   = found->unit;
        decision.description = found->description;
        decision.reason      = SelectionChangeReason::Adoption;

        return decision;
    }

    if (devices.empty())
    {
        decision.selection = std::nullopt;
        decision.reason    = SelectionChangeReason::Cleared;

        return decision;
    }

    decision.selection   = devices.front().unit;
    decision.description = devices.front().description;
    decision.reason      = SelectionChangeReason::Replacement;

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
