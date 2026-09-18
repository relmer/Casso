#include "Pch.h"

#include "Controllers/ControllerProfileStore.h"

#include "Controllers/ControllerTokens.h"
#include "Controllers/DeadzoneShaper.h"





static constexpr const char *  s_kpszModelsKey      = "models";
static constexpr const char *  s_kpszDeadzoneKey    = "deadzone";
static constexpr const char *  s_kpszProfilesKey    = "profiles";
static constexpr const char *  s_kpszNameKey        = "name";
static constexpr const char *  s_kpszDefaultKey     = "default";
static constexpr const char *  s_kpszMappingKey     = "mapping";
static constexpr const char *  s_kpszAnalogKey      = "analog";
static constexpr const char *  s_kpszInvertedKey    = "inverted";
static constexpr const char *  s_kpszResponseKey    = "response";
static constexpr const char *  s_kpszMaxSpeedKey    = "maxSpeed";
static constexpr const char *  s_kpszNegativeKey    = "negative";
static constexpr const char *  s_kpszPositiveKey    = "positive";
static constexpr const char *  s_kpszControlKey     = "control";
static constexpr const char *  s_kpszThresholdKey   = "threshold";
static constexpr const char *  s_kpszRateResponse   = "rate";
static constexpr const char *  s_kpszAbsoluteResp   = "absolute";
static constexpr const char *  s_kpszPdl0Key        = "pdl0";
static constexpr const char *  s_kpszPdl1Key        = "pdl1";
static constexpr const char *  s_kpszPdl2Key        = "pdl2";
static constexpr const char *  s_kpszPdl3Key        = "pdl3";
static constexpr const char *  s_kpszPb0Key         = "pb0";
static constexpr const char *  s_kpszPb1Key         = "pb1";
static constexpr const char *  s_kpszPb2Key         = "pb2";
static constexpr const char *  s_kpszCalibrationKey = "calibration";
static constexpr const char *  s_kpszModeKey        = "mode";
static constexpr const char *  s_kpszAxesKey        = "axes";
static constexpr const char *  s_kpszIndexKey       = "index";
static constexpr const char *  s_kpszCenterKey      = "center";
static constexpr const char *  s_kpszMinKey         = "min";
static constexpr const char *  s_kpszMaxKey         = "max";
static constexpr const char *  s_kpszUserMode       = "user";
static constexpr const char *  s_kpszAutomaticMode  = "automatic";





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::FindDefaultProfile
//
////////////////////////////////////////////////////////////////////////////////

