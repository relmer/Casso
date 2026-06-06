#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Core/IDxuiViewportInputSink.h"



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiViewport
//
//  Leaf `IDxuiControl` describing an externally-rendered region inside
//  a `DxuiHostWindow`'s control tree. Used to give the Apple ][
//  emulator (or any other client of an externally-managed framebuffer)
//  a known rectangle inside the Dxui control tree without requiring
//  the host to know anything about the renderer.
//
//  Responsibilities:
//      - Reports its size policy to enclosing layouts (`Fixed` /
//        `Preferred` / `Fill`). Layouts that honor the policy can ask
//        for the preferred size via `PreferredSizeDip()`. Fill clients
//        report {0,0} as a "no opinion" preferred size.
//      - Notifies a registered bounds-changed callback when the
//        rectangle changes so the external renderer can resize its
//        render target.
//      - Optionally consumes mouse / key input and forwards it to an
//        `IDxuiViewportInputSink`. Reserved Dxui chords (Tab,
//        Shift+Tab, Esc, Alt-alone, F10) bypass the sink so focus
//        traversal and menu activation continue to work.
//
//  Painting is always a no-op: the external renderer draws into the
//  same swap chain at the rectangle reported by `Bounds()`. Chrome
//  above the viewport paints on top through the normal control-tree
//  fanout.
//
//  All public methods are called on the UI thread (FR-083).
//
////////////////////////////////////////////////////////////////////////////////



class DxuiViewport : public IDxuiControl
{
public:
    enum class SizePolicy
    {
        Fixed,
        Preferred,
        Fill,
    };

    using BoundsChangedFn = std::function<void(const RECT &)>;

    DxuiViewport () = default;
    ~DxuiViewport() override = default;

    void  SetSizePolicy          (SizePolicy policy);
    void  SetPreferredSizeDip    (SIZE       sizeDip);
    void  SetConsumesInput       (bool       consumesInput);
    void  SetInputSink           (IDxuiViewportInputSink * sink);
    void  SetOnBoundsChanged     (BoundsChangedFn callback) { m_onBoundsChanged = std::move (callback); }

    SizePolicy                Policy             () const { return m_policy;            }
    SIZE                      PreferredSizeDip   () const { return m_preferredSizeDip;  }
    bool                      ConsumesInput      () const { return m_consumesInput;     }
    IDxuiViewportInputSink *  InputSink          () const { return m_sink;              }

    static bool  IsReservedChord (const DxuiKeyEvent & ev);

    void              Layout       (const RECT          & boundsDip,
                                    const DxuiDpiScaler & scaler) override;
    void              Paint        (IDxuiPainter        & painter,
                                    IDxuiTextRenderer   & text,
                                    const IDxuiTheme    & theme) override;

    bool              OnMouse      (const DxuiMouseEvent & ev) override;
    bool              OnKey        (const DxuiKeyEvent   & ev) override;

    DxuiHitTestKind   ClassifyHit  (POINT clientDip) const override;

    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::Viewport; }

private:
    BoundsChangedFn           m_onBoundsChanged;
    RECT                      m_lastNotifiedBoundsDip = {};
    bool                      m_hasNotifiedBounds     = false;
    SizePolicy                m_policy                = SizePolicy::Fill;
    SIZE                      m_preferredSizeDip      = { 0, 0 };
    bool                      m_consumesInput         = false;
    IDxuiViewportInputSink *  m_sink                  = nullptr;
};
