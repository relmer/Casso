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


protected:
    std::wstring           m_title;
    bool                   m_dirty       = false;
    std::function<void()>  m_onDirtyChanged;
    bool                   m_hasViewport = false;
    RECT                   m_viewportPx  = {};
};
