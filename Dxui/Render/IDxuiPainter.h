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

    // The same blend run left to right, for a fill that grows across a
    // field. Defaulted to the left color, solid, on the same terms as the
    // rounded shapes below: a painter without it still shows how far the fill
    // reaches, which is the part that holds the meaning.
    virtual void  FillHorizontalGradientRect (float    xPx,
                                              float    yPx,
                                              float    widthPx,
                                              float    heightPx,
                                              uint32_t argbLeft,
                                              uint32_t argbRight)
    { (void) argbRight; FillRect (xPx, yPx, widthPx, heightPx, argbLeft); }

    virtual void  OutlineRect       (float    xPx,
                                     float    yPx,
                                     float    widthPx,
                                     float    heightPx,
                                     float    thicknessPx,
                                     uint32_t argbColor)                        = 0;

    virtual void  FillCircle        (float    cxPx,
                                     float    cyPx,
                                     float    radiusPx,
                                     uint32_t argbColor)                        = 0;

    // A rounded outline, for focus rings around text-shaped controls: a
    // square ring around a run of text reads as a box the text sits in, and
    // a link is not a box.
    //
    // Defaulted to the square outline rather than made pure, so a mock or a
    // simpler painter is not obliged to implement arc slicing to compile --
    // and so a painter that cannot round corners still draws a ring, which is
    // the part that carries the meaning.
    virtual void  OutlineRoundedRect (float    xPx,
                                      float    yPx,
                                      float    widthPx,
                                      float    heightPx,
                                      float    radiusPx,
                                      float    thicknessPx,
                                      uint32_t argbColor)
    { (void) radiusPx; OutlineRect (xPx, yPx, widthPx, heightPx, thicknessPx, argbColor); }

    // A rounded fill, for hover, selection and pressed states and for any
    // surface drawn as a card. Defaulted to the square fill on the same terms
    // as the outline above: a painter that cannot round a corner still marks
    // the state, which is what the fill is for.
    virtual void  FillRoundedRect (float    xPx,
                                   float    yPx,
                                   float    widthPx,
                                   float    heightPx,
                                   float    radiusPx,
                                   uint32_t argbColor)
    { (void) radiusPx; FillRect (xPx, yPx, widthPx, heightPx, argbColor); }

    // Glyph-painting primitives (input-device selector). Defaulted
    // to no-ops on the interface so test mocks and simple painters compile
    // unchanged; the concrete DxuiPainter implements them with analytic
    // edge coverage, as it does FillCircle. The quad must be convex,
    // its points given in order (clockwise or counter-clockwise).
    virtual void  FillConvexQuad    (float x0, float y0, float x1, float y1,
                                     float x2, float y2, float x3, float y3,
                                     uint32_t argbColor)
    { (void) x0; (void) y0; (void) x1; (void) y1; (void) x2; (void) y2; (void) x3; (void) y3; (void) argbColor; }

    virtual void  FillEllipse       (float cxPx, float cyPx,
                                     float radiusXPx, float radiusYPx,
                                     uint32_t argbColor)
    { (void) cxPx; (void) cyPx; (void) radiusXPx; (void) radiusYPx; (void) argbColor; }

    virtual void  DrawLine          (float x0, float y0, float x1, float y1,
                                     float thicknessPx, uint32_t argbColor)
    { (void) x0; (void) y0; (void) x1; (void) y1; (void) thicknessPx; (void) argbColor; }

    // Global alpha multiplier applied to every vertex's alpha channel.
    // Defaulted to a no-op on the interface so test mocks don't have to
    // implement alpha tracking; the concrete DxuiPainter overrides
    // these to drive the live-preview fade pipeline.
    virtual void   SetGlobalAlpha (float alpha)                               { (void) alpha; }

    // Offset added to every coordinate drawn after it, in pixels. A popup
    // host sets it so a content hook that paints from (0,0) lands inside a
    // shadow margin the hook knows nothing about. Defaulted to a no-op so a
    // mock or a simple painter compiles unchanged.
    virtual void   SetOrigin (float xPx, float yPx)                           { (void) xPx; (void) yPx; }

    // Clip every shape drawn after it to `clipPx` (same coordinates as the
    // draw calls), or stop clipping with nullptr. One level, not a stack: a
    // scrolling container sets it around its children and clears it after.
    // A container nested in another reads the clip in force with GetClipRect
    // first, clips to the overlap, and sets the one it found again after;
    // GetClipRect returns false while nothing clips.
    virtual void   SetClipRect (const RECT * clipPx)                          { (void) clipPx; }
    virtual bool   GetClipRect (RECT & clipPx) const                          { (void) clipPx; return false; }
    virtual float  GetGlobalAlpha () const                                    { return 1.0f; }

    // A rectangle nothing drawn after it reaches outside of, until the
    // matching PopClip. Clips nest, each inside the one before. Defaulted to
    // a no-op so a mock or a simple painter compiles unchanged.
    virtual void   PushClip (float xPx, float yPx, float widthPx, float heightPx) { (void) xPx; (void) yPx; (void) widthPx; (void) heightPx; }
    virtual void   PopClip  ()                                                    {}

    // While set, everything drawn up to the batch's end takes coverage away
    // from what is under it instead of drawing over it: a shape of alpha a
    // leaves (1 - a) of each pixel, alpha included, so a composited window
    // shows the desktop through it. Source-over cannot do that, since it
    // never lowers an opaque pixel's alpha. Cleared by the next batch.
    virtual void   SetErase (bool erase)                                          { (void) erase; }
};
