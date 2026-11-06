#pragma once

#include "Pch.h"

#include "Ui/Chrome/BranchPreview.h"
#include "Ui/Chrome/ReplayIndicator.h"
#include "Ui/Chrome/RewindBar.h"

class DxuiHwndSource;
class DxuiPanel;
class EmulatorShell;
class ShellChrome;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellHistoryOverlays
//
//  History's controls over the emulated screen: the rewind bar along its
//  bottom, "Replaying" across its top while the machine runs behind live,
//  and the last frame of the branch whose fork row is under the pointer. On
//  the desk scene they follow the tube's bounds, which the composition's
//  glass rectangle is not. A capture paint hides all three.
//
////////////////////////////////////////////////////////////////////////////////

class ShellHistoryOverlays
{
public:
    ShellHistoryOverlays (EmulatorShell & shell, ShellChrome & chrome) : m_shell (shell), m_chrome (chrome) {}

    RewindBar &  GetRewindBar    () { return m_rewindBar; }

    void         Layout          (const RECT & viewportRect);
    void         Adopt           (DxuiPanel & root);
    void         SetPopupHost    (DxuiHwndSource & host) { m_rewindBar.SetPopupHost (&host); }
    void         Sync            ();
    void         SetHidden       (bool hidden);
    bool         WantsTick       () const { return m_rewindBar.WantsTick() || m_replayIndicator.IsVisible(); }

private:
    void         SyncReplayIndicator ();
    void         SyncBranchPreview   ();

    EmulatorShell    & m_shell;
    ShellChrome      & m_chrome;
    RewindBar          m_rewindBar;
    ReplayIndicator    m_replayIndicator;
    BranchPreview      m_branchPreview;
    bool               m_isHidden     = false;
    RECT               m_screenRectPx = {};   // the emulated screen as drawn, from the last layout
};
