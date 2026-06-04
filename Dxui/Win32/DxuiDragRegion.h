#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragRegion
//
//  Invisible caption-area filler. Lives in the chrome layer wherever
//  a caption-bar element wants `WM_NCHITTEST` to classify the area
//  as `HTCAPTION` (so Win32 gives us free drag / double-click-to-
//  maximize) without painting anything. ClassifyHit unconditionally
//  returns DxuiHitTestKind::Caption; Paint is a no-op.
//
//  All public methods are called on the UI thread (FR-083).
//
////////////////////////////////////////////////////////////////////////////////



class DxuiDragRegion : public IDxuiControl
{
public:
    DxuiDragRegion  () = default;
    ~DxuiDragRegion () override = default;

    void  Layout         (const RECT          & boundsDip,
                          const DxuiDpiScaler & scaler) override;
    void  Paint          (IDxuiPainter        & painter,
                          IDxuiTextRenderer   & text,
                          const IDxuiTheme    & theme) override;

    DxuiHitTestKind     ClassifyHit       (POINT clientDip) const override;
    DxuiAccessibleRole  AccessibleRole    () const          override { return DxuiAccessibleRole::CaptionBar; }
};
