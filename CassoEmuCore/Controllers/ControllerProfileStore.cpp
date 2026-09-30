#include "Pch.h"

#include "Controllers/ControllerProfileStore.h"

#include "Config/MachineInputPrefs.h"
#include "Controllers/ControllerTokens.h"
#include "Controllers/DeadzoneShaper.h"





static constexpr const char *  s_kpszModelsKey      = "models";
static constexpr const char *  s_kpszDeadzoneKey    = "deadzone";
static constexpr const char *  s_kpszProfilesKey    = "profiles";
static constexpr const char *  s_kpszNameKey        = "name";
static constexpr const char *  s_kpszDefaultKey     = "default";
static constexpr const char *  s_kpszJoyportKey     = "joyport";
static constexpr const char *  s_kpszPaddlesKey     = "paddles";
static constexpr const char *  s_kpszProfileModeKey = "profileMode";
static constexpr const char *  s_kpszJoyportMode    = "joyport";
static constexpr const char *  s_kpszPaddleMode     = "paddle";
static constexpr const char *  s_kpszJoystickMode   = "joystick";
static constexpr const char *  s_kpszJoyportLeft    = "joyportLeft";
static constexpr const char *  s_kpszJoyportRight   = "joyportRight";
static constexpr const char *  s_kpszSameAsPlayer1  = "sameAsPlayer1";
static constexpr const char *  s_kpszTwoPaddles     = "twoPaddles";
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
static constexpr const char *  s_kpszActiveKey      = "activeProfiles";
static constexpr const char *  s_kpszPaddleActive   = "paddleActiveProfiles";
static constexpr const char *  s_kpszJoyportActive  = "joyportActiveProfiles";
static constexpr const char *  s_kpszModeKey        = "mode";
static constexpr const char *  s_kpszAxesKey        = "axes";
static constexpr const char *  s_kpszIndexKey       = "index";
static constexpr const char *  s_kpszCenterKey      = "center";
static constexpr const char *  s_kpszMinKey         = "min";
static constexpr const char *  s_kpszMaxKey         = "max";
static constexpr const char *  s_kpszUserMode       = "user";
static constexpr const char *  s_kpszAutomaticMode  = "automatic";
static constexpr const char *  s_kpszPlayersKey     = "players";
static constexpr const char *  s_kpszLastHoldersKey = "lastHolders";
static constexpr const char *  s_kpszEntryKey       = "entry";
static constexpr const char *  s_kpszControllerKey  = "controller";
static constexpr const char *  s_kpszMapsKey        = "maps";
static constexpr const char *  s_kpszPlayerModeKey  = "mode";
static constexpr const char *  s_kpszAutomaticEntry = "automatic";
static constexpr const char *  s_kpszPickEntry      = "controller";
static constexpr const char *  s_kpszKeysEntry      = "keys";
static constexpr const char *  s_kpszMouseEntry     = "mouse";
static constexpr const char *  s_kpszDisabledEntry  = "disabled";





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::FindDefaultProfile
//
////////////////////////////////////////////////////////////////////////////////

