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
//  ControllerAxisAssignment
//
//  One controller and the machine axes it holds. A machine keeps a list of
//  these beside its selection; each axis is held by at most one entry.
//
//  THE SELECTED CONTROLLER NEEDS NO ENTRY. Without one it holds PDL0 and
//  PDL1, less any axis another entry holds, which is exactly what a machine
//  with only a `controller` saved has always done. An entry for it replaces
//  that default outright.
//
//  THE ASSIGNMENT REMAPS, THE PROFILE BINDS. A controller's mapping drives its
//  own PDL0-PDL3 targets; its held axes, in ascending order, are where those
//  land. A controller holding PDL1 alone plays its pdl0 bindings on PDL1, and
//  one holding PDL2 and PDL3 plays its pdl0 and pdl1 there. So two players can
//  use the same Default or Paddles profile, and one controller holding all
//  four plays a mapping that binds all four as written.
//
////////////////////////////////////////////////////////////////////////////////

struct ControllerAxisAssignment
{
    using AxisSet = std::bitset<GamePortContribution::kAxisCount>;

    ControllerUnitKey  unit;
    AxisSet            axes;

    bool operator== (const ControllerAxisAssignment &) const = default;
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

    // PDL0 and PDL1: what the selected controller holds with no entry of its
    // own, and all a newly connected controller claims (FR-038).
    static constexpr unsigned long  kDefaultAxisBits = 0x3;

    static Decision  Evaluate           (const std::optional<ControllerUnitKey> &      current,
                                         const std::vector<ControllerDeviceInfo> &     devices,
                                         bool                                          hasGamePort,
                                         const std::vector<ControllerAxisAssignment> & assignments = {});

    static bool      IsSelectedAttached (const std::optional<ControllerUnitKey> &   selection,
                                         const std::vector<ControllerDeviceInfo> &  devices);

    // Gives the unit exactly these axes, and takes each of them from whichever
    // controller held it before, which keeps its other axes (FR-036).
    static void      AssignAxes         (std::vector<ControllerAxisAssignment> & assignments,
                                         const ControllerUnitKey &               unit,
                                         ControllerAxisAssignment::AxisSet       axes);

    // The axes a unit drives on a machine with axisCount axes. Axes past the
    // count are left out, not removed from the assignment (FR-035).
    static ControllerAxisAssignment::AxisSet  GetAxesFor (const std::vector<ControllerAxisAssignment> & assignments,
                                                          const ControllerUnitKey &                     unit,
                                                          const std::optional<ControllerUnitKey> &      selection,
                                                          size_t                                        axisCount);

    static bool      HasAssignment      (const std::vector<ControllerAxisAssignment> & assignments,
                                         const ControllerUnitKey &                     unit);

private:

    static const ControllerDeviceInfo *  FindUnit          (const std::vector<ControllerDeviceInfo> &  devices,
                                                            const ControllerUnitKey &                  unit);

    static const ControllerDeviceInfo &  GetPreferredDevice (const std::vector<ControllerDeviceInfo> &     devices,
                                                             const std::vector<ControllerAxisAssignment> & assignments);

    static const ControllerDeviceInfo *  FindSoleSameModel (const std::vector<ControllerDeviceInfo> &  devices,
                                                            const ControllerModelKey &                 model);
};
