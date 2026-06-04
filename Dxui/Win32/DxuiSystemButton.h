#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSystemButton
//
//  Win11-style minimize / maximize-restore / close button that lives
//  on a `DxuiCaptionBar`. Owns its `Kind` (which classification it
//  reports to `WM_NCHITTEST` via `ClassifyHit`) and an associated
//  HWND used at click time to dispatch the standard Win32 system
//  command:
//      Min   → ShowWindow (hwnd, SW_MINIMIZE)
//      Max   → SendMessage (hwnd, WM_SYSCOMMAND,
//                           IsZoomed (hwnd) ? SC_RESTORE
//                                           : SC_MAXIMIZE, 0)
//      Close → SendMessage (hwnd, WM_CLOSE, 0, 0)
//
//  Paint draws the Win11 glyph (line for Min, hollow square for Max,
//  diagonal X for Close) using the supplied IDxuiPainter — vector
//  primitives only, no glyph fonts required.
//
//  All public methods are called on the UI thread (FR-083).
//
////////////////////////////////////////////////////////////////////////////////



enum class DxuiSystemButtonKind
{
    Min,
    Max,
    Close,
};



class DxuiSystemButton : public IDxuiControl
{
public:
    explicit DxuiSystemButton  (DxuiSystemButtonKind kind);
    ~DxuiSystemButton          () override = default;

    void  SetHwnd        (HWND hwnd);
    HWND  Hwnd           () const { return m_hwnd; }

    DxuiSystemButtonKind  Kind () const { return m_kind; }

    bool  Hovered        () const { return m_hovered; }
    bool  Pressed        () const { return m_pressed; }

    void  Layout         (const RECT          & boundsDip,
                          const DxuiDpiScaler & scaler) override;
    void  Paint          (IDxuiPainter        & painter,
                          IDxuiTextRenderer   & text,
                          const IDxuiTheme    & theme) override;

    bool  OnMouse        (const DxuiMouseEvent & ev) override;

    DxuiHitTestKind     ClassifyHit       (POINT clientDip) const override;
    DxuiAccessibleRole  AccessibleRole    () const          override { return DxuiAccessibleRole::Button; }
    std::wstring        AccessibleName    () const          override;

private:
    void  DispatchClick  ();


    DxuiSystemButtonKind  m_kind     = DxuiSystemButtonKind::Min;
    HWND                  m_hwnd     = nullptr;
    DxuiDpiScaler         m_scaler;
    bool                  m_hovered  = false;
    bool                  m_pressed  = false;
};
