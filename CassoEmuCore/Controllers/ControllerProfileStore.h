#pragma once

#include "Pch.h"

#include "Controllers/ControlMapping.h"
#include "Controllers/ControllerCalibration.h"
#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerProfileKind
//
//  Every model has one Default and one Joyport profile, which cannot be
//  renamed or deleted and reset to their own built-in mappings. Everything
//  else is the user's.
//
////////////////////////////////////////////////////////////////////////////////

enum class ControllerProfileKind
{
    User,
    Default,
    Joyport,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileMode
//
//  Each controller's chosen profile is kept once for play without a Joyport
//  and once for play with one, so attaching or detaching the Joyport switches
//  to the profile last chosen for that mode.
//
////////////////////////////////////////////////////////////////////////////////

enum class ProfileMode
{
    Normal,
    Joyport,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerProfile
//
//  One named mapping for a controller model. A controller with no profile
//  chosen for the mode it is playing in plays the model's Joyport profile
//  while a Joyport is attached and its Default otherwise.
//
////////////////////////////////////////////////////////////////////////////////

struct ControllerProfile
{
    static constexpr const char *  kpszDefaultName = "Default";
    static constexpr const char *  kpszJoyportName = "Joyport";

    std::string            name;
    ControllerProfileKind  kind = ControllerProfileKind::User;
    ControlMapping         mapping;

