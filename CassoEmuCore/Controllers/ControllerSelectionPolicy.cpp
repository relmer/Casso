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
//  A CONTROLLER THAT HOLDS AXES OF ITS OWN IS CHOSEN LAST. It is already
//  driving its own axes, so a free controller is the better stand-in; and
//  when one is chosen anyway its own axes come with it rather than PDL0 and
//  PDL1, so the player still on the machine does not move (SC-012).
//
////////////////////////////////////////////////////////////////////////////////

ControllerSelectionPolicy::Decision ControllerSelectionPolicy::Evaluate (
    const std::optional<ControllerUnitKey> &      current,
    const std::vector<ControllerDeviceInfo> &     devices,
    bool                                          hasGamePort,
    const std::vector<ControllerAxisAssignment> & assignments)
{
    Decision                      decision;
    const ControllerDeviceInfo *  found     = nullptr;
    const ControllerDeviceInfo *  preferred = nullptr;



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

        preferred                      = &GetPreferredDevice (devices, assignments);
        decision.selection             = preferred->unit;
        decision.description           = preferred->description;
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

    preferred            = &GetPreferredDevice (devices, assignments);
    decision.selection   = preferred->unit;
    decision.description = preferred->description;
    decision.reason      = SelectionChangeReason::Replacement;

    return decision;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssignAxes
//
//  Each axis has one owner, so the axes handed to this unit leave every other
//  entry. The selected controller's default PDL0 and PDL1 need no edit: they
//  are computed less whatever the entries hold.
//
//  An entry left holding nothing is kept. A controller whose last axis was
//  taken stays off the game port until it is given one, rather than falling
//  back to a default nobody chose for it.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerSelectionPolicy::AssignAxes (
    std::vector<ControllerAxisAssignment> & assignments,
    const ControllerUnitKey &               unit,
    ControllerAxisAssignment::AxisSet       axes)
{
    bool  isFound = false;



    for (ControllerAxisAssignment & entry : assignments)
    {
        if (entry.unit == unit)
        {
            entry.axes = axes;
            isFound    = true;
        }
        else
        {
            entry.axes &= ~axes;
        }
    }

    if (!isFound)
    {
        assignments.push_back ({ unit, axes });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAxesFor
//
////////////////////////////////////////////////////////////////////////////////

ControllerAxisAssignment::AxisSet ControllerSelectionPolicy::GetAxesFor (
    const std::vector<ControllerAxisAssignment> & assignments,
    const ControllerUnitKey &                     unit,
    const std::optional<ControllerUnitKey> &      selection,
    size_t                                        axisCount)
{
    ControllerAxisAssignment::AxisSet  axes;
    ControllerAxisAssignment::AxisSet  heldByOthers;
    bool                               isFound      = false;
    size_t                             axis         = 0;



    for (const ControllerAxisAssignment & entry : assignments)
    {
        if (entry.unit == unit)
        {
            axes    = entry.axes;
            isFound = true;
        }
        else
        {
            heldByOthers |= entry.axes;
        }
    }

    if (!isFound && selection.has_value() && selection.value() == unit)
    {
        axes = ControllerAxisAssignment::AxisSet (kDefaultAxisBits) & ~heldByOthers;
    }

    for (axis = axisCount; axis < axes.size(); axis++)
    {
        axes.reset (axis);
    }

    return axes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasAssignment
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerSelectionPolicy::HasAssignment (
    const std::vector<ControllerAxisAssignment> & assignments,
    const ControllerUnitKey &                     unit)
{
    for (const ControllerAxisAssignment & entry : assignments)
    {
        if (entry.unit == unit)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPreferredDevice
//
//  The first listed controller with no axes of its own, or the first listed
//  when every one has some. The caller guarantees the list is not empty.
//
////////////////////////////////////////////////////////////////////////////////

const ControllerDeviceInfo & ControllerSelectionPolicy::GetPreferredDevice (
    const std::vector<ControllerDeviceInfo> &     devices,
    const std::vector<ControllerAxisAssignment> & assignments)
{
    for (const ControllerDeviceInfo & device : devices)
    {
        if (!HasAssignment (assignments, device.unit))
        {
            return device;
        }
    }

    return devices.front();
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