const ControllerProfile * ControllerModelSettings::FindDefaultProfile() const
{
    return FindBuiltInProfile (ControllerProfileKind::Default);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::FindBuiltInProfile
//
////////////////////////////////////////////////////////////////////////////////

const ControllerProfile * ControllerModelSettings::FindBuiltInProfile (ControllerProfileKind kind) const
{
    for (const ControllerProfile & profile : profiles)
    {
        if (profile.kind == kind)
        {
            return &profile;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::FindBuiltInProfile
//
////////////////////////////////////////////////////////////////////////////////

ControllerProfile * ControllerModelSettings::FindBuiltInProfile (ControllerProfileKind kind)
{
    const ControllerModelSettings &  self = *this;



    return const_cast<ControllerProfile *> (self.FindBuiltInProfile (kind));
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
//  ControllerModelSettings::GetProfileNames
//
//  The built-in profile is listed whether or not the model has it saved,
//  since it is what a controller of the model plays with nothing chosen.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ControllerModelSettings::GetProfileNames (ProfileMode mode) const
{
    ControllerProfileKind      builtInKind = GetAutomaticKind (mode);
    const ControllerProfile *  builtIn     = FindBuiltInProfile (builtInKind);
    std::vector<std::string>   names;



    if (builtIn != nullptr)
    {
        names.push_back (builtIn->name);
    }
    else
    {
        names.push_back (GetBuiltInName (builtInKind));
    }

    for (const ControllerProfile & profile : profiles)
    {
        if (profile.kind == ControllerProfileKind::User && profile.mode == mode)
        {
            names.push_back (profile.name);
        }
    }

    return names;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::IsOfOtherMode
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerModelSettings::IsOfOtherMode (const std::string & name, ProfileMode mode) const
{
    const ControllerProfile *  profile = FindProfile (name);
    ControllerProfileKind      kind    = GetBuiltInKind (name);



    if (profile != nullptr)
    {
        return profile->mode != mode;
    }

    if (kind != ControllerProfileKind::User)
    {
        return GetBuiltInMode (kind) != mode;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::AddProfile
//
//  A profile added here is always the user's; the built-in profiles come from
//  EnsureBuiltInProfiles. It belongs to `mode` from here on.
//
////////////////////////////////////////////////////////////////////////////////

ProfileEditResult ControllerModelSettings::AddProfile (const std::string & name, const ControlMapping & mapping, ProfileMode mode)
{
    ProfileEditResult  result = CheckProfileName (name);



    if (result != ProfileEditResult::Ok)
    {
        return result;
    }

    profiles.push_back ({ TrimProfileName (name), ControllerProfileKind::User, mapping, mode });
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

    if (profile->kind != ControllerProfileKind::User)
    {
        return ProfileEditResult::IsBuiltInProfile;
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

    if (profile->kind != ControllerProfileKind::User)
    {
        return ProfileEditResult::IsBuiltInProfile;
    }

    profiles.erase (profiles.begin() + (profile - profiles.data()));
    return ProfileEditResult::Ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::ResetProfile
//
//  Any profile, the built-in ones included, can be reset. Each built-in
//  profile goes back to its own mapping, and a user's goes back to the
//  mapping of its mode's built-in profile.
//
////////////////////////////////////////////////////////////////////////////////

ProfileEditResult ControllerModelSettings::ResetProfile (
    const std::string             & name,
    const ControllerModelKey      & model,
    ControllerFormFactor            formFactor,
    const std::vector<ControlId>  & controls)
{
    ControllerProfile *  profile = FindProfile (name);



    if (profile == nullptr)
    {
        return ProfileEditResult::NotFound;
    }

    profile->mapping = MakeBuiltInMapping (GetResetKind (*profile), model, formFactor, controls);
    return ProfileEditResult::Ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::EnsureBuiltInProfiles
//
//  A model missing a built-in profile -- never edited, saved by a build that
//  had no Joyport or Paddles profile, or its copy was unreadable and dropped
//  on load -- gets one from that profile's built-in mapping. A surviving user
//  profile of the same name becomes the built-in one rather than gaining a
//  second profile of that name, keeping its mapping and taking the built-in
//  profile's kind. The Default leads the list, then the Joyport profile, then
//  Paddles. Every built-in profile belongs to the kind it stands for.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerModelSettings::EnsureBuiltInProfiles (
    const ControllerModelKey      & model,
    ControllerFormFactor            formFactor,
    const std::vector<ControlId>  & controls)
{
    constexpr ControllerProfileKind  kKinds[] = { ControllerProfileKind::Default, ControllerProfileKind::Joyport, ControllerProfileKind::Paddles };
    ControllerProfile              * named    = nullptr;
    size_t                           i        = 0;



    for (i = 0; i < std::size (kKinds); i++)
    {
        if (FindBuiltInProfile (kKinds[i]) != nullptr)
        {
            continue;
        }

        named = FindProfile (GetBuiltInName (kKinds[i]));

        if (named != nullptr)
        {
            named->kind = kKinds[i];
            continue;
        }

        profiles.insert (profiles.begin() + std::min (i, profiles.size()),
                         { GetBuiltInName (kKinds[i]), kKinds[i], MakeBuiltInMapping (kKinds[i], model, formFactor, controls), GetBuiltInMode (kKinds[i]) });
    }

    for (ControllerProfile & profile : profiles)
    {
        if (profile.kind != ControllerProfileKind::User)
        {
            profile.mode = GetBuiltInMode (profile.kind);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::MakeBuiltInMapping
//
////////////////////////////////////////////////////////////////////////////////

ControlMapping ControllerModelSettings::MakeBuiltInMapping (
    ControllerProfileKind           kind,
    const ControllerModelKey      & model,
    ControllerFormFactor            formFactor,
    const std::vector<ControlId>  & controls)
{
    if (kind == ControllerProfileKind::Joyport)
    {
        return DefaultMapping::MakeJoyport (model, formFactor, controls);
    }

    if (kind == ControllerProfileKind::Paddles)
    {
        return DefaultMapping::MakePaddles (model, formFactor, controls);
    }

    return DefaultMapping::For (model, controls);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::GetBuiltInName
//
//  The name a built-in profile is created with; the Default's for User.
//
////////////////////////////////////////////////////////////////////////////////

const char * ControllerModelSettings::GetBuiltInName (ControllerProfileKind kind)
{
    const char *  pszName = ControllerProfile::kpszDefaultName;



    switch (kind)
    {
        case ControllerProfileKind::Joyport:  pszName = ControllerProfile::kpszJoyportName;  break;
        case ControllerProfileKind::Paddles:  pszName = ControllerProfile::kpszPaddlesName;  break;

        case ControllerProfileKind::Default:
        case ControllerProfileKind::User:
        default:                                                                           break;
    }

    return pszName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::GetAutomaticKind
//
////////////////////////////////////////////////////////////////////////////////

ControllerProfileKind ControllerModelSettings::GetAutomaticKind (ProfileMode mode)
{
    ControllerProfileKind  kind = ControllerProfileKind::Default;



    switch (mode)
    {
        case ProfileMode::Paddle:    kind = ControllerProfileKind::Paddles;  break;
        case ProfileMode::Joyport:   kind = ControllerProfileKind::Joyport;  break;

        case ProfileMode::Joystick:
        default:                                                             break;
    }

    return kind;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::GetBuiltInKind
//
////////////////////////////////////////////////////////////////////////////////

ControllerProfileKind ControllerModelSettings::GetBuiltInKind (const std::string & name)
{
    std::string            trimmed = TrimProfileName (name);
    ControllerProfileKind  kind    = ControllerProfileKind::User;



    if (_stricmp (trimmed.c_str(), ControllerProfile::kpszDefaultName) == 0)
    {
        kind = ControllerProfileKind::Default;
    }
    else if (_stricmp (trimmed.c_str(), ControllerProfile::kpszJoyportName) == 0)
    {
        kind = ControllerProfileKind::Joyport;
    }
    else if (_stricmp (trimmed.c_str(), ControllerProfile::kpszPaddlesName) == 0)
    {
        kind = ControllerProfileKind::Paddles;
    }

    return kind;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::GetBuiltInMode
//
////////////////////////////////////////////////////////////////////////////////

ProfileMode ControllerModelSettings::GetBuiltInMode (ControllerProfileKind kind)
{
    ProfileMode  mode = ProfileMode::Joystick;



    switch (kind)
    {
        case ControllerProfileKind::Paddles:  mode = ProfileMode::Paddle;   break;
        case ControllerProfileKind::Joyport:  mode = ProfileMode::Joyport;  break;

        case ControllerProfileKind::Default:
        case ControllerProfileKind::User:
        default:                                                            break;
    }

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::IsSourceOfOtherMode
//
//  Whether a built-in starting point belongs to a kind other than `mode`: the
//  default mapping to Joystick, the Paddles mapping to Paddle and the Joyport
//  mapping to Joyport. A copy belongs to none; the profile it copies decides.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerModelSettings::IsSourceOfOtherMode (ProfileSource source, ProfileMode mode)
{
    bool  isOtherMode = false;



    switch (source)
    {
        case ProfileSource::DefaultMapping:  isOtherMode = mode != ProfileMode::Joystick;  break;
        case ProfileSource::PaddleMapping:   isOtherMode = mode != ProfileMode::Paddle;    break;
        case ProfileSource::JoyportMapping:  isOtherMode = mode != ProfileMode::Joyport;   break;

        case ProfileSource::CopyOfProfile:
        default:                                                                           break;
    }

    return isOtherMode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::ClassifyLegacyProfile
//
//  A mapping that binds PDL0 and leaves PDL1 alone is one knob on one axis,
//  which is what the Paddles starting point makes. Anything else, a mapping
//  that binds neither included, is a Joystick profile.
//
////////////////////////////////////////////////////////////////////////////////

ProfileMode ControllerModelSettings::ClassifyLegacyProfile (const ControlMapping & mapping)
{
    bool  isPaddle = !mapping.pdl0.empty() && mapping.pdl1.empty();



    return isPaddle ? ProfileMode::Paddle : ProfileMode::Joystick;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::GetPlayerProfileMode
//
////////////////////////////////////////////////////////////////////////////////

ProfileMode ControllerModelSettings::GetPlayerProfileMode (PlayerMode mode)
{
    ProfileMode  kind = ProfileMode::Joystick;



    if (mode == PlayerMode::JoyportLeft || mode == PlayerMode::JoyportRight)
    {
        kind = ProfileMode::Joyport;
    }
    else if (mode == PlayerMode::Paddle || mode == PlayerMode::TwoPaddles)
    {
        kind = ProfileMode::Paddle;
    }

    return kind;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerProfileStore::PlayerModeToToken
//
////////////////////////////////////////////////////////////////////////////////

const char * ControllerProfileStore::PlayerModeToToken (PlayerMode mode)
{
    const char  * pszToken = s_kpszJoystickMode;



    switch (mode)
    {
        case PlayerMode::JoyportLeft:    pszToken = s_kpszJoyportLeft;    break;
        case PlayerMode::JoyportRight:   pszToken = s_kpszJoyportRight;   break;
        case PlayerMode::Paddle:         pszToken = s_kpszPaddleMode;     break;
        case PlayerMode::SameAsPlayer1:  pszToken = s_kpszSameAsPlayer1;  break;
        case PlayerMode::TwoPaddles:     pszToken = s_kpszTwoPaddles;     break;

        case PlayerMode::Joystick:
        default:                                                          break;
    }

    return pszToken;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerProfileStore::PlayerModeFromToken
//
//  An unrecognized mode is Joystick.
//
////////////////////////////////////////////////////////////////////////////////

PlayerMode ControllerProfileStore::PlayerModeFromToken (const std::string & token)
{
    PlayerMode  mode = PlayerMode::Joystick;



    if (token == s_kpszJoyportLeft)
    {
        mode = PlayerMode::JoyportLeft;
    }
    else if (token == s_kpszJoyportRight)
    {
        mode = PlayerMode::JoyportRight;
    }
    else if (token == s_kpszPaddleMode)
    {
        mode = PlayerMode::Paddle;
    }
    else if (token == s_kpszSameAsPlayer1)
    {
        mode = PlayerMode::SameAsPlayer1;
    }
    else if (token == s_kpszTwoPaddles)
    {
        mode = PlayerMode::TwoPaddles;
    }

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings::GetResetKind
//
////////////////////////////////////////////////////////////////////////////////

ControllerProfileKind ControllerModelSettings::GetResetKind (const ControllerProfile & profile)
{
    if (profile.kind != ControllerProfileKind::User)
    {
        return profile.kind;
    }

    return GetAutomaticKind (profile.mode);
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
    const JsonValue *  activeObj      = nullptr;
    const JsonValue *  playersArr     = nullptr;
    const JsonValue *  holdersArr     = nullptr;



    models.clear();
    calibrations.clear();
    activeProfiles.clear();
    paddleActiveProfiles.clear();
    joyportActiveProfiles.clear();
    players.reset();
    hasPlayerModes = false;
    lastHolders    = PlayerLastHolders();

    if (controllers.GetType() != JsonType::Object)
    {
        return;
    }

    // Present in any form, `players` means the adoption has run: an
    // unreadable value plays both players on Automatic rather than adopting
    // again from whichever machine happens to launch next.
    if (controllers.HasArray (s_kpszPlayersKey, playersArr))
    {
        players        = PlayerEntries();
        hasPlayerModes = HasAnyPlayerMode (*playersArr);
        ReadPlayers (*playersArr, players.value(), outRejected);
    }
    else if (HasMember (controllers, s_kpszPlayersKey))
    {
        players = PlayerEntries();
        outRejected.push_back (s_kpszPlayersKey);
    }

    if (controllers.HasArray (s_kpszLastHoldersKey, holdersArr))
    {
        ReadLastHolders (*holdersArr, lastHolders, outRejected);
    }
    else if (HasMember (controllers, s_kpszLastHoldersKey))
    {
        outRejected.push_back (s_kpszLastHoldersKey);
    }

    if (controllers.HasObject (s_kpszModelsKey, modelsObj))
    {
        ReadModels (*modelsObj, outRejected);
    }

    if (controllers.HasObject (s_kpszCalibrationKey, calibrationObj))
    {
        ReadCalibrations (*calibrationObj, outRejected);
    }

    if (controllers.HasObject (s_kpszActiveKey, activeObj))
    {
        ReadActiveProfiles (*activeObj, activeProfiles, outRejected);
    }

    if (controllers.HasObject (s_kpszPaddleActive, activeObj))
    {
        ReadActiveProfiles (*activeObj, paddleActiveProfiles, outRejected);
    }

    if (controllers.HasObject (s_kpszJoyportActive, activeObj))
    {
        ReadActiveProfiles (*activeObj, joyportActiveProfiles, outRejected);
    }

    MoveLegacyPaddleChoices();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MoveLegacyPaddleChoices
//
//  Before profiles had three kinds, a Paddle profile was chosen in the one
//  map there was for play without the Joyport, which is now the Joystick
//  map. Such a choice moves to the Paddle map, unless that controller has a
//  Paddle choice already, so the controller plays it again once its player is
//  in Paddle mode. Either way it leaves the Joystick map, where it could not
//  be played.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerProfileStore::MoveLegacyPaddleChoices()
{
    HRESULT            hr   = S_OK;
    ControllerUnitKey  unit;
    auto               it   = activeProfiles.begin();



    while (it != activeProfiles.end())
    {
        const ControllerProfile  * profile  = nullptr;
        ControllerProfileKind      builtIn  = ControllerModelSettings::GetBuiltInKind (it->second);
        bool                       isPaddle = builtIn == ControllerProfileKind::Paddles;

        hr = ControllerTokens::UnitFromToken (it->first, unit);

        if (SUCCEEDED (hr) && builtIn == ControllerProfileKind::User)
        {
            profile  = FindProfile (ControllerTokens::ModelToToken (unit.model), it->second);
            isPaddle = profile != nullptr && profile->mode == ProfileMode::Paddle;
        }

        if (!isPaddle)
        {
            ++it;
            continue;
        }

        paddleActiveProfiles.try_emplace (it->first, it->second);
        it = activeProfiles.erase (it);
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
    std::vector<std::pair<std::string, JsonValue>>  activeEntries;
    std::vector<std::pair<std::string, JsonValue>>  paddleEntries;
    std::vector<std::pair<std::string, JsonValue>>  joyportEntries;



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

    for (const auto & kv : activeProfiles)
    {
        activeEntries.emplace_back (kv.first, JsonValue (kv.second));
    }

    for (const auto & kv : paddleActiveProfiles)
    {
        paddleEntries.emplace_back (kv.first, JsonValue (kv.second));
    }

    for (const auto & kv : joyportActiveProfiles)
    {
        joyportEntries.emplace_back (kv.first, JsonValue (kv.second));
    }

    ReplaceMember (members, s_kpszModelsKey,      std::move (modelEntries));
    ReplaceMember (members, s_kpszCalibrationKey, std::move (calibrationEntries));
    ReplaceMember (members, s_kpszActiveKey,      std::move (activeEntries));
    ReplaceMember (members, s_kpszPaddleActive,   std::move (paddleEntries));
    ReplaceMember (members, s_kpszJoyportActive,  std::move (joyportEntries));

    // The players are written in full once the adoption has run, and left as
    // the section holds them before then.
    if (players.has_value())
    {
        SetMember (members, s_kpszPlayersKey,     WritePlayers (players.value()));
        SetMember (members, s_kpszLastHoldersKey, WriteLastHolders (lastHolders));
    }

    if (members.empty() && controllers.GetType() != JsonType::Object)
    {
        return JsonValue();
    }

    return JsonValue (std::move (members));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActiveProfiles
//
//  One kind's map: the Joystick map is the one saved before profiles had
//  kinds, under its old key, so an older build reads it as it always did.
//
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, std::string> & ControllerProfileStore::GetActiveProfiles (ProfileMode mode)
{
    const ControllerProfileStore &  self = *this;



    return const_cast<std::map<std::string, std::string> &> (self.GetActiveProfiles (mode));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActiveProfiles
//
////////////////////////////////////////////////////////////////////////////////

const std::map<std::string, std::string> & ControllerProfileStore::GetActiveProfiles (ProfileMode mode) const
{
    const std::map<std::string, std::string> *  profiles = &activeProfiles;



    switch (mode)
    {
        case ProfileMode::Paddle:    profiles = &paddleActiveProfiles;   break;
        case ProfileMode::Joyport:   profiles = &joyportActiveProfiles;  break;

        case ProfileMode::Joystick:
        default:                                                         break;
    }

    return *profiles;
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
    ControllerFormFactor            formFactor,
    const std::vector<ControlId>  & controls,
    ControlMapping                & outMapping,
    float                         & outDeadzone) const
{
    GetBuiltInSettings (ControllerProfileKind::Default, model, formFactor, controls, outMapping, outDeadzone);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBuiltInSettings
//
////////////////////////////////////////////////////////////////////////////////

void ControllerProfileStore::GetBuiltInSettings (
    ControllerProfileKind           kind,
    const ControllerModelKey      & model,
    ControllerFormFactor            formFactor,
    const std::vector<ControlId>  & controls,
    ControlMapping                & outMapping,
    float                         & outDeadzone) const
{
    auto                       found   = models.find (ControllerTokens::ModelToToken (model));
    const ControllerProfile *  profile = nullptr;



    outMapping  = ControllerModelSettings::MakeBuiltInMapping (kind, model, formFactor, controls);
    outDeadzone = DeadzoneShaper::GetDefaultDeadzone (model.kind);

    if (found == models.end())
    {
        return;
    }

    outDeadzone = found->second.deadzone;
    profile     = found->second.FindBuiltInProfile (kind);

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

ControllerModelSettings & ControllerProfileStore::GetOrCreateModel (
    const ControllerModelKey      & model,
    ControllerFormFactor            formFactor,
    const std::vector<ControlId>  & controls)
{
    std::string  token = ControllerTokens::ModelToToken (model);
    auto         found = models.find (token);



    if (found == models.end())
    {
        found = models.emplace (token, ControllerModelSettings()).first;
        found->second.deadzone = DeadzoneShaper::GetDefaultDeadzone (model.kind);
    }

    found->second.EnsureBuiltInProfiles (model, formFactor, controls);
    return found->second;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateProfile
//
//  The source mapping is copied before the profile is added, since adding one
//  can move the profile it was copied from. Only `mode`'s own starting points
//  are accepted: a built-in mapping of another kind, or a copy of another
//  kind's profile, is refused as not found.
//
////////////////////////////////////////////////////////////////////////////////

ProfileEditResult ControllerProfileStore::CreateProfile (
    const ControllerModelKey      & model,
    ControllerFormFactor            formFactor,
    const std::vector<ControlId>  & controls,
    const std::string             & name,
    ProfileSource                   source,
    ProfileMode                     mode,
    const std::string             & sourceName)
{
    ControllerModelSettings  & settings = GetOrCreateModel (model, formFactor, controls);
    const ControllerProfile  * copied   = nullptr;
    ControlMapping             mapping  = DefaultMapping::For (model, controls);



    if (ControllerModelSettings::IsSourceOfOtherMode (source, mode))
    {
        return ProfileEditResult::NotFound;
    }

    if (source == ProfileSource::PaddleMapping)
    {
        mapping = ControllerModelSettings::MakeBuiltInMapping (ControllerProfileKind::Paddles, model, formFactor, controls);
    }
    else if (source == ProfileSource::JoyportMapping)
    {
        mapping = ControllerModelSettings::MakeBuiltInMapping (ControllerProfileKind::Joyport, model, formFactor, controls);
    }
    else if (source == ProfileSource::CopyOfProfile)
    {
        copied = settings.FindProfile (sourceName);

        if (copied == nullptr || settings.IsOfOtherMode (sourceName, mode))
        {
            return ProfileEditResult::NotFound;
        }

        mapping = copied->mapping;
    }

    return settings.AddProfile (name, mapping, mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetProfile
//
////////////////////////////////////////////////////////////////////////////////

ProfileEditResult ControllerProfileStore::ResetProfile (
    const ControllerModelKey      & model,
    ControllerFormFactor            formFactor,
    const std::vector<ControlId>  & controls,
    const std::string             & name)
{
    ControllerModelSettings &  settings = GetOrCreateModel (model, formFactor, controls);



    return settings.ResetProfile (name, model, formFactor, controls);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryAdoptLegacyProfile
//
//  A machine's own active profile is from before each controller carried its
//  own. It passes to the machine's saved controller once, when that
//  controller has no Joystick choice recorded, a choice of the Default
//  included, and is written back to no machine after. False when nothing
//  passed: no legacy name, no saved controller, or a choice already there.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerProfileStore::TryAdoptLegacyProfile (
    std::map<std::string, std::string>      & normalProfiles,
    const std::optional<ControllerUnitKey>  & selection,
    const std::string                       & legacyName)
{
    std::string  token;



    if (legacyName.empty() || !selection.has_value())
    {
        return false;
    }

    token = ControllerTokens::UnitToToken (selection.value());

    if (normalProfiles.count (token) != 0)
    {
        return false;
    }

    normalProfiles[token] = legacyName;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadModels
//
//  One model at a time. A model whose token cannot be read is dropped whole,
//  since there is no model to rebuild. A model whose entry cannot be read is
//  reported and rebuilt empty, which leaves it the built-in dead zone and a
//  Default recreated from the default mapping. Within a readable model, each
//  profile stands or falls on its own, and a later profile whose name matches
//  an earlier one, ignoring case, is dropped. Only the first profile marked
//  as each built-in profile stays that profile.
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
                      entry.second.HasArray (s_kpszProfilesKey, profilesArr));

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

        if (entry.second.HasArray (s_kpszProfilesKey, profilesArr))
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

                if (profile.kind != ControllerProfileKind::User && settings.FindBuiltInProfile (profile.kind) != nullptr)
                {
                    profile.kind = ControllerProfileKind::User;
                }

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
//  ReadActiveProfiles
//
//  Each controller's active profile, keyed by unit token. An empty name is
//  the Default, kept as an entry so that choosing the Default is remembered
//  as a choice rather than read as nothing having been chosen.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerProfileStore::ReadActiveProfiles (
    const JsonValue                     & activeObj,
    std::map<std::string, std::string>  & outProfiles,
    std::vector<std::string>            & outRejected)
{
    ControllerUnitKey  unit;
    HRESULT            hr   = S_OK;



    for (const auto & entry : activeObj.GetObjectEntries())
    {
        hr = ControllerTokens::UnitFromToken (entry.first, unit);

        if (FAILED (hr) || entry.second.GetType() != JsonType::String)
        {
            outRejected.push_back (entry.first);
            continue;
        }

        outProfiles[entry.first] = entry.second.GetString();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadPlayers
//
//  Player 1 then Player 2. A player whose entry cannot be played -- not an
//  object, an unknown entry, an entry that player cannot have, or a pick
//  whose controller cannot be read -- plays on Automatic and is reported,
//  keeping its mode. An array of the wrong length is reported and read as
//  far as it goes.
//
//  The pair is then normalized as a hand-edited file could not make it: a
//  second pick of Player 1's controller is Automatic, and the keys and the
//  mouse play in their own modes.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerProfileStore::ReadPlayers (
    const JsonValue           & playersArr,
    PlayerEntries             & outEntries,
    std::vector<std::string>  & outRejected)
{
    size_t  count  = playersArr.GetArraySize();
    size_t  player = 0;



    outEntries = PlayerSlotPolicy::MakeDefaultEntries();

    if (count != PlayerSlotPolicy::kPlayerCount)
    {
        outRejected.push_back (s_kpszPlayersKey);
    }

    for (player = 0; player < PlayerSlotPolicy::kPlayerCount && player < count; player++)
    {
        if (!TryReadPlayer (playersArr.GetArrayElement (player), player, outEntries[player]))
        {
            outEntries[player].kind = PlayerEntryKind::Automatic;
            outEntries[player].unit.reset();
            outRejected.push_back (std::string (s_kpszPlayersKey) + " " + std::to_string (player + 1));
        }
    }

    outEntries = PlayerSlotPolicy::NormalizeEntries (outEntries);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasAnyPlayerMode
//
//  Whether any saved player carries a mode, readable or not. Players saved
//  before players had modes carry none.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerProfileStore::HasAnyPlayerMode (const JsonValue & playersArr)
{
    size_t  count  = playersArr.GetArraySize();
    size_t  player = 0;



    for (player = 0; player < count; player++)
    {
        if (HasMember (playersArr.GetArrayElement (player), s_kpszPlayerModeKey))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryReadPlayer
//
//  One player's entry and its mode. False when the entry cannot be played by
//  this player; the mode is read first, so it survives that. An
//  unrecognized mode is Joystick, and a Player 2 with no mode is Same as
//  Player 1. A player saved before players had modes
//  carries the paddles its slot mapped to instead, under `maps`, and a
//  single paddle there reads as Paddle mode.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerProfileStore::TryReadPlayer (const JsonValue & playerObj, size_t player, PlayerEntry & outEntry)
{
    HRESULT            hr     = S_OK;
    std::string        kind;
    std::string        mode;
    std::string        maps;
    std::string        token;
    ControllerUnitKey  unit;
    bool               isOne  = player == 0;
    PlayerAxisTarget   target = PlayerAxisTarget::Joystick0;



    outEntry = PlayerEntry();

    if (playerObj.GetType() != JsonType::Object)
    {
        return false;
    }

    if (!isOne)
    {
        outEntry.mode = PlayerMode::SameAsPlayer1;
    }

    if (playerObj.HasString (s_kpszPlayerModeKey, mode))
    {
        outEntry.mode = PlayerModeFromToken (mode);
    }
    else if (playerObj.HasString (s_kpszMapsKey, maps))
    {
        target        = MachineInputPrefs::TargetFromToken (maps, PlayerAxisTarget::Joystick0);
        outEntry.mode = PlayerTargetRules::IsPaddleTarget (target) ? PlayerMode::Paddle : PlayerMode::Joystick;
    }

    if (!playerObj.HasString (s_kpszEntryKey, kind))
    {
        return false;
    }

    if (kind == s_kpszAutomaticEntry)
    {
        return true;
    }

    if (kind == s_kpszKeysEntry || kind == s_kpszMouseEntry)
    {
        outEntry.kind = (kind == s_kpszKeysEntry) ? PlayerEntryKind::ArrowKeys : PlayerEntryKind::MousePaddle;
        return isOne;
    }

    if (kind == s_kpszDisabledEntry)
    {
        outEntry.kind = PlayerEntryKind::Disabled;
        return !isOne;
    }

    if (kind != s_kpszPickEntry || !playerObj.HasString (s_kpszControllerKey, token))
    {
        return false;
    }

    hr = ControllerTokens::UnitFromToken (token, unit);

    if (FAILED (hr))
    {
        return false;
    }

    outEntry.kind = PlayerEntryKind::Controller;
    outEntry.unit = unit;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadLastHolders
//
//  A null is a slot nobody has held. An entry that is neither null nor a
//  readable unit token reads as null and is reported: the next time
//  Automatic fills that slot, the notice shows.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerProfileStore::ReadLastHolders (
    const JsonValue           & holdersArr,
    PlayerLastHolders         & outHolders,
    std::vector<std::string>  & outRejected)
{
    HRESULT            hr     = S_OK;
    size_t             count  = holdersArr.GetArraySize();
    size_t             player = 0;
    ControllerUnitKey  unit;



    outHolders = PlayerLastHolders();

    if (count != PlayerSlotPolicy::kPlayerCount)
    {
        outRejected.push_back (s_kpszLastHoldersKey);
    }

    for (player = 0; player < PlayerSlotPolicy::kPlayerCount && player < count; player++)
    {
        const JsonValue  & holder = holdersArr.GetArrayElement (player);

        if (holder.GetType() == JsonType::Null)
        {
            continue;
        }

        hr = (holder.GetType() == JsonType::String) ? ControllerTokens::UnitFromToken (holder.GetString(), unit)
                                                     : HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);

        if (FAILED (hr))
        {
            outRejected.push_back (std::string (s_kpszLastHoldersKey) + " " + std::to_string (player + 1));
            continue;
        }

        outHolders[player] = unit;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadProfile
//
//  A profile's kind is saved with it. A profile saved before profiles had
//  three kinds carries none, or "joyport": the Joyport ones keep their kind,
//  and the rest are classified by their mapping, so one made from the
//  Paddles starting point becomes a Paddle profile. So is a profile whose
//  kind is not recognized: its mapping is sound, so it is kept rather than
//  dropped. A built-in profile belongs to the kind it stands for, whatever
//  the file holds.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerProfileStore::ReadProfile (const JsonValue & profileObj, ControllerProfile & outProfile)
{
    const JsonValue *  mappingObj = nullptr;
    bool               isDefault  = false;
    bool               isJoyport  = false;
    bool               isPaddles  = false;
    bool               isRead     = false;
    std::string        mode;



    if (profileObj.GetType() != JsonType::Object ||
        !profileObj.HasString (s_kpszNameKey, outProfile.name) ||
        outProfile.name.empty() ||
        !profileObj.HasObject (s_kpszMappingKey, mappingObj))
    {
        return false;
    }

    isDefault = profileObj.HasBool (s_kpszDefaultKey, isDefault) && isDefault;
    isJoyport = profileObj.HasBool (s_kpszJoyportKey, isJoyport) && isJoyport;
    isPaddles = profileObj.HasBool (s_kpszPaddlesKey, isPaddles) && isPaddles;

    if (isDefault)
    {
        outProfile.kind = ControllerProfileKind::Default;
    }
    else if (isJoyport)
    {
        outProfile.kind = ControllerProfileKind::Joyport;
    }
    else if (isPaddles)
    {
        outProfile.kind = ControllerProfileKind::Paddles;
    }

    isRead = ReadMapping (*mappingObj, outProfile.mapping);

    (void) profileObj.HasString (s_kpszProfileModeKey, mode);

    if (mode == s_kpszJoyportMode)
    {
        outProfile.mode = ProfileMode::Joyport;
    }
    else if (mode == s_kpszPaddleMode)
    {
        outProfile.mode = ProfileMode::Paddle;
    }
    else if (mode == s_kpszJoystickMode)
    {
        outProfile.mode = ProfileMode::Joystick;
    }
    else
    {
        outProfile.mode = ControllerModelSettings::ClassifyLegacyProfile (outProfile.mapping);
    }

    if (outProfile.kind != ControllerProfileKind::User)
    {
        outProfile.mode = ControllerModelSettings::GetBuiltInMode (outProfile.kind);
    }

    return isRead;
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

        if (profile.kind == ControllerProfileKind::Default)
        {
            profileObj.emplace_back (s_kpszDefaultKey, JsonValue (true));
        }
        else if (profile.kind == ControllerProfileKind::Joyport)
        {
            profileObj.emplace_back (s_kpszJoyportKey, JsonValue (true));
        }
        else if (profile.kind == ControllerProfileKind::Paddles)
        {
            profileObj.emplace_back (s_kpszPaddlesKey, JsonValue (true));
        }

        // Every kind is written, Joystick included: a profile with none is
        // one saved before profiles had three kinds, and is classified by
        // its mapping when it is read. A build that knew only Joyport mode
        // reads the other two as its normal mode.
        profileObj.emplace_back (s_kpszProfileModeKey, JsonValue (std::string (GetModeToken (profile.mode))));
        profileObj.emplace_back (s_kpszMappingKey,     WriteMapping (profile.mapping));
        profiles.emplace_back (std::move (profileObj));
    }

    modelObj.emplace_back (s_kpszDeadzoneKey, JsonValue ((double) settings.deadzone));
    modelObj.emplace_back (s_kpszProfilesKey, JsonValue (std::move (profiles)));

    return JsonValue (std::move (modelObj));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetModeToken
//
////////////////////////////////////////////////////////////////////////////////

const char * ControllerProfileStore::GetModeToken (ProfileMode mode)
{
    const char *  pszToken = s_kpszJoystickMode;



    switch (mode)
    {
        case ProfileMode::Paddle:    pszToken = s_kpszPaddleMode;   break;
        case ProfileMode::Joyport:   pszToken = s_kpszJoyportMode;  break;

        case ProfileMode::Joystick:
        default:                                                    break;
    }

    return pszToken;
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
//  WritePlayers
//
//  Both players, always, each with its mode: the controller only for a pick.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ControllerProfileStore::WritePlayers (const PlayerEntries & entries)
{
    std::vector<JsonValue>  arr;



    for (const PlayerEntry & entry : entries)
    {
        std::vector<std::pair<std::string, JsonValue>>  obj;
        const char                                    * pszKind = s_kpszAutomaticEntry;

        switch (entry.kind)
        {
            case PlayerEntryKind::Controller:   pszKind = s_kpszPickEntry;      break;
            case PlayerEntryKind::ArrowKeys:    pszKind = s_kpszKeysEntry;      break;
            case PlayerEntryKind::MousePaddle:  pszKind = s_kpszMouseEntry;     break;
            case PlayerEntryKind::Disabled:     pszKind = s_kpszDisabledEntry;  break;

            case PlayerEntryKind::Automatic:
            default:                                                            break;
        }

        obj.emplace_back (s_kpszEntryKey, JsonValue (std::string (pszKind)));

        if (entry.kind == PlayerEntryKind::Controller && entry.unit.has_value())
        {
            obj.emplace_back (s_kpszControllerKey, JsonValue (ControllerTokens::UnitToToken (entry.unit.value())));
        }

        obj.emplace_back (s_kpszPlayerModeKey, JsonValue (std::string (PlayerModeToToken (entry.mode))));

        arr.emplace_back (std::move (obj));
    }

    return JsonValue (std::move (arr));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteLastHolders
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ControllerProfileStore::WriteLastHolders (const PlayerLastHolders & holders)
{
    std::vector<JsonValue>  arr;



    for (const std::optional<ControllerUnitKey> & holder : holders)
    {
        if (holder.has_value())
        {
            arr.emplace_back (ControllerTokens::UnitToToken (holder.value()));
        }
        else
        {
            arr.emplace_back (nullptr);
        }
    }

    return JsonValue (std::move (arr));
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





////////////////////////////////////////////////////////////////////////////////
//
//  SetMember
//
//  Puts the value into the section under the key, in place of an existing
//  member or as a new one.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerProfileStore::SetMember (
    std::vector<std::pair<std::string, JsonValue>>  & members,
    const char                                      * pszKey,
    JsonValue                                      && value)
{
    for (auto & member : members)
    {
        if (member.first == pszKey)
        {
            member.second = std::move (value);
            return;
        }
    }

    members.emplace_back (pszKey, std::move (value));
}
