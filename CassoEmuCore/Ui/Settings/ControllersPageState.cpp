#include "Pch.h"

#include "Ui/Settings/ControllersPageState.h"

#include "Controllers/ControllerTokens.h"
#include "Controllers/DeadzoneShaper.h"





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
    m_activeProfiles         = activeProfiles;
    m_baselineActiveProfiles = activeProfiles;

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
//  LoadEditedProfile
//
//  m_editedProfile is the edited controller's entry in m_activeProfiles: read
//  from the map when Editing moves to a controller, written back to it each
//  time it changes.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::LoadEditedProfile()
{
    auto  found = m_activeProfiles.end();



    m_editedProfile.clear();

    if (!m_selected.has_value() || m_selected.value() >= m_controllers.size())
    {
        return;
    }

    found = m_activeProfiles.find (ControllerTokens::UnitToToken (m_controllers[m_selected.value()].unit));

    if (found != m_activeProfiles.end())
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

    m_activeProfiles[ControllerTokens::UnitToToken (m_controllers[m_selected.value()].unit)] = m_editedProfile;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RetargetActiveProfiles
//
//  A profile renamed or deleted on one controller is the same profile for
//  every controller of its model, so each of them that had it active follows:
//  to the new name, or to the Default when `to` is empty.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::RetargetActiveProfiles (const std::string & modelToken, const std::string & from, const std::string & to)
{
    ControllerUnitKey  unit;
    HRESULT            hr   = S_OK;



    for (auto & entry : m_activeProfiles)
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





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDevices
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::UpdateDevices (const std::vector<ControllerDeviceInfo> & devices)
{
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
            m_controllers.push_back ({ device.unit, device.description, device.controls, true });
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

    // The Profile drop-down shows this controller's own active profile.
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
//  SetMultiplayer
//
//  The machine's mode as the service holds it. Set when the page opens and
//  whenever the mode is turned on from the toolbar picker while it is open,
//  so the page never shows a setup the machine is not playing.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::SetMultiplayer (const MultiplayerSetup & setup, size_t axisCount)
{
    m_multiplayer = ControllerSelectionPolicy::Normalize (setup);
    m_axisCount   = axisCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetOnMultiplayerChanged
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::SetOnMultiplayerChanged (MultiplayerChangedFn onChanged)
{
    m_onMultiplayerChanged = std::move (onChanged);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMultiplayer / GetAxisCount / IsMultiplayerEnabled
//
////////////////////////////////////////////////////////////////////////////////

const MultiplayerSetup & ControllersPageState::GetMultiplayer() const
{
    return m_multiplayer;
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
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::IsMultiplayerEnabled() const
{
    return m_multiplayer.isEnabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMultiplayerUnit
//
//  The controller one player holds.
//
//  FILLING A SLOT MOVES IT OFF A PADDLE THE OTHER PLAYER HOLDS rather than
//  refusing it. Both slots start on joystick 0, so a user who picks a
//  controller for the second player is asking for two players, not for the
//  paddles the first one already has; leaving the choice to be emptied by
//  normalization read as the drop-down ignoring the pick.
//
//  PICKING THE OTHER PLAYER'S CONTROLLER SWAPS THE TWO. The other player takes
//  the controller this one gave up, or none if this one held none. Leaving
//  that controller out of the list, as the page once did, meant two players
//  could never trade controllers, and emptying the other slot made every
//  trade two steps.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::SetMultiplayerUnit (size_t player, const std::optional<ControllerUnitKey> & unit)
{
    std::vector<PlayerAxisTarget>     choices;
    std::optional<ControllerUnitKey>  previous;
    size_t                            other    = 0;



    if (player >= MultiplayerSetup::kPlayerCount)
    {
        return;
    }

    previous = m_multiplayer.players[player].unit;
    other    = (player == 0) ? 1 : 0;

    if (unit.has_value() && m_multiplayer.players[other].unit == unit)
    {
        m_multiplayer.players[other].unit = previous;
    }

    m_multiplayer.players[player].unit = unit;

    if (unit.has_value())
    {
        choices = ControllerSelectionPolicy::GetTargetChoices (m_multiplayer, player, m_axisCount);

        if (!choices.empty() &&
            std::find (choices.begin(), choices.end(), m_multiplayer.players[player].target) == choices.end())
        {
            m_multiplayer.players[player].target = choices.front();
        }
    }

    m_multiplayer = ControllerSelectionPolicy::Normalize (m_multiplayer);

    if (m_onMultiplayerChanged)
    {
        m_onMultiplayerChanged (m_multiplayer);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMultiplayerTarget
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::SetMultiplayerTarget (size_t player, PlayerAxisTarget target)
{
    if (player >= MultiplayerSetup::kPlayerCount)
    {
        return;
    }

    m_multiplayer.players[player].target = target;
    m_multiplayer                        = ControllerSelectionPolicy::Normalize (m_multiplayer);

    if (m_onMultiplayerChanged)
    {
        m_onMultiplayerChanged (m_multiplayer);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTargetChoices
//
////////////////////////////////////////////////////////////////////////////////

std::vector<PlayerAxisTarget> ControllersPageState::GetTargetChoices (size_t player) const
{
    return ControllerSelectionPolicy::GetTargetChoices (m_multiplayer, player, m_axisCount);
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

    return ControllerSelectionPolicy::FindPlayer (m_multiplayer, selected->unit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsTargetInPlay
//
//  What the page grays out while two people play. A row left in play is one
//  the controller being edited actually drives; the rest keep their bindings
//  and show them, because a binding the user cannot see is one they cannot
//  understand losing.
//
//  THE PADDLE COUNT DECIDES THE AXIS ROWS. A slot mapped to a joystick plays
//  the controller's PDL0 and PDL1; one mapped to a single paddle plays PDL0
//  alone, since the slot's paddles take the mapping's targets in ascending
//  order. A player whose paddles this machine does not have plays nothing at
//  all, so nothing of theirs is in play either.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::IsTargetInPlay (PaddleTarget target) const
{
    std::optional<size_t>  player = FindEditedPlayer();
    size_t                 axes   = 0;



    if (!m_multiplayer.isEnabled)
    {
        return true;
    }

    if (!player.has_value())
    {
        return false;
    }

    axes = ControllerSelectionPolicy::GetAxesForPlayer (m_multiplayer, player.value(), m_axisCount).count();

    switch (target)
    {
        case PaddleTarget::Pdl0:  return axes >= 1;
        case PaddleTarget::Pdl1:  return axes >= 2;

        // Each player drives ONE button line, and it is their PB0 bindings
        // that drive it whichever line that is (FR-039).
        case PaddleTarget::Pb0:   return axes >= 1;

        default:                  return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTargetLabel
//
//  Zero-based throughout, matching what the machine's own software calls
//  these: a paddle game reads PDL(0).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPageState::GetTargetPlayLabel (PaddleTarget target) const
{
    static constexpr const wchar_t *  s_kPaddleNames[] = { L"PDL0:", L"PDL1:", L"PDL2:", L"PDL3:" };
    std::optional<size_t>             player           = FindEditedPlayer();
    MultiplayerSetup::AxisSet         axes;
    size_t                            axis             = 0;
    size_t                            nth              = 0;



    // Outside the mode the controller drives the port on its own, so its own
    // targets are what to call them.
    if (!m_multiplayer.isEnabled || !player.has_value())
    {
        return L"";
    }

    // A player's PB0 bindings drive their own line: player one's is PB0 and
    // player two's is PB1 (FR-039), so the row is named for the line the
    // guest reads rather than for the target the profile holds.
    if (target == PaddleTarget::Pb0)
    {
        return (player.value() == 0) ? L"PB0:" : L"PB1:";
    }

    if (target != PaddleTarget::Pdl0 && target != PaddleTarget::Pdl1)
    {
        return L"";
    }

    // The slot's paddles take the mapping's axis targets in ascending order,
    // so PDL0 is the first paddle the player holds and PDL1 the second.
    axes = ControllerSelectionPolicy::GetAxesForPlayer (m_multiplayer, player.value(), m_axisCount);
    nth  = (target == PaddleTarget::Pdl0) ? 0 : 1;

    for (axis = 0; axis < axes.size(); axis++)
    {
        if (!axes.test (axis))
        {
            continue;
        }

        if (nth == 0)
        {
            return s_kPaddleNames[axis];
        }

        nth--;
    }

    return L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTargetLabel
//
//  Zero-based throughout, matching what the machine's own software calls
//  these: a paddle game reads PDL(0).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPageState::GetTargetLabel (PlayerAxisTarget target)
{
    switch (target)
    {
        case PlayerAxisTarget::Joystick0:  return L"Joystick 0 (paddles 0 and 1)";
        case PlayerAxisTarget::Joystick1:  return L"Joystick 1 (paddles 2 and 3)";
        case PlayerAxisTarget::Paddle0:    return L"Paddle 0";
        case PlayerAxisTarget::Paddle1:    return L"Paddle 1";
        case PlayerAxisTarget::Paddle2:    return L"Paddle 2";
        default:                           return L"Paddle 3";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMapping
//
//  The edited profile's mapping as edited, or the built-in default for a
//  model nothing has been saved or edited for.
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

    m_builtInMapping = DefaultMapping::For (selected->unit.model, selected->controls);
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
//  A model with nothing saved still lists its Default, which is what it
//  plays with.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ControllersPageState::GetProfileNames() const
{
    const ControllerModelSettings *  settings    = FindSelectedModel();
    const ControllerProfile *        defaultProf = settings != nullptr ? settings->FindDefaultProfile() : nullptr;
    std::vector<std::string>         names;



    names.push_back (defaultProf != nullptr ? defaultProf->name : std::string (ControllerProfile::kpszDefaultName));

    if (settings == nullptr)
    {
        return names;
    }

    for (const ControllerProfile & profile : settings->profiles)
    {
        if (&profile != defaultProf)
        {
            names.push_back (profile.name);
        }
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



    return profile != nullptr ? profile->name : std::string (ControllerProfile::kpszDefaultName);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsEditingDefaultProfile
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::IsEditingDefaultProfile() const
{
    const ControllerProfile *  profile = FindEditedProfile();



    return profile == nullptr || profile->isDefault;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectProfile
//
//  A name the model has no profile for selects the Default. A capture in
//  progress belongs to the profile it was started on, so it is called off.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::SelectProfile (const std::string & name)
{
    const ControllerModelSettings *  settings = FindSelectedModel();
    const ControllerProfile *        profile  = settings != nullptr ? settings->FindProfile (name) : nullptr;



    m_editedProfile = (profile == nullptr || profile->isDefault) ? std::string() : profile->name;
    StoreEditedProfile();

    m_capture.Cancel();
    m_liveEvaluator.ResetRate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CheckProfileName
//
//  Checked against the model's profiles as edited, with the Default counted
//  even before the model has one saved. A rename is not a duplicate of the
//  profile being renamed.
//
////////////////////////////////////////////////////////////////////////////////

ProfileEditResult ControllersPageState::CheckProfileName (const std::string & name, bool isRename) const
{
    const ControllerModelSettings *  settings  = FindSelectedModel();
    ControllerModelSettings          check     = settings != nullptr ? *settings : ControllerModelSettings();
    const ControllerProfile *        excluding = nullptr;



    check.EnsureDefaultProfile (ControlMapping());

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
//  The new profile becomes the edited one. Edits on the profile it was
//  created from stay pending there.
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

    if (source == ProfileSource::Paddles)
    {
        mapping = DefaultMapping::MakePaddles (selected->unit.model, selected->controls);
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
    result = target->AddProfile (name, mapping);

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



    if (IsEditingDefaultProfile() || selected == nullptr)
    {
        return ProfileEditResult::IsDefaultProfile;
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



    if (IsEditingDefaultProfile())
    {
        return ProfileEditResult::IsDefaultProfile;
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
//  The edited profile back to the built-in mapping (FR-024). Other profiles
//  and the model's deadzone are left alone.
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

    profile->mapping = DefaultMapping::For (selected->unit.model, selected->controls);
    m_liveEvaluator.ResetRate();
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
        fresh.EnsureDefaultProfile (DefaultMapping::For (selected->unit.model, selected->controls));

        if (m_models[token] == fresh)
        {
            m_models.erase (token);
        }
    }

    m_liveEvaluator.ResetRate();
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
//  A controller with no entry plays the Default, the same as one whose entry
//  is empty, so picking the Default for it is no change.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::HasActiveProfileChanged() const
{
    return !IsEachPlayedAlike (m_activeProfiles, m_baselineActiveProfiles) ||
           !IsEachPlayedAlike (m_baselineActiveProfiles, m_activeProfiles);
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
//  GetSharedControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<ControlId> ControllersPageState::GetSharedControls() const
{
    const ControlMapping &                           mapping = GetMapping();
    std::vector<std::pair<ControlId, PaddleTarget>>  uses;
    std::vector<ControlId>                           shared;



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

    for (const auto & use : uses)
    {
        bool  isOnAnotherTarget = std::any_of (uses.begin(), uses.end(),
            [&use] (const auto & other) { return other.first == use.first && other.second != use.second; });
        bool  isListed          = std::find (shared.begin(), shared.end(), use.first) != shared.end();

        if (isOnAnotherTarget && !isListed)
        {
            shared.push_back (use.first);
        }
    }

    return shared;
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
    m_models          = m_baselineModels;
    m_calibrations    = m_baselineCalibrations;
    m_activeProfiles  = m_baselineActiveProfiles;
    m_calibrationStep = CalibrationStep::None;

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
    m_baselineModels         = m_models;
    m_baselineCalibrations   = m_calibrations;
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

    return profile != nullptr ? profile : settings->FindDefaultProfile();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureEditedProfile
//
//  The profile the edits go to, with the model's Default created from the
//  built-in default mapping when the model has none.
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

    settings->EnsureDefaultProfile (DefaultMapping::For (selected->unit.model, selected->controls));

    if (!m_editedProfile.empty())
    {
        profile = settings->FindProfile (m_editedProfile);
    }

    if (profile != nullptr)
    {
        return profile;
    }

    for (ControllerProfile & candidate : settings->profiles)
    {
        if (candidate.isDefault)
        {
            return &candidate;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindCommittedMapping
//
//  What the save-or-discard prompt compares against: the edited profile's
//  mapping in the committed settings, found under the name it was committed
//  with. A model or Default never committed has the built-in mapping. False
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



    if (selected == nullptr)
    {
        mapping = ControlMapping();
        return true;
    }

    token = ControllerTokens::ModelToToken (selected->unit.model);

    auto  found = m_baselineModels.find (token);

    settings = found != m_baselineModels.end() ? &found->second : nullptr;

    if (edited != nullptr && !edited->isDefault)
    {
        profile = settings != nullptr ? settings->FindProfile (GetCommittedName (token, edited->name)) : nullptr;

        if (profile == nullptr)
        {
            return false;
        }
    }
    else if (settings != nullptr)
    {
        profile = settings->FindDefaultProfile();
    }

    mapping = profile != nullptr ? profile->mapping : DefaultMapping::For (selected->unit.model, selected->controls);
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
