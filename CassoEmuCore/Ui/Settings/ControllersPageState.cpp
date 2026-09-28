#include "Pch.h"

#include "Ui/Settings/ControllersPageState.h"

#include "Controllers/ControllerTokens.h"
#include "Controllers/DeadzoneShaper.h"
#include "Controllers/JoyportJackRules.h"
#include "Controllers/PlayerModeRules.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Load
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::Load (
    const std::vector<ControllerDeviceInfo>              & devices,
    const std::map<std::string, ControllerModelSettings> & models,
    const std::map<std::string, ControllerCalibration>   & calibrations,
    bool                                                   hasPb2,
    const std::map<std::string, std::string>             & activeProfiles,
    const std::optional<ControllerUnitKey>               & selection)
{
    size_t  i = 0;



    m_controllers.clear();
    m_selected.reset();

    m_hasPb2                 = hasPb2;
    m_models                 = models;
    m_calibrations           = calibrations;
    m_baselineModels         = models;
    m_baselineCalibrations   = calibrations;
    m_activeProfiles         = {};
    m_baselineActiveProfiles = {};

    m_committedNames.clear();
    m_capture.Cancel();
    m_calibrationStep = CalibrationStep::None;

    UpdateDevices (devices);

    if (!m_controllers.empty())
    {
        m_selected = 0;
    }

    // The machine's selected controller, when it is attached, is the one
    // the page opens on; the first attached is only the fallback.
    for (i = 0; selection.has_value() && i < m_controllers.size(); i++)
    {
        if (m_controllers[i].unit == selection.value())
        {
            m_selected = i;
        }
    }

    // The choices handed in are for the kind of the controller the page
    // opens on.
    m_profileMode                                          = GetEditedPlayerProfileMode();
    m_activeProfiles[GetModeIndex (m_profileMode)]         = activeProfiles;
    m_baselineActiveProfiles[GetModeIndex (m_profileMode)] = activeProfiles;

    LoadEditedProfile();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Load
//
//  One name, recorded as the active profile of whichever controller the page
//  opens on, once the full Load has decided which that is.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::Load (
    const std::vector<ControllerDeviceInfo>              & devices,
    const std::map<std::string, ControllerModelSettings> & models,
    const std::map<std::string, ControllerCalibration>   & calibrations,
    bool                                                   hasPb2,
    const std::string                                    & openedProfile,
    const std::optional<ControllerUnitKey>               & selection)
{
    Load (devices, models, calibrations, hasPb2, std::map<std::string, std::string>(), selection);

    if (openedProfile.empty() || !m_selected.has_value())
    {
        return;
    }

    m_editedProfile = openedProfile;
    StoreEditedProfile();
    m_baselineActiveProfiles = m_activeProfiles;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetActiveProfiles
//
//  One kind's choices as the service holds them, after Load. The kind the
//  page is in reloads the edited profile from them.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::SetActiveProfiles (ProfileMode mode, const std::map<std::string, std::string> & activeProfiles)
{
    m_activeProfiles[GetModeIndex (mode)]         = activeProfiles;
    m_baselineActiveProfiles[GetModeIndex (mode)] = activeProfiles;

    if (mode == m_profileMode)
    {
        LoadEditedProfile();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetJoyportAvailable
//
//  Whether the running machine has a Joyport, which puts its jacks among
//  each player's modes; a player in one plays Joyport profiles, so the page
//  moves to that kind, or back to the edited player's.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::SetJoyportAvailable (bool hasJoyport)
{
    m_hasJoyport = hasJoyport;
    SyncPlayers();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsJoyportInEffect
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::IsJoyportInEffect() const
{
    return PlayerModeRules::IsJoyportOn (m_entries, m_hasJoyport);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncPlayers
//
//  After any change to the players or the Joyport. Each slot's target is
//  what the two modes give it, as the service would set it, so the page
//  shows the new targets at once, before the service's slots come back; then
//  the profile kind follows.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::SyncPlayers()
{
    std::array<PlayerAxisTarget, kPlayerCount>  targets = PlayerSlotPolicy::GetModeTargets (m_entries, m_hasJoyport);
    size_t                                      player  = 0;



    for (player = 0; player < kPlayerCount; player++)
    {
        m_slots[player].target = targets[player];
    }

    SyncProfileMode();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncProfileMode
//
//  The kind of the controller in Editing, as it stands: the list and the
//  edited profile follow it at once, each controller going to its choice for
//  the new kind, or with none, to that kind's built-in profile. Edits on the
//  profile left stay pending there, as they do when another profile is
//  selected.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::SyncProfileMode()
{
    ProfileMode  mode = GetEditedPlayerProfileMode();



    if (mode == m_profileMode)
    {
        return;
    }

    m_profileMode = mode;
    LoadEditedProfile();

    m_capture.Cancel();
    m_liveEvaluator.ResetRate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetEditedPlayerProfileMode
//
//  The kind of profile the controller in Editing plays: the mode of the
//  player whose slot holds it, as it plays, either Joyport jack being the
//  Joyport kind. A controller in no slot drives nothing, and is edited as a
//  joystick.
//
////////////////////////////////////////////////////////////////////////////////

ProfileMode ControllersPageState::GetEditedPlayerProfileMode() const
{
    std::optional<size_t>  player = FindHoldingPlayer();
    PlayerMode             mode   = PlayerMode::Joystick;



    if (player.has_value())
    {
        mode = PlayerModeRules::ResolveMode (m_entries, player.value(), m_hasJoyport);
    }

    return ControllerModelSettings::GetPlayerProfileMode (mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActiveProfiles
//
////////////////////////////////////////////////////////////////////////////////

const std::map<std::string, std::string> & ControllersPageState::GetActiveProfiles (ProfileMode mode) const
{
    return m_activeProfiles[GetModeIndex (mode)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActiveProfiles
//
////////////////////////////////////////////////////////////////////////////////

const std::map<std::string, std::string> & ControllersPageState::GetActiveProfiles() const
{
    return GetActiveProfiles (m_profileMode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetModeProfiles
//
//  The choices of the kind the page is in, which the edited profile is read
//  from and written back to.
//
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, std::string> & ControllersPageState::GetModeProfiles()
{
    return m_activeProfiles[GetModeIndex (m_profileMode)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetModeIndex
//
////////////////////////////////////////////////////////////////////////////////

size_t ControllersPageState::GetModeIndex (ProfileMode mode)
{
    return static_cast<size_t> (mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadEditedProfile
//
//  m_editedProfile is the edited controller's entry in the map of the page's
//  kind: read from the map when Editing moves to a controller, written back
//  to it each time it changes. An entry that holds a profile of another kind is
//  no choice, so the kind's built-in profile is edited.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::LoadEditedProfile()
{
    auto  found = GetModeProfiles().end();



    m_editedProfile.clear();
    m_assignedTargets.clear();

    if (!m_selected.has_value() || m_selected.value() >= m_controllers.size())
    {
        return;
    }

    found = GetModeProfiles().find (ControllerTokens::UnitToToken (m_controllers[m_selected.value()].unit));

    if (found != GetModeProfiles().end() && !IsNameOfOtherMode (found->second))
    {
        m_editedProfile = found->second;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StoreEditedProfile
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::StoreEditedProfile()
{
    if (!m_selected.has_value() || m_selected.value() >= m_controllers.size())
    {
        return;
    }

    GetModeProfiles()[ControllerTokens::UnitToToken (m_controllers[m_selected.value()].unit)] = m_editedProfile;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RetargetActiveProfiles
//
//  A profile renamed or deleted on one controller is the same profile for
//  every controller of its model, so each of them that had it active follows,
//  in either mode: to the new name, or to the mode's built-in profile when
//  `to` is empty.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::RetargetActiveProfiles (const std::string & modelToken, const std::string & from, const std::string & to)
{
    ControllerUnitKey  unit;
    HRESULT            hr   = S_OK;



    for (std::map<std::string, std::string> & map : m_activeProfiles)
    {
        for (auto & entry : map)
        {
            hr = ControllerTokens::UnitFromToken (entry.first, unit);

            if (FAILED (hr) || ControllerTokens::ModelToToken (unit.model) != modelToken)
            {
                continue;
            }

            if (entry.second.size() == from.size() && _stricmp (entry.second.c_str(), from.c_str()) == 0)
            {
                entry.second = to;
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDevices
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::UpdateDevices (const std::vector<ControllerDeviceInfo> & devices)
{
    m_devices = devices;

    for (ControllerEntry & entry : m_controllers)
    {
        entry.isConnected = std::any_of (devices.begin(), devices.end(),
            [&entry] (const ControllerDeviceInfo & device) { return device.unit == entry.unit; });
    }

    for (const ControllerDeviceInfo & device : devices)
    {
        bool  isKnown = std::any_of (m_controllers.begin(), m_controllers.end(),
            [&device] (const ControllerEntry & entry) { return entry.unit == device.unit; });

        if (!isKnown)
        {
            m_controllers.push_back ({ device.unit, device.description, device.controls, device.formFactor, true });
        }
    }

    if (!m_selected.has_value() && !m_controllers.empty())
    {
        m_selected = 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetControllers
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<ControllersPageState::ControllerEntry> & ControllersPageState::GetControllers() const
{
    return m_controllers;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSelectedIndex
//
////////////////////////////////////////////////////////////////////////////////

std::optional<size_t> ControllersPageState::GetSelectedIndex() const
{
    return m_selected;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindController
//
//  The row the page lists a controller on, or none when it does not list
//  it, for opening the page on a controller picked somewhere else.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<size_t> ControllersPageState::FindController (const ControllerUnitKey & unit) const
{
    size_t  index = 0;



    for (index = 0; index < m_controllers.size(); index++)
    {
        if (m_controllers[index].unit == unit)
        {
            return index;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectController
//
//  Switching controllers abandons a capture or a calibration in progress:
//  both belong to the controller they were started on.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::SelectController (size_t index)
{
    if (index >= m_controllers.size())
    {
        return;
    }

    m_selected = index;
    m_capture.Cancel();
    m_calibrationStep = CalibrationStep::None;
    m_liveEvaluator.ResetRate();

    // The Profile drop-down shows this controller's own active profile, of
    // the kind its player plays.
    m_profileMode = GetEditedPlayerProfileMode();
    LoadEditedProfile();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsCalibratable
//
//  Xbox-class controllers are factory-calibrated, so the Calibrate action is
//  offered for DirectInput units only (FR-018a).
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::IsCalibratable() const
{
    const ControllerEntry *  selected = GetSelected();



    return selected != nullptr && selected->unit.model.kind == ControllerKind::DirectInput;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPlayers
//
//  The players as the service holds them. Set when the page opens and
//  whenever they change while it is open -- from the picker, or a controller
//  coming or going -- so the page never shows players the machine is not
//  playing.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::SetPlayers (const PlayerEntries & entries, const PlayerSlots & slots, size_t axisCount)
{
    m_entries   = PlayerSlotPolicy::NormalizeEntries (entries);
    m_slots     = slots;
    m_axisCount = axisCount;

    SyncPlayers();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetOnPlayerPicked
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::SetOnPlayerPicked (PlayerPickedFn onPicked)
{
    m_onPlayerPicked = std::move (onPicked);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetOnPlayerModeSet
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::SetOnPlayerModeSet (PlayerModeFn onModeSet)
{
    m_onPlayerModeSet = std::move (onModeSet);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPlayerEntries
//
////////////////////////////////////////////////////////////////////////////////

const PlayerEntries & ControllersPageState::GetPlayerEntries() const
{
    return m_entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPlayerSlots
//
////////////////////////////////////////////////////////////////////////////////

const PlayerSlots & ControllersPageState::GetPlayerSlots() const
{
    return m_slots;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAxisCount
//
////////////////////////////////////////////////////////////////////////////////

size_t ControllersPageState::GetAxisCount() const
{
    return m_axisCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsMultiplayerEnabled
//
//  AS PLAYED, not as chosen: two people are playing while both players hold
//  a controller, or both picked one. A machine that fell back to one
//  controller because a player's is not attached shows the single-player
//  settings, so the page never disagrees with the picker.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::IsMultiplayerEnabled() const
{
    return MakePlayView().isEnabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPlayerUnit
//
//  The player's pick, or the controller Automatic chose for them.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<ControllerUnitKey> ControllersPageState::GetPlayerUnit (size_t player) const
{
    if (player >= kPlayerCount)
    {
        return std::nullopt;
    }

    return MakePlayView().players[player].unit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickPlayerEntry
//
//  An entry from a player's drop-down; the player keeps its mode.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::PickPlayerEntry (size_t player, PlayerEntry entry)
{
    if (player >= kPlayerCount)
    {
        return;
    }

    ApplyPlayerEntry (player, entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPlayerMode
//
//  Applied at once, as a pick is, through the same path the picker uses. A
//  Joyport jack the other player holds is not a choice.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::SetPlayerMode (size_t player, PlayerMode mode)
{
    if (player >= kPlayerCount || m_entries[player].mode == mode || PlayerModeRules::IsModeTaken (m_entries, player, mode, m_hasJoyport))
    {
        return;
    }

    m_entries = PlayerSlotPolicy::ApplyMode (m_entries, player, mode);

    if (m_onPlayerModeSet)
    {
        m_onPlayerModeSet (player, mode);
    }

    SyncPlayers();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyPlayerEntry
//
//  The page's own copy takes the pick the way the service does, so the page
//  shows it at once, and the pick goes on to the service. Picking the other
//  player's controller returns the other player to Automatic.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::ApplyPlayerEntry (size_t player, const PlayerEntry & entry)
{
    m_entries = PlayerSlotPolicy::ApplyPick (m_entries, player, entry);

    if (m_onPlayerPicked)
    {
        m_onPlayerPicked (player, entry);
    }

    SyncPlayers();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetEntryChoices
//
//  The picker's own list for the player, from the same rules, so the page
//  and the picker offer the same entries under the same words.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<InputModeRules::PlayerChoice> ControllersPageState::GetEntryChoices (size_t player) const
{
    InputModeRules::PickerSource               source;
    std::vector<InputModeRules::PlayerChoice>  choices;



    if (player >= kPlayerCount)
    {
        return choices;
    }

    source.entries    = m_entries;
    source.slots      = m_slots;
    source.devices    = m_devices;
    source.hasJoyport = m_hasJoyport;

    for (const ControllerEntry & entry : m_controllers)
    {
        source.knownDescriptions[ControllerTokens::UnitToToken (entry.unit)] = entry.description;
    }

    choices = InputModeRules::BuildPicker (source).rows[player].choices;

    return choices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetModeChoices
//
//  The picker's own modes for the player, a jack the other player holds
//  among them and not to be chosen.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<InputModeRules::PlayerModeChoice> ControllersPageState::GetModeChoices (size_t player) const
{
    return PlayerModeRules::BuildModeChoices (m_entries, player, m_hasJoyport);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetButtonsCutNotice
//
//  Under a player whose buttons the Joyport has taken: the Joyport owns
//  all three button lines, so a player on Joystick or Paddle beside it keeps
//  its paddles and loses its buttons, as on the hardware. Empty otherwise.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPageState::GetButtonsCutNotice (size_t player) const
{
    size_t  other = (player == 0) ? 1 : 0;



    if (player >= kPlayerCount || !PlayerModeRules::AreButtonsCut (m_entries, player, m_hasJoyport))
    {
        return std::wstring();
    }

    return std::format (L"This controller's buttons are disabled because {} is using the Joyport.",
                        PlayerModeRules::GetPlayerLabel (other));
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsEditedOnJoyport
//
//  Whether the controller in Editing plays for a player in a Joyport jack,
//  which the page shows as the jack's switches rather than a stick.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::IsEditedOnJoyport() const
{
    std::optional<size_t>  player = FindHoldingPlayer();



    return player.has_value() && PlayerModeRules::IsOnJoyport (m_entries, player.value(), m_hasJoyport);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsEditedOnPaddles
//
//  Whether the controller in Editing plays for a player in Paddle or Two
//  paddles mode. A controller no player holds is taken by the kind of
//  profile the page edits for it.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::IsEditedOnPaddles() const
{
    std::optional<size_t>  player = FindHoldingPlayer();



    if (!player.has_value())
    {
        return m_profileMode == ProfileMode::Paddle;
    }

    return !PlayerModeRules::IsOnJoyport (m_entries, player.value(), m_hasJoyport)
           && PlayerModeRules::IsPaddleMode (PlayerModeRules::ResolveMode (m_entries, player.value(), m_hasJoyport));
}





////////////////////////////////////////////////////////////////////////////////
//
//  AreEditedButtonsCut
//
//  Whether the controller in Editing plays for a player whose buttons the
//  Joyport has taken, which leaves its button rows on the page and disabled.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::AreEditedButtonsCut() const
{
    std::optional<size_t>  player = FindHoldingPlayer();



    return player.has_value() && PlayerModeRules::AreButtonsCut (m_entries, player.value(), m_hasJoyport);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetEditedHeading
//
//  The heading above the input picture: what the controller in Editing
//  drives, by its player's place on the game port. A controller no player
//  holds is headed by the kind of profile the page edits for it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPageState::GetEditedHeading() const
{
    std::optional<size_t>  player = FindHoldingPlayer();
    std::wstring           heading;



    if (player.has_value())
    {
        return GetPlayerHeading (player.value());
    }

    switch (m_profileMode)
    {
        case ProfileMode::Paddle:    heading = L"Paddle";                               break;
        case ProfileMode::Joyport:   heading = GetJoyportHeading (JoyportJack::None);   break;

        case ProfileMode::Joystick:
        default:                     heading = L"Joystick";                             break;
    }

    return heading;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPlayerHeading
//
//  What a player drives: its Joyport jack or jacks for a player in one, and
//  otherwise the joystick, paddle or pair of paddles its place on the game
//  port is wired to, numbered as the machine's own software numbers them.
//  A player whose place this machine does not have drives nothing, and the
//  heading says so.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPageState::GetPlayerHeading (size_t player) const
{
    PlayerTargetRules::Route  route;
    std::vector<size_t>       paddles;
    PlayerMode                mode    = PlayerMode::Joystick;



    if (player >= kPlayerCount)
    {
        return std::wstring();
    }

    if (PlayerModeRules::IsOnJoyport (m_entries, player, m_hasJoyport))
    {
        return GetJoyportHeading (GetPlayerJack (player));
    }

    route = PlayerSlotPolicy::GetPlayerRoute (m_slots, m_entries, player, m_axisCount, m_hasJoyport);

    for (const std::optional<size_t> & paddle : route.paddles)
    {
        if (paddle.has_value())
        {
            paddles.push_back (paddle.value());
        }
    }

    if (paddles.empty())
    {
        return kpszNotUsedHeading;
    }

    mode = PlayerModeRules::ResolveMode (m_entries, player, m_hasJoyport);

    if (mode == PlayerMode::TwoPaddles && paddles.size() >= kPaddlesPerJoystick)
    {
        return std::format (L"Paddles {} and {}", paddles[0], paddles[1]);
    }

    if (paddles.size() == 1 || PlayerModeRules::IsPaddleMode (mode))
    {
        return std::format (L"Paddle {}", paddles[0]);
    }

    return std::format (L"Joystick {}", paddles[0] / kPaddlesPerJoystick);
}




////////////////////////////////////////////////////////////////////////////////
//
//  GetPlayerJack
//
//  The Joyport jack or jacks a player drives: both while it drives alone,
//  its own otherwise, and the jack its mode gives it while it drives none.
//  None for a player in no jack.
//
////////////////////////////////////////////////////////////////////////////////

JoyportJack ControllersPageState::GetPlayerJack (size_t player) const
{
    JoyportJackRules::JackSources          sources = JoyportJackRules::AssignJacks (JoyportJackRules::ReducePlayers (m_slots, m_entries, m_hasJoyport));
    std::bitset<JoyportJacks::kJackCount>  jacks   = JoyportJackRules::GetPlayerJacks (sources, player);
    std::optional<size_t>                  own;



    if (!PlayerModeRules::IsOnJoyport (m_entries, player, m_hasJoyport))
    {
        return JoyportJack::None;
    }

    if (jacks.all())
    {
        return JoyportJack::Both;
    }

    own = PlayerModeRules::GetJack (PlayerModeRules::ResolveMode (m_entries, player, m_hasJoyport));

    if (jacks.test (JoyportJacks::kRightJack) || own == std::optional<size_t> (JoyportJacks::kRightJack))
    {
        return JoyportJack::Right;
    }

    return JoyportJack::Left;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakePlayView
//
//  The players as they play, one controller and one target each: the pick,
//  or what Automatic chose.
//
////////////////////////////////////////////////////////////////////////////////

MultiplayerSetup ControllersPageState::MakePlayView() const
{
    return PlayerSlotPolicy::MakeSetupView (m_entries, m_slots);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetJoyportJack
//
//  Which Joyport jack the controller in Editing drives: its player's, both
//  when that player drives alone. None when nothing is being edited, or the
//  controller's player is in no jack.
//
////////////////////////////////////////////////////////////////////////////////

JoyportJack ControllersPageState::GetJoyportJack() const
{
    std::optional<size_t>  player = FindHoldingPlayer();



    if (!player.has_value())
    {
        return JoyportJack::None;
    }

    return GetPlayerJack (player.value());
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetJoyportHeading
//
//  The heading above the switch lights: the Atari joystick the controller
//  in Editing is, and the jack it drives.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPageState::GetJoyportHeading (JoyportJack jack)
{
    std::wstring  heading = L"Atari joystick";
    std::wstring  note    = GetJoyportJackNote (jack);



    if (!note.empty())
    {
        heading += L": " + note;
    }

    return heading;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetJoyportJackNote
//
//  Empty for no jack.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPageState::GetJoyportJackNote (JoyportJack jack)
{
    std::wstring  note;



    switch (jack)
    {
        case JoyportJack::Left:   note = L"left jack";   break;
        case JoyportJack::Right:  note = L"right jack";  break;
        case JoyportJack::Both:   note = L"both jacks";  break;

        case JoyportJack::None:
        default:                                         break;
    }

    return note;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsJoyportTarget
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::IsJoyportTarget (PaddleTarget target)
{
    return target == PaddleTarget::Pdl0 || target == PaddleTarget::Pdl1 || target == PaddleTarget::Pb0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetJoyportRowLabel
//
//  Empty for a target the Joyport does not use.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPageState::GetJoyportRowLabel (PaddleTarget target)
{
    std::wstring  label;



    switch (target)
    {
        case PaddleTarget::Pdl0:
            label = L"Left/right:";
            break;

        case PaddleTarget::Pdl1:
            label = L"Up/down:";
            break;

        case PaddleTarget::Pb0:
            label = L"Fire:";
            break;

        default:
            break;
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindEditedPlayer
//
////////////////////////////////////////////////////////////////////////////////

std::optional<size_t> ControllersPageState::FindEditedPlayer() const
{
    const ControllerEntry *  selected = GetSelected();



    if (selected == nullptr)
    {
        return std::nullopt;
    }

    return ControllerSelectionPolicy::FindPlayer (MakePlayView(), selected->unit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsTargetInPlay
//
//  Which of the edited controller's rows the page shows. A row left in play
//  is one the controller being edited actually drives; the rest keep their
//  bindings in the profile.
//
//  THE PLAYER'S ROUTE DECIDES. The player's paddles take the mapping's axis
//  targets in ascending order, so a player in Paddle mode plays PDL0 alone,
//  and its button bindings reach the lines its place is wired to: PB0 and
//  PB1 for joystick 0, PB2 alone for joystick 1, one line for a paddle. A
//  player whose paddles this machine does not have plays nothing at all, so
//  nothing of theirs is in play either. A controller in no slot while two
//  play drives nothing; with one playing or none it is edited whole. A
//  player in a Joyport jack plays the rows the jack reads.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::IsTargetInPlay (PaddleTarget target) const
{
    std::optional<size_t>     player = FindHoldingPlayer();
    PlayerTargetRules::Route  route;



    if (!player.has_value())
    {
        return !MakePlayView().isEnabled;
    }

    if (PlayerModeRules::IsOnJoyport (m_entries, player.value(), m_hasJoyport))
    {
        return IsJoyportTarget (target);
    }

    route = PlayerSlotPolicy::GetPlayerRoute (m_slots, m_entries, player.value(), m_axisCount, m_hasJoyport);

    if (PlayerTargetRules::CountPaddles (route) == 0)
    {
        return false;
    }

    switch (target)
    {
        case PaddleTarget::Pdl0:  return route.paddles[0].has_value();
        case PaddleTarget::Pdl1:  return route.paddles[1].has_value();
        case PaddleTarget::Pb0:   return route.buttons[0].has_value();
        case PaddleTarget::Pb1:   return route.buttons[1].has_value();
        case PaddleTarget::Pb2:   return route.buttons[2].has_value();

        default:                  return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTargetPlayLabel
//
//  What the guest reads a row on, when that differs from the row's own
//  target: a player on joystick 1 drives PDL2 and PDL3, and its PB0
//  bindings drive PB2. Zero-based throughout, matching what the machine's own
//  software calls these: a paddle game reads PDL(0). Empty where the guest
//  reads the row on its own target, and for a controller in no slot.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPageState::GetTargetPlayLabel (PaddleTarget target) const
{
    static constexpr const wchar_t *  s_kPaddleNames[] = { L"PDL0:", L"PDL1:", L"PDL2:", L"PDL3:" };
    static constexpr const wchar_t *  s_kButtonNames[] = { L"PB0:", L"PB1:", L"PB2:" };
    constexpr size_t                  kPb2             = 2;
    std::optional<size_t>             player           = FindHoldingPlayer();
    PlayerTargetRules::Route          route;
    std::optional<size_t>             line;
    size_t                            own              = 0;
    bool                              isAxis           = target == PaddleTarget::Pdl0 || target == PaddleTarget::Pdl1;



    if (!player.has_value())
    {
        return L"";
    }

    route = PlayerSlotPolicy::GetPlayerRoute (m_slots, m_entries, player.value(), m_axisCount, m_hasJoyport);

    switch (target)
    {
        case PaddleTarget::Pdl0:  own = 0;     line = route.paddles[own];  break;
        case PaddleTarget::Pdl1:  own = 1;     line = route.paddles[own];  break;
        case PaddleTarget::Pb0:   own = 0;     line = route.buttons[own];  break;
        case PaddleTarget::Pb1:   own = 1;     line = route.buttons[own];  break;
        case PaddleTarget::Pb2:   own = kPb2;  line = route.buttons[own];  break;

        default:                                                           break;
    }

    if (!line.has_value() || line.value() == own)
    {
        return L"";
    }

    if (isAxis)
    {
        return (line.value() < std::size (s_kPaddleNames)) ? s_kPaddleNames[line.value()] : L"";
    }

    return (line.value() < std::size (s_kButtonNames)) ? s_kButtonNames[line.value()] : L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindHoldingPlayer
//
//  The player whose slot holds the controller in Editing, whatever its
//  state, or none.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<size_t> ControllersPageState::FindHoldingPlayer() const
{
    const ControllerEntry  * selected = GetSelected();
    size_t                   player   = 0;



    for (player = 0; selected != nullptr && player < kPlayerCount; player++)
    {
        if (m_slots[player].holder == selected->unit)
        {
            return player;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMapping
//
//  The edited profile's mapping as edited, or the built-in profile's own
//  mapping for a model nothing has been saved or edited for.
//
////////////////////////////////////////////////////////////////////////////////

const ControlMapping & ControllersPageState::GetMapping() const
{
    const ControllerProfile *  profile  = FindEditedProfile();
    const ControllerEntry *    selected = GetSelected();



    if (profile != nullptr)
    {
        return profile->mapping;
    }

    if (selected == nullptr)
    {
        return m_emptyMapping;
    }

    m_builtInMapping = ControllerModelSettings::MakeBuiltInMapping (GetEditedBuiltInKind(), selected->unit.model, selected->formFactor, selected->controls);
    return m_builtInMapping;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDeadzone
//
////////////////////////////////////////////////////////////////////////////////

float ControllersPageState::GetDeadzone() const
{
    const ControllerModelSettings *  settings = FindSelectedModel();
    const ControllerEntry *          selected = GetSelected();



    if (settings != nullptr)
    {
        return settings->deadzone;
    }

    return selected != nullptr ? DeadzoneShaper::GetDefaultDeadzone (selected->unit.model.kind) : 0.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDeadzone
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::SetDeadzone (float deadzone)
{
    ControllerModelSettings *  settings = EnsureSelectedModel();



    if (settings == nullptr || !std::isfinite (deadzone))
    {
        return;
    }

    settings->deadzone = std::clamp (deadzone, 0.0f, DeadzoneShaper::kMaxDeadzone);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsTargetAvailable
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::IsTargetAvailable (PaddleTarget target) const
{
    return target != PaddleTarget::Pb2 || m_hasPb2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetUnassignedLabel
//
//  What a binding drop-down shows for no control: None when a control could
//  be assigned, or why none can be. A target the machine lacks says so first,
//  since that holds whatever is attached; otherwise, with no controller to
//  edit, the page says there is none rather than blaming the machine.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPageState::GetUnassignedLabel (PaddleTarget target) const
{
    if (!IsTargetAvailable (target))
    {
        return L"Not supported on " + (m_machineName.empty() ? std::wstring (L"this machine") : m_machineName);
    }

    if (!m_selected.has_value())
    {
        return L"No controller attached";
    }

    return L"None";
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddAxisBinding / AddButtonBinding
//
//  Adding to one target never takes a control away from another (FR-025):
//  the page reports the sharing instead, and the user decides.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::AddAxisBinding (PaddleTarget target, const AxisBinding & binding)
{
    ControllerProfile *         profile = nullptr;
    std::vector<AxisBinding> *  list    = nullptr;



    if (!IsAxisTarget (target))
    {
        return false;
    }

    profile = EnsureEditedProfile();

    if (profile == nullptr)
    {
        return false;
    }

    list = FindAxisList (profile->mapping, target);
    list->push_back (binding);
    list->back().maxSpeed = std::clamp (binding.maxSpeed, ControllerProfileStore::kMinMaxSpeed, ControllerProfileStore::kMaxMaxSpeed);
    NoteAssigned (target, binding);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddButtonBinding
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::AddButtonBinding (PaddleTarget target, const ButtonBinding & binding)
{
    ControllerProfile *  profile = nullptr;



    if (IsAxisTarget (target) || !IsTargetAvailable (target))
    {
        return false;
    }

    profile = EnsureEditedProfile();

    if (profile == nullptr)
    {
        return false;
    }

    FindButtonList (profile->mapping, target)->push_back (binding);
    NoteAssigned (target, binding.control);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplaceAxisBinding
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::ReplaceAxisBinding (PaddleTarget target, size_t index, const AxisBinding & binding)
{
    ControllerProfile *         profile = IsAxisTarget (target) ? EnsureEditedProfile() : nullptr;
    std::vector<AxisBinding> *  list    = profile != nullptr ? FindAxisList (profile->mapping, target) : nullptr;



    if (list == nullptr || index >= list->size())
    {
        return false;
    }

    (*list)[index]          = binding;
    (*list)[index].maxSpeed = std::clamp (binding.maxSpeed, ControllerProfileStore::kMinMaxSpeed, ControllerProfileStore::kMaxMaxSpeed);
    m_liveEvaluator.ResetRate();
    NoteAssigned (target, binding);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplaceButtonBinding
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::ReplaceButtonBinding (PaddleTarget target, size_t index, const ButtonBinding & binding)
{
    ControllerProfile *           profile = (IsAxisTarget (target) || !IsTargetAvailable (target)) ? nullptr : EnsureEditedProfile();
    std::vector<ButtonBinding> *  list    = profile != nullptr ? FindButtonList (profile->mapping, target) : nullptr;



    if (list == nullptr || index >= list->size())
    {
        return false;
    }

    (*list)[index] = binding;
    NoteAssigned (target, binding.control);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemoveBinding
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::RemoveBinding (PaddleTarget target, size_t index)
{
    ControllerProfile *  profile = EnsureEditedProfile();



    if (profile == nullptr)
    {
        return false;
    }

    if (IsAxisTarget (target))
    {
        std::vector<AxisBinding> *  list = FindAxisList (profile->mapping, target);

        if (index >= list->size())
        {
            return false;
        }

        list->erase (list->begin() + (ptrdiff_t) index);
        return true;
    }

    std::vector<ButtonBinding> *  list = FindButtonList (profile->mapping, target);

    if (index >= list->size())
    {
        return false;
    }

    list->erase (list->begin() + (ptrdiff_t) index);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetInverted
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::SetInverted (PaddleTarget target, size_t index, bool inverted)
{
    ControllerProfile *         profile = IsAxisTarget (target) ? EnsureEditedProfile() : nullptr;
    std::vector<AxisBinding> *  list    = profile != nullptr ? FindAxisList (profile->mapping, target) : nullptr;



    if (list == nullptr || index >= list->size() || (*list)[index].kind != AxisBindingKind::Analog)
    {
        return false;
    }

    (*list)[index].inverted = inverted;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetResponse
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::SetResponse (PaddleTarget target, size_t index, AxisResponse response, float maxSpeed)
{
    ControllerProfile *         profile = IsAxisTarget (target) ? EnsureEditedProfile() : nullptr;
    std::vector<AxisBinding> *  list    = profile != nullptr ? FindAxisList (profile->mapping, target) : nullptr;



    if (list == nullptr || index >= list->size() || (*list)[index].kind != AxisBindingKind::Analog || !std::isfinite (maxSpeed))
    {
        return false;
    }

    (*list)[index].response = response;
    (*list)[index].maxSpeed = std::clamp (maxSpeed, ControllerProfileStore::kMinMaxSpeed, ControllerProfileStore::kMaxMaxSpeed);
    m_liveEvaluator.ResetRate();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetThreshold
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::SetThreshold (PaddleTarget target, size_t index, float threshold)
{
    ControllerProfile *           profile = IsAxisTarget (target) ? nullptr : EnsureEditedProfile();
    std::vector<ButtonBinding> *  list    = profile != nullptr ? FindButtonList (profile->mapping, target) : nullptr;



    if (list == nullptr || index >= list->size() || !std::isfinite (threshold) || threshold <= 0.0f || threshold > 1.0f)
    {
        return false;
    }

    (*list)[index].threshold = threshold;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetProfileNames
//
//  A model with nothing saved still lists the mode's built-in profile, which
//  is what it plays with.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ControllersPageState::GetProfileNames() const
{
    const ControllerModelSettings *  settings = FindSelectedModel();



    if (settings == nullptr)
    {
        return ControllerModelSettings().GetProfileNames (m_profileMode);
    }

    return settings->GetProfileNames (m_profileMode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCopySourceNames
//
//  The page mode's profiles, pending edits included. Its built-in profile
//  leads them only once its mapping is no longer the built-in mapping: until
//  then a copy of it would only duplicate the built-in mapping, which is a
//  starting point of its own.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ControllersPageState::GetCopySourceNames() const
{
    const ControllerEntry *          selected = GetSelected();
    const ControllerModelSettings *  settings = FindSelectedModel();
    const ControllerProfile *        builtIn  = nullptr;
    ControllerProfileKind            kind     = ControllerModelSettings::GetAutomaticKind (m_profileMode);
    ControllerModelSettings          empty;
    std::vector<std::string>         names;
    bool                             isEdited = false;



    if (selected == nullptr)
    {
        return names;
    }

    if (settings == nullptr)
    {
        settings = &empty;
    }

    names   = settings->GetProfileNames (m_profileMode);
    builtIn = settings->FindBuiltInProfile (kind);

    isEdited = builtIn != nullptr &&
               !(builtIn->mapping == ControllerModelSettings::MakeBuiltInMapping (kind, selected->unit.model, selected->formFactor, selected->controls));

    if (!isEdited && !names.empty())
    {
        names.erase (names.begin());
    }

    return names;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetEditedProfileName
//
////////////////////////////////////////////////////////////////////////////////

std::string ControllersPageState::GetEditedProfileName() const
{
    const ControllerProfile *  profile = FindEditedProfile();



    if (profile != nullptr)
    {
        return profile->name;
    }

    return ControllerModelSettings::GetBuiltInName (GetEditedBuiltInKind());
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsEditingBuiltInProfile
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::IsEditingBuiltInProfile() const
{
    const ControllerProfile *  profile = FindEditedProfile();



    return profile == nullptr || profile->kind != ControllerProfileKind::User;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectProfile
//
//  The choice is recorded for the page's mode. A name the model has no
//  profile for selects no profile, which plays the mode's built-in profile,
//  and so does selecting that built-in profile itself. A profile of the
//  other mode cannot be chosen, so it leaves the choice as it was. A capture
//  in progress belongs to the profile it was started on, so it is called
//  off.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::SelectProfile (const std::string & name)
{
    const ControllerModelSettings *  settings  = FindSelectedModel();
    const ControllerProfile *        profile   = settings != nullptr ? settings->FindProfile (name) : nullptr;
    ControllerProfileKind            kind      = profile != nullptr ? profile->kind : ControllerModelSettings::GetBuiltInKind (name);
    ControllerProfileKind            automatic = ControllerModelSettings::GetAutomaticKind (m_profileMode);



    if (IsNameOfOtherMode (name))
    {
        return;
    }

    if (kind == automatic || (profile == nullptr && kind == ControllerProfileKind::User))
    {
        m_editedProfile.clear();
    }
    else
    {
        m_editedProfile = profile != nullptr ? profile->name : ControllerModelSettings::TrimProfileName (name);
    }

    StoreEditedProfile();
    m_assignedTargets.clear();

    m_capture.Cancel();
    m_liveEvaluator.ResetRate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CheckProfileName
//
//  Checked against the model's profiles as edited, with the built-in profiles
//  counted even before the model has them saved. A rename is not a duplicate of the
//  profile being renamed.
//
////////////////////////////////////////////////////////////////////////////////

ProfileEditResult ControllersPageState::CheckProfileName (const std::string & name, bool isRename) const
{
    const ControllerModelSettings *  settings  = FindSelectedModel();
    ControllerModelSettings          check     = settings != nullptr ? *settings : ControllerModelSettings();
    const ControllerProfile *        excluding = nullptr;



    // Only the built-in profiles' names matter here, not their mappings.
    check.EnsureBuiltInProfiles (ControllerModelKey(), ControllerFormFactor::Gamepad, std::vector<ControlId>());

    if (isRename)
    {
        excluding = check.FindProfile (GetEditedProfileName());
    }

    return check.CheckProfileName (name, excluding);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateProfile
//
//  The new profile belongs to the page's kind and becomes the edited one.
//  Only that kind's own starting points are accepted, so a built-in mapping
//  of another kind, or a copy of another kind's profile, is refused and
//  nothing changes. Edits on the profile it was created from stay pending
//  there.
//
////////////////////////////////////////////////////////////////////////////////

ProfileEditResult ControllersPageState::CreateProfile (const std::string & name, ProfileSource source, const std::string & sourceName)
{
    const ControllerEntry *          selected = GetSelected();
    const ControllerModelSettings *  settings = FindSelectedModel();
    const ControllerProfile *        copied   = nullptr;
    ProfileEditResult                result   = CheckProfileName (name, false);
    ControlMapping                   mapping;
    ControllerModelSettings *        target   = nullptr;



    if (result != ProfileEditResult::Ok)
    {
        return result;
    }

    if (selected == nullptr)
    {
        return ProfileEditResult::NotFound;
    }

    if (ControllerModelSettings::IsSourceOfOtherMode (source, m_profileMode))
    {
        return ProfileEditResult::NotFound;
    }

    if (source == ProfileSource::CopyOfProfile && IsNameOfOtherMode (sourceName))
    {
        return ProfileEditResult::NotFound;
    }

    if (source == ProfileSource::PaddleMapping)
    {
        mapping = ControllerModelSettings::MakeBuiltInMapping (ControllerProfileKind::Paddles, selected->unit.model, selected->formFactor, selected->controls);
    }
    else if (source == ProfileSource::JoyportMapping)
    {
        mapping = ControllerModelSettings::MakeBuiltInMapping (ControllerProfileKind::Joyport, selected->unit.model, selected->formFactor, selected->controls);
    }
    else if (source == ProfileSource::CopyOfProfile)
    {
        copied = settings != nullptr ? settings->FindProfile (sourceName) : nullptr;

        if (copied != nullptr)
        {
            mapping = copied->mapping;
        }
        else if (sourceName == GetEditedProfileName())
        {
            mapping = GetMapping();
        }
        else
        {
            return ProfileEditResult::NotFound;
        }
    }
    else
    {
        mapping = DefaultMapping::For (selected->unit.model, selected->controls);
    }

    EnsureEditedProfile();
    target = EnsureSelectedModel();
    result = target->AddProfile (name, mapping, m_profileMode);

    if (result == ProfileEditResult::Ok)
    {
        m_committedNames[ControllerTokens::ModelToToken (selected->unit.model)][ControllerModelSettings::TrimProfileName (name)] = std::string();
        SelectProfile (ControllerModelSettings::TrimProfileName (name));
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenameProfile
//
////////////////////////////////////////////////////////////////////////////////

ProfileEditResult ControllersPageState::RenameProfile (const std::string & newName)
{
    const ControllerEntry *    selected = GetSelected();
    ControllerModelSettings *  settings = nullptr;
    ProfileEditResult          result   = ProfileEditResult::Ok;
    std::string                oldName;
    std::string                token;
    std::string                committedName;



    if (IsEditingBuiltInProfile() || selected == nullptr)
    {
        return ProfileEditResult::IsBuiltInProfile;
    }

    token         = ControllerTokens::ModelToToken (selected->unit.model);
    oldName       = FindEditedProfile()->name;
    committedName = GetCommittedName (token, oldName);
    settings      = EnsureSelectedModel();
    result        = settings->RenameProfile (GetEditedProfileName(), newName);

    if (result == ProfileEditResult::Ok)
    {
        m_editedProfile = ControllerModelSettings::TrimProfileName (newName);
        RetargetActiveProfiles (token, oldName, m_editedProfile);
        StoreEditedProfile();

        m_committedNames[token].erase (oldName);
        m_committedNames[token][m_editedProfile] = committedName;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeleteProfile
//
//  Deleting the edited profile selects the Default.
//
////////////////////////////////////////////////////////////////////////////////

ProfileEditResult ControllersPageState::DeleteProfile()
{
    ControllerModelSettings *  settings = nullptr;
    ProfileEditResult          result   = ProfileEditResult::Ok;
    std::string                name;



    if (IsEditingBuiltInProfile())
    {
        return ProfileEditResult::IsBuiltInProfile;
    }

    name     = GetEditedProfileName();
    settings = EnsureSelectedModel();
    result   = settings->DeleteProfile (name);

    if (result == ProfileEditResult::Ok)
    {
        m_committedNames[ControllerTokens::ModelToToken (GetSelected()->unit.model)].erase (name);
        RetargetActiveProfiles (ControllerTokens::ModelToToken (GetSelected()->unit.model), name, std::string());
        SelectProfile (std::string());
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetProfile
//
//  The edited profile back to the built-in mapping (FR-024): each built-in
//  profile to its own, and a user's to its mode's built-in profile's. Other
//  profiles and the model's deadzone are left alone.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::ResetProfile()
{
    const ControllerEntry *  selected = GetSelected();
    ControllerProfile *      profile  = EnsureEditedProfile();



    if (selected == nullptr || profile == nullptr)
    {
        return;
    }

    profile->mapping = ControllerModelSettings::MakeBuiltInMapping (ControllerModelSettings::GetResetKind (*profile), selected->unit.model, selected->formFactor, selected->controls);
    m_liveEvaluator.ResetRate();
    m_assignedTargets.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasUnappliedProfileEdits
//
//  Judged against the committed settings, not against the profile as it was
//  when switched to, so edits stay unapplied across controller switches. A
//  profile created on the page is unapplied until it is saved.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::HasUnappliedProfileEdits() const
{
    ControlMapping  committed;



    if (!FindCommittedMapping (committed))
    {
        return true;
    }

    return !(GetMapping() == committed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiscardProfileEdits
//
//  The edited profile goes back to its committed mapping. A profile created
//  on the page has none, so discarding removes it and the Default becomes
//  the edited profile. A model that gained its settings entry only through
//  the edits being discarded loses it again, so discarding leaves nothing
//  pending.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::DiscardProfileEdits()
{
    const ControllerEntry *    selected = GetSelected();
    ControllerProfile *        profile  = nullptr;
    ControlMapping             committed;
    std::string                token;
    std::string                name;



    if (selected == nullptr || !HasUnappliedProfileEdits())
    {
        return;
    }

    profile = EnsureEditedProfile();
    token   = ControllerTokens::ModelToToken (selected->unit.model);

    if (FindCommittedMapping (committed))
    {
        profile->mapping = committed;
    }
    else
    {
        name = profile->name;
        EnsureSelectedModel()->DeleteProfile (name);
        m_committedNames[token].erase (name);
        RetargetActiveProfiles (token, name, std::string());
        m_editedProfile.clear();
        StoreEditedProfile();
    }

    if (m_baselineModels.find (token) == m_baselineModels.end())
    {
        ControllerModelSettings  fresh;

        fresh.deadzone = DeadzoneShaper::GetDefaultDeadzone (selected->unit.model.kind);
        fresh.EnsureBuiltInProfiles (selected->unit.model, selected->formFactor, selected->controls);

        if (m_models[token] == fresh)
        {
            m_models.erase (token);
        }
    }

    m_liveEvaluator.ResetRate();
    m_assignedTargets.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveProfileEdits
//
//  The whole model is committed rather than the edited profile's mapping
//  alone. Profile names and the deadzone live in the same model settings, so
//  committing part of it would leave a baseline matching neither the page nor
//  the prefs file: a pending rename would revert to two profiles. A model
//  with no settings entry as edited is dropped from the committed set.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ControllersPageState::SaveProfileEdits (const CommitFn & commit)
{
    HRESULT                                         hr        = S_OK;
    const ControllerEntry *                         selected  = GetSelected();
    std::map<std::string, ControllerModelSettings>  committed = m_baselineModels;
    bool                                            hasCommit = static_cast<bool> (commit);
    std::string                                     token;



    CBRA (hasCommit);
    CBR  (selected != nullptr);

    token = ControllerTokens::ModelToToken (selected->unit.model);

    if (m_models.find (token) != m_models.end())
    {
        committed[token] = m_models.at (token);
    }
    else
    {
        committed.erase (token);
    }

    hr = commit (committed, m_baselineCalibrations);
    CHR (hr);

    m_baselineModels = std::move (committed);
    m_committedNames.erase (token);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasActiveProfileChanged
//
//  Either mode's choices count.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::HasActiveProfileChanged() const
{
    return HasActiveProfileChanged (ProfileMode::Joystick) ||
           HasActiveProfileChanged (ProfileMode::Paddle)   ||
           HasActiveProfileChanged (ProfileMode::Joyport);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasActiveProfileChanged
//
//  A controller with no entry plays the mode's built-in profile, the same as
//  one whose entry is empty, so picking that profile for it is no change.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::HasActiveProfileChanged (ProfileMode mode) const
{
    const std::map<std::string, std::string> &  active   = GetActiveProfiles (mode);
    const std::map<std::string, std::string> &  baseline = m_baselineActiveProfiles[GetModeIndex (mode)];



    return !IsEachPlayedAlike (active, baseline) || !IsEachPlayedAlike (baseline, active);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsEachPlayedAlike
//
//  Every controller in `from` plays the same profile in `in`, where no entry
//  plays the Default.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::IsEachPlayedAlike (const std::map<std::string, std::string> & from, const std::map<std::string, std::string> & in)
{
    for (const auto & entry : from)
    {
        auto  found = in.find (entry.first);

        if ((found != in.end() ? found->second : std::string()) != entry.second)
        {
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActiveProfileName
//
//  The name as chosen rather than as resolved against the selected model, so
//  a profile chosen for one controller stays active after the page moves on
//  to a controller that has no profile of that name.
//
////////////////////////////////////////////////////////////////////////////////

const std::string & ControllersPageState::GetActiveProfileName() const
{
    return m_editedProfile;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryDescribeNameError
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::TryDescribeNameError (ProfileEditResult result, std::wstring & outLabel, std::wstring & outRule)
{
    constexpr const wchar_t *  kpszLengthRule = L"Profile names are 1-40 characters.";



    switch (result)
    {
        case ProfileEditResult::EmptyName:
            outLabel = L"Error: profile name is empty";
            outRule  = kpszLengthRule;
            return true;

        case ProfileEditResult::NameTooLong:
            outLabel = L"Error: profile name is too long";
            outRule  = kpszLengthRule;
            return true;

        case ProfileEditResult::DuplicateName:
            outLabel = L"Error: profile name is in use";
            outRule  = L"Each profile name for a controller must be different, ignoring capitalization.";
            return true;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetStartingPoints
//
//  Only the kind's own: its built-in mapping -- the Default mapping for a
//  Joystick profile, the Paddles mapping for a Paddle profile and the Joyport
//  mapping for a Joyport profile -- and a copy of one of the kind's profiles,
//  offered only when there is one to copy.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<ProfileSource> ControllersPageState::GetStartingPoints (ProfileMode mode, bool canCopy)
{
    std::vector<ProfileSource>  sources;



    switch (mode)
    {
        case ProfileMode::Paddle:    sources.push_back (ProfileSource::PaddleMapping);   break;
        case ProfileMode::Joyport:   sources.push_back (ProfileSource::JoyportMapping);  break;

        case ProfileMode::Joystick:
        default:                     sources.push_back (ProfileSource::DefaultMapping);  break;
    }

    if (canCopy)
    {
        sources.push_back (ProfileSource::CopyOfProfile);
    }

    return sources;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetStartingPointLabel
//
//  A copy's label is followed by the list of profiles to copy.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPageState::GetStartingPointLabel (ProfileSource source)
{
    const wchar_t *  pszLabel = L"Default mapping";



    switch (source)
    {
        case ProfileSource::JoyportMapping:  pszLabel = L"Joyport mapping";  break;
        case ProfileSource::CopyOfProfile:   pszLabel = L"Copy of";          break;
        case ProfileSource::PaddleMapping:   pszLabel = L"Paddles mapping";  break;

        case ProfileSource::DefaultMapping:
        default:                                                             break;
    }

    return pszLabel;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListControlUses
//
//  Every control on every target of the edited mapping that the page shows,
//  in the page's row order: PDL0, PDL1, PB0, PB1, then PB2 on a machine that
//  has it. A target out of play for the controller in Editing -- PB1 and PB2
//  in a Joyport jack, say -- keeps its bindings but is left out, so a control
//  there is not reported as shared. A D-pad pair on an axis row counts as
//  both of its directions.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::pair<ControlId, PaddleTarget>> ControllersPageState::ListControlUses() const
{
    const ControlMapping &                           mapping = GetMapping();
    std::vector<std::pair<ControlId, PaddleTarget>>  uses;



    auto addAxes = [&uses] (const std::vector<AxisBinding> & bindings, PaddleTarget target)
    {
        for (const AxisBinding & binding : bindings)
        {
            if (binding.kind == AxisBindingKind::DigitalPair)
            {
                uses.push_back ({ binding.negative, target });
                uses.push_back ({ binding.positive, target });
            }
            else
            {
                uses.push_back ({ binding.analog, target });
            }
        }
    };

    auto addButtons = [&uses] (const std::vector<ButtonBinding> & bindings, PaddleTarget target)
    {
        for (const ButtonBinding & binding : bindings)
        {
            uses.push_back ({ binding.control, target });
        }
    };

    addAxes    (mapping.pdl0, PaddleTarget::Pdl0);
    addAxes    (mapping.pdl1, PaddleTarget::Pdl1);
    addButtons (mapping.pb0,  PaddleTarget::Pb0);
    addButtons (mapping.pb1,  PaddleTarget::Pb1);

    if (m_hasPb2)
    {
        addButtons (mapping.pb2, PaddleTarget::Pb2);
    }

    std::erase_if (uses, [this] (const auto & use) { return !IsTargetInPlay (use.second); });

    return uses;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetControlTargets
//
//  Each target once, however many of its rows hold the control.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<PaddleTarget> ControllersPageState::GetControlTargets (const ControlId & control) const
{
    std::vector<PaddleTarget>  targets;



    for (const auto & use : ListControlUses())
    {
        bool  isListed = std::find (targets.begin(), targets.end(), use.second) != targets.end();

        if (use.first == control && !isListed)
        {
            targets.push_back (use.second);
        }
    }

    return targets;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSharedControls
//
//  Every control on more than one target, in the order of its first row.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<ControlId> ControllersPageState::GetSharedControls() const
{
    std::vector<ControlId>  shared;



    for (const auto & use : ListControlUses())
    {
        bool  isListed = std::find (shared.begin(), shared.end(), use.first) != shared.end();

        if (!isListed && GetControlTargets (use.first).size() > 1)
        {
            shared.push_back (use.first);
        }
    }

    return shared;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSharedWarningTarget
//
//  Where the page puts the warning for a control on more than one target.
//  Just after an edit that shared it, that is the target the edit put it on,
//  so the warning shows beside the rows the user changed. A control shared
//  when its mapping was loaded or switched to has no such target, and its
//  warning goes under the last of its targets, below every row it is on.
//
////////////////////////////////////////////////////////////////////////////////

PaddleTarget ControllersPageState::GetSharedWarningTarget (const ControlId & control) const
{
    std::vector<PaddleTarget>  targets = GetControlTargets (control);
    PaddleTarget               result  = targets.empty() ? PaddleTarget::Pdl0 : targets.back();



    for (const auto & assigned : m_assignedTargets)
    {
        bool  isStillThere = std::find (targets.begin(), targets.end(), assigned.second) != targets.end();

        if (assigned.first == control && isStillThere)
        {
            result = assigned.second;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NoteAssigned
//
//  A D-pad pair records both of its directions.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::NoteAssigned (PaddleTarget target, const AxisBinding & binding)
{
    if (binding.kind == AxisBindingKind::DigitalPair)
    {
        NoteAssigned (target, binding.negative);
        NoteAssigned (target, binding.positive);
    }
    else
    {
        NoteAssigned (target, binding.analog);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NoteAssigned
//
//  One record per control, holding the latest target.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::NoteAssigned (PaddleTarget target, const ControlId & control)
{
    std::erase_if (m_assignedTargets, [&control] (const auto & assigned) { return assigned.first == control; });

    m_assignedTargets.push_back ({ control, target });
}





////////////////////////////////////////////////////////////////////////////////
//
//  JoinWithAnd
//
//  Commas between the items and "and" before the last, with no comma ahead
//  of it. Empty for no items.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPageState::JoinWithAnd (const std::vector<std::wstring> & items)
{
    std::wstring  joined;
    size_t        i      = 0;



    for (i = 0; i < items.size(); i++)
    {
        if (i > 0)
        {
            joined += (i + 1 == items.size()) ? L" and " : L", ";
        }

        joined += items[i];
    }

    return joined;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginCapture
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::BeginCapture (PaddleTarget target, const ControllerSample & baseline, std::optional<size_t> replaceIndex)
{
    const ControllerEntry *  selected = GetSelected();
    std::vector<ControlId>   controls;



    if (selected == nullptr || !IsTargetAvailable (target))
    {
        return;
    }

    // An axis takes an analog control or a D-pad direction, which brings its
    // opposite along as a pair. A plain button has no opposite, so it is not
    // offered.
    for (const ControlId & control : selected->controls)
    {
        if (!IsAxisTarget (target) || control.kind != ControlKind::Button)
        {
            controls.push_back (control);
        }
    }

    m_captureTarget  = target;
    m_captureReplace = replaceIndex;
    m_capture.Begin (baseline, controls);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FeedCapture
//
//  An analog control captured for an axis target becomes an analog binding;
//  a digital one becomes both halves of a pair, which the user then splits
//  into its two directions. For a button target, whatever was activated is
//  the button, with an axis's direction and a trigger's early threshold.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::FeedCapture (const ControllerSample & sample)
{
    std::optional<CapturedControl>  captured = m_capture.Feed (sample);
    bool                            isAnalog = false;



    if (!captured.has_value())
    {
        return false;
    }

    isAnalog = captured->control.kind == ControlKind::Axis || captured->control.kind == ControlKind::Trigger;

    if (IsAxisTarget (m_captureTarget))
    {
        AxisBinding  binding;

        if (isAnalog)
        {
            binding.analog = captured->control;
        }
        else
        {
            bool  isHorizontal = captured->control.kind == ControlKind::DpadLeft || captured->control.kind == ControlKind::DpadRight;

            binding.kind     = AxisBindingKind::DigitalPair;
            binding.negative = { isHorizontal ? ControlKind::DpadLeft  : ControlKind::DpadUp,   captured->control.index };
            binding.positive = { isHorizontal ? ControlKind::DpadRight : ControlKind::DpadDown, captured->control.index };
        }

        if (m_captureReplace.has_value() && ReplaceAxisBinding (m_captureTarget, m_captureReplace.value(), binding))
        {
            return true;
        }

        return AddAxisBinding (m_captureTarget, binding);
    }

    ButtonBinding  binding;

    binding.control           = captured->control;
    binding.negativeDirection = captured->negativeDirection;

    if (captured->control.kind == ControlKind::Trigger)
    {
        binding.threshold = ButtonBinding::kTriggerThreshold;
    }

    if (m_captureReplace.has_value() && ReplaceButtonBinding (m_captureTarget, m_captureReplace.value(), binding))
    {
        return true;
    }

    return AddButtonBinding (m_captureTarget, binding);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CancelCapture / IsCapturing
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::CancelCapture()
{
    m_capture.Cancel();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsCapturing
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::IsCapturing() const
{
    return m_capture.IsActive();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginCalibration
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::BeginCalibration()
{
    if (!IsCalibratable())
    {
        return;
    }

    m_calibrationStep  = CalibrationStep::Center;
    m_calibrationLast  = ControllerSample();
    m_calibrationDraft = ControllerCalibration();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FeedCalibration
//
//  During Center, the latest reading is kept for when the user moves on.
//  During Travel, every reading widens the limits.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::FeedCalibration (const ControllerSample & sample)
{
    size_t  i = 0;



    m_calibrationLast = sample;

    if (m_calibrationStep != CalibrationStep::Travel)
    {
        return;
    }

    for (i = 0; i < m_calibrationDraft.axes.size(); i++)
    {
        m_calibrationDraft.axes[i].minimum = std::min (m_calibrationDraft.axes[i].minimum, sample.axes[i]);
        m_calibrationDraft.axes[i].maximum = std::max (m_calibrationDraft.axes[i].maximum, sample.axes[i]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdvanceCalibration
//
//  From Center: the stick's last reading is its center. From Travel: the
//  measured calibration becomes the selected unit's pending user calibration.
//
//  AN AXIS THAT SHOWED NO TRAVEL ON A SIDE GETS THE FULL RANGE ON THAT SIDE.
//  A user calibration must have its center strictly between its limits to be
//  saved at all, and an axis the user did not move -- a throttle, a pedal
//  axis with nothing attached -- should read as it arrives rather than
//  block the calibration of the stick they did move.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::AdvanceCalibration()
{
    constexpr float          kMinimumSide = 0.01f;
    const ControllerEntry *  selected     = GetSelected();
    size_t                   i            = 0;



    if (selected == nullptr)
    {
        m_calibrationStep = CalibrationStep::None;
        return;
    }

    if (m_calibrationStep == CalibrationStep::Center)
    {
        m_calibrationDraft.mode = CalibrationMode::User;

        for (i = 0; i < m_calibrationDraft.axes.size(); i++)
        {
            float  center = m_calibrationLast.axes[i];

            m_calibrationDraft.axes[i] = { center, center, center };
        }

        m_calibrationStep = CalibrationStep::Travel;
        return;
    }

    if (m_calibrationStep != CalibrationStep::Travel)
    {
        return;
    }

    for (i = 0; i < m_calibrationDraft.axes.size(); i++)
    {
        AxisCalibration &  axis = m_calibrationDraft.axes[i];

        if (axis.center - axis.minimum < kMinimumSide)
        {
            axis.minimum = -1.0f;
        }

        if (axis.maximum - axis.center < kMinimumSide)
        {
            axis.maximum = 1.0f;
        }

        if (!ControllerCalibration::IsValid (axis, CalibrationMode::User))
        {
            axis = { 0.0f, -1.0f, 1.0f };
        }
    }

    m_calibrations[ControllerTokens::UnitToToken (selected->unit)] = m_calibrationDraft;
    m_calibrationStep = CalibrationStep::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CancelCalibration / GetCalibrationStep
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::CancelCalibration()
{
    m_calibrationStep = CalibrationStep::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCalibrationStep
//
////////////////////////////////////////////////////////////////////////////////

CalibrationStep ControllersPageState::GetCalibrationStep() const
{
    return m_calibrationStep;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UseAutomaticCalibration
//
//  Drops the unit's saved calibration, so it is learned again from its next
//  connect (FR-007a).
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::UseAutomaticCalibration()
{
    const ControllerEntry *  selected = GetSelected();



    if (!IsCalibratable() || selected == nullptr)
    {
        return;
    }

    m_calibrations.erase (ControllerTokens::UnitToToken (selected->unit));
    m_calibrationStep = CalibrationStep::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeLiveReading
//
//  A saved automatic calibration's center is captured by the service at
//  connect, which a page cannot replay, so only a user calibration is
//  applied here; an automatic one reads as the controller reports it.
//
////////////////////////////////////////////////////////////////////////////////

GamePortContribution ControllersPageState::ComputeLiveReading (const ControllerSample & sample)
{
    const ControllerEntry *  selected   = GetSelected();
    ControllerSample         calibrated = sample;



    if (selected == nullptr)
    {
        return GamePortContribution();
    }

    if (selected->unit.model.kind == ControllerKind::DirectInput)
    {
        auto  found = m_calibrations.find (ControllerTokens::UnitToToken (selected->unit));

        if (found != m_calibrations.end() && found->second.mode == CalibrationMode::User)
        {
            calibrated = found->second.Apply (sample);
        }
    }

    return m_liveEvaluator.Evaluate (calibrated, GetMapping(), GetDeadzone(), MappingEvaluator::kMaxRateStep);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDirty / Revert / MarkCommitted
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::IsDirty() const
{
    return m_models != m_baselineModels || m_calibrations != m_baselineCalibrations || HasActiveProfileChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Revert
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::Revert()
{
    m_models              = m_baselineModels;
    m_calibrations        = m_baselineCalibrations;
    m_activeProfiles = m_baselineActiveProfiles;
    m_calibrationStep     = CalibrationStep::None;

    LoadEditedProfile();

    m_committedNames.clear();
    m_capture.Cancel();
    m_liveEvaluator.ResetRate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MarkCommitted
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::MarkCommitted()
{
    m_baselineModels              = m_models;
    m_baselineCalibrations        = m_calibrations;
    m_baselineActiveProfiles = m_activeProfiles;

    m_committedNames.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetModels / GetCalibrations
//
////////////////////////////////////////////////////////////////////////////////

const std::map<std::string, ControllerModelSettings> & ControllersPageState::GetModels() const
{
    return m_models;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCalibrations
//
////////////////////////////////////////////////////////////////////////////////

const std::map<std::string, ControllerCalibration> & ControllersPageState::GetCalibrations() const
{
    return m_calibrations;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSelected
//
////////////////////////////////////////////////////////////////////////////////

const ControllersPageState::ControllerEntry * ControllersPageState::GetSelected() const
{
    if (!m_selected.has_value() || m_selected.value() >= m_controllers.size())
    {
        return nullptr;
    }

    return &m_controllers[m_selected.value()];
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSelectedModel / EnsureSelectedModel
//
//  A model is added to the settings only when something about it is
//  edited, so opening the page and pressing OK writes nothing new.
//
////////////////////////////////////////////////////////////////////////////////

const ControllerModelSettings * ControllersPageState::FindSelectedModel() const
{
    const ControllerEntry *  selected = GetSelected();



    if (selected == nullptr)
    {
        return nullptr;
    }

    auto  found = m_models.find (ControllerTokens::ModelToToken (selected->unit.model));

    return found != m_models.end() ? &found->second : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureSelectedModel
//
////////////////////////////////////////////////////////////////////////////////

ControllerModelSettings * ControllersPageState::EnsureSelectedModel()
{
    const ControllerEntry *  selected = GetSelected();
    std::string              token;



    if (selected == nullptr)
    {
        return nullptr;
    }

    token = ControllerTokens::ModelToToken (selected->unit.model);

    if (m_models.find (token) == m_models.end())
    {
        ControllerModelSettings  settings;

        settings.deadzone = DeadzoneShaper::GetDefaultDeadzone (selected->unit.model.kind);
        m_models[token]   = settings;
    }

    return &m_models[token];
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindEditedProfile
//
//  The profile the chosen name resolves to on the selected model, or its
//  Default when it has none of that name. Null when the model has no saved
//  Default.
//
////////////////////////////////////////////////////////////////////////////////

const ControllerProfile * ControllersPageState::FindEditedProfile() const
{
    const ControllerModelSettings *  settings = FindSelectedModel();
    const ControllerProfile *        profile  = nullptr;



    if (settings == nullptr)
    {
        return nullptr;
    }

    if (!m_editedProfile.empty())
    {
        profile = settings->FindProfile (m_editedProfile);
    }

    return profile != nullptr ? profile : settings->FindBuiltInProfile (GetEditedBuiltInKind());
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetEditedBuiltInKind
//
//  The built-in profile the edited controller plays when its chosen name is
//  not a profile its model has saved: always the page mode's, since the
//  other mode's cannot be chosen in it.
//
////////////////////////////////////////////////////////////////////////////////

ControllerProfileKind ControllersPageState::GetEditedBuiltInKind() const
{
    return ControllerModelSettings::GetAutomaticKind (m_profileMode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsNameOfOtherMode
//
//  On the selected model, or by name alone for a model with nothing saved.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::IsNameOfOtherMode (const std::string & name) const
{
    const ControllerModelSettings *  settings = FindSelectedModel();



    if (settings == nullptr)
    {
        return ControllerModelSettings().IsOfOtherMode (name, m_profileMode);
    }

    return settings->IsOfOtherMode (name, m_profileMode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureEditedProfile
//
//  The profile the edits go to, with the model's built-in profiles created
//  from their built-in mappings when the model has none.
//
////////////////////////////////////////////////////////////////////////////////

ControllerProfile * ControllersPageState::EnsureEditedProfile()
{
    const ControllerEntry *    selected = GetSelected();
    ControllerModelSettings *  settings = EnsureSelectedModel();
    ControllerProfile *        profile  = nullptr;



    if (selected == nullptr || settings == nullptr)
    {
        return nullptr;
    }

    settings->EnsureBuiltInProfiles (selected->unit.model, selected->formFactor, selected->controls);

    if (!m_editedProfile.empty())
    {
        profile = settings->FindProfile (m_editedProfile);
    }

    return profile != nullptr ? profile : settings->FindBuiltInProfile (GetEditedBuiltInKind());
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindCommittedMapping
//
//  What the save-or-discard prompt compares against: the edited profile's
//  mapping in the committed settings, found under the name it was committed
//  with. A model or built-in profile never committed has its built-in
//  mapping. False
//  for a profile created on the page, which has no committed mapping.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::FindCommittedMapping (ControlMapping & mapping) const
{
    const ControllerEntry *          selected  = GetSelected();
    const ControllerProfile *        edited    = FindEditedProfile();
    const ControllerModelSettings *  settings  = nullptr;
    const ControllerProfile *        profile   = nullptr;
    std::string                      token;
    ControllerProfileKind            kind      = ControllerProfileKind::User;



    if (selected == nullptr)
    {
        mapping = ControlMapping();
        return true;
    }

    token = ControllerTokens::ModelToToken (selected->unit.model);

    auto  found = m_baselineModels.find (token);

    settings = found != m_baselineModels.end() ? &found->second : nullptr;

    kind = edited != nullptr ? edited->kind : GetEditedBuiltInKind();

    if (edited != nullptr && kind == ControllerProfileKind::User)
    {
        profile = settings != nullptr ? settings->FindProfile (GetCommittedName (token, edited->name)) : nullptr;

        if (profile == nullptr)
        {
            return false;
        }
    }
    else if (settings != nullptr)
    {
        profile = settings->FindBuiltInProfile (kind);
    }

    mapping = profile != nullptr ? profile->mapping
                                 : ControllerModelSettings::MakeBuiltInMapping (kind, selected->unit.model, selected->formFactor, selected->controls);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCommittedName
//
//  An empty name, which no profile has, for a profile created on the page.
//
////////////////////////////////////////////////////////////////////////////////

std::string ControllersPageState::GetCommittedName (const std::string & token, const std::string & name) const
{
    auto  model = m_committedNames.find (token);



    if (model == m_committedNames.end())
    {
        return name;
    }

    auto  entry = model->second.find (name);

    return entry != model->second.end() ? entry->second : name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindAxisList / FindButtonList / IsAxisTarget
//
////////////////////////////////////////////////////////////////////////////////

std::vector<AxisBinding> * ControllersPageState::FindAxisList (ControlMapping & mapping, PaddleTarget target)
{
    return target == PaddleTarget::Pdl0 ? &mapping.pdl0 : &mapping.pdl1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindButtonList
//
////////////////////////////////////////////////////////////////////////////////

std::vector<ButtonBinding> * ControllersPageState::FindButtonList (ControlMapping & mapping, PaddleTarget target)
{
    switch (target)
    {
        case PaddleTarget::Pb1: return &mapping.pb1;
        case PaddleTarget::Pb2: return &mapping.pb2;
        default:                return &mapping.pb0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsAxisTarget
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::IsAxisTarget (PaddleTarget target)
{
    return target == PaddleTarget::Pdl0 || target == PaddleTarget::Pdl1;
}
