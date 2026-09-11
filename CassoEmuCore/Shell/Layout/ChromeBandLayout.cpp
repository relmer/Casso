#include "Pch.h"

#include "Shell/Layout/ChromeBandLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBandDp
//
//  A machine with no Disk ][ controller has no drive widgets, so the band
//  collapses and the viewport grows into the room they would have taken.
//  Under the desk scene there is no bottom band at all: the drives are scene
//  objects. Otherwise the band zooms with the scene so it hugs the scaled
//  widgets instead of leaving dead space around them.
//
////////////////////////////////////////////////////////////////////////////////

int ChromeBandLayout::DriveBandDp (const ChromeBandInputs & inputs)
{
    int  dp = 0;



    if (inputs.hasDiskController && !inputs.crtMonitorActive)
    {
        dp = (int) lroundf ((float) inputs.driveBarThicknessDp * inputs.chromeSceneScale);
    }

    return dp;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SwitchBandDp
//
//  The //c switch strip only exists on the //c; everywhere else the band
//  collapses to zero height so the dock leaves the viewport unchanged.
//
////////////////////////////////////////////////////////////////////////////////

int ChromeBandLayout::SwitchBandDp (const ChromeBandInputs & inputs)
{
    return inputs.hasCaseSwitches ? kSwitchBandDp : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Compute
//
//  The two notice bands arrive in pixels: they are measured against the
//  client width, which is what the band will be given, and the shell has that
//  and this does not.
//
////////////////////////////////////////////////////////////////////////////////

ChromeBandHeightsPx ChromeBandLayout::Compute (const ChromeBandInputs & inputs, const DxuiDpiScaler & scaler)
{
    ChromeBandHeightsPx  px;



    px.title    = scaler.ToPx (kTitleBarBandDp);
    px.nav      = scaler.ToPx (kNavStripBandDp);
    px.toolbar  = scaler.ToPx (inputs.toolbarBandDp);
    px.change   = inputs.changeBandPx;
    px.capture  = inputs.captureBandPx;
    px.drive    = scaler.ToPx (DriveBandDp (inputs));
    px.switches = scaler.ToPx (SwitchBandDp (inputs));

    return px;
}
