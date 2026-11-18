#pragma once

#include "Pch.h"
#include "Core/DxuiPaneLayout.h"


class DxuiDpiScaler;
class DxuiTabGroup;
class IDxuiControl;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiDockDragHost
//
//  The dock site a DxuiDockDrag runs in: its layout and groups, the areas
//  the panes are laid out in, and what a drop does to the site.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiDockDragHost
{
public:
    using GroupRect = DxuiPaneLayout::GroupRect;

    virtual                          ~IDxuiDockDragHost() = default;

    virtual const DxuiPaneLayout   & GetDragLayout       () const                                      = 0;
    virtual DxuiPaneLayout         & EditDragLayout      ()                                            = 0;
    virtual size_t                   GetDragGroupCount   () const                                      = 0;
    virtual DxuiTabGroup           * GetDragGroup        (size_t index) const                          = 0;
    virtual const DxuiTabGroup     * FindDragGroupOf     (const std::wstring & pane) const             = 0;
    virtual std::wstring             GetDragPaneOf       (const IDxuiControl * content) const          = 0;
    virtual const DxuiDpiScaler    & GetDragScaler       () const                                      = 0;
    virtual RECT                     GetDragDockedArea   () const                                      = 0;
    virtual RECT                     GetDragPaneArea     () const                                      = 0;
    virtual long                     GetDragGapPx        () const                                      = 0;
    virtual std::vector<GroupRect>   ArrangeForDrag      (const DxuiPaneLayout & layout) const         = 0;
    virtual bool                     IsDragDocumentGroup (const std::vector<std::wstring> & panes) const = 0;
    virtual void                     SlideInForDrag      (const std::wstring & pane)                   = 0;
    virtual void                     RequestDragFloat    (const std::wstring & pane, POINT pointDip)   = 0;
    virtual void                     CommitDragDrop      ()                                            = 0;
};
