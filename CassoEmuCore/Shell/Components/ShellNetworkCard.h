#pragma once

#include "Pch.h"

class EmulatorShell;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellNetworkCard
//
//  The Uthernet II's infobar: a port the running guest cannot listen on,
//  because the machine's serial connector or another program on the PC has
//  it. The infobar is up until the port opens or the user closes it, and a
//  different problem raises it again.
//
//  The guest listens on the CPU thread, so that thread notes the sentence
//  (NoteListenProblem) and the UI thread applies it on its next frame
//  (Tick), as ShellSerialCard does for the serial card's notice. The infobar
//  shares the chrome's change band, below the serial card's.
//
////////////////////////////////////////////////////////////////////////////////

class ShellNetworkCard
{
public:
    explicit ShellNetworkCard (EmulatorShell & shell);

    // CPU thread: the sentence for the guest's failed listens, empty when it
    // has none.
    void  NoteListenProblem (const wstring & text);

    // Any thread: the sentence last noted, for the Network settings page.
    wstring  GetListenProblem () const;

    // UI thread, once per frame: applies a noted sentence.
    void  Tick ();

    DxuiInfoBanner &  GetBanner           () { return m_banner; }
    int               GetBandThicknessPx  (int clientWidthPx) const;
    void              LayoutBanner        (const RECT & bounds);
    bool              OfferMouseToBanner  (DxuiMouseEventKind kind, int x, int y);

private:
    void  Show (bool visible);

    EmulatorShell     & m_shell;
    DxuiInfoBanner      m_banner;

    mutable mutex       m_pendingLock;
    wstring             m_pending;
    atomic<uint32_t>    m_pendingGeneration = 0;
    uint32_t            m_appliedGeneration = 0;

    wstring             m_shown;   // the sentence the infobar has, closed or not
};
