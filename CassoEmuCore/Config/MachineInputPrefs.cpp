#include "Pch.h"

#include "Config/MachineInputPrefs.h"

#include "Controllers/ControllerTokens.h"





static constexpr const char *  s_kpszInputModeOff      = "off";
static constexpr const char *  s_kpszInputModeJoystick = "joystick";
static constexpr const char *  s_kpszInputModePaddle   = "paddle";
static constexpr const char *  s_kpszInputModeMouse    = "mouse";

static constexpr const char *  s_kpszEnabledKey        = "enabled";
static constexpr const char *  s_kpszPlayersKey        = "players";
static constexpr const char *  s_kpszMapsKey           = "maps";

static constexpr const char *  s_kpszTargetJoystick0   = "joystick0";
static constexpr const char *  s_kpszTargetJoystick1   = "joystick1";
static constexpr const char *  s_kpszTargetPaddle0     = "paddle0";
static constexpr const char *  s_kpszTargetPaddle1     = "paddle1";
static constexpr const char *  s_kpszTargetPaddle2     = "paddle2";
static constexpr const char *  s_kpszTargetPaddle3     = "paddle3";





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





////////////////////////////////////////////////////////////////////////////////
//
//  MachineInputPrefs::TargetToToken
//
//  What a player slot maps to, in its persisted spelling. Names rather than
//  ordinals, for the same reason the mapping modes use them: inserting a
//  target later cannot silently reinterpret a saved slot as another one.
//
////////////////////////////////////////////////////////////////////////////////

const char * MachineInputPrefs::TargetToToken (PlayerAxisTarget target)
{
    // Joystick 0 is both the first target and the safe spelling for one this
    // build does not know: every machine with a game port has PDL0 and PDL1.
    const char *  token = s_kpszTargetJoystick0;



    switch (target)
    {
        case PlayerAxisTarget::Joystick1:  token = s_kpszTargetJoystick1; break;
        case PlayerAxisTarget::Paddle0:    token = s_kpszTargetPaddle0;   break;
        case PlayerAxisTarget::Paddle1:    token = s_kpszTargetPaddle1;   break;
        case PlayerAxisTarget::Paddle2:    token = s_kpszTargetPaddle2;   break;
        case PlayerAxisTarget::Paddle3:    token = s_kpszTargetPaddle3;   break;

        case PlayerAxisTarget::Joystick0:
        default:                                                          break;
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineInputPrefs::TargetFromToken
//
//  The inverse, answering with `fallback` for an empty or unrecognized token
//  so a file written by a newer build degrades to a playable slot.
//
////////////////////////////////////////////////////////////////////////////////

PlayerAxisTarget MachineInputPrefs::TargetFromToken (
    const std::string &  token,
    PlayerAxisTarget     fallback)
{
    PlayerAxisTarget  target = fallback;



    if      (token == s_kpszTargetJoystick0) { target = PlayerAxisTarget::Joystick0; }
    else if (token == s_kpszTargetJoystick1) { target = PlayerAxisTarget::Joystick1; }
    else if (token == s_kpszTargetPaddle0)   { target = PlayerAxisTarget::Paddle0;   }
    else if (token == s_kpszTargetPaddle1)   { target = PlayerAxisTarget::Paddle1;   }
    else if (token == s_kpszTargetPaddle2)   { target = PlayerAxisTarget::Paddle2;   }
    else if (token == s_kpszTargetPaddle3)   { target = PlayerAxisTarget::Paddle3;   }

    return target;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineInputPrefs::ReadMultiplayer
//
//  The block is { "enabled": <bool>, "players": [ <slot>, <slot> ] }, where a
//  slot is { "controller": <unit token>, "maps": <target token> }.
//
//  A SLOT WHOSE CONTROLLER CANNOT BE READ IS LEFT EMPTY rather than dropping
//  the block: the other player keeps playing, and the empty slot is what the
//  settings page shows for the one that could not be restored. An empty token
//  reads the same way, which is how a slot nobody has filled round-trips.
//
////////////////////////////////////////////////////////////////////////////////

MultiplayerSetup MachineInputPrefs::ReadMultiplayer (const JsonValue * uiPrefs)
{
    MultiplayerSetup    setup;
    const JsonValue   * block     = nullptr;
    const JsonValue   * players   = nullptr;
    bool                isEnabled = false;
    size_t              i         = 0;



    if (uiPrefs == nullptr || !uiPrefs->HasObject (kpszMultiplayerKey, block) || block == nullptr)
    {
        return setup;
    }

    if (block->HasBool (s_kpszEnabledKey, isEnabled))
    {
        setup.isEnabled = isEnabled;
    }

    if (!block->HasArray (s_kpszPlayersKey, players) || players == nullptr)
    {
        return setup;
    }

    for (i = 0; i < players->GetArraySize() && i < MultiplayerSetup::kPlayerCount; i++)
    {
        const JsonValue &  entry = players->GetArrayElement (i);
        HRESULT            hr    = S_OK;
        std::string        token;
        std::string        maps;
        ControllerUnitKey  unit;

        if (entry.GetType() != JsonType::Object)
        {
            continue;
        }

        if (entry.HasString (s_kpszMapsKey, maps))
        {
            setup.players[i].target = TargetFromToken (maps, PlayerAxisTarget::Joystick0);
        }

        if (!entry.HasString (kpszControllerKey, token))
        {
            continue;
        }

        hr = ControllerTokens::UnitFromToken (token, unit);

        if (FAILED (hr))
        {
            continue;
        }

        setup.players[i].unit = unit;
    }

    return ControllerSelectionPolicy::Normalize (setup);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineInputPrefs::BuildMultiplayerEntry
//
//  BOTH SLOTS ARE ALWAYS WRITTEN, an empty one as an empty controller token.
//  The block is spliced key by key, so a slot left out would leave the one
//  already in the file behind, and a player the user cleared would come back
//  on the next launch.
//
////////////////////////////////////////////////////////////////////////////////

std::pair<std::string, JsonValue> MachineInputPrefs::BuildMultiplayerEntry (
    const MultiplayerSetup &  setup)
{
    std::vector<std::pair<std::string, JsonValue>>  block;
    std::vector<JsonValue>                          players;
    size_t                                          i     = 0;



    for (i = 0; i < MultiplayerSetup::kPlayerCount; i++)
    {
        std::vector<std::pair<std::string, JsonValue>>  entry;
        std::string                                     token;

        if (setup.players[i].unit.has_value())
        {
            token = ControllerTokens::UnitToToken (setup.players[i].unit.value());
        }

        entry.emplace_back (kpszControllerKey, JsonValue (token));
        entry.emplace_back (s_kpszMapsKey,     JsonValue (std::string (TargetToToken (setup.players[i].target))));
        players.emplace_back (std::move (entry));
    }

    block.emplace_back (s_kpszEnabledKey, JsonValue (setup.isEnabled));
    block.emplace_back (s_kpszPlayersKey, JsonValue (std::move (players)));

    return { kpszMultiplayerKey, JsonValue (std::move (block)) };
}
