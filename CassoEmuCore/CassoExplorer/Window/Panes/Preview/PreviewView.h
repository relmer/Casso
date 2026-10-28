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

    //  Preview text is drawn this far from the background toward the theme's
    //  foreground, a gray like a terminal's text rather than full white.
    static constexpr float  kTextStrength = 0.8f;

    virtual void  Create               ()                      = 0;
    virtual void  Configure            ()                      = 0;
    virtual void  Layout               (const RECT & content)  = 0;
    virtual void  Hide                 ()                      = 0;
    virtual bool  UpdateScrollbarHover (POINT)                 { return false; }
    virtual bool  TickScrollbars       (int64_t)               { return false; }

protected:
    ExplorerPreviewManager  & m_manager;
    ExplorerContext         & m_context;
};
