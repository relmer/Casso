#pragma once

#include "Pch.h"

#include "Controllers/ControllerSelectionPolicy.h"
#include "Controllers/ControllerTypes.h"
#include "Controllers/GamePortInputMixer.h"
#include "Controllers/PlayerSlotPolicy.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InputModeRules
//
//  Who owns the paddle axes, and what turning one input source on does to the
//  others. Player 1 plays on the arrow keys, the mouse in paddle mode, or a
//  controller, and only one of them, so choosing any of them gives up the
//  other two. A controller connecting chooses nothing: the keys or the mouse
//  picked for Player 1 stay until the user picks something else.
//
//  This is pure so the exclusivity can be asserted without a machine. It was
//  spread across `SetArrowsJoystick`, `SetPointerMapping` and
//  `SyncGamePortAxisOwner` before, where each path enforced its own half and
//  nothing could state the whole rule.
//
//  The command bar's picker is built here as data for the same reason: a
//  row for each player, what each player's submenu lists and checks, and the
//  word the closed picker wears, all from the players' entries and slots.
//
////////////////////////////////////////////////////////////////////////////////

class InputModeRules
{
public:

    // What the user has chosen, plus whether the selected controller reads.
    struct State
    {
        bool  arrowsJoystick       = false;
        bool  mousePaddle          = false;
        bool  hasController        = false;
        bool  isControllerAttached = false;

        bool operator== (const State &) const = default;
    };

    // The words the picker writes for the players and for Player 2's
    // Disabled entry, kept apart from the rows so that another mode's words
    // can take their place.
    struct PlayerLabels
    {
        std::array<std::wstring, PlayerSlotPolicy::kPlayerCount>  players  = { L"Player 1", L"Player 2" };
        std::wstring                                               disabled = L"Disabled";
    };

    // One controller's profiles of the mode in effect, that mode's built-in
    // profile first, and its active one: empty, or a name the list lacks,
    // is the built-in profile.
    struct ProfileChoices
    {
        std::vector<std::string>  names;
        std::string               active;
    };

    // One entry above the separator in a player's submenu, and the entry
    // picking it gives that player. A picked controller that is not attached
    // stays listed and checked, and is not connected.
    struct PlayerChoice
    {
        std::wstring  label;
        PlayerEntry   entry;
        bool          isChecked   = false;
        bool          isConnected = true;

        bool operator== (const PlayerChoice &) const = default;
    };

    // The profiles at the foot of a player's submenu, for the controller
    // playing there, under its description; `checked` is the active one.
    struct PlayerProfileSection
    {
        ControllerUnitKey         unit;
        std::wstring              header;
        std::vector<std::string>  names;
        size_t                    checked = 0;

        bool operator== (const PlayerProfileSection &) const = default;
    };

    // One player's row in the picker: what is playing, the submenu's
    // entries, and the profile section while a controller plays there.
    struct PlayerRow
    {
        std::wstring                         label;
        std::vector<PlayerChoice>            choices;
        std::optional<PlayerProfileSection>  profiles;

        bool operator== (const PlayerRow &) const = default;
    };

    using PlayerRows = std::array<PlayerRow, PlayerSlotPolicy::kPlayerCount>;

    // What drives the game port for the picker's face: its drawing follows
    // the device, not the API that reads it (FR-008b).
    enum class PickerDriver
    {
        None,
        ArrowKeys,
        MousePaddle,
        Controller,
    };

    // What the picker is built from. `profiles` and `knownDescriptions` are
    // by unit token; a description is looked up among the attached devices
    // first, then among those seen earlier, for a picked controller that is
    // not attached.
    struct PickerSource
    {
        PlayerEntries                          entries;
        PlayerSlots                            slots;
        std::vector<ControllerDeviceInfo>      devices;
        std::map<std::string, ProfileChoices>  profiles;
        std::map<std::string, std::wstring>    knownDescriptions;
        PlayerLabels                           labels;

        // Whether the running machine reads the Joyport, which leaves the
        // mouse as paddle out of Player 1's submenu.
        bool                                   isJoyportInEffect = false;
    };

    // The picker: the two players' rows, and what its closed face wears.
    struct Picker
    {
        PlayerRows            rows;
        std::wstring          label;
        PickerDriver          driver     = PickerDriver::None;
        ControllerFormFactor  formFactor = ControllerFormFactor::Gamepad;
    };

    // Ends the picker's label while Player 2 is also playing (FR-008b).
    static constexpr const wchar_t *  kpszSecondPlayerSuffix = L" +1";

    // Drops a device description's trailing vendor and product parenthetical,
    // for the picker's face. How much of what is left fits is measured where
    // the label is drawn.
    static std::wstring  Shorten (const std::wstring & text);

    static Picker  BuildPicker (const PickerSource & source);

    // The line the persistent banner carries while the keys or the mouse
    // drive the game port, and empty while a controller drives or nothing
    // does. Follows the MODE; how the mouse is read while paddle
    // mode is on is not a question this answers.
    static std::wstring  GetStandInBannerText (const State & state);

    using AxisOwners = std::array<AxisOwner, GamePortContribution::kAxisCount>;

    static AxisOwners  GetAxisOwners           (const State & state);
    static State       AfterSelectingController (State state);
    static State       AfterSettingArrows       (State state, bool on);
    static State       AfterSettingMousePaddle  (State state, bool on);

    // PB0 and PB1 as the keys-as-joystick fire keys drive them: X or left Alt,
    // and Z or right Alt. With the Joyport attached the Alt keys are left out.
    static std::bitset<2>  GetFireKeyButtons (bool xDown,
                                              bool zDown,
                                              bool leftAltDown,
                                              bool rightAltDown,
                                              bool isJoyportAttached);

private:

    static constexpr const wchar_t *  kpszAutomatic      = L"Automatic";
    static constexpr const wchar_t *  kpszKeys           = L"Keys";
    static constexpr const wchar_t *  kpszMouse          = L"Mouse";
    static constexpr const wchar_t *  kpszNotConnected   = L" (not connected)";
    static constexpr const wchar_t *  kpszNothingDriving = L"Controller";

    static const ControllerDeviceInfo *         FindDevice         (const std::vector<ControllerDeviceInfo> & devices, const ControllerUnitKey & unit);
    static std::wstring                         DescribeUnit        (const PickerSource & source, const ControllerUnitKey & unit);
    static std::wstring                         DescribePlaying     (const PickerSource & source, size_t player);
    static std::vector<PlayerChoice>            BuildChoices        (const PickerSource & source, size_t player);
    static std::optional<PlayerProfileSection>  BuildProfileSection (const PickerSource & source, size_t player);
    static void                                 SetFace             (const PickerSource & source, Picker & picker);
};
