#pragma once

#include "Pch.h"

#include "Core/JsonValue.h"
#include "../UiCommandTypes.h"





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
    static constexpr const char *  kpszArrowsKey  = "arrowsToJoystick";
    static constexpr const char *  kpszPointerKey = "pointerMapping";

    static void  ReadFromUiPrefs (const JsonValue  * uiPrefs,
                                  bool               seedArrows,
                                  InputMappingMode   seedPointer,
                                  bool             & outArrows,
                                  InputMappingMode & outPointer);

    static std::vector<std::pair<std::string, JsonValue>>  BuildUiPrefEntries (
        bool              arrows,
        InputMappingMode  pointer);

    static const char *      ModeToToken   (InputMappingMode    mode);
    static InputMappingMode  ModeFromToken (const std::string & token,
                                            InputMappingMode    fallback);
};
