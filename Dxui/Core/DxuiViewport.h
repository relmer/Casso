#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiViewport
//
//  Minimal leaf placeholder for an externally-rendered viewport region
//  inside a `DxuiHostWindow`'s control tree. Used by Phase 11 to give
//  the Apple ][ emulator (or any other client of an externally-managed
//  framebuffer) a known rectangle inside the Dxui control tree without
//  requiring the host to know anything about the renderer.
//
//  This is the **placeholder** form. The full `DxuiViewport` lands in
//  Phase 12: it grows size policies (Fixed / Preferred / Fill), an
//  `IDxuiViewportInputSink`, reserved-chord routing, and integration
//  with `DxuiDockLayout`.
//
//  Today this control:
//      - paints nothing (the external renderer draws into the same
//        swap chain at the rectangle reported by `Bounds()`)
//      - swallows no input (mouse / key events pass through to the
//        host window, which still owns top-level input routing)
//      - notifies a single std::function callback whenever the
//        bounds rectangle changes, so the external renderer can
//        track where to draw
//
//  All public methods are called on the UI thread (FR-083).
//
////////////////////////////////////////////////////////////////////////////////



class DxuiViewport : public IDxuiControl
{
public:
    using BoundsChangedFn = std::function<void(const RECT &)>;

    DxuiViewport () = default;
    ~DxuiViewport() override = default;

    void  SetOnBoundsChanged (BoundsChangedFn callback) { m_onBoundsChanged = std::move (callback); }

    void  Layout             (const RECT          & boundsDip,
                              const DxuiDpiScaler & scaler) override;
    void  Paint              (IDxuiPainter        & painter,
                              IDxuiTextRenderer   & text,
                              const IDxuiTheme    & theme) override;

    bool  OnMouse            (const DxuiMouseEvent & ev) override { (void) ev; return false; }
    bool  OnKey              (const DxuiKeyEvent   & ev) override { (void) ev; return false; }

    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::Viewport; }

private:
    BoundsChangedFn  m_onBoundsChanged;
    RECT             m_lastNotifiedBoundsDip = {};
    bool             m_hasNotifiedBounds     = false;
};
