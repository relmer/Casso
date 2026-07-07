#include "Pch.h"

#include "DxuiDragRegion.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  Stores the bounds; no children, no per-child math.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDragRegion::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    DXUI_ASSERT_UI_THREAD();

    (void) scaler;

    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  No-op. Drag regions are invisible by design — the parent caption
//  bar paints the background.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDragRegion::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DXUI_ASSERT_UI_THREAD();

    (void) painter;
    (void) text;
    (void) theme;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClassifyHit
//
//  Always returns Caption — the whole point of this control.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHitTestKind DxuiDragRegion::ClassifyHit (POINT clientDip) const
{
    (void) clientDip;
    return DxuiHitTestKind::Caption;
}
