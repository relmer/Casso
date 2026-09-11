#pragma once

#include "Pch.h"

#include "Core/DxuiDpiScaler.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ChromeBandInputs
//
//  Everything the band heights depend on, read off the shell in one place.
//
////////////////////////////////////////////////////////////////////////////////

struct ChromeBandInputs
{
    bool   hasDiskController   = false;   // a machine with no Disk ][ has no drive band
    bool   crtMonitorActive    = false;   // the desk scene models the drives itself
    bool   hasCaseSwitches     = false;   // only the //c has the switch strip
    int    driveBarThicknessDp = 0;
    float  chromeSceneScale    = 1.0f;    // the drive band zooms with the scene
    int    toolbarBandDp       = 0;
    int    changeBandPx        = 0;       // already measured against the client width
    int    captureBandPx       = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ChromeBandHeightsPx
//
//  One height per band, in pixels, in the order they are docked.
//
////////////////////////////////////////////////////////////////////////////////

struct ChromeBandHeightsPx
{
    int  title    = 0;
    int  nav      = 0;
    int  toolbar  = 0;
    int  change   = 0;
    int  capture  = 0;
    int  drive    = 0;
    int  switches = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ChromeBandLayout
//
//  How tall each chrome band is, given what the machine has and how the
//  window is being shown.
//
//  The dock lays the bands out from these heights and gives the viewport what
//  is left, so a band that should be gone -- the drive band on a machine with
//  no controller, or under the desk scene where the drives are scene objects
//  -- has to come out as zero here, not merely be painted nothing. This is
//  the arithmetic that used to sit inside the shell's band sync, where the
//  only way to see it was to run the window.
//
////////////////////////////////////////////////////////////////////////////////

class ChromeBandLayout
{
public:

    //  Fixed band metrics, in design pixels. Coupled to the widgets they hold:
    //  a change to a widget's font or padding is a change here too, since
    //  nothing recomputes these from the widget.
    static constexpr int  kTitleBarBandDp = 32;
    static constexpr int  kNavStripBandDp = 32;
    static constexpr int  kSwitchBandDp   = 40;

    static int  DriveBandDp  (const ChromeBandInputs & inputs);
    static int  SwitchBandDp (const ChromeBandInputs & inputs);

    static ChromeBandHeightsPx  Compute (const ChromeBandInputs & inputs, const DxuiDpiScaler & scaler);
};
