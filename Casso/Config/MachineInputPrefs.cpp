#include "Pch.h"

#include "MachineInputPrefs.h"





static constexpr const char *  s_kpszInputModeOff      = "off";
static constexpr const char *  s_kpszInputModeJoystick = "joystick";
static constexpr const char *  s_kpszInputModePaddle   = "paddle";
static constexpr const char *  s_kpszInputModeMouse    = "mouse";





////////////////////////////////////////////////////////////////////////////////
//
//  MachineInputPrefs::ModeToToken
//
//  Maps a mapping mode to its persisted spelling.
//
//  Prefs are stored as NAMES rather than as enum ordinals, so inserting a mode
//  later cannot silently reinterpret everyone's saved setting as a different
//  one.
//
////////////////////////////////////////////////////////////////////////////////

const char * MachineInputPrefs::ModeToToken (InputMappingMode mode)
{
    // "off" is both the Off mode and the safe spelling for a mode this build
    // does not know -- writing an unknown token back out would strand it.
    const char *  token = s_kpszInputModeOff;



    switch (mode)
    {
        case InputMappingMode::Joystick:  token = s_kpszInputModeJoystick; break;
        case InputMappingMode::Paddle:    token = s_kpszInputModePaddle;   break;
        case InputMappingMode::Mouse:     token = s_kpszInputModeMouse;    break;

        case InputMappingMode::Off:
        default:                                                           break;
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineInputPrefs::ModeFromToken
//
//  Parses a serialized mode token, returning `fallback` for an empty or
//  unrecognized string so an unknown future value degrades gracefully.
//
////////////////////////////////////////////////////////////////////////////////

InputMappingMode MachineInputPrefs::ModeFromToken (
    const std::string & token,
    InputMappingMode    fallback)
{
    // The inverse of ModeToToken. `fallback` (not Off) is the miss result so a
    // prefs file written by a newer build keeps whatever the caller was
    // already using rather than silently disabling input mapping.
    InputMappingMode  mode = fallback;



    if      (token == s_kpszInputModeJoystick) { mode = InputMappingMode::Joystick; }
    else if (token == s_kpszInputModePaddle)   { mode = InputMappingMode::Paddle;   }
    else if (token == s_kpszInputModeMouse)    { mode = InputMappingMode::Mouse;    }
    else if (token == s_kpszInputModeOff)      { mode = InputMappingMode::Off;      }

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineInputPrefs::ReadFromUiPrefs
//
//  Resolves the mapping to restore for a machine. A key the block does not
//  carry falls back to the matching seed, which is how a machine that has
//  never had its own mapping stored inherits the pre-1.23 global setting.
//
//  PADDLE IS NEVER RESTORED. It captures the host mouse, so restoring it would
//  light the indicator while the pointer is not actually captured, and the
//  first click the user aimed at a control would be read as a fire button.
//  It resolves to Off, and the user re-enters it deliberately. Joystick
//  resolves to Off on the pointer axis for a different reason: it belongs to
//  the keys axis, so it is not an answer to this question at all.
//
////////////////////////////////////////////////////////////////////////////////

void MachineInputPrefs::ReadFromUiPrefs (
    const JsonValue  * uiPrefs,
    bool               seedArrows,
    InputMappingMode   seedPointer,
    bool             & outArrows,
    InputMappingMode & outPointer)
{
    std::string  token;
    bool         storedArrows = false;



    // Seeds first, so a null block or a block missing either key leaves the
    // fallback in place.
    outArrows  = seedArrows;
    outPointer = seedPointer;

    if (uiPrefs != nullptr)
    {
        if (uiPrefs->HasBool (kpszArrowsKey, storedArrows))
        {
            outArrows = storedArrows;
        }

        if (uiPrefs->HasString (kpszPointerKey, token))
        {
            outPointer = ModeFromToken (token, seedPointer);
        }
    }

    if (outPointer == InputMappingMode::Paddle || outPointer == InputMappingMode::Joystick)
    {
        outPointer = InputMappingMode::Off;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineInputPrefs::BuildUiPrefEntries
//
//  The two $cassoUiPrefs entries that record a mapping, ready to splice into a
//  machine's block.
//
//  NEITHER KEY IS IN UserConfigStore::BuildUiPrefsDefaults, deliberately. That
//  table is what a delta is measured against, so a key listed there is dropped
//  from the file whenever it matches -- and an absent mapping means "fall back
//  to the old global setting", not "off". A user who turned the mapping off on
//  one machine would get it back on the next launch.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::pair<std::string, JsonValue>> MachineInputPrefs::BuildUiPrefEntries (
    bool              arrows,
    InputMappingMode  pointer)
{
    std::vector<std::pair<std::string, JsonValue>>  entries;



    entries.emplace_back (kpszArrowsKey,  JsonValue (arrows));
    entries.emplace_back (kpszPointerKey, JsonValue (std::string (ModeToToken (pointer))));

    return entries;
}
