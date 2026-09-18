#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AxisCalibration
//
//  Where one axis rests and how far it travels each way, in the raw [-1, 1]
//  reading the backend reports. All three zero is an axis nothing has been
//  learned about yet.
//
////////////////////////////////////////////////////////////////////////////////

struct AxisCalibration
{
    float  center  = 0.0f;
    float  minimum = 0.0f;
    float  maximum = 0.0f;

    bool operator== (const AxisCalibration &) const = default;
};





enum class CalibrationMode
{
    Automatic,
    User
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerCalibration
//
//  One DirectInput unit's calibration: raw axis readings in, readings with
//  the rest position at 0 and the travel actually available at -1 and 1 out.
//  Xbox-class controllers are factory-calibrated and never get one.
//
//  AUTOMATIC calibration captures the center from the first reading after the
//  controller connects and widens the limits to whatever travel it goes on to
//  show. The limits only ever widen: a stick that once reached a point can
//  reach it again, and narrowing them would make a light touch read as full
//  deflection. An axis that never moves -- a rudder axis with its pedals
//  unplugged, pinned at a rail -- therefore never widens, and reads center.
//
//  USER calibration is the one the Calibrate action measured, and is used as
//  it stands: no center capture at connect, no widening.
//
////////////////////////////////////////////////////////////////////////////////

class ControllerCalibration
{
public:

    // Travel assumed each way until a stick has shown more. With no floor, a
    // stick whose limits were just set to its rest position would read full
    // deflection for the first hair's width of movement, and rest jitter
    // would swing the paddle from end to end.
    static constexpr float  kMinimumTravel  = 0.5f;

    // Furthest from zero a reading at connect can be and still be taken as
    // where the stick rests. A worn stick's rest error is a small fraction of
    // this; a stick held over while it connects is far past it, and taking
    // that as center would read full deflection at rest until a reconnect.
    static constexpr float  kMaxRestOffset  = 0.25f;

    // How far an axis must move from its reading at connect before it counts.
    // Above rest jitter, well below any deliberate movement.
    static constexpr float  kMovedThreshold = 0.02f;

    CalibrationMode                                            mode = CalibrationMode::Automatic;
    std::array<AxisCalibration, ControllerSample::kAxisCount>  axes = {};

    // Automatic only, and never saved: each axis's reading at connect and
    // whether it has moved from it since. AN AXIS THAT HAS NOT MOVED READS
    // CENTER. That covers an axis pinned at a rail with no hardware behind it,
    // and a stick held over as it connects, whose reading cannot be trusted
    // as its rest until it is let go.
    std::array<float, ControllerSample::kAxisCount>            connectReading = {};
    std::array<bool, ControllerSample::kAxisCount>             hasMoved       = {};

    void              CaptureCenter     (const ControllerSample & sample);
    void              Observe           (const ControllerSample & sample);
    ControllerSample  Apply             (const ControllerSample & sample) const;
    void              ResetToAutomatic  ();

    // Whether an axis read from saved prefs can be used. A user calibration
    // needs the center strictly between its limits; an automatic one only
    // needs limits in order, since its center is recaptured at every connect.
    static bool       IsValid           (const AxisCalibration & axis, CalibrationMode mode);

    bool operator== (const ControllerCalibration &) const = default;

private:

    static float      ApplyAxis         (float value, const AxisCalibration & axis, CalibrationMode mode, bool hasMoved);
};
