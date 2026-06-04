#pragma once

#include "Pch.h"



////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiPainter
//
//  Pure-virtual interface for the geometry painter. Widgets paint
//  through this interface so they remain mockable for unit tests; the
//  concrete DxuiPainter (Direct3D 11) implements it for the runtime.
//
//  Coordinates are pixels in the host's swap-chain space. DIP-to-pixel
//  conversion happens at the widget call site via DxuiDpiScaler.
//
//  All public methods are called on the UI thread (FR-083); the
//  concrete painter asserts this in debug builds.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiPainter
{
public:
    virtual ~IDxuiPainter() = default;

    virtual void  FillRect          (float    xPx,
                                     float    yPx,
                                     float    widthPx,
                                     float    heightPx,
                                     uint32_t argbColor)                        = 0;

    virtual void  FillGradientRect  (float    xPx,
                                     float    yPx,
                                     float    widthPx,
                                     float    heightPx,
                                     uint32_t argbTop,
                                     uint32_t argbBottom)                       = 0;

    virtual void  OutlineRect       (float    xPx,
                                     float    yPx,
                                     float    widthPx,
                                     float    heightPx,
                                     float    thicknessPx,
                                     uint32_t argbColor)                        = 0;

    virtual void  FillCircleApprox  (float    cxPx,
                                     float    cyPx,
                                     float    radiusPx,
                                     uint32_t argbColor)                        = 0;
};
