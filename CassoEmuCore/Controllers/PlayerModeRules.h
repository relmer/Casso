#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"
#include "Controllers/InputModeRules.h"
#include "Controllers/PlayerSlotPolicy.h"
#include "Core/JsonValue.h"





// What a launch found of the Joyport setting that per-player modes replaced:
// whether it was on, which puts the players in its jacks, and whether the
// old key is to be marked as read, which the caller saves so that no later
// launch reads it again.
struct JoyportMigration
{
    bool  isJoyport  = false;
    bool  shouldMark = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PlayerModeRules
//
//  What each player's mode means for the game port, from the two players'
//  entries and whether the running machine has a Joyport. Pure, so every
//  consumer -- the controller service, the machine, the picker and the
//  Controllers page -- reads the modes through the same rules and none of
//  them can disagree about what the machine reads.
//
////////////////////////////////////////////////////////////////////////////////

class PlayerModeRules
{
public:

    using JackSet = std::bitset<JoyportJacks::kJackCount>;

    static PlayerMode             ResolveMode         (const PlayerEntries & entries, size_t player, bool hasJoyport);
    static bool                   IsJoyportMode       (PlayerMode mode);
    static std::optional<size_t>  GetJack             (PlayerMode mode);
    static bool                   IsOnJoyport         (const PlayerEntries & entries, size_t player, bool hasJoyport);
    static bool                   IsJoyportOn         (const PlayerEntries & entries, bool hasJoyport);
    static bool                   AreButtonsCut       (const PlayerEntries & entries, size_t player, bool hasJoyport);
    static bool                   ArePaddlesConnected (const PlayerEntries & entries, bool hasJoyport);
    static bool                   IsModeTaken         (const PlayerEntries & entries, size_t player, PlayerMode mode, bool hasJoyport);
    static bool                   AreKeysOffered      (const PlayerEntries & entries, bool hasJoyport);
    static bool                   IsMouseOffered      (const PlayerEntries & entries, bool hasJoyport);
    static std::wstring           GetModeLabel        (PlayerMode mode);
    static std::wstring           GetPlayerLabel      (size_t player);

    static std::vector<InputModeRules::PlayerModeChoice>  BuildModeChoices (const PlayerEntries & entries, size_t player, bool hasJoyport);

    // The notice for a controller Automatic gave a player, with the jacks it
    // drives when it plays in the Joyport.
    static std::wstring           DescribeAssignment  (size_t player, const std::wstring & description, JackSet jacks);

    // The Joyport setting that per-player modes replaced, read once.
    static JoyportMigration       MigrateAdapter      (const std::string & globalToken,
                                                       const JsonValue   * launchedUiPrefs,
                                                       bool                launchedHasAnnunciators);
    static PlayerEntries          ApplyMigration      (PlayerEntries entries);

private:

    static constexpr size_t  kPlayerOne = 0;
    static constexpr size_t  kPlayerTwo = 1;

    static PlayerMode  GetOtherJackMode (PlayerMode mode);
    static bool        IsPlaying        (const PlayerEntries & entries, size_t player);
};
