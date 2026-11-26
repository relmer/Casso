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
//  its label row, the side of a comparison, and whether the strip is being
//  panned.
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

    float  GetWidth () const { return static_cast<float> (bounds.right - bounds.left); }
};
