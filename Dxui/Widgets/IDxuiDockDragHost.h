#pragma once

#include "Pch.h"
#include "Core/DxuiPaneLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiDockDragHost
//
//  The dock site a DxuiDockDrag runs in: the areas the panes are laid out
//  in, a layout arranged there as the site would, and what a drop does to
//  the site.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiDockDragHost
{
public:
    using GroupRect = DxuiPaneLayout::GroupRect;

    virtual                          ~IDxuiDockDragHost() = default;

    virtual RECT                     GetDragDockedArea   () const                                        = 0;
    virtual RECT                     GetDragPaneArea     () const                                        = 0;
    virtual long                     GetDragGapPx        () const                                        = 0;
    virtual std::vector<GroupRect>   ArrangeForDrag      (const DxuiPaneLayout & layout) const           = 0;
    virtual bool                     IsDragDocumentGroup (const std::vector<std::wstring> & panes) const = 0;
    virtual void                     SlideInForDrag      (const std::wstring & pane)                     = 0;
    virtual void                     RequestDragFloat    (const std::wstring & pane, POINT pointDip)     = 0;
    virtual void                     CommitDragDrop      ()                                              = 0;
};