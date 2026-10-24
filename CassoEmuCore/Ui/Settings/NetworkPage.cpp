#include "Pch.h"

#include "NetworkPage.h"

#include "Core/TextEncoding.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Net/FirewallText.h"
#include "Seams/Win32FirewallHelper.h"
#include "SettingsPanelState.h"




// Layout metrics (DIP).
static constexpr int  s_kRowHeightDp     = 28;
static constexpr int  s_kLabelWidthDp    = 130;
static constexpr int  s_kMacWidthDp      = 220;
static constexpr int  s_kListWidthDp     = 380;
static constexpr int  s_kListHeightDp    = 150;
static constexpr int  s_kProtocolWidthDp = 80;
static constexpr int  s_kPortLabelDp     = 80;
static constexpr int  s_kPortWidthDp     = 70;
static constexpr int  s_kButtonWidthDp   = 90;
static constexpr int  s_kSmallGapDp      = 8;
static constexpr int  s_kSectionGapDp    = 14;
static constexpr int  s_kPagePadDp       = 16;

// The program in the firewall's words.
static constexpr const wchar_t *  s_kFirewallTitle = L"Casso";

static constexpr const wchar_t *  s_kpszCardBanner =
    L"The Uthernet II in slot {} is an Ethernet card. Casso assigns it the address "
    L"10.0.65.15 and the gateway 10.0.65.1, which is this PC, by DHCP. ADTPro and the "
    L"CassoLink boot disk get their settings this way.";





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::NetworkPage
//
////////////////////////////////////////////////////////////////////////////////

