#pragma once

#include "Pch.h"
#include "Core/DxuiInput.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiEvents
//
//  Pure value types describing UI input events delivered by
//  DxuiHwndSource to the control tree. All coordinates are in DIPs
//  (FR-022, FR-082).
//
//  DxuiMouseButton is shared with the legacy DxuiInput translator and
//  declared in Core/DxuiInput.h.
//
////////////////////////////////////////////////////////////////////////////////



enum class DxuiMouseEventKind
{
    Move,
    Down,
    Up,
    Wheel,
    Enter,
    Leave,
};


//  Whole wheel notches from WM_MOUSEWHEEL deltas, the rest left in
//  `remainder` for the next delta, so a high-resolution wheel or a precision
//  touchpad, whose deltas are a part of WHEEL_DELTA, scrolls once its deltas
//  add up to a notch.
struct DxuiWheelNotches
{
    int  Take (int delta)
    {
        int  notches = 0;



        remainder += delta;
        notches    = remainder / WHEEL_DELTA;
        remainder -= notches * WHEEL_DELTA;

        return notches;
    }

    int  remainder = 0;
};


struct DxuiMouseEvent
{
    DxuiMouseEventKind  kind            = DxuiMouseEventKind::Move;
    DxuiMouseButton     button          = DxuiMouseButton::None;
    POINT               positionDip     = {};
    float               wheelDelta      = 0.0f;     // signed notches; +1 per wheel notch up
    bool                wheelHorizontal = false;    // Wheel: true = horizontal (WM_MOUSEHWHEEL / trackpad)
    bool                shift           = false;
    bool                ctrl            = false;
    bool                alt             = false;
};



enum class DxuiKeyEventKind
{
    Down,
    Up,
    Char,
};


struct DxuiKeyEvent
{
    DxuiKeyEventKind  kind       = DxuiKeyEventKind::Down;
    WPARAM            vk         = 0;     // virtual-key code for Down/Up; UTF-16 unit for Char
    bool              repeat     = false;
    bool              shift      = false;
    bool              ctrl       = false;
    bool              alt        = false;
};
