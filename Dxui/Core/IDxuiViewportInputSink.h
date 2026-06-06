#pragma once

#include "Pch.h"
#include "Core/DxuiEvents.h"



////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiViewportInputSink
//
//  Consumer-supplied callback target for input forwarded by a
//  `DxuiViewport` operating in "consumes input" mode. Casso's Apple ][
//  emulator implements this to translate viewport key / mouse events
//  into Apple ][ keyboard and joystick events.
//
//  The sink receives only the events the viewport elects to forward;
//  reserved Dxui chords (Tab / Shift+Tab / Esc / Alt-alone / F10) stay
//  with the framework so focus traversal and menu activation continue
//  to work even while the emulator has the viewport focus.
//
//  Return semantics mirror `IDxuiControl::OnMouse` / `OnKey`: return
//  `true` to mark the event consumed and stop further propagation, or
//  `false` to let it bubble back up the control tree.
//
//  All methods are called on the UI thread (FR-083).
//
////////////////////////////////////////////////////////////////////////////////



class IDxuiViewportInputSink
{
public:
    virtual ~IDxuiViewportInputSink() = default;

    virtual bool  OnViewportMouse (const DxuiMouseEvent & ev) = 0;
    virtual bool  OnViewportKey   (const DxuiKeyEvent   & ev) = 0;
};
