#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPropertyPage
//
//  One tab-page of a DxuiPropertySheet (the Win32 PROPSHEETPAGE analog).
//  A page is a DxuiPanel that carries a tab GetTitle(), a dirty flag the
//  sheet reads to enable Apply, and validate/commit + activation hooks:
//
//      OnActivated() -- the tab became the visible page.
//      OnApply()     -- validate + commit this page's edits; return false
//                       to block OK / Apply (e.g. invalid input).
//
//  Subclasses build their controls as children (CreateChild<...>) and
//  call MarkDirty() when an edit changes committed state.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiPropertyPage : public DxuiPanel
{
public:
    explicit DxuiPropertyPage (std::wstring title) : m_title (std::move (title)) {}
    ~DxuiPropertyPage () override = default;

    const std::wstring &  GetTitle () const { return m_title; }
    bool                  IsDirty  () const { return m_dirty; }

    //
    //  Set/clear the dirty flag and notify the sheet (which re-evaluates
    //  the Apply button). The sheet installs the callback via
    //  SetOnDirtyChanged when the page is added.
    //
    void  MarkDirty        (bool dirty = true);
    void  SetOnDirtyChanged (std::function<void()> fn) { m_onDirtyChanged = std::move (fn); }

    //
    //  Subclass hooks. Default: activation is a no-op, apply succeeds.
    //
    virtual void  OnActivated () {}
    virtual bool  OnApply     () { return true; }

    //
    //  The viewport the sheet shows this page through, in client pixels, or
    //  nullptr when the page is shown whole. The sheet does the clipping and
    //  the input filtering; the page records it so a press outside it cannot
    //  focus one of its controls.
    //
    void  SetViewport    (const RECT * viewportPx);
    bool  HasViewport    () const { return m_hasViewport; }
    RECT  GetViewport    () const { return m_viewportPx; }
    bool  IsPointClipped (POINT clientPx) const override;

    //
    //  A page is a tab group at its viewport, so Tab takes every control on
    //  it, those scrolled out of view included, after the tab strip and
    //  before the buttons below it.
    //
    bool  IsTabGroup       () const override { return true; }
    RECT  GetTabGroupPlace () const override { return m_hasViewport ? m_viewportPx : GetBounds(); }

    //
    //  How tall the page's content is, in pixels from the top of the rect it
    //  was laid out in, or 0 for a page that fits whatever rect it is given.
    //  The sheet scrolls a page whose content runs past the viewport, and
    //  takes a change reported after layout -- rows added or sliding -- as a
    //  change to the scroll range.
    //
    void  SetContentHeightPx        (int heightPx);
    int   GetContentHeightPx        () const { return m_contentHeightPx; }

    //
    //  How wide the page's content is, in pixels from the left of the rect it
    //  was laid out in, or 0 for a page that fits whatever width it is given.
    //  With the height, this is what the sheet sizes its largest window to.
    //
    void  SetContentWidthPx         (int widthPx) { m_contentWidthPx = widthPx; }
    int   GetContentWidthPx         () const { return m_contentWidthPx; }

    //
    //  How wide the rect the page is laid out in would be at the sheet's
    //  design size, in pixels, or 0 when the sheet has none. The sheet sets
    //  it before each layout; a page that stretches with a wider sheet
    //  measures the stretch from it, and keeps what it reports as its content
    //  width to what it shows at the design width.
    //
    void  SetDesignWidthPx          (int widthPx) { m_designWidthPx = widthPx; }
    int   GetDesignWidthPx          () const { return m_designWidthPx; }
    void  SetOnContentHeightChanged (std::function<void()> fn) { m_onContentHeightChanged = std::move (fn); }

    //
    //  Asks the sheet to scroll a scrolled page just far enough to show a
    //  rect of it, in client pixels as laid out now: a control that just
    //  appeared below the part of the page in view, say. Ignored while the
    //  page is not scrolled.
    //
    void  RequestReveal           (const RECT & rectPx);
    void  SetOnRevealRequested    (std::function<void (const RECT &)> fn) { m_onRevealRequested = std::move (fn); }


protected:
    //
    //  The lowest bottom edge and the rightmost right edge among the page's
    //  visible children, in client pixels, or 0 when none is visible. A page
    //  of fixed-size controls adds its padding to these to report its
    //  content extents.
    //
    int  GetLowestChildBottomPx  () const;
    int  GetRightmostChildEdgePx () const;

    std::wstring           m_title;
    bool                   m_dirty           = false;
    std::function<void()>  m_onDirtyChanged;
    bool                   m_hasViewport     = false;
    RECT                   m_viewportPx      = {};
    int                    m_contentHeightPx = 0;
    int                    m_contentWidthPx  = 0;
    int                    m_designWidthPx   = 0;
    std::function<void()>  m_onContentHeightChanged;
    std::function<void (const RECT &)>  m_onRevealRequested;
};