const ControllerProfile * ControllerModelSettings::FindDefaultProfile() const
{
    for (const ControllerProfile & profile : profiles)
    {
        if (profile.isDefault)
        {
            return &profile;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::FindProfile
//
//  By name, ignoring case and surrounding whitespace.
//
////////////////////////////////////////////////////////////////////////////////

const ControllerProfile * ControllerModelSettings::FindProfile (const std::string & name) const
{
    std::string  trimmed = TrimProfileName (name);



    for (const ControllerProfile & profile : profiles)
    {
        if (_stricmp (profile.name.c_str(), trimmed.c_str()) == 0)
        {
            return &profile;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::FindProfile
//
////////////////////////////////////////////////////////////////////////////////

ControllerProfile * ControllerModelSettings::FindProfile (const std::string & name)
{
    const ControllerModelSettings &  self = *this;



    return const_cast<ControllerProfile *> (self.FindProfile (name));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::CheckProfileName
//
//  Length is counted in characters, not UTF-8 bytes.
//
////////////////////////////////////////////////////////////////////////////////

ProfileEditResult ControllerModelSettings::CheckProfileName (const std::string & name, const ControllerProfile * excluding) const
{
    constexpr unsigned char    kUtf8ContinuationMask = 0xC0;
    constexpr unsigned char    kUtf8ContinuationBits = 0x80;
    std::string                trimmed               = TrimProfileName (name);
    size_t                     length                = 0;
    const ControllerProfile *  existing              = nullptr;



    for (char ch : trimmed)
    {
        if (((unsigned char) ch & kUtf8ContinuationMask) != kUtf8ContinuationBits)
        {
            length++;
        }
    }

    if (trimmed.empty())
    {
        return ProfileEditResult::EmptyName;
    }

    if (length > kMaxProfileNameLength)
    {
        return ProfileEditResult::NameTooLong;
    }

    existing = FindProfile (trimmed);

    if (existing != nullptr && existing != excluding)
    {
        return ProfileEditResult::DuplicateName;
    }

    return ProfileEditResult::Ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::AddProfile
//
//  A profile added here is never the Default; the model already has one, or
//  gets it from EnsureDefaultProfile.
//
////////////////////////////////////////////////////////////////////////////////

ProfileEditResult ControllerModelSettings::AddProfile (const std::string & name, const ControlMapping & mapping)
{
    ProfileEditResult  result = CheckProfileName (name);



    if (result != ProfileEditResult::Ok)
    {
        return result;
    }

    profiles.push_back ({ TrimProfileName (name), false, mapping });
    return ProfileEditResult::Ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::RenameProfile
//
////////////////////////////////////////////////////////////////////////////////

ProfileEditResult ControllerModelSettings::RenameProfile (const std::string & name, const std::string & newName)
{
    ControllerProfile *  profile = FindProfile (name);
    ProfileEditResult    result  = ProfileEditResult::Ok;



    if (profile == nullptr)
    {
        return ProfileEditResult::NotFound;
    }

    if (profile->isDefault)
    {
        return ProfileEditResult::IsDefaultProfile;
    }

    result = CheckProfileName (newName, profile);

    if (result == ProfileEditResult::Ok)
    {
        profile->name = TrimProfileName (newName);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::DeleteProfile
//
////////////////////////////////////////////////////////////////////////////////

ProfileEditResult ControllerModelSettings::DeleteProfile (const std::string & name)
{
    const ControllerProfile *  profile = FindProfile (name);



    if (profile == nullptr)
    {
        return ProfileEditResult::NotFound;
    }

    if (profile->isDefault)
    {
        return ProfileEditResult::IsDefaultProfile;
    }

    profiles.erase (profiles.begin() + (profile - profiles.data()));
    return ProfileEditResult::Ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::ResetProfile
//
//  Any profile, the Default included, can be reset.
//
////////////////////////////////////////////////////////////////////////////////

ProfileEditResult ControllerModelSettings::ResetProfile (const std::string & name, const ControlMapping & defaultMapping)
{
    ControllerProfile *  profile = FindProfile (name);



    if (profile == nullptr)
    {
        return ProfileEditResult::NotFound;
    }

    profile->mapping = defaultMapping;
    return ProfileEditResult::Ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::EnsureDefaultProfile
//
//  A model with no Default -- never edited, or its Default was unreadable and
//  dropped on load -- gets one from the default mapping. A surviving profile
//  already called Default becomes the Default rather than gaining a second
//  profile of the same name.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerModelSettings::EnsureDefaultProfile (const ControlMapping & defaultMapping)
{
    ControllerProfile *  named = nullptr;



    if (FindDefaultProfile() != nullptr)
    {
        return;
    }

    named = FindProfile (ControllerProfile::kpszDefaultName);

    if (named != nullptr)
    {
        named->isDefault = true;
        return;
    }

    profiles.insert (profiles.begin(), { ControllerProfile::kpszDefaultName, true, defaultMapping });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::TrimProfileName
//
////////////////////////////////////////////////////////////////////////////////

std::string ControllerModelSettings::TrimProfileName (const std::string & name)
{
    constexpr const char *  kpszWhitespace = " \t\r\n";
    size_t                  first          = name.find_first_not_of (kpszWhitespace);
    size_t                  last           = name.find_last_not_of (kpszWhitespace);



    if (first == std::string::npos)
    {
        return std::string();
    }

    return name.substr (first, last - first + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FromJson
//
//  Replaces what the store holds with the section's models and
//  calibrations. Everything that cannot be used is left out and named in
//  outRejected.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerProfileStore::FromJson (const JsonValue & controllers, std::vector<std::string> & outRejected)
{
    const JsonValue *  modelsObj      = nullptr;
    const JsonValue *  calibrationObj = nullptr;



    models.clear();
    calibrations.clear();

    if (controllers.GetType() != JsonType::Object)
    {
        return;
    }

    if (controllers.HasObject (s_kpszModelsKey, modelsObj) && modelsObj != nullptr)
    {
        ReadModels (*modelsObj, outRejected);
    }

    if (controllers.HasObject (s_kpszCalibrationKey, calibrationObj) && calibrationObj != nullptr)
    {
        ReadCalibrations (*calibrationObj, outRejected);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToJson
//
//  The section handed in, with its models and calibration members replaced by
//  what the store holds. A store with nothing to save adds no member to a
//  section that had none, and returns null for a section that did not exist,
//  so an install that never touched a controller's settings carries nothing
//  for them.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ControllerProfileStore::ToJson (const JsonValue & controllers) const
{
    std::vector<std::pair<std::string, JsonValue>>  members;
    std::vector<std::pair<std::string, JsonValue>>  modelEntries;
    std::vector<std::pair<std::string, JsonValue>>  calibrationEntries;



    if (controllers.GetType() == JsonType::Object)
    {
        members = controllers.GetObjectEntries();
    }

    for (const auto & kv : models)
    {
        modelEntries.emplace_back (kv.first, WriteModel (kv.second));
    }

    for (const auto & kv : calibrations)
    {
        if (HasAnythingToSave (kv.second))
        {
            calibrationEntries.emplace_back (kv.first, WriteCalibration (kv.second));
        }
    }

    ReplaceMember (members, s_kpszModelsKey,      std::move (modelEntries));
    ReplaceMember (members, s_kpszCalibrationKey, std::move (calibrationEntries));

    if (members.empty() && controllers.GetType() != JsonType::Object)
    {
        return JsonValue();
    }

    return JsonValue (std::move (members));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDefaultSettings
//
//  A model with no saved Default profile -- never edited, or its Default was
//  unreadable and dropped -- plays with the built-in default mapping, which
//  is the Default recreated.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerProfileStore::GetDefaultSettings (
    const ControllerModelKey      & model,
    const std::vector<ControlId>  & controls,
    ControlMapping                & outMapping,
    float                         & outDeadzone) const
{
    auto                       found   = models.find (ControllerTokens::ModelToToken (model));
    const ControllerProfile *  profile = nullptr;



    outMapping  = DefaultMapping::For (model, controls);
    outDeadzone = DeadzoneShaper::GetDefaultDeadzone (model.kind);

    if (found == models.end())
    {
        return;
    }

    outDeadzone = found->second.deadzone;
    profile     = found->second.FindDefaultProfile();

    if (profile != nullptr)
    {
        outMapping = profile->mapping;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindProfile
//
//  The lookup a disk association uses: a model token and a profile name, both
//  as saved.
//
////////////////////////////////////////////////////////////////////////////////

const ControllerProfile * ControllerProfileStore::FindProfile (const std::string & modelToken, const std::string & name) const
{
    auto  found = models.find (modelToken);



    if (found == models.end())
    {
        return nullptr;
    }

    return found->second.FindProfile (name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetOrCreateModel
//
////////////////////////////////////////////////////////////////////////////////

ControllerModelSettings & ControllerProfileStore::GetOrCreateModel (const ControllerModelKey & model, const std::vector<ControlId> & controls)
{
    std::string  token = ControllerTokens::ModelToToken (model);
    auto         found = models.find (token);



    if (found == models.end())
    {
        found = models.emplace (token, ControllerModelSettings()).first;
        found->second.deadzone = DeadzoneShaper::GetDefaultDeadzone (model.kind);
    }

    found->second.EnsureDefaultProfile (DefaultMapping::For (model, controls));
    return found->second;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateProfile
//
//  The source mapping is copied before the profile is added, since adding one
//  can move the profile it was copied from.
//
////////////////////////////////////////////////////////////////////////////////

ProfileEditResult ControllerProfileStore::CreateProfile (
    const ControllerModelKey      & model,
    const std::vector<ControlId>  & controls,
    const std::string             & name,
    ProfileSource                   source,
    const std::string             & sourceName)
{
    ControllerModelSettings  & settings = GetOrCreateModel (model, controls);
    const ControllerProfile  * copied   = nullptr;
    ControlMapping             mapping  = DefaultMapping::For (model, controls);



    if (source == ProfileSource::Paddles)
    {
        mapping = DefaultMapping::MakePaddles (model, controls);
    }
    else if (source == ProfileSource::CopyOfProfile)
    {
        copied = settings.FindProfile (sourceName);

        if (copied == nullptr)
        {
            return ProfileEditResult::NotFound;
        }

        mapping = copied->mapping;
    }

    return settings.AddProfile (name, mapping);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetProfile
//
////////////////////////////////////////////////////////////////////////////////

ProfileEditResult ControllerProfileStore::ResetProfile (
    const ControllerModelKey      & model,
    const std::vector<ControlId>  & controls,
    const std::string             & name)
{
    ControllerModelSettings &  settings = GetOrCreateModel (model, controls);



    return settings.ResetProfile (name, DefaultMapping::For (model, controls));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadModels
//
//  One model at a time. A model whose token cannot be read is dropped whole,
//  since there is no model to rebuild. A model whose entry cannot be read is
//  reported and rebuilt empty, which leaves it the built-in deadzone and a
//  Default recreated from the default mapping. Within a readable model, each
//  profile stands or falls on its own, and a later profile whose name matches
//  an earlier one, ignoring case, is dropped. Only the first profile marked
//  Default stays the Default.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerProfileStore::ReadModels (const JsonValue & modelsObj, std::vector<std::string> & outRejected)
{
    for (const auto & entry : modelsObj.GetObjectEntries())
    {
        HRESULT                  hr          = S_OK;
        ControllerModelKey       model;
        ControllerModelSettings  settings;
        const JsonValue *        profilesArr = nullptr;
        double                   deadzone    = 0.0;
        bool                     hasDefault  = false;
        bool                     isReadable  = false;
        size_t                   i           = 0;

        hr = ControllerTokens::ModelFromToken (entry.first, model);

        if (FAILED (hr))
        {
            outRejected.push_back (entry.first);
            continue;
        }

        settings.deadzone = DeadzoneShaper::GetDefaultDeadzone (model.kind);

        isReadable = entry.second.GetType() == JsonType::Object &&
                     (!HasMember (entry.second, s_kpszProfilesKey) ||
                      (entry.second.HasArray (s_kpszProfilesKey, profilesArr) && profilesArr != nullptr));

        if (!isReadable)
        {
            outRejected.push_back (entry.first);
            models[entry.first] = settings;
            continue;
        }

        if (entry.second.HasNumber (s_kpszDeadzoneKey, deadzone) && std::isfinite (deadzone))
        {
            settings.deadzone = std::clamp ((float) deadzone, 0.0f, DeadzoneShaper::kMaxDeadzone);
        }

        if (entry.second.HasArray (s_kpszProfilesKey, profilesArr) && profilesArr != nullptr)
        {
            for (i = 0; i < profilesArr->GetArraySize(); i++)
            {
                ControllerProfile  profile;
                bool               isDuplicate = false;

                if (!ReadProfile (profilesArr->GetArrayElement (i), profile))
                {
                    outRejected.push_back (entry.first + " profile " + std::to_string (i + 1));
                    continue;
                }

                for (const ControllerProfile & kept : settings.profiles)
                {
                    isDuplicate = isDuplicate ||
                                  _stricmp (kept.name.c_str(), profile.name.c_str()) == 0;
                }

                if (isDuplicate)
                {
                    outRejected.push_back (entry.first + " profile " + profile.name);
                    continue;
                }

                profile.isDefault = profile.isDefault && !hasDefault;
                hasDefault        = hasDefault || profile.isDefault;

                settings.profiles.push_back (profile);
            }
        }

        models[entry.first] = settings;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadCalibrations
//
////////////////////////////////////////////////////////////////////////////////

void ControllerProfileStore::ReadCalibrations (const JsonValue & calibrationObj, std::vector<std::string> & outRejected)
{
    for (const auto & entry : calibrationObj.GetObjectEntries())
    {
        ControllerCalibration  calibration;

        if (ReadCalibration (entry.first, entry.second, calibration))
        {
            calibrations[entry.first] = calibration;
        }
        else
        {
            outRejected.push_back (entry.first);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadProfile
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerProfileStore::ReadProfile (const JsonValue & profileObj, ControllerProfile & outProfile)
{
    const JsonValue *  mappingObj = nullptr;
    bool               isDefault  = false;



    if (profileObj.GetType() != JsonType::Object ||
        !profileObj.HasString (s_kpszNameKey, outProfile.name) ||
        outProfile.name.empty() ||
        !profileObj.HasObject (s_kpszMappingKey, mappingObj))
    {
        return false;
    }

    // Tested on its own. Joined to the lookup above by ||, the null test is
    // one the code analysis on the build server does not carry to the
    // dereference below.
    if (mappingObj == nullptr)
    {
        return false;
    }

    outProfile.isDefault = profileObj.HasBool (s_kpszDefaultKey, isDefault) && isDefault;

    return ReadMapping (*mappingObj, outProfile.mapping);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadMapping
//
//  A target the mapping does not list is left empty, which is what an
//  unassigned target is. PB2 is kept for every machine; whether it is used is
//  decided when the mapping is evaluated.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerProfileStore::ReadMapping (const JsonValue & mappingObj, ControlMapping & outMapping)
{
    return ReadAxisBindings   (mappingObj, s_kpszPdl0Key, outMapping.pdl0) &&
           ReadAxisBindings   (mappingObj, s_kpszPdl1Key, outMapping.pdl1) &&
           ReadAxisBindings   (mappingObj, s_kpszPdl2Key, outMapping.pdl2) &&
           ReadAxisBindings   (mappingObj, s_kpszPdl3Key, outMapping.pdl3) &&
           ReadButtonBindings (mappingObj, s_kpszPb0Key,  outMapping.pb0)  &&
           ReadButtonBindings (mappingObj, s_kpszPb1Key,  outMapping.pb1)  &&
           ReadButtonBindings (mappingObj, s_kpszPb2Key,  outMapping.pb2);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadAxisBindings
//
//  An axis binding carries either an analog control or both halves of a
//  digital pair, never both and never neither.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerProfileStore::ReadAxisBindings (const JsonValue & mappingObj, const char * pszKey, std::vector<AxisBinding> & outBindings)
{
    const JsonValue *  bindingsArr = nullptr;
    size_t             i           = 0;



    outBindings.clear();

    if (!HasMember (mappingObj, pszKey))
    {
        return true;
    }

    if (!mappingObj.HasArray (pszKey, bindingsArr))
    {
        return false;
    }

    if (bindingsArr == nullptr)
    {
        return false;
    }

    for (i = 0; i < bindingsArr->GetArraySize(); i++)
    {
        const JsonValue &  bindingObj = bindingsArr->GetArrayElement (i);
        AxisBinding        binding;
        std::string        analog;
        std::string        negative;
        std::string        positive;
        std::string        response;
        double             maxSpeed   = AxisBinding::kDefaultMaxSpeed;
        bool               hasAnalog  = false;
        bool               hasPair    = false;
        HRESULT            hrFirst    = S_OK;
        HRESULT            hrSecond   = S_OK;

        if (bindingObj.GetType() != JsonType::Object)
        {
            return false;
        }

        hasAnalog = bindingObj.HasString (s_kpszAnalogKey, analog);
        hasPair   = bindingObj.HasString (s_kpszNegativeKey, negative) &&
                    bindingObj.HasString (s_kpszPositiveKey, positive);

        if (hasAnalog == hasPair)
        {
            return false;
        }

        if (hasPair)
        {
            binding.kind = AxisBindingKind::DigitalPair;

            hrFirst  = ControllerTokens::ControlFromToken (negative, binding.negative);
            hrSecond = ControllerTokens::ControlFromToken (positive, binding.positive);

            if (FAILED (hrFirst) || FAILED (hrSecond))
            {
                return false;
            }

            outBindings.push_back (binding);
            continue;
        }

        hrFirst = ControllerTokens::ControlFromToken (analog, binding.analog);

        if (FAILED (hrFirst))
        {
            return false;
        }

        (void) bindingObj.HasBool (s_kpszInvertedKey, binding.inverted);

        if (bindingObj.HasString (s_kpszResponseKey, response))
        {
            if (response == s_kpszRateResponse)
            {
                binding.response = AxisResponse::Rate;
            }
            else if (response != s_kpszAbsoluteResp)
            {
                return false;
            }
        }

        if (bindingObj.HasNumber (s_kpszMaxSpeedKey, maxSpeed) && std::isfinite (maxSpeed))
        {
            binding.maxSpeed = std::clamp ((float) maxSpeed, kMinMaxSpeed, kMaxMaxSpeed);
        }

        outBindings.push_back (binding);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadButtonBindings
//
//  A threshold is optional, and the default depends on the control: a
//  trigger presses early in its pull, an axis at half its travel.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerProfileStore::ReadButtonBindings (const JsonValue & mappingObj, const char * pszKey, std::vector<ButtonBinding> & outBindings)
{
    const JsonValue *  bindingsArr = nullptr;
    size_t             i           = 0;



    outBindings.clear();

    if (!HasMember (mappingObj, pszKey))
    {
        return true;
    }

    if (!mappingObj.HasArray (pszKey, bindingsArr))
    {
        return false;
    }

    if (bindingsArr == nullptr)
    {
        return false;
    }

    for (i = 0; i < bindingsArr->GetArraySize(); i++)
    {
        const JsonValue &  bindingObj = bindingsArr->GetArrayElement (i);
        ButtonBinding      binding;
        std::string        control;
        double             threshold  = 0.0;
        HRESULT            hr         = S_OK;

        if (bindingObj.GetType() != JsonType::Object || !bindingObj.HasString (s_kpszControlKey, control))
        {
            return false;
        }

        hr = ControllerTokens::ControlFromToken (control, binding.control);

        if (FAILED (hr))
        {
            return false;
        }

        if (binding.control.kind == ControlKind::Trigger)
        {
            binding.threshold = ButtonBinding::kTriggerThreshold;
        }

        if (bindingObj.HasNumber (s_kpszThresholdKey, threshold))
        {
            if (!std::isfinite (threshold) || threshold <= 0.0 || threshold > 1.0)
            {
                return false;
            }

            binding.threshold = (float) threshold;
        }

        (void) bindingObj.HasBool (s_kpszNegativeKey, binding.negativeDirection);

        outBindings.push_back (binding);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadCalibration
//
//  One entry, all or nothing. Only a DirectInput unit can own one: an
//  Xbox-class controller is factory-calibrated and keyed by model, so an
//  entry for one is data this build did not write.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerProfileStore::ReadCalibration (const std::string & token, const JsonValue & entry, ControllerCalibration & outCalibration)
{
    HRESULT            hr      = S_OK;
    ControllerUnitKey  unit;
    std::string        mode;
    const JsonValue *  axesArr = nullptr;
    size_t             i       = 0;



    hr = ControllerTokens::UnitFromToken (token, unit);

    if (FAILED (hr) || unit.model.kind != ControllerKind::DirectInput)
    {
        return false;
    }

    if (entry.GetType() != JsonType::Object ||
        !entry.HasString (s_kpszModeKey, mode) ||
        !entry.HasArray (s_kpszAxesKey, axesArr))
    {
        return false;
    }

    if (axesArr == nullptr)
    {
        return false;
    }

    outCalibration = ControllerCalibration();

    if (mode == s_kpszUserMode)
    {
        // An axis the entry does not list keeps the full, centered range, so
        // a user calibration of X and Y leaves the other axes reading as they
        // arrive.
        outCalibration.mode = CalibrationMode::User;
        outCalibration.axes.fill (AxisCalibration { 0.0f, -1.0f, 1.0f });
    }
    else if (mode != s_kpszAutomaticMode)
    {
        return false;
    }

    for (i = 0; i < axesArr->GetArraySize(); i++)
    {
        const JsonValue &  axisObj = axesArr->GetArrayElement (i);
        AxisCalibration    axis;
        int                index   = 0;
        double             center  = 0.0;
        double             minimum = 0.0;
        double             maximum = 0.0;

        if (axisObj.GetType() != JsonType::Object ||
            !axisObj.HasInt    (s_kpszIndexKey,  index)  ||
            !axisObj.HasNumber (s_kpszCenterKey, center) ||
            !axisObj.HasNumber (s_kpszMinKey,    minimum) ||
            !axisObj.HasNumber (s_kpszMaxKey,    maximum))
        {
            return false;
        }

        if (index < 0 || index >= ControllerSample::kAxisCount)
        {
            return false;
        }

        axis.center  = (float) center;
        axis.minimum = (float) minimum;
        axis.maximum = (float) maximum;

        if (!ControllerCalibration::IsValid (axis, outCalibration.mode))
        {
            return false;
        }

        outCalibration.axes[(size_t) index] = axis;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteModel
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ControllerProfileStore::WriteModel (const ControllerModelSettings & settings)
{
    std::vector<std::pair<std::string, JsonValue>>  modelObj;
    std::vector<JsonValue>                          profiles;



    for (const ControllerProfile & profile : settings.profiles)
    {
        std::vector<std::pair<std::string, JsonValue>>  profileObj;

        profileObj.emplace_back (s_kpszNameKey, JsonValue (profile.name));

        if (profile.isDefault)
        {
            profileObj.emplace_back (s_kpszDefaultKey, JsonValue (true));
        }

        profileObj.emplace_back (s_kpszMappingKey, WriteMapping (profile.mapping));
        profiles.emplace_back (std::move (profileObj));
    }

    modelObj.emplace_back (s_kpszDeadzoneKey, JsonValue ((double) settings.deadzone));
    modelObj.emplace_back (s_kpszProfilesKey, JsonValue (std::move (profiles)));

    return JsonValue (std::move (modelObj));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteMapping
//
//  Only what differs from a binding's defaults is written, so a mapping in
//  the prefs file reads the way the contract shows one.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ControllerProfileStore::WriteMapping (const ControlMapping & mapping)
{
    std::vector<std::pair<std::string, JsonValue>>  mappingObj;



    auto writeAxes = [] (const std::vector<AxisBinding> & bindings)
    {
        std::vector<JsonValue>  arr;

        for (const AxisBinding & binding : bindings)
        {
            std::vector<std::pair<std::string, JsonValue>>  obj;

            if (binding.kind == AxisBindingKind::DigitalPair)
            {
                obj.emplace_back (s_kpszNegativeKey, JsonValue (ControllerTokens::ControlToToken (binding.negative)));
                obj.emplace_back (s_kpszPositiveKey, JsonValue (ControllerTokens::ControlToToken (binding.positive)));
            }
            else
            {
                obj.emplace_back (s_kpszAnalogKey, JsonValue (ControllerTokens::ControlToToken (binding.analog)));

                if (binding.inverted)
                {
                    obj.emplace_back (s_kpszInvertedKey, JsonValue (true));
                }

                if (binding.response == AxisResponse::Rate)
                {
                    obj.emplace_back (s_kpszResponseKey, JsonValue (std::string (s_kpszRateResponse)));
                    obj.emplace_back (s_kpszMaxSpeedKey, JsonValue ((double) binding.maxSpeed));
                }
            }

            arr.emplace_back (std::move (obj));
        }

        return JsonValue (std::move (arr));
    };

    auto writeButtons = [] (const std::vector<ButtonBinding> & bindings)
    {
        std::vector<JsonValue>  arr;

        for (const ButtonBinding & binding : bindings)
        {
            std::vector<std::pair<std::string, JsonValue>>  obj;
            bool                                            isAnalog = binding.control.kind == ControlKind::Axis ||
                                                                       binding.control.kind == ControlKind::Trigger;

            obj.emplace_back (s_kpszControlKey, JsonValue (ControllerTokens::ControlToToken (binding.control)));

            if (isAnalog)
            {
                obj.emplace_back (s_kpszThresholdKey, JsonValue ((double) binding.threshold));
            }

            if (binding.negativeDirection)
            {
                obj.emplace_back (s_kpszNegativeKey, JsonValue (true));
            }

            arr.emplace_back (std::move (obj));
        }

        return JsonValue (std::move (arr));
    };

    mappingObj.emplace_back (s_kpszPdl0Key, writeAxes    (mapping.pdl0));
    mappingObj.emplace_back (s_kpszPdl1Key, writeAxes    (mapping.pdl1));

    // PDL2 and PDL3 only when bound, so a two-axis mapping is written exactly
    // as it was before the game port had four axes, and an older build reads
    // it unchanged. Absent reads back as empty.
    if (!mapping.pdl2.empty())
    {
        mappingObj.emplace_back (s_kpszPdl2Key, writeAxes (mapping.pdl2));
    }

    if (!mapping.pdl3.empty())
    {
        mappingObj.emplace_back (s_kpszPdl3Key, writeAxes (mapping.pdl3));
    }

    mappingObj.emplace_back (s_kpszPb0Key,  writeButtons (mapping.pb0));
    mappingObj.emplace_back (s_kpszPb1Key,  writeButtons (mapping.pb1));
    mappingObj.emplace_back (s_kpszPb2Key,  writeButtons (mapping.pb2));

    return JsonValue (std::move (mappingObj));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteCalibration
//
//  A user calibration writes every axis. An automatic one writes only the
//  axes that have shown travel: the rest have learned nothing worth keeping.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ControllerProfileStore::WriteCalibration (const ControllerCalibration & calibration)
{
    std::vector<std::pair<std::string, JsonValue>>  entry;
    std::vector<JsonValue>                          axes;
    bool                                            isUser = calibration.mode == CalibrationMode::User;
    size_t                                          i      = 0;



    for (i = 0; i < calibration.axes.size(); i++)
    {
        const AxisCalibration &                         axis = calibration.axes[i];
        std::vector<std::pair<std::string, JsonValue>>  axisObj;

        if (!isUser && !(axis.minimum < axis.maximum))
        {
            continue;
        }

        axisObj.emplace_back (s_kpszIndexKey,  JsonValue ((double) i));
        axisObj.emplace_back (s_kpszCenterKey, JsonValue ((double) axis.center));
        axisObj.emplace_back (s_kpszMinKey,    JsonValue ((double) axis.minimum));
        axisObj.emplace_back (s_kpszMaxKey,    JsonValue ((double) axis.maximum));

        axes.emplace_back (std::move (axisObj));
    }

    entry.emplace_back (s_kpszModeKey, JsonValue (std::string (isUser ? s_kpszUserMode : s_kpszAutomaticMode)));
    entry.emplace_back (s_kpszAxesKey, JsonValue (std::move (axes)));

    return JsonValue (std::move (entry));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasAnythingToSave
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerProfileStore::HasAnythingToSave (const ControllerCalibration & calibration)
{
    if (calibration.mode == CalibrationMode::User)
    {
        return true;
    }

    for (const AxisCalibration & axis : calibration.axes)
    {
        if (axis.minimum < axis.maximum)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasMember
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerProfileStore::HasMember (const JsonValue & obj, const char * pszKey)
{
    for (const auto & member : obj.GetObjectEntries())
    {
        if (member.first == pszKey)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplaceMember
//
//  Puts the entries into the section under the key: in place of an existing
//  member, or as a new one when there is something to add.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerProfileStore::ReplaceMember (
    std::vector<std::pair<std::string, JsonValue>>   & members,
    const char                                       * pszKey,
    std::vector<std::pair<std::string, JsonValue>>  && entries)
{
    for (auto & member : members)
    {
        if (member.first == pszKey)
        {
            member.second = JsonValue (std::move (entries));
            return;
        }
    }

    if (!entries.empty())
    {
        members.emplace_back (pszKey, JsonValue (std::move (entries)));
    }
}
