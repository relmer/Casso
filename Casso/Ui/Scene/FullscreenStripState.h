#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullscreenStripState
//
//  The fullscreen drive overlay strip's state machine: edge-reveal (only while
//  the host owns the pointer), hotkey summon with guest-capture release and
//  restore, auto-hide with tooltip/browse pinning, and the hidden-state
//  activity indicator. A pure FSM -- inputs in, state out -- so the capture
//  sequencing rules are property-testable.
//
////////////////////////////////////////////////////////////////////////////////

class FullscreenStripState
{
public:
};
