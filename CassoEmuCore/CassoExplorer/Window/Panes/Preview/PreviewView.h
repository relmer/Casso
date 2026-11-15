#pragma once

#include "Pch.h"

class ExplorerPreviewManager;
struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewView
//
//  One kind of preview in the Preview pane: its widget, created as a child of
//  the window, laid into the pane's content and hidden with the pane.
//  ExplorerPreviewManager creates each and makes the one for the selection's
//  preview visible.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewView
{
public:
    PreviewView (ExplorerPreviewManager & manager, ExplorerContext & context) : m_manager (manager), m_context (context) {}
    virtual ~PreviewView() = default;

    virtual void  Create               ()                      = 0;
    virtual void  Configure            ()                      = 0;
    virtual void  Layout               (const RECT & content)  = 0;
    virtual void  Hide                 ()                      = 0;
    virtual bool  UpdateScrollbarHover (POINT)                 { return false; }
    virtual bool  TickScrollbars       (int64_t)               { return false; }

    //  The height the view's content takes at its width, and whether it is
    //  scrolled to its top, for a disk image's thumbnail above it, which the
    //  wheel scrolls away before the view; 0, and at its top, for a view that
    //  is hidden or doesn't scroll.
    virtual int   GetContentHeightPx   () const                { return 0; }
    virtual bool  IsAtTop              () const                { return true; }

    //  How far the view is scrolled, in pixels, set to the nearest row or
    //  line; and its vertical bar hidden while the pane's bar scrolls the
    //  thumbnail and the view as one.
    virtual int   GetScrollPx          () const                { return 0; }
    virtual void  SetScrollPx          (int)                   {}
    virtual void  SetScrollbarHidden   (bool)                  {}

protected:
    ExplorerPreviewManager  & m_manager;
    ExplorerContext         & m_context;
};
