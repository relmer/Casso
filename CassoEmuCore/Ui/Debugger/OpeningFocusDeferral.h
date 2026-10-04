#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  OpeningFocusDeferral
//
//  The focus the debugger gives back when it opens goes to the pane that had
//  it when the window closed. Some panes exist only after the machine's first
//  snapshot -- a second disassembly view, a device panel -- so the first
//  layout cannot reach them and the focus goes to the console for the moment.
//  This decides, snapshot by snapshot, whether the saved pane takes it once it
//  shows: only while the views the window reopens are still settling, and
//  never once the user has clicked or typed in the window.
//
////////////////////////////////////////////////////////////////////////////////

class OpeningFocusDeferral
{
public:
    enum class Action
    {
        None,
        Place,
    };

    void    Defer      ();
    void    OnUserInput();
    Action  OnSnapshot (bool isSavedPaneReady, bool isRestoringViews);
    bool    IsPending  () const  { return m_isPending; }

private:
    bool  m_isPending    = false;
    bool  m_hasUserActed = false;
};
