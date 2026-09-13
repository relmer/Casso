#pragma once

#include "Pch.h"

#include "Controllers/ControllerCalibration.h"
#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerProfileStore
//
//  What Casso remembers about controllers across sessions, read from and
//  written to the `controllers` section of the global prefs. Today that is
//  each DirectInput unit's calibration, keyed by its unit token.
//
//  The section is handed in and handed back whole, and only the members this
//  class owns are replaced, so a member written by a later build survives a
//  save by this one.
//
//  A SAVED ENTRY THAT CANNOT BE USED IS DROPPED AND NAMED, never repaired. Its
//  unit falls back to automatic calibration, which is always safe, and the
//  caller reports the name once so a calibration does not vanish silently.
//
////////////////////////////////////////////////////////////////////////////////

class ControllerProfileStore
{
public:

    std::map<std::string, ControllerCalibration>  calibrations;

    void       FromJson (const JsonValue & controllers, std::vector<std::string> & outRejected);
    JsonValue  ToJson   (const JsonValue & controllers) const;

private:

    static bool       ReadCalibration   (const std::string & token, const JsonValue & entry, ControllerCalibration & outCalibration);
    static JsonValue  WriteCalibration  (const ControllerCalibration & calibration);
    static bool       HasAnythingToSave (const ControllerCalibration & calibration);
};
