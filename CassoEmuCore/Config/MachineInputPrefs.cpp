#include "Pch.h"

#include "Config/MachineInputPrefs.h"





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
    InputMappingMode   seedPointer,
    bool             & outArrows,
    InputMappingMode & outPointer)
{
    std::string  token;



    // Seed first, so a null block or a block missing the key leaves the
    // fallback in place.
    outPointer = seedPointer;

    if (uiPrefs != nullptr && uiPrefs->HasString (kpszPointerKey, token))
    {
        outPointer = ModeFromToken (token, seedPointer);
    }

    //  NEITHER MODE THAT SWALLOWS HOST INPUT IS EVER RESUMED. Paddle mode
    //  takes the pointer; arrows-to-joystick takes X and Z for the fire
    //  buttons, so a machine that comes up in it is one where two letter keys
    //  do not type and nothing about a fresh boot says why. Both are modes a
    //  user turns on for the session they are playing in, and the setting is
    //  still saved -- it is resuming it unasked that is refused.
    outArrows = false;

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
    InputMappingMode  pointer)
{
    std::vector<std::pair<std::string, JsonValue>>  entries;



    //  NOTHING IS WRITTEN THAT WILL NOT BE READ BACK. Arrows-to-joystick and
    //  a captured pointer are never resumed, so storing them would leave the
    //  file describing a machine that will not come up that way -- and the
    //  next person to read it would have to find the coercion to know.
    //
    //  The reader still coerces, because files written before this did store
    //  them.
    if (pointer == InputMappingMode::Paddle || pointer == InputMappingMode::Joystick)
    {
        pointer = InputMappingMode::Off;
    }

    entries.emplace_back (kpszPointerKey, JsonValue (std::string (ModeToToken (pointer))));

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineInputPrefs::ReadControllerToken
//
//  The chosen controller, or an empty string where none is chosen.
//
////////////////////////////////////////////////////////////////////////////////

std::string MachineInputPrefs::ReadControllerToken (const JsonValue * uiPrefs)
{
    std::string  token;



    if (uiPrefs != nullptr && !uiPrefs->HasString (kpszControllerKey, token))
    {
        token.clear();
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineInputPrefs::ReadProfileName
//
//  The active profile, or an empty string, which means Default (FR-029).
//
////////////////////////////////////////////////////////////////////////////////

std::string MachineInputPrefs::ReadProfileName (const JsonValue * uiPrefs)
{
    std::string  name;



    if (uiPrefs != nullptr && !uiPrefs->HasString (kpszProfileKey, name))
    {
        name.clear();
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineInputPrefs::BuildControllerEntries
//
//  An EMPTY TOKEN IS STILL WRITTEN, as an empty string. The absence of the
//  key means this machine has never chosen a controller, and the policy is
//  free to choose one for it; the empty string means the user turned the
//  controller off in favor of the arrows or the paddle, and choosing one for
//  them again on the next launch would undo that (FR-032).
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::pair<std::string, JsonValue>> MachineInputPrefs::BuildControllerEntries (
    const std::string &  controllerToken,
    const std::string &  profileName)
{
    std::vector<std::pair<std::string, JsonValue>>  entries;



    entries.emplace_back (kpszControllerKey, JsonValue (controllerToken));

    if (!profileName.empty())
    {
        entries.emplace_back (kpszProfileKey, JsonValue (profileName));
    }

    return entries;
}