NetworkPage::NetworkPage (std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    std::vector<DxuiListView::Column>  columns (3);



    for (IDxuiControl * control : std::initializer_list<IDxuiControl *> {
             &m_banner, &m_macLabel, &m_mac,
             &m_lan, &m_forwardsLabel, &m_forwards, &m_protocol, &m_hostPortLabel, &m_hostPort,
             &m_guestPortLabel, &m_guestPort, &m_add, &m_remove, &m_firewallBanner, &m_error })
    {
        Adopt (*control);
    }

    m_firewallBanner.SetSeverity (DxuiInfoBanner::Severity::Warning);
    m_firewallBanner.SetVisible  (false);
    m_firewallBanner.SetOnAction ([this] (size_t index) { OnFirewallAction (index); });

    columns[0].title   = L"Protocol";
    columns[1].title   = L"Host port";
    columns[2].title   = L"Guest port";
    columns[2].stretch = true;
    m_forwards.SetColumns            (std::move (columns));
    m_forwards.SetShowHeader         (true);
    m_forwards.SetPreciseAutoFit     (true);
    m_forwards.SetAlwaysShowSelection (true);

    m_error.SetSeverity (DxuiInfoBanner::Severity::Warning);
    m_error.SetVisible  (false);

    WireCallbacks();
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::~NetworkPage
//
//  A running helper is left to exit; only the handle is closed.
//
////////////////////////////////////////////////////////////////////////////////

NetworkPage::~NetworkPage()
{
    if (m_firewallHelper != nullptr)
    {
        CloseHandle (m_firewallHelper);
        m_firewallHelper = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::SetFirewall
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::SetFirewall (FirewallMonitor * firewall, GuestPortsFn guestPorts)
{
    m_firewall      = firewall;
    m_guestPorts    = std::move (guestPorts);
    m_firewallDrawn = false;

    SyncFirewall();
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::SetState
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::SetState (SettingsPanelState * state)
{
    m_state = state;
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::SetPopupHost
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::SetPopupHost (DxuiHwndSource * host)
{
    m_popupHost = host;
    m_protocol.SetPopupHost (host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::MakeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT NetworkPage::MakeRect (
    int  l,
    int  t,
    int  w,
    int  h)
{
    RECT  rc = { l, t, l + w, t + h };



    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::Layout
//
//  The banners' heights are measured, since their text wraps; the controls
//  between them are fixed rows on one control column, like the other pages.
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::Layout (
    const RECT           & rect,
    const DxuiDpiScaler  & scaler)
{
    int   pad       = scaler.ToPx (s_kPagePadDp);
    int   gap       = scaler.ToPx (s_kSectionGapDp);
    int   x         = rect.left + pad;
    int   y         = rect.top  + pad;
    int   width     = (rect.right - rect.left) - pad * 2;
    int   bannerH   = (int) std::ceil (m_banner.GetPreferredHeightPx ((float) width, scaler));
    int   firewallH = 0;
    int   errorH    = 0;



    m_lastRect   = rect;
    m_lastScaler = scaler;
    m_hasLayout  = true;

    m_banner.SetRect (MakeRect (x, y, width, bannerH));
    y += bannerH + gap;

    y = LayoutCardRows    (x, y, width, scaler);
    y = LayoutForwardRows (x + scaler.ToPx (s_kLabelWidthDp), y, scaler);
    y += gap;

    //  Under the forwards and the LAN access setting it concerns.
    if (m_firewallBanner.IsVisible())
    {
        firewallH = (int) std::ceil (m_firewallBanner.GetPreferredHeightPx ((float) width, scaler));
        m_firewallBanner.Layout (MakeRect (x, y, width, firewallH), scaler);
        y += firewallH + gap;
    }

    errorH =m_error.IsVisible() ? (int) std::ceil (m_error.GetPreferredHeightPx ((float) width, scaler)) : 0;
    m_error.SetRect (MakeRect (x, y, width, errorH));

    ApplyDpi (scaler.GetDpi());

    DxuiPanel::SetBounds (rect);
    SetContentHeightPx (GetLowestChildBottomPx() + pad - rect.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::LayoutCardRows
//
//  The MAC address and LAN access rows; returns the y below them.
//
////////////////////////////////////////////////////////////////////////////////

int NetworkPage::LayoutCardRows (
    int                    x,
    int                    y,
    int                    width,
    const DxuiDpiScaler  & scaler)
{
    int  rowHeight  = scaler.ToPx (s_kRowHeightDp);
    int  labelWidth = scaler.ToPx (s_kLabelWidthDp);
    int  gap        = scaler.ToPx (s_kSectionGapDp);
    int  controlsX  = x + labelWidth;



    m_macLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_macLabel.SetText (L"MAC address:");
    m_mac.SetRect      (MakeRect (controlsX, y, scaler.ToPx (s_kMacWidthDp), rowHeight));
    y += rowHeight + gap;

    m_lan.SetRect  (MakeRect (controlsX, y, width - labelWidth, rowHeight));
    m_lan.SetLabel (L"Allow connections from other computers");
    y += rowHeight + gap;

    return y;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::LayoutForwardRows
//
//  The port forward list, the row of fields for a new forward, and the Add
//  and Remove buttons; returns the y below them.
//
////////////////////////////////////////////////////////////////////////////////

int NetworkPage::LayoutForwardRows (
    int                    controlsX,
    int                    y,
    const DxuiDpiScaler  & scaler)
{
    int  rowHeight  = scaler.ToPx (s_kRowHeightDp);
    int  labelWidth = scaler.ToPx (s_kLabelWidthDp);
    int  smallGap   = scaler.ToPx (s_kSmallGapDp);
    int  portLabel  = scaler.ToPx (s_kPortLabelDp);
    int  portWidth  = scaler.ToPx (s_kPortWidthDp);
    int  button     = scaler.ToPx (s_kButtonWidthDp);
    int  x          = controlsX;



    m_forwardsLabel.SetRect (MakeRect (controlsX - labelWidth, y, labelWidth, rowHeight));
    m_forwardsLabel.SetText (L"Port forwards:");
    m_forwards.SetRect      (MakeRect (controlsX, y, scaler.ToPx (s_kListWidthDp), scaler.ToPx (s_kListHeightDp)));
    y += scaler.ToPx (s_kListHeightDp) + smallGap;

    m_protocol.SetRect  (MakeRect (x, y, scaler.ToPx (s_kProtocolWidthDp), rowHeight));
    m_protocol.SetItems ({ L"TCP", L"UDP" });
    x += scaler.ToPx (s_kProtocolWidthDp) + smallGap;

    m_hostPortLabel.SetRect (MakeRect (x, y, portLabel, rowHeight));
    m_hostPortLabel.SetText (L"Host port:");
    m_hostPort.SetRect      (MakeRect (x + portLabel, y, portWidth, rowHeight));
    x += portLabel + portWidth + smallGap;

    m_guestPortLabel.SetRect (MakeRect (x, y, portLabel, rowHeight));
    m_guestPortLabel.SetText (L"Guest port:");
    m_guestPort.SetRect      (MakeRect (x + portLabel, y, portWidth, rowHeight));
    y += rowHeight + smallGap;

    m_add.SetLabel    (L"Add");
    m_add.Layout      (MakeRect (controlsX, y, button, rowHeight));
    m_remove.SetLabel (L"Remove");
    m_remove.Layout   (MakeRect (controlsX + button + smallGap, y, button, rowHeight));
    y += rowHeight;

    return y;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::ApplyDpi
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::ApplyDpi (UINT dpi)
{
    m_banner.SetDpi         (dpi);
    m_error.SetDpi          (dpi);
    m_macLabel.SetDpi       (dpi);
    m_mac.SetDpi            (dpi);
    m_lan.SetDpi            (dpi);
    m_forwardsLabel.SetDpi  (dpi);
    m_forwards.SetDpi       (dpi);
    m_protocol.SetDpi       (dpi);
    m_hostPortLabel.SetDpi  (dpi);
    m_hostPort.SetDpi       (dpi);
    m_guestPortLabel.SetDpi (dpi);
    m_guestPort.SetDpi      (dpi);
    m_add.SetDpi            (dpi);
    m_remove.SetDpi         (dpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::Relayout
//
//  The error banner's height changes with its text, which moves nothing
//  above it but changes the content height.
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::Relayout()
{
    if (m_hasLayout)
    {
        Layout (m_lastRect, m_lastScaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::WireCallbacks
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::WireCallbacks()
{
    m_mac.SetOnChange       ([this] (const std::wstring & text)  { OnMacChanged (text); });
    m_lan.SetOnChange       ([this] (bool on)                    { OnLanChanged (on); });
    m_add.SetOnClick        ([this] ()                           { OnAddForward(); });
    m_remove.SetOnClick     ([this] ()                           { OnRemoveForward(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::ReadCard
//
////////////////////////////////////////////////////////////////////////////////

NetworkPage::CardView NetworkPage::ReadCard() const
{
    CardView                view;
    const HardwareEntry   * card = nullptr;



    if (m_state == nullptr)
    {
        return view;
    }

    card = m_state->GetNetworkCard();

    if (card != nullptr)
    {
        view.slot    = card->slot;
        view.network = card->network;
    }

    return view;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::Rebuild
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::Rebuild()
{
    m_view = ReadCard();

    m_banner.SetText (std::vformat (s_kpszCardBanner, std::make_wformat_args (m_view.slot)));
    ShowError ("");
    SyncWidgets();
    m_firewallDrawn = false;
    SyncFirewall();
    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::Sync
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::Sync()
{
    if (!(ReadCard() == m_view))
    {
        Rebuild();
    }

    SyncFirewall();
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::CollectInboundPorts
//
////////////////////////////////////////////////////////////////////////////////

std::vector<FirewallPort> NetworkPage::CollectInboundPorts (const NetworkCardSettings & network, const std::vector<FirewallPort> & guestPorts)
{
    std::vector<FirewallPort>  ports;



    if (!network.lanAccess)
    {
        return ports;
    }

    for (const PortForward & forward : network.portForwards)
    {
        ports.push_back ({ forward.protocol == PortForwardProtocol::Udp ? HostSocketKind::Udp : HostSocketKind::Tcp, forward.hostPort });
    }

    for (const FirewallPort & port : guestPorts)
    {
        if (std::find (ports.begin(), ports.end(), port) == ports.end())
        {
            ports.push_back (port);
        }
    }

    return ports;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::SyncFirewall
//
//  Called each dialog tick. The banner is redrawn only when the ports or the
//  firewall's verdicts changed. Its text is the first blocked port's; the
//  helper admits every blocked one.
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::SyncFirewall()
{
    HRESULT                    result   = S_OK;
    std::vector<FirewallPort>  inbound;
    std::wstring               text;
    bool                       onPublic = false;



    if (m_firewall == nullptr)
    {
        return;
    }

    //  The helper changed the rules, which no notification reports.
    if (m_firewallHelper != nullptr && Win32FirewallHelper::TryFinish (m_firewallHelper, result))
    {
        m_firewall->Invalidate();

        if (FAILED (result))
        {
            ShowError ("Error: Windows Firewall could not be changed\n" + TextEncoding::WideToNarrow (ChangePrompt::DescribeError (result)));
        }
    }

    m_firewall->Update();
    inbound = CollectInboundPorts (m_view.network, m_guestPorts ? m_guestPorts() : std::vector<FirewallPort>());

    if (m_firewallDrawn && inbound == m_inbound && m_firewall->GetVersion() == m_firewallVersion)
    {
        return;
    }

    m_firewallDrawn   = true;
    m_inbound         = inbound;
    m_firewallVersion = m_firewall->GetVersion();
    m_blocked.clear();

    for (const FirewallPort & port : inbound)
    {
        FirewallVerdict  verdict = m_firewall->Check (port.protocol, port.port);

        if (verdict.IsAdmitted())
        {
            continue;
        }

        if (m_blocked.empty())
        {
            text     = FirewallText::DescribeCause (verdict, s_kFirewallTitle, port);
            onPublic = verdict.profile == FirewallProfile::Public;
        }

        m_blocked.push_back (port);
    }

    if (m_blocked.size() > 1)
    {
        text += L" Other ports Casso has open for the network are blocked as well.";
    }

    if (onPublic)
    {
        text += std::wstring (L" ") + FirewallText::kPublicChoice;
    }

    m_firewallBanner.SetText    (text);
    m_firewallBanner.SetActions (onPublic ? std::vector<std::wstring> { FirewallText::kAllowShortAction, FirewallText::kPrivateShortAction }
                                          : std::vector<std::wstring> { FirewallText::kAllowShortAction });
    m_firewallBanner.SetVisible (!m_blocked.empty());
    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::OnFirewallAction
//
//  Allow is the first action and Make private the second. Declining the UAC
//  prompt changes nothing and is not an error.
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::OnFirewallAction (size_t index)
{
    HRESULT       hr = S_OK;
    FirewallPlan  plan;



    BAIL_OUT_IF (m_firewall == nullptr || m_firewallHelper != nullptr, S_OK);

    plan = FirewallRemedy::MakePlan (m_firewall->GetSnapshot(), m_firewall->GetProgram(), s_kFirewallTitle, m_blocked, index == 1);
    BAIL_OUT_IF (plan.ports.empty() && !plan.markPrivate, S_OK);

    hr = Win32FirewallHelper::Launch (m_popupHost != nullptr ? m_popupHost->GetHwnd() : nullptr, plan, m_firewallHelper);
    BAIL_OUT_IF (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED), S_OK);
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        ShowError ("Error: Windows Firewall could not be changed\n" + TextEncoding::WideToNarrow (ChangePrompt::DescribeError (hr)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::SyncWidgets
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::SyncWidgets()
{
    m_mac.SetText    (m_view.network.mac.IsSet() ? TextEncoding::NarrowToWide (m_view.network.mac.Format()) : L"");
    m_lan.SetChecked (m_view.network.lanAccess);

    if (m_protocol.GetSelectedIndex() < 0)
    {
        m_protocol.SetSelected (0);
    }

    SyncForwardList();
    ApplyEnabledState();
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::SyncForwardList
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::SyncForwardList()
{
    std::vector<std::vector<DxuiListView::Cell>>  rows;



    for (const PortForward & forward : m_view.network.portForwards)
    {
        rows.push_back ({ { forward.protocol == PortForwardProtocol::Tcp ? L"TCP" : L"UDP" },
                          { std::to_wstring (forward.hostPort) },
                          { std::to_wstring (forward.guestPort) } });
    }

    m_forwards.SetRows (std::move (rows));
    m_forwards.UpdateAutoFitFromRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::ApplyEnabledState
//
//  Remove applies to the selected forward, so it is off while there is none.
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::ApplyEnabledState()
{
    m_remove.SetEnabled (!m_view.network.portForwards.empty());
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::ShowError
//
//  An empty error hides the banner.
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::ShowError (const std::string & error)
{
    bool  visible = !error.empty();



    m_error.SetText    (TextEncoding::NarrowToWide (error));
    m_error.SetVisible (visible);
    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::OnMacChanged
//
//  Applied once the text parses. A full-length address that does not parse
//  produces the error; a shorter one is treated as mid-edit.
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::OnMacChanged (const std::wstring & text)
{
    constexpr size_t  kAddressLength = 17;
    HRESULT           hr             = S_OK;
    std::string       error;
    std::string       narrow         = TextEncoding::WideToNarrow (text);



    CBR (m_state != nullptr);

    hr = m_state->SetNetworkCardMac (narrow, error);

    if (SUCCEEDED (hr))
    {
        ShowError ("");
        m_view = ReadCard();
        MarkDirty();
    }
    else if (narrow.size() >= kAddressLength)
    {
        ShowError (error);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::OnLanChanged
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::OnLanChanged (bool lanAccess)
{
    if (m_state == nullptr)
    {
        return;
    }

    m_state->SetNetworkLanAccess (lanAccess);
    m_view = ReadCard();
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::TryParsePort
//
//  Decimal digits only, 1-65535.
//
////////////////////////////////////////////////////////////////////////////////

bool NetworkPage::TryParsePort (
    const std::wstring  & text,
    uint16_t            & outPort)
{
    constexpr uint32_t  kMaxPort   = 65535;
    constexpr uint32_t  kDecimal   = 10;
    constexpr size_t    kMaxDigits = 5;
    uint32_t            value      = 0;



    if (text.empty() || text.size() > kMaxDigits)
    {
        return false;
    }

    for (wchar_t ch : text)
    {
        if (ch < L'0' || ch > L'9')
        {
            return false;
        }

        value = value * kDecimal + (uint32_t) (ch - L'0');
    }

    if (value == 0 || value > kMaxPort)
    {
        return false;
    }

    outPort = (uint16_t) value;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::OnAddForward
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::OnAddForward()
{
    HRESULT      hr       = S_OK;
    PortForward  forward;
    std::string  error;
    bool         parsed   = false;



    CBR (m_state != nullptr);

    forward.protocol = (m_protocol.GetSelectedIndex() == 1) ? PortForwardProtocol::Udp : PortForwardProtocol::Tcp;
    parsed           = TryParsePort (m_hostPort.GetText(), forward.hostPort) && TryParsePort (m_guestPort.GetText(), forward.guestPort);

    CBRF (parsed, ShowError ("Error: invalid port\nHost and guest ports are whole numbers from 1 to 65535."));

    hr = m_state->AddPortForward (forward, error);
    CHRF (hr, ShowError (error));

    ShowError ("");
    m_hostPort.SetText  (L"");
    m_guestPort.SetText (L"");

    m_view = ReadCard();
    SyncForwardList();
    ApplyEnabledState();
    MarkDirty();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPage::OnRemoveForward
//
////////////////////////////////////////////////////////////////////////////////

void NetworkPage::OnRemoveForward()
{
    HRESULT  hr       = S_OK;
    int      selected = m_forwards.GetSelectedRow();



    CBR (m_state != nullptr && selected >= 0);

    hr = m_state->RemovePortForward ((size_t) selected);
    CHR (hr);

    m_view = ReadCard();
    SyncForwardList();
    ApplyEnabledState();
    MarkDirty();

Error:
    return;
}
