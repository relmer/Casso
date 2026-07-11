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

    // Glyph-painting primitives (T030d input-device selector). Defaulted
    // to no-ops on the interface so test mocks and simple painters compile
    // unchanged; the concrete DxuiPainter implements them with the same
    // rect-slicing technique as FillCircleApprox. The quad must be convex,
    // its points given in order (clockwise or counter-clockwise).
    virtual void  FillConvexQuad    (float x0, float y0, float x1, float y1,
                                     float x2, float y2, float x3, float y3,
                                     uint32_t argbColor)
    { (void) x0; (void) y0; (void) x1; (void) y1; (void) x2; (void) y2; (void) x3; (void) y3; (void) argbColor; }

    virtual void  FillEllipseApprox (float cxPx, float cyPx,
                                     float radiusXPx, float radiusYPx,
                                     uint32_t argbColor)
    { (void) cxPx; (void) cyPx; (void) radiusXPx; (void) radiusYPx; (void) argbColor; }

    virtual void  DrawLineApprox    (float x0, float y0, float x1, float y1,
                                     float thicknessPx, uint32_t argbColor)
    { (void) x0; (void) y0; (void) x1; (void) y1; (void) thicknessPx; (void) argbColor; }

    // Global alpha multiplier applied to every vertex's alpha channel.
    // Defaulted to a no-op on the interface so test mocks don't have to
    // implement alpha tracking; the concrete DxuiPainter overrides
    // these to drive the live-preview fade pipeline.
    virtual void   SetGlobalAlpha   (float alpha)                               { (void) alpha; }
    virtual float  GlobalAlpha      () const                                    { return 1.0f; }
};
