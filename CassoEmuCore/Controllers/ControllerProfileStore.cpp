#include "Pch.h"

#include "Controllers/ControllerProfileStore.h"

#include "Controllers/ControllerTokens.h"





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
//  FromJson
//
//  Replaces what the store holds with the calibrations in the section. Every
//  entry that cannot be used is left out and its token added to outRejected.
//
////////////////////////////////////////////////////////////////////////////////

void ControllerProfileStore::FromJson (const JsonValue & controllers, std::vector<std::string> & outRejected)
{
    const JsonValue *  calibrationObj = nullptr;



    calibrations.clear();

    if (controllers.GetType() != JsonType::Object ||
        !controllers.HasObject (s_kpszCalibrationKey, calibrationObj) ||
        calibrationObj == nullptr)
    {
        return;
    }

    for (const auto & entry : calibrationObj->GetObjectEntries())
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
//  ToJson
//
//  The section handed in, with its calibration member replaced by what the
//  store holds. A store with nothing to save adds no member to a section that
//  had none, and returns null for a section that did not exist, so an install
//  that never calibrated a controller carries nothing for it.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ControllerProfileStore::ToJson (const JsonValue & controllers) const
{
    std::vector<std::pair<std::string, JsonValue>>  members;
    std::vector<std::pair<std::string, JsonValue>>  entries;
    bool                                            hasMember = false;



    if (controllers.GetType() == JsonType::Object)
    {
        members = controllers.GetObjectEntries();
    }

    for (const auto & kv : calibrations)
    {
        if (HasAnythingToSave (kv.second))
        {
            entries.emplace_back (kv.first, WriteCalibration (kv.second));
        }
    }

    for (auto & member : members)
    {
        if (member.first == s_kpszCalibrationKey)
        {
            member.second = JsonValue (std::move (entries));
            hasMember     = true;
        }
    }

    if (!hasMember && !entries.empty())
    {
        members.emplace_back (s_kpszCalibrationKey, JsonValue (std::move (entries)));
    }

    if (members.empty() && controllers.GetType() != JsonType::Object)
    {
        return JsonValue();
    }

    return JsonValue (std::move (members));
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
        !entry.HasArray (s_kpszAxesKey, axesArr) ||
        axesArr == nullptr)
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
