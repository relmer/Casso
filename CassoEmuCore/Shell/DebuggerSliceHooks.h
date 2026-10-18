#pragma once

#include "Pch.h"

class CpuManagerRunDriver;
class HeldInputWatch;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerSliceHooks
//
//  What the CPU thread's slice loop needs of the debugger, taken once per
//  frame so that each slice reads plain fields rather than calling into the
//  debugger. The pointers belong to the debugger and hold for the frame.
//
////////////////////////////////////////////////////////////////////////////////

struct DebuggerSliceHooks
{
    CpuManagerRunDriver  * runDriver      = nullptr;   // null on a machine nobody is debugging
    HeldInputWatch       * heldInputWatch = nullptr;   // the lines input held behind live would change
    bool                   isBehindLive   = false;
};
