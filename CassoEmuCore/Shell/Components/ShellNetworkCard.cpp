#include "Pch.h"

#include "Shell/Components/ShellNetworkCard.h"
#include "Shell/Components/ShellChrome.h"
#include "Shell/EmulatorShell.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellNetworkCard
//
////////////////////////////////////////////////////////////////////////////////

ShellNetworkCard::ShellNetworkCard (EmulatorShell & shell)
    : m_shell (shell)
{
    m_banner.SetSeverity    (DxuiInfoBanner::Severity::Warning);
    m_banner.SetDismissible (true);
    m_banner.SetVisible     (false);
    m_banner.SetOnDismiss   ([this] () { Show (false); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  NoteListenProblem
//
//  Only a sentence that differs from the last one counts as news, so a frame
//  that finds the same failed listens costs a comparison.
//
////////////////////////////////////////////////////////////////////////////////

void ShellNetworkCard::NoteListenProblem (const wstring & text, const wstring & serialText)
{
    lock_guard<mutex>  guard (m_pendingLock);



    m_pendingSerial = serialText;

    if (text != m_pending)
    {
        m_pending = text;
        ++m_pendingGeneration;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetListenProblem
//
////////////////////////////////////////////////////////////////////////////////

wstring ShellNetworkCard::GetListenProblem() const
{
    lock_guard<mutex>  guard (m_pendingLock);



    return m_pending;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSerialListenProblem
//
////////////////////////////////////////////////////////////////////////////////

wstring ShellNetworkCard::GetSerialListenProblem() const
{
    lock_guard<mutex>  guard (m_pendingLock);



    return m_pendingSerial;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
//  A new sentence raises the infobar even after the last one was closed: it
//  reports a different port, or a problem that cleared and came back. An
//  empty one takes the infobar down, as the port has opened.
//
////////////////////////////////////////////////////////////////////////////////

void ShellNetworkCard::Tick()
{
    uint32_t  generation = m_pendingGeneration.load();
    wstring   text;



    if (generation != m_appliedGeneration)
    {
        m_appliedGeneration = generation;
        text                = GetListenProblem();

        if (text != m_shown)
        {
            m_shown = text;
            m_banner.SetText (text);
            Show (!text.empty());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBandThicknessPx
//
//  Zero while hidden, so the change band is only as tall as the visible
//  infobars.
//
////////////////////////////////////////////////////////////////////////////////

int ShellNetworkCard::GetBandThicknessPx (int clientWidthPx) const
{
    int  height = 0;



    if (m_banner.IsVisible() && clientWidthPx > 0)
    {
        height = static_cast<int> (m_banner.GetPreferredHeightPx (static_cast<float> (clientWidthPx), m_shell.m_scaler));
    }

    return height;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutBanner
//
////////////////////////////////////////////////////////////////////////////////

void ShellNetworkCard::LayoutBanner (const RECT & bounds)
{
    if (m_banner.IsVisible() && bounds.right > bounds.left)
    {
        m_banner.Layout (bounds, m_shell.m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OfferMouseToBanner
//
//  Only while the infobar is up, and only inside it.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellNetworkCard::OfferMouseToBanner (DxuiMouseEventKind kind, int x, int y)
{
    RECT            bounds  = m_banner.GetBounds();
    DxuiMouseEvent  ev      = {};
    bool            inside  = m_banner.IsVisible()
                              && x >= bounds.left && x < bounds.right
                              && y >= bounds.top  && y < bounds.bottom;
    bool            handled = false;



    if (inside)
    {
        ev.kind        = kind;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = POINT { x, y };

        handled = m_banner.OnMouse (ev);
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
//  The band changes height, so the chrome is docked again around it.
//
////////////////////////////////////////////////////////////////////////////////

void ShellNetworkCard::Show (bool visible)
{
    m_banner.SetVisible (visible);

    if (m_shell.m_chrome != nullptr)
    {
        m_shell.m_chrome->ReflowChromeForChangeBand();
    }
}
