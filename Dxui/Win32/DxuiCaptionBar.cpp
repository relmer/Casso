#include "Pch.h"

#include "DxuiCaptionBar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCaptionBar
//
//  Constructor — defers entirely to DxuiPanel; nothing to add yet.
//
////////////////////////////////////////////////////////////////////////////////

DxuiCaptionBar::DxuiCaptionBar()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClassifyHit
//
//  Front-to-back walk of children: the topmost child whose bounds
//  contain the point gets to classify. Blank caption area (no child
//  consumed it) falls through to DxuiHitTestKind::Caption.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHitTestKind DxuiCaptionBar::ClassifyHit (POINT clientDip) const
{
    IDxuiControl *   child = nullptr;
    DxuiHitTestKind  kind  = DxuiHitTestKind::Caption;
    RECT             rc    = {};
    size_t           n     = 0;
    size_t           i     = 0;



    n = ChildCount();

    // Reverse order so visually-topmost children win.
    for (i = n; i > 0; --i)
    {
        child = Child (i - 1);
        if (child == nullptr || !child->Visible())
        {
            continue;
        }

        rc = child->Bounds();
        if (clientDip.x < rc.left || clientDip.x >= rc.right ||
            clientDip.y < rc.top  || clientDip.y >= rc.bottom)
        {
            continue;
        }

        kind = child->ClassifyHit (clientDip);
        if (kind != DxuiHitTestKind::None)
        {
            return kind;
        }
    }

    return DxuiHitTestKind::Caption;
}