    bool operator== (const ControllerProfile &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileEditResult
//
//  The outcome of creating, renaming, deleting or resetting a profile. The
//  caller turns a refusal into the message the user sees.
//
////////////////////////////////////////////////////////////////////////////////

enum class ProfileEditResult
{
    Ok,
    EmptyName,
    NameTooLong,
    DuplicateName,
    NotFound,
    IsBuiltInProfile,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileSource
//
//  What a new profile's mapping starts from.
//
////////////////////////////////////////////////////////////////////////////////

enum class ProfileSource
{
    DefaultMapping,
    CopyOfProfile,
    Paddles,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings
//
//  What is kept for every unit of one controller model: its deadzone and its
//  profiles. Names are compared ignoring case, and a name is trimmed of
//  surrounding whitespace before it is checked or stored.
//
////////////////////////////////////////////////////////////////////////////////

struct ControllerModelSettings
{
    static constexpr size_t  kMaxProfileNameLength = 40;

    float                           deadzone = 0.0f;
    std::vector<ControllerProfile>  profiles;

    const ControllerProfile *  FindDefaultProfile  () const;
    const ControllerProfile *  FindBuiltInProfile  (ControllerProfileKind kind) const;
    ControllerProfile       *  FindBuiltInProfile  (ControllerProfileKind kind);
    const ControllerProfile *  FindProfile         (const std::string & name) const;
    ControllerProfile       *  FindProfile         (const std::string & name);

    // Excluding, when given, is the profile being renamed, so a rename that
    // changes only case is not a duplicate of itself.
    ProfileEditResult          CheckProfileName    (const std::string & name, const ControllerProfile * excluding = nullptr) const;

    ProfileEditResult          AddProfile            (const std::string & name, const ControlMapping & mapping);
    ProfileEditResult          RenameProfile         (const std::string & name, const std::string & newName);
    ProfileEditResult          DeleteProfile         (const std::string & name);
    ProfileEditResult          ResetProfile          (const std::string & name, const ControllerModelKey & model, const std::vector<ControlId> & controls);
    void                       EnsureBuiltInProfiles (const ControllerModelKey & model, const std::vector<ControlId> & controls);

    static std::string         TrimProfileName       (const std::string & name);

    // The built-in mapping a profile of this kind resets to; a user profile
    // resets to the Default's.
    static ControlMapping         MakeBuiltInMapping    (ControllerProfileKind           kind,
                                                         const ControllerModelKey      & model,
                                                         const std::vector<ControlId>  & controls);

    // Which built-in profile a controller with no profile chosen plays, and
    // which one a name is, ignoring case; User for any other name.
    static ControllerProfileKind  GetAutomaticKind      (ProfileMode mode);
    static ControllerProfileKind  GetBuiltInKind        (const std::string & name);

    bool operator== (const ControllerModelSettings &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerProfileStore
//
//  What Casso remembers about controllers across sessions, read from and
//  written to the `controllers` section of the global prefs: each model's
//  settings, keyed by model token, and each DirectInput unit's calibration,
//  keyed by unit token.
//
//  The section is handed in and handed back whole, and only the members this
//  class owns are replaced, so a member written by a later build survives a
//  save by this one.
//
//  SAVED DATA THAT CANNOT BE USED IS DROPPED AND NAMED, never repaired. What
//  replaces it is always safe -- automatic calibration, the default mapping --
//  and the caller reports the names once, so the user's settings do not
//  vanish silently.
//
////////////////////////////////////////////////////////////////////////////////

class ControllerProfileStore
{
public:

    static constexpr float  kMinMaxSpeed = 16.0f;
    static constexpr float  kMaxMaxSpeed = 1024.0f;

    std::map<std::string, ControllerModelSettings>  models;
    std::map<std::string, ControllerCalibration>    calibrations;

    // Each controller's active profile, by unit token, once for play without
    // a Joyport and once for play with one; an empty name is that mode's
    // built-in profile. Global rather than per machine, so a profile made
    // for one machine can be played on any of them.
    std::map<std::string, std::string>              activeProfiles;
    std::map<std::string, std::string>              joyportActiveProfiles;

    std::map<std::string, std::string> &  GetActiveProfiles (ProfileMode mode) { return mode == ProfileMode::Joyport ? joyportActiveProfiles : activeProfiles; }

    void       FromJson (const JsonValue & controllers, std::vector<std::string> & outRejected);
    JsonValue  ToJson   (const JsonValue & controllers) const;

    // The mapping and deadzone a controller of this model plays with: its
    // saved Default profile when it has one, and the built-in default
    // otherwise.
    void       GetDefaultSettings (const ControllerModelKey        & model,
                                   const std::vector<ControlId>    & controls,
                                   ControlMapping                  & outMapping,
                                   float                           & outDeadzone) const;

    // The same for either built-in profile.
    void       GetBuiltInSettings (ControllerProfileKind             kind,
                                   const ControllerModelKey        & model,
                                   const std::vector<ControlId>    & controls,
                                   ControlMapping                  & outMapping,
                                   float                           & outDeadzone) const;

    // The profile of that name for the model, or nullptr when either is
    // missing, in which case the caller plays the model's Default.
    const ControllerProfile *  FindProfile (const std::string & modelToken, const std::string & name) const;

    // The model's settings, added with the default deadzone when the model
    // has none, and always holding a Default profile.
    ControllerModelSettings &  GetOrCreateModel (const ControllerModelKey & model, const std::vector<ControlId> & controls);

    ProfileEditResult  CreateProfile (const ControllerModelKey        & model,
                                      const std::vector<ControlId>    & controls,
                                      const std::string               & name,
                                      ProfileSource                     source,
                                      const std::string               & sourceName = std::string());
    ProfileEditResult  ResetProfile  (const ControllerModelKey        & model,
                                      const std::vector<ControlId>    & controls,
                                      const std::string               & name);

private:

    void              ReadModels        (const JsonValue & modelsObj, std::vector<std::string> & outRejected);
    void              ReadCalibrations  (const JsonValue & calibrationObj, std::vector<std::string> & outRejected);
    static void       ReadActiveProfiles(const JsonValue & activeObj, std::map<std::string, std::string> & outProfiles, std::vector<std::string> & outRejected);

    static bool       ReadProfile       (const JsonValue & profileObj, ControllerProfile & outProfile);
    static bool       ReadMapping       (const JsonValue & mappingObj, ControlMapping & outMapping);
    static bool       ReadAxisBindings  (const JsonValue & mappingObj, const char * pszKey, std::vector<AxisBinding> & outBindings);
    static bool       ReadButtonBindings(const JsonValue & mappingObj, const char * pszKey, std::vector<ButtonBinding> & outBindings);
    static bool       ReadCalibration   (const std::string & token, const JsonValue & entry, ControllerCalibration & outCalibration);

    static JsonValue  WriteModel        (const ControllerModelSettings & settings);
    static JsonValue  WriteMapping      (const ControlMapping & mapping);
    static JsonValue  WriteCalibration  (const ControllerCalibration & calibration);
    static bool       HasAnythingToSave (const ControllerCalibration & calibration);
    static bool       HasMember         (const JsonValue & obj, const char * pszKey);

    static void       ReplaceMember     (std::vector<std::pair<std::string, JsonValue>> & members,
                                         const char                                     * pszKey,
                                         std::vector<std::pair<std::string, JsonValue>> && entries);
};
