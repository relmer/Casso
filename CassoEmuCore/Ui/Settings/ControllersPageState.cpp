#include "Pch.h"

#include "Ui/Settings/ControllersPageState.h"

#include "Controllers/ControllerTokens.h"
#include "Controllers/DeadzoneShaper.h"





static constexpr const char *  s_kpszDefaultProfileName = "Default";





////////////////////////////////////////////////////////////////////////////////
//
//  Load
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::Load (
    const std::vector<ControllerDeviceInfo>              & devices,
    const std::map<std::string, ControllerModelSettings> & models,
    const std::map<std::string, ControllerCalibration>   & calibrations,
    bool                                                   hasPb2)
{
    m_controllers.clear();
    m_selected.reset();

    m_hasPb2               = hasPb2;
    m_models               = models;
    m_calibrations         = calibrations;
    m_baselineModels       = models;
    m_baselineCalibrations = calibrations;

    m_capture.Cancel();
    m_calibrationStep = CalibrationStep::None;

    UpdateDevices (devices);

    if (!m_controllers.empty())
    {
        m_selected = 0;
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
//  GetMapping
//
//  The selected model's Default mapping as edited, or the built-in default
//  for a model nothing has been saved or edited for.
//
////////////////////////////////////////////////////////////////////////////////

const ControlMapping & ControllersPageState::GetMapping() const
{
    const ControllerModelSettings *  settings = FindSelectedModel();
    const ControllerProfile *        profile  = settings != nullptr ? settings->FindDefaultProfile() : nullptr;
    const ControllerEntry *          selected = GetSelected();



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

    profile = EnsureDefaultProfile();

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

    profile = EnsureDefaultProfile();

    if (profile == nullptr)
    {
        return false;
    }

    FindButtonList (profile->mapping, target)->push_back (binding);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemoveBinding
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPageState::RemoveBinding (PaddleTarget target, size_t index)
{
    ControllerProfile *  profile = EnsureDefaultProfile();



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
    ControllerProfile *         profile = IsAxisTarget (target) ? EnsureDefaultProfile() : nullptr;
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
    ControllerProfile *         profile = IsAxisTarget (target) ? EnsureDefaultProfile() : nullptr;
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
    ControllerProfile *           profile = IsAxisTarget (target) ? nullptr : EnsureDefaultProfile();
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
//  ResetToDefaults
//
//  The selected model's Default profile back to the built-in mapping and its
//  deadzone back to the published default (FR-024).
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPageState::ResetToDefaults()
{
    const ControllerEntry *    selected = GetSelected();
    ControllerModelSettings *  settings = EnsureSelectedModel();
    ControllerProfile *        profile  = EnsureDefaultProfile();



    if (selected == nullptr || settings == nullptr || profile == nullptr)
    {
        return;
    }

    profile->mapping   = DefaultMapping::For (selected->unit.model, selected->controls);
    settings->deadzone = DeadzoneShaper::GetDefaultDeadzone (selected->unit.model.kind);
    m_liveEvaluator.ResetRate();
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

void ControllersPageState::BeginCapture (PaddleTarget target, const ControllerSample & baseline)
{
    const ControllerEntry *  selected = GetSelected();



    if (selected == nullptr || !IsTargetAvailable (target))
    {
        return;
    }

    m_captureTarget = target;
    m_capture.Begin (baseline, selected->controls);
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
            binding.kind     = AxisBindingKind::DigitalPair;
            binding.negative = captured->control;
            binding.positive = captured->control;
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
    return m_models != m_baselineModels || m_calibrations != m_baselineCalibrations;
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
    m_calibrationStep = CalibrationStep::None;

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
    m_baselineModels       = m_models;
    m_baselineCalibrations = m_calibrations;
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
//  EnsureDefaultProfile
//
//  The Default profile the edits go to, created from the built-in default
//  mapping when the model has none.
//
////////////////////////////////////////////////////////////////////////////////

ControllerProfile * ControllersPageState::EnsureDefaultProfile()
{
    const ControllerEntry *    selected = GetSelected();
    ControllerModelSettings *  settings = EnsureSelectedModel();



    if (selected == nullptr || settings == nullptr)
    {
        return nullptr;
    }

    for (ControllerProfile & profile : settings->profiles)
    {
        if (profile.isDefault)
        {
            return &profile;
        }
    }

    settings->profiles.insert (settings->profiles.begin(),
        ControllerProfile { s_kpszDefaultProfileName, true, DefaultMapping::For (selected->unit.model, selected->controls) });

    return &settings->profiles.front();
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
