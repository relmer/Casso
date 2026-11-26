#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Widgets/DxuiInfoBar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTimedInfoBar
//
//  A message bar that reports something that just happened and takes itself
//  down after a few seconds: a DxuiInfoBar set up as a centered overlay with
//  the default duration.
//
//  AN OVERLAY, NOT A DOCKED BAND, BY DEFAULT. A band that appears and vanishes
//  on a timer reflows whatever sits beneath it twice per notice, so this one
//  is laid over the content and the scrim dims the strip it covers rather
//  than hiding it. Where it goes is the caller's; it fills the bounds it is
//  given. A docked timed bar is a DxuiInfoBar with SetDurationMs.
//
//  THE CLOCK IS PASSED IN. Show and Tick take the time rather than reading it,
//  so a test can expire a notice without waiting for one.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTimedInfoBar : public DxuiInfoBar
{
public:

    DxuiTimedInfoBar  ();
    ~DxuiTimedInfoBar () override = default;
};
