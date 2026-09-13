#pragma once

#include "Pch.h"

#include "Controllers/ControlMapping.h"
#include "Controllers/ControllerCalibration.h"
#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerProfile
//
//  One named mapping for a controller model. Exactly one profile per model is
//  its Default, which is what the model plays with until the user picks
//  another.
//
////////////////////////////////////////////////////////////////////////////////

struct ControllerProfile
{
    std::string     name;
    bool            isDefault = false;
    ControlMapping  mapping;

    bool operator== (const ControllerProfile &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerModelSettings
//
//  What is kept for every unit of one controller model: its deadzone and its
//  profiles.
//
////////////////////////////////////////////////////////////////////////////////

struct ControllerModelSettings
{
    float                           deadzone = 0.0f;
    std::vector<ControllerProfile>  profiles;

    const ControllerProfile *  FindDefaultProfile () const;

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

    void       FromJson (const JsonValue & controllers, std::vector<std::string> & outRejected);
    JsonValue  ToJson   (const JsonValue & controllers) const;

    // The mapping and deadzone a controller of this model plays with: its
    // saved Default profile when it has one, and the built-in default
    // otherwise.
    void       GetDefaultSettings (const ControllerModelKey        & model,
                                   const std::vector<ControlId>    & controls,
                                   ControlMapping                  & outMapping,
                                   float                           & outDeadzone) const;

private:

    void              ReadModels        (const JsonValue & modelsObj, std::vector<std::string> & outRejected);
    void              ReadCalibrations  (const JsonValue & calibrationObj, std::vector<std::string> & outRejected);

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
