#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"
#include "Ui/DiskInspector/StripHitTest.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StripPaintScene
//
//  What the track strip's painters read: the inspector's views' context,
//  the scaler, the strip's hit tests, the strip's bounds and the bar under
//  its label row, the side of a comparison, whether the strip is being
//  panned, and the rows the shimmer crosses, which span both strips of a
//  comparison so its stripe lines up from one to the other.
//
////////////////////////////////////////////////////////////////////////////////

struct StripPaintScene
{
    static constexpr float  kOutlineDip = 2.0f;

    const InspectorViewContext  & context;
    const DxuiDpiScaler         & scaler;
    const StripHitTest          & hits;
    RECT                          bounds    = {};
    RECT                          bar       = {};
    int                           side      = 0;
    bool                          isPanning = false;
    LONG                          lightTop  = 0;
    LONG                          lightEnd  = 0;

    float  GetWidth () const { return static_cast<float> (bounds.right - bounds.left); }
};
