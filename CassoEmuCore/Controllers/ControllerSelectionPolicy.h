#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionChangeReason
//
//  Why the policy changed the selection, which decides what the user is told.
//
////////////////////////////////////////////////////////////////////////////////

enum class SelectionChangeReason
{
    None,
    AutomaticSelection,   // nothing was selected and a controller is attached
    Adoption,             // the selected unit returned under a different identity
    Replacement,          // the selected controller left and an attached one took over
    Cleared               // the selected controller left and nothing is attached
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerSelectionPolicy
//
//  Which controller drives the game port, given what is attached and what was
//  chosen before. Pure: it reads no device and writes nothing, so every rule
//  in it is reachable from the unit tests with a list of made-up devices.
//
//  THE SELECTION STAYS ON THE CONTROLLER IN USE until that controller goes or
//  the user picks another (FR-008a). Keeping an absent one chosen, with
//  another driving until it returned, left the user picking an entry the
//  picker already showed checked to keep the controller that had taken over.
//
////////////////////////////////////////////////////////////////////////////////

class ControllerSelectionPolicy
{
public:

    struct Decision
    {
        std::optional<ControllerUnitKey>  selection;
        SelectionChangeReason             reason                = SelectionChangeReason::None;
        bool                              hasChanged            = false;
        bool                              clearsOtherInputModes = false;
        std::wstring                      description;

        // The controller that left, when that is what moved the selection.
        std::wstring                      departedDescription;

        // Whether the user hears about it. A saved controller that is not
        // plugged in at launch was never driving anything, so dropping it is
        // not news.
        bool                              isAnnounced           = true;
    };

    static Decision  Evaluate           (const std::optional<ControllerUnitKey> &   current,
                                         const std::vector<ControllerDeviceInfo> &  devices,
                                         bool                                       hasGamePort);

    static bool      IsSelectedAttached (const std::optional<ControllerUnitKey> &   selection,
                                         const std::vector<ControllerDeviceInfo> &  devices);

private:

    static const ControllerDeviceInfo *  FindUnit          (const std::vector<ControllerDeviceInfo> &  devices,
                                                            const ControllerUnitKey &                  unit);

    static const ControllerDeviceInfo *  FindSoleSameModel (const std::vector<ControllerDeviceInfo> &  devices,
                                                            const ControllerModelKey &                 model);
};
