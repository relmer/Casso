#pragma once

#include "Pch.h"

#include "Controllers/ControlMapping.h"
#include "Controllers/ControllerCalibration.h"
#include "Controllers/PlayerSlotPolicy.h"
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
//  Play without a Joyport is normal mode, and play with one is Joyport mode.
//  Every profile belongs to one of them, fixed when it is created, and is
//  listed and played only in that mode. Each controller's chosen profile is
//  kept once per mode, so attaching or detaching the Joyport switches to the
//  profile last chosen for the new mode.
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
//  One named mapping for a controller model, belonging to one mode: the mode
//  in effect when it was created. The Default belongs to normal mode and the
//  Joyport profile to Joyport mode, each leading its mode's list. A
//  controller with no profile chosen for the mode it is playing in plays that
//  mode's built-in profile.
//
////////////////////////////////////////////////////////////////////////////////

struct ControllerProfile
{
    static constexpr const char *  kpszDefaultName = "Default";
    static constexpr const char *  kpszJoyportName = "Joyport";

    std::string            name;
    ControllerProfileKind  kind = ControllerProfileKind::User;
    ControlMapping         mapping;
    ProfileMode            mode = ProfileMode::Normal;

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
//  What a new profile's mapping starts from. The New profile dialog offers
//  only the mode's own mappings, and a copy is only of a profile of the mode
//  the new profile belongs to.
//
////////////////////////////////////////////////////////////////////////////////

enum class ProfileSource
{
    DefaultMapping,
    JoyportMapping,
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

    // The mode's built-in profile first, then the mode's own profiles in the
    // order they are kept.
    std::vector<std::string>   GetProfileNames     (ProfileMode mode) const;

    // Whether a name is a profile of the mode other than `mode`: a built-in
    // profile by its name, or a saved profile made in that mode. A name the
    // model has no profile for is not.
    bool                       IsOfOtherMode       (const std::string & name, ProfileMode mode) const;

    // The built-in mappings depend on the device's form factor as well as its
    // model and controls, since the Joyport profile's second stick does.
    ProfileEditResult          AddProfile            (const std::string & name, const ControlMapping & mapping, ProfileMode mode);
    ProfileEditResult          RenameProfile         (const std::string & name, const std::string & newName);
    ProfileEditResult          DeleteProfile         (const std::string & name);
    ProfileEditResult          ResetProfile          (const std::string             & name,
                                                      const ControllerModelKey      & model,
                                                      ControllerFormFactor            formFactor,
                                                      const std::vector<ControlId>  & controls);
    void                       EnsureBuiltInProfiles (const ControllerModelKey      & model,
                                                      ControllerFormFactor            formFactor,
                                                      const std::vector<ControlId>  & controls);

    static std::string         TrimProfileName       (const std::string & name);

    // The built-in mapping a profile of this kind resets to.
    static ControlMapping         MakeBuiltInMapping    (ControllerProfileKind           kind,
                                                         const ControllerModelKey      & model,
                                                         ControllerFormFactor            formFactor,
                                                         const std::vector<ControlId>  & controls);

    // Which built-in profile a controller with no profile chosen plays, and
    // which one a name is, ignoring case; User for any other name.
    static ControllerProfileKind  GetAutomaticKind      (ProfileMode mode);
    static ControllerProfileKind  GetBuiltInKind        (const std::string & name);

    // The mode a built-in profile belongs to, and the built-in profile whose
    // mapping a profile resets to: its own for a built-in profile, and its
    // mode's for a user's.
    static ProfileMode            GetBuiltInMode        (ControllerProfileKind kind);
    static ControllerProfileKind  GetResetKind          (const ControllerProfile & profile);

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

    // The two players' entries and the controller that last held each slot,
    // global like the rest. `players` is absent until the one-time adoption
    // from the launched machine has run, and its presence is what marks that
    // adoption done.
    std::optional<PlayerEntries>                    players;
    PlayerLastHolders                               lastHolders;

    std::map<std::string, std::string> &  GetActiveProfiles (ProfileMode mode) { return mode == ProfileMode::Joyport ? joyportActiveProfiles : activeProfiles; }

    void       FromJson (const JsonValue & controllers, std::vector<std::string> & outRejected);
    JsonValue  ToJson   (const JsonValue & controllers) const;

    // The mapping and deadzone a controller of this model plays with: its
    // saved Default profile when it has one, and the built-in default
    // otherwise.
    void       GetDefaultSettings (const ControllerModelKey        & model,
                                   ControllerFormFactor              formFactor,
                                   const std::vector<ControlId>    & controls,
                                   ControlMapping                  & outMapping,
                                   float                           & outDeadzone) const;

    // The same for either built-in profile.
    void       GetBuiltInSettings (ControllerProfileKind             kind,
                                   const ControllerModelKey        & model,
                                   ControllerFormFactor              formFactor,
                                   const std::vector<ControlId>    & controls,
                                   ControlMapping                  & outMapping,
                                   float                           & outDeadzone) const;

    // The profile of that name for the model, or nullptr when either is
    // missing, in which case the caller plays the model's Default.
    const ControllerProfile *  FindProfile (const std::string & modelToken, const std::string & name) const;

    // The model's settings, added with the default deadzone when the model
    // has none, and always holding a Default profile.
    ControllerModelSettings &  GetOrCreateModel (const ControllerModelKey      & model,
                                                 ControllerFormFactor            formFactor,
                                                 const std::vector<ControlId>  & controls);

    // The new profile belongs to `mode`, the mode in effect, whatever it
    // starts from.
    ProfileEditResult  CreateProfile (const ControllerModelKey        & model,
                                      ControllerFormFactor              formFactor,
                                      const std::vector<ControlId>    & controls,
                                      const std::string               & name,
                                      ProfileSource                     source,
                                      ProfileMode                       mode,
                                      const std::string               & sourceName = std::string());
    ProfileEditResult  ResetProfile  (const ControllerModelKey        & model,
                                      ControllerFormFactor              formFactor,
                                      const std::vector<ControlId>    & controls,
                                      const std::string               & name);

private:

    void              ReadModels        (const JsonValue & modelsObj, std::vector<std::string> & outRejected);
    void              ReadCalibrations  (const JsonValue & calibrationObj, std::vector<std::string> & outRejected);
    static void       ReadActiveProfiles(const JsonValue & activeObj, std::map<std::string, std::string> & outProfiles, std::vector<std::string> & outRejected);
    static void       ReadPlayers       (const JsonValue & playersArr, PlayerEntries & outEntries, std::vector<std::string> & outRejected);
    static bool       TryReadPlayer     (const JsonValue & playerObj, size_t player, PlayerEntry & outEntry);
    static void       ReadLastHolders   (const JsonValue & holdersArr, PlayerLastHolders & outHolders, std::vector<std::string> & outRejected);
    static JsonValue  WritePlayers      (const PlayerEntries & entries);
    static JsonValue  WriteLastHolders  (const PlayerLastHolders & holders);

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
    static void       SetMember         (std::vector<std::pair<std::string, JsonValue>> & members,
                                         const char                                     * pszKey,
                                         JsonValue                                     && value);
};
