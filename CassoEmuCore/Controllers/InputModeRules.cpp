#include "Pch.h"

#include "Controllers/InputModeRules.h"

#include "Controllers/ControllerTokens.h"
#include "Controllers/JoyportSetting.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BuildPicker
//
//  A row for each player, and the word the closed picker wears. Every row
//  reports what is PLAYED, not only what was chosen: the controller Automatic
//  gave the player, or a pick that is away and waited for (FR-040).
//
//  The rows are data. The chrome turns them into commands, so every rule
//  about what is listed and what is checked can be asserted here without a
//  menu.
//
////////////////////////////////////////////////////////////////////////////////

InputModeRules::Picker InputModeRules::BuildPicker (const PickerSource & source)
{
    Picker  picker;
    size_t  player = 0;



    for (player = 0; player < PlayerSlotPolicy::kPlayerCount; player++)
    {
        PlayerRow &  row = picker.rows[player];

        row.label    = source.labels.players[player] + L": " + DescribePlaying (source, player);
        row.choices  = BuildChoices (source, player);
        row.profiles = BuildProfileSection (source, player);
    }

    SetFace (source, picker);

    return picker;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindDevice
//
////////////////////////////////////////////////////////////////////////////////

const ControllerDeviceInfo * InputModeRules::FindDevice (
    const std::vector<ControllerDeviceInfo>  & devices,
    const ControllerUnitKey                  & unit)
{
    for (const ControllerDeviceInfo & device : devices)
    {
        if (device.unit == unit)
        {
            return &device;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DescribeUnit
//
//  An attached controller's own description. One that is away has the
//  description it had when last seen, or else one made from its model: a
//  pick restored at launch may not have been attached at all this session.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InputModeRules::DescribeUnit (
    const PickerSource       & source,
    const ControllerUnitKey  & unit)
{
    const ControllerDeviceInfo  * device = FindDevice (source.devices, unit);
    auto                          known  = source.knownDescriptions.find (ControllerTokens::UnitToToken (unit));



    if (device != nullptr)
    {
        return device->description;
    }

    if (known != source.knownDescriptions.end())
    {
        return known->second;
    }

    if (unit.model.kind == ControllerKind::XInput)
    {
        return L"Xbox Controller";
    }

    return std::format (L"Controller ({:04x}:{:04x})", unit.model.vendorId, unit.model.productId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DescribePlaying
//
//  What a player's row shows after the player's label: the keys, the mouse,
//  Disabled, the controller in the slot -- marked not connected while it is
//  away -- or Automatic while Automatic has no controller for the player.
//  A player whose words give an idle text reads it instead while on
//  Automatic with no controller playing, a holder that has not given input
//  included.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InputModeRules::DescribePlaying (
    const PickerSource  & source,
    size_t                player)
{
    const PlayerEntry                 & entry = source.entries[player];
    std::optional<ControllerUnitKey>    unit  = source.slots[player].holder;



    switch (entry.kind)
    {
        case PlayerEntryKind::ArrowKeys:   return kpszKeys;
        case PlayerEntryKind::MousePaddle: return kpszMouse;
        case PlayerEntryKind::Disabled:    return source.labels.disabled;

        case PlayerEntryKind::Automatic:
        case PlayerEntryKind::Controller:
        default:                           break;
    }

    if (entry.kind == PlayerEntryKind::Automatic && !source.labels.idle[player].empty() &&
        !PlayerSlotPolicy::IsDrivingSlot (source.slots[player]) && source.slots[player].state != PlayerSlotState::Held)
    {
        return source.labels.idle[player];
    }

    if (!unit.has_value() && entry.kind == PlayerEntryKind::Controller)
    {
        unit = entry.unit;
    }

    if (!unit.has_value())
    {
        return kpszAutomatic;
    }

    if (FindDevice (source.devices, unit.value()) == nullptr)
    {
        return DescribeUnit (source, unit.value()) + kpszNotConnected;
    }

    return DescribeUnit (source, unit.value());
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildChoices
//
//  Automatic first, written "Automatic (description)" once it has given the
//  player a controller; then every attached controller, the other player's
//  included, since picking it moves it (FR-041); then a pick that is not
//  attached, still checked; then the keys and the mouse for Player 1, or
//  Disabled for Player 2. Exactly one is checked: the player's entry.
//
//  The mouse is left out while the Joyport is in effect: an Atari stick has
//  no paddle for it to stand in for (JoyportSetting::IsMousePaddleOffered).
//  A mouse picked before the Joyport was turned on stays Player 1's entry,
//  driving nothing until it is turned off, so it stays listed while it is
//  the checked one.
//
//  A choice carries no mode: the player keeps its own through any pick
//  (PlayerSlotPolicy::ApplyPick).
//
////////////////////////////////////////////////////////////////////////////////

std::vector<InputModeRules::PlayerChoice> InputModeRules::BuildChoices (
    const PickerSource  & source,
    size_t                player)
{
    const PlayerEntry          & current  = source.entries[player];
    const PlayerSlot           & slot     = source.slots[player];
    std::vector<PlayerChoice>    choices;
    PlayerChoice                 automatic;
    PlayerChoice                 away;
    PlayerChoice                 keys;
    PlayerChoice                 mouse;
    PlayerChoice                 disabled;
    bool                         isListed = false;
    bool                         hasMouse = false;



    automatic.label     = kpszAutomatic;
    automatic.isChecked = current.kind == PlayerEntryKind::Automatic;

    if (automatic.isChecked && slot.holder.has_value())
    {
        automatic.label = std::format (L"{} ({})", kpszAutomatic, DescribeUnit (source, slot.holder.value()));
    }

    choices.push_back (automatic);

    for (const ControllerDeviceInfo & device : source.devices)
    {
        PlayerChoice  choice;

        choice.label      = device.description;
        choice.entry.kind = PlayerEntryKind::Controller;
        choice.entry.unit = device.unit;
        choice.isChecked  = current.kind == PlayerEntryKind::Controller && current.unit == device.unit;
        isListed          = isListed || choice.isChecked;

        choices.push_back (choice);
    }

    if (current.kind == PlayerEntryKind::Controller && current.unit.has_value() && !isListed)
    {
        away.label       = DescribeUnit (source, current.unit.value()) + kpszNotConnected;
        away.entry       = current;
        away.isChecked   = true;
        away.isConnected = false;

        choices.push_back (away);
    }

    if (player == 0)
    {
        keys.label       = L"Use keys as joystick";
        keys.entry.kind  = PlayerEntryKind::ArrowKeys;
        keys.isChecked   = current.kind == PlayerEntryKind::ArrowKeys;
        mouse.label      = L"Use mouse as paddle";
        mouse.entry.kind = PlayerEntryKind::MousePaddle;
        mouse.isChecked  = current.kind == PlayerEntryKind::MousePaddle;
        hasMouse         = mouse.isChecked || JoyportSetting::IsMousePaddleOffered (source.isJoyportInEffect);

        choices.push_back (keys);

        if (hasMouse)
        {
            choices.push_back (mouse);
        }
    }
    else
    {
        disabled.label      = source.labels.disabled;
        disabled.entry.kind = PlayerEntryKind::Disabled;
        disabled.isChecked  = current.kind == PlayerEntryKind::Disabled;

        choices.push_back (disabled);
    }

    return choices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildProfileSection
//
//  Only while a controller is in the player's slot and attached: none for
//  Automatic before it has chosen, the keys, the mouse, Disabled, or a
//  controller that is away (FR-028). The active profile is checked, matched
//  ignoring case; one the list lacks is played as the built-in profile,
//  which leads the list, so that is the one checked.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<InputModeRules::PlayerProfileSection> InputModeRules::BuildProfileSection (
    const PickerSource  & source,
    size_t                player)
{
    using ChoicesIt = std::map<std::string, ProfileChoices>::const_iterator;

    const PlayerSlot            & slot      = source.slots[player];
    const PlayerEntry           & entry     = source.entries[player];
    const ControllerDeviceInfo  * device    = nullptr;
    ChoicesIt                     choices   = source.profiles.end();
    PlayerProfileSection          section;
    size_t                        i         = 0;
    bool                          isEntry   = entry.kind == PlayerEntryKind::Automatic || entry.kind == PlayerEntryKind::Controller;
    bool                          isHolding = slot.state == PlayerSlotState::Provisional ||
                                              slot.state == PlayerSlotState::Waiting     ||
                                              slot.state == PlayerSlotState::Playing;



    if (!isEntry || !isHolding || !slot.holder.has_value())
    {
        return std::nullopt;
    }

    device = FindDevice (source.devices, slot.holder.value());

    if (device == nullptr)
    {
        return std::nullopt;
    }

    choices = source.profiles.find (ControllerTokens::UnitToToken (device->unit));

    if (choices == source.profiles.end())
    {
        return std::nullopt;
    }

    section.unit   = device->unit;
    section.header = device->description;
    section.names  = choices->second.names;

    for (i = 0; i < section.names.size(); i++)
    {
        if (_stricmp (section.names[i].c_str(), choices->second.active.c_str()) == 0)
        {
            section.checked = i;
            break;
        }
    }

    return section;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFace
//
//  The closed picker wears Player 1's keys, mouse or controller, ending in
//  " +1" while Player 2 is also playing. Player 2's controller shows while
//  it drives alone, and "Controller" while nothing drives the game port.
//  A controller's vendor and product parenthetical is dropped; what is left
//  is fitted to the button where it is drawn (FR-008b).
//
////////////////////////////////////////////////////////////////////////////////

void InputModeRules::SetFace (
    const PickerSource  & source,
    Picker              & picker)
{
    const PlayerEntry           & first     = source.entries[0];
    const ControllerDeviceInfo  * device    = nullptr;
    size_t                        driving   = PlayerSlotPolicy::kPlayerCount;
    size_t                        player    = 0;
    bool                          isSecond  = source.slots[1].state == PlayerSlotState::Playing;



    picker.label  = kpszNothingDriving;
    picker.driver = PickerDriver::None;

    if (first.kind == PlayerEntryKind::ArrowKeys || first.kind == PlayerEntryKind::MousePaddle)
    {
        picker.driver = (first.kind == PlayerEntryKind::ArrowKeys) ? PickerDriver::ArrowKeys : PickerDriver::MousePaddle;
        picker.label  = (first.kind == PlayerEntryKind::ArrowKeys) ? kpszKeys : kpszMouse;
        picker.label += isSecond ? kpszSecondPlayerSuffix : L"";
        return;
    }

    for (player = 0; player < PlayerSlotPolicy::kPlayerCount && driving == PlayerSlotPolicy::kPlayerCount; player++)
    {
        if (PlayerSlotPolicy::IsDrivingSlot (source.slots[player]))
        {
            driving = player;
        }
    }

    if (driving == PlayerSlotPolicy::kPlayerCount)
    {
        return;
    }

    device = FindDevice (source.devices, source.slots[driving].holder.value());

    if (device == nullptr)
    {
        return;
    }

    picker.driver     = PickerDriver::Controller;
    picker.formFactor = device->formFactor;
    picker.label      = Shorten (device->description);

    if (driving == 0 && isSecond)
    {
        picker.label += kpszSecondPlayerSuffix;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetStandInBannerText
//
//  The line the persistent banner carries while the keys or the mouse drive
//  the game port, and nothing while a real controller does.
//
//  PERSISTENT RATHER THAN A FEW SECONDS, because it answers a question the
//  user has for as long as the mode lasts, not only at the moment they chose
//  it. Both modes bind host controls that carry no marking: nothing on
//  screen says X and Z became the buttons, or that Escape is the way out of
//  paddle mode. A notice that expires leaves a user who looked away with no
//  way to find out short of trying keys until one fires.
//
//  THE MODE, NOT THE POINTER CAPTURE. Capture is how paddle mode reads the
//  mouse, which is nothing the user asked about; Escape leaves paddle mode
//  whether or not the pointer is held at that moment, so the line is true
//  for as long as the mode is on.
//
//  A controller needs none of this. Its buttons are labeled on the device.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InputModeRules::GetStandInBannerText (const State & state)
{
    if (state.mousePaddle)
    {
        return L"Using the mouse for paddle input. Press Esc to exit paddle mode.";
    }

    if (state.arrowsJoystick)
    {
        return L"Using the arrow keys as a joystick. X and Z are the buttons.";
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shorten
//
//  A device description as the command bar wears it, without the vendor and
//  product that end it. It is not cut to a character count: the toolbar fits
//  it to a width, with a middle ellipsis, where it is drawn.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InputModeRules::Shorten (const std::wstring & text)
{
    std::wstring  shortened = text;
    size_t        paren     = std::wstring::npos;



    // A trailing parenthetical is the vendor and product that tell two units
    // of a model apart. That is worth having where a user is choosing between
    // them; on the strip it is noise, and cutting into it mid-word reads as a
    // truncation bug rather than as a name.
    if (!shortened.empty() && shortened.back() == L')')
    {
        paren = shortened.rfind (L" (");

        if (paren != std::wstring::npos && paren > 0)
        {
            shortened.erase (paren);
        }
    }

    return shortened;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAxisOwners
//
//  PLAYER 1'S KEYS OR MOUSE KEEP PDL0 AND PDL1, whatever controllers are
//  attached: a controller connecting does not take them, and a controller
//  playing as Player 2 plays the axes of its own slot beside them. Every
//  other axis, and all four when neither is picked, belongs to the controller
//  source, which leaves an axis no controller drives at center.
//
//  THE ARROW KEYS NEVER TAKE THE AXES ON THEIR OWN. Arrows-to-joystick also
//  turns X and Z into the buttons, which takes them from the guest's
//  keyboard, so it is on only because the user turned it on.
//
////////////////////////////////////////////////////////////////////////////////

InputModeRules::AxisOwners InputModeRules::GetAxisOwners (const State & state)
{
    static constexpr size_t  kPlayerOneAxes = 2;
    AxisOwners               owners;
    AxisOwner                playerOne      = AxisOwner::Controller;
    size_t                   axis           = 0;



    if (state.mousePaddle)
    {
        playerOne = AxisOwner::MousePaddle;
    }
    else if (state.arrowsJoystick)
    {
        playerOne = AxisOwner::ArrowKeys;
    }

    for (axis = 0; axis < owners.size(); axis++)
    {
        owners[axis] = (axis < kPlayerOneAxes) ? playerOne : AxisOwner::Controller;
    }

    return owners;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AfterSelectingController
//
//  Choosing a controller gives the axes to it, so the arrow keys and the
//  paddle stop driving them (FR-008).
//
////////////////////////////////////////////////////////////////////////////////

InputModeRules::State InputModeRules::AfterSelectingController (State state)
{
    state.hasController  = true;
    state.arrowsJoystick = false;
    state.mousePaddle    = false;

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AfterSettingArrows
//
//  Turning the arrow keys on takes the axes from the paddle and from the
//  controller, which clears the selection: the user asked for the keys.
//
////////////////////////////////////////////////////////////////////////////////

InputModeRules::State InputModeRules::AfterSettingArrows (State state, bool on)
{
    state.arrowsJoystick = on;

    if (on)
    {
        state.mousePaddle   = false;
        state.hasController = false;
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AfterSettingMousePaddle
//
////////////////////////////////////////////////////////////////////////////////

InputModeRules::State InputModeRules::AfterSettingMousePaddle (State state, bool on)
{
    state.mousePaddle = on;

    if (on)
    {
        state.arrowsJoystick = false;
        state.hasController  = false;
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetFireKeyButtons
//
//  The Alt keys are also the //e's Open Apple and Closed Apple. With the
//  Joyport attached those keys must change nothing the guest reads, so a held
//  left Alt cannot close the Joyport's fire switch; X and Z still fire.
//
////////////////////////////////////////////////////////////////////////////////

std::bitset<2> InputModeRules::GetFireKeyButtons (
    bool  xDown,
    bool  zDown,
    bool  leftAltDown,
    bool  rightAltDown,
    bool  isJoyportAttached)
{
    std::bitset<2>  buttons;
    bool            isAltUsed = !isJoyportAttached;



    buttons.set (0, xDown || (isAltUsed && leftAltDown));
    buttons.set (1, zDown || (isAltUsed && rightAltDown));

    return buttons;
}
