#pragma once

#include "Pch.h"

#include "Core/JsonValue.h"
#include "Ui/UiCommandTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineInputPrefs
//
//  The per-machine input mapping -- arrows-to-joystick on the keys axis, and
//  Off / Paddle / Mouse on the pointer axis -- as it is stored in and read
//  back out of a machine's $cassoUiPrefs block.
//
//  PER MACHINE, not global: the mapping describes how host input reaches the
//  emulated game port, and a ][+ with no mouse, a //e with a joystick and a
//  //c with its own mouse are three different answers. It was global through
//  1.22, so a launch that finds no stored value for a machine falls back to
//  the old global setting rather than to Off.
//
////////////////////////////////////////////////////////////////////////////////

class MachineInputPrefs
{
public:
    static constexpr const char *  kpszArrowsKey     = "arrowsToJoystick";
    static constexpr const char *  kpszPointerKey    = "pointerMapping";
    static constexpr const char *  kpszControllerKey = "controller";
    static constexpr const char *  kpszProfileKey    = "controllerProfile";

    static void  ReadFromUiPrefs (const JsonValue  * uiPrefs,
                                  bool               seedArrows,
                                  InputMappingMode   seedPointer,
                                  bool             & outArrows,
                                  InputMappingMode & outPointer);

    static std::vector<std::pair<std::string, JsonValue>>  BuildUiPrefEntries (
        bool              arrows,
        InputMappingMode  pointer);

    // The chosen controller and its active profile. Separate from the pair
    // above because they are read and written on their own: a controller is
    // chosen on the controller thread's schedule, not when the user touches
    // the arrows or the pointer.
    //
    // An empty token means no controller is chosen, which is not the same as
    // a controller that is merely unplugged -- that one keeps its token.
    static std::string  ReadControllerToken (const JsonValue * uiPrefs);
    static std::string  ReadProfileName     (const JsonValue * uiPrefs);

    static std::vector<std::pair<std::string, JsonValue>>  BuildControllerEntries (
        const std::string &  controllerToken,
        const std::string &  profileName);

    static const char *      ModeToToken   (InputMappingMode    mode);
    static InputMappingMode  ModeFromToken (const std::string & token,
                                            InputMappingMode    fallback);
};
