#include "Pch.h"

#include "Shell/Components/ShellNetworkCard.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Net/FirewallRemedy.h"
#include "Net/FirewallText.h"
#include "Seams/Win32FirewallHelper.h"
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
    m_banner.SetOnAction    ([this] (size_t) { OnAllow(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~ShellNetworkCard
//
//  A running helper is left to exit; only the handle is closed.
//
////////////////////////////////////////////////////////////////////////////////

ShellNetworkCard::~ShellNetworkCard()
{
    if (m_firewallHelper != nullptr)
    {
        CloseHandle (m_firewallHelper);
        m_firewallHelper = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NoteListenProblem
//
//  Only a sentence that differs from the last one counts as news, so a frame
//  that finds the same failed listens costs a comparison.
//
////////////////////////////////////////////////////////////////////////////////

void ShellNetworkCard::NoteListenProblem (const wstring & text, const wstring & serialText, const std::vector<FirewallPort> & deferred)
{
    lock_guard<mutex>  guard (m_pendingLock);



    m_pendingSerial = serialText;

    if (text != m_pending || deferred != m_pendingDeferred)
    {
        m_pending     = text;
        m_pendingDeferred = deferred;
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



    ReadFirewall (false);

    if (generation != m_appliedGeneration)
    {
        {
            lock_guard<mutex>  guard (m_pendingLock);

            m_appliedGeneration = generation;
            text                = m_pending;
            m_deferred              = m_pendingDeferred;
        }

        m_banner.SetActions (m_deferred.empty() ? std::vector<std::wstring>() : std::vector<std::wstring> { FirewallText::kAllowShortAction });

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
//  ReadFirewall
//
//  The helper changes the rules without a notification, so its exit marks
//  the snapshot out of date. The gate is set only when the monitor read the
//  firewall again, so a deferred socket opens only after a new reading.
//
////////////////////////////////////////////////////////////////////////////////

void ShellNetworkCard::ReadFirewall (bool force)
{
    HRESULT   result = S_OK;
    uint64_t  now    = GetTickCount64();



    if (m_firewallHelper != nullptr && Win32FirewallHelper::TryFinish (m_firewallHelper, result))
    {
        m_shell.m_firewall.Invalidate();
        force = true;

        if (FAILED (result))
        {
            m_shown = m_shown + L"\nWindows Firewall could not be changed: " + ChangePrompt::DescribeError (result);
            m_banner.SetText (m_shown);
            Show (true);
        }
    }

    if (!force && now < m_nextFirewallReadMs)
    {
        return;
    }

    m_nextFirewallReadMs = now + kFirewallReadMs;
    m_shell.m_firewall.Update();

    if (m_shell.m_firewall.GetVersion() != m_gateFirewallVersion)
    {
        m_gateFirewallVersion = m_shell.m_firewall.GetVersion();
        m_shell.m_listenGate.Set (m_shell.m_firewall.GetSnapshot(), m_shell.m_firewall.GetProgram());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnAllow
//
//  The helper admits every deferred port on the profiles of the networks the PC
//  is connected to. Declining the UAC prompt changes nothing and is not an
//  error.
//
////////////////////////////////////////////////////////////////////////////////

void ShellNetworkCard::OnAllow()
{
    HRESULT       hr = S_OK;
    FirewallPlan  plan;



    BAIL_OUT_IF (m_deferred.empty() || m_firewallHelper != nullptr, S_OK);

    plan = FirewallRemedy::MakePlan (m_shell.m_firewall.GetSnapshot(), m_shell.m_firewall.GetProgram(), kFirewallTitle, m_deferred, false);
    BAIL_OUT_IF (plan.ports.empty(), S_OK);

    hr = Win32FirewallHelper::Launch (m_shell.m_hwnd, plan, m_firewallHelper);
    BAIL_OUT_IF (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED), S_OK);
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        m_shown = m_shown + L"\nWindows Firewall could not be changed: " + ChangePrompt::DescribeError (hr);
        m_banner.SetText (m_shown);
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
