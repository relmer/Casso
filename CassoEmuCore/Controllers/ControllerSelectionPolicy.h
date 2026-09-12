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
    Adoption              // the selected unit returned under a different identity
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerSelectionPolicy
//
//  Which controller drives the game port, given what is attached and what was
//  chosen before. Pure: it reads no device and writes nothing, so every rule
//  in it is reachable from the unit tests with a list of made-up devices.
//
//  A selection whose controller is absent is KEPT, never discarded. The user
//  chose that controller, and a controller that is unplugged for a minute has
//  not been un-chosen (FR-011).
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
