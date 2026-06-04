#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCaptionBar
//
//  Container panel covering the custom-chrome title-bar strip at the
//  top of a `DxuiHostWindow`. Inherits all `DxuiPanel` semantics
//  (children, layout, paint fan-out) and adds caption-specific hit-
//  test defaults: any point not consumed by a child resolves to
//  `DxuiHitTestKind::Caption`, giving Win32 free drag / double-click-
//  to-maximize on blank areas of the title bar.
//
//  Typical children:
//      - app icon (DxuiDragRegion or custom control)
//      - title text label (DxuiLabel)
//      - flexible drag-spacer (DxuiDragRegion)
//      - system buttons (DxuiSystemButton min/max/close)
//
//  All public methods are called on the UI thread (FR-083).
//
////////////////////////////////////////////////////////////////////////////////



class DxuiCaptionBar : public DxuiPanel
{
public:
    DxuiCaptionBar  ();
    ~DxuiCaptionBar () override = default;

    DxuiHitTestKind     ClassifyHit       (POINT clientDip) const override;
    DxuiAccessibleRole  AccessibleRole    () const          override { return DxuiAccessibleRole::CaptionBar; }
};
