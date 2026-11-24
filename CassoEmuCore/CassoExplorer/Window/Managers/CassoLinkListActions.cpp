#include "Pch.h"

#include "CassoExplorer/Window/Managers/CassoLinkListActions.h"
#include "CassoExplorer/Window/Managers/CassoLinkManager.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/ExplorerMetrics.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/Model/CassoLinkConnectionText.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "CassoLink/CassoLinkSession.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Core/TextEncoding.h"
#include "Net/FirewallMonitor.h"
#include "Net/FirewallRemedy.h"
#include "Net/FirewallText.h"
#include "Seams/Win32FirewallHelper.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkListActions::CassoLinkListActions
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkListActions::CassoLinkListActions (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkListActions::~CassoLinkListActions
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkListActions::~CassoLinkListActions()
{
    //  A running helper is left to exit; only the handle is closed.
    if (m_firewallHelper != nullptr)
    {
        CloseHandle (m_firewallHelper);
        m_firewallHelper = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkListActions::CreateListActions
//
//  The buttons under the list's message for a link: bootstrap the Apple or
//  connect to it, or allow it through Windows Firewall on private or on all
//  networks.
//
//  The list's button under its message: Connect for a link the user
//  disconnected, Bootstrap the Apple for a serial link whose Apple is
//  not replying, or Allow in Windows Firewall for a connection the
//  firewall blocks. On a public network the second button beside it is
//  Make this network private.
//  The buttons under the list's message for a link, and whether the
//  message is the firewall's.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkListActions::CreateListActions()
{
    m_listAction = m_context.window.CreateChild<DxuiButton> (L"Bootstrap the Apple");
    m_listAction->SetVisible (false);
    m_listAction->SetOnClick ([this] ()
    {
        std::string  link = m_listActionConnection;

        if (m_listActionFirewall)
        {
            FixFirewall (false);
            return;
        }

        if (link.empty())
        {
            return;
        }

        if (m_listActionConnects)
        {
            m_context.cassoLinkManager.Connect (link);
        }
        else
        {
            m_context.cassoLinkManager.BootstrapConnection (link);
        }
    });

    m_listActionAlt = m_context.window.CreateChild<DxuiButton> (FirewallText::kPrivateAction);
    m_listActionAlt->SetVisible (false);
    m_listActionAlt->SetOnClick ([this] () { FixFirewall (true); });

    for (size_t i = 0; i < m_driveLinks.size(); i++)
    {
        m_driveLabels[i] = m_context.window.CreateChild<DxuiLabel> (std::format (L"Drive {}:", i + 1), DxuiTextRole::Muted, DxuiTextHAlign::Right, DxuiTextVAlign::Center);
        m_driveLabels[i]->SetVisible (false);

        m_driveLinks[i] = m_context.window.CreateChild<DxuiButton> (std::wstring());
        m_driveLinks[i]->SetVariant          (DxuiButton::Variant::Link);
        m_driveLinks[i]->SetUnderlineOnHover (true);
        m_driveLinks[i]->SetVisible          (false);
        m_driveLinks[i]->SetOnClick ([this, i] ()
        {
            if (!m_driveLinkPaths[i].empty())
            {
                m_context.addressBar.GoToLocation (Location::MakeDiskImage (m_driveLinkPaths[i]));
            }
        });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkListActions::LayoutListActions
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkListActions::LayoutListActions()
{
    m_listAction->Layout    (GetListActionRect(),    m_context.scaler);
    m_listActionAlt->Layout (GetListActionAltRect(), m_context.scaler);

    for (size_t i = 0; i < m_driveLinks.size(); i++)
    {
        m_driveLabels[i]->Layout (GetDriveLabelRect ((int) i), m_context.scaler);
        m_driveLinks[i]->Layout  (GetDriveLinkRect ((int) i), m_context.scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkListActions::GetListActionRect
//
//  Centered under the list's message, which is centered in the list.
//
////////////////////////////////////////////////////////////////////////////////

RECT CassoLinkListActions::GetListActionRect() const
{
    RECT  message = m_context.listPane.GetListMessageRect();
    bool  pair    = m_listActionAlt->IsVisible();
    int   width   = m_context.scaler.ToPx (m_listActionFirewall ? kListActionWideDip : kListActionWidthDip);
    int   gap     = m_context.scaler.ToPx (ExplorerMetrics::kListActionGapDip);
    int   height  = m_context.scaler.ToPx (kListActionHeightDip);
    int   span    = pair ? width * 2 + gap : width;
    int   left    = (message.left + message.right - span) / 2;
    int   top     = m_listActionFirewall ? message.bottom + gap : (message.top + message.bottom) / 2 + m_context.scaler.ToPx (ExplorerMetrics::kListActionOffsetDip);



    return RECT { left, top, left + width, top + height };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkListActions::GetListActionAltRect
//
//  Right of the first button, the two centered as a pair.
//
////////////////////////////////////////////////////////////////////////////////

RECT CassoLinkListActions::GetListActionAltRect() const
{
    RECT  first = GetListActionRect();
    int   gap   = m_context.scaler.ToPx (ExplorerMetrics::kListActionGapDip);
    int   width = first.right - first.left;



    return RECT { first.right + gap, first.top, first.right + gap + width, first.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkListActions::IsUsingVirtualDrives
//
//  Whether the CassoLink ProDOS disk's driver uses the connection's virtual
//  drives rather than Casso's program, and the drives' image paths.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkListActions::IsUsingVirtualDrives (const std::string & name, std::vector<std::wstring> & outDrives) const
{
    CassoLinkSession  * session = m_context.app.links->GetSession (name);
    LinkState           state   = (session != nullptr) ? session->GetState() : LinkState::Closed;
    bool                inUse   = session != nullptr && session->IsServingVirtualDrives() && state != LinkState::Ready && state != LinkState::Busy;



    outDrives.clear();

    for (const CassoLinkSettings & settings : m_context.app.links->GetLinks())
    {
        if (inUse && settings.name == name)
        {
            outDrives = { settings.virtualDrives.drive1, settings.virtualDrives.drive2 };
        }
    }

    return !outDrives.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkListActions::GetDriveLabelRect
//
//  "Drive 1:" or "Drive 2:", ending just left of the middle, beside its
//  image name.
//
////////////////////////////////////////////////////////////////////////////////

RECT CassoLinkListActions::GetDriveLabelRect (int drive) const
{
    RECT  link  = GetDriveLinkRect (drive);
    int   right = link.left - m_context.scaler.ToPx (kDriveLabelGapDip);



    return RECT { right - m_context.scaler.ToPx (kDriveLabelWidthDip), link.top, right, link.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkListActions::GetDriveLinkRect
//
//  Drive 1's and drive 2's image links, one under the other, below the
//  message, which ends at the list's middle; each starts just right of the
//  middle, after its label.
//
////////////////////////////////////////////////////////////////////////////////

RECT CassoLinkListActions::GetDriveLinkRect (int drive) const
{
    RECT  message = m_context.listPane.GetListMessageRect();
    int   width   = m_context.scaler.ToPx (kDriveLinkWidthDip);
    int   height  = m_context.scaler.ToPx (kDriveLinkHeightDip);
    int   gap     = m_context.scaler.ToPx (ExplorerMetrics::kListActionGapDip);
    int   left    = (message.left + message.right) / 2 + m_context.scaler.ToPx (kDriveLabelGapDip) / 2;
    int   top     = message.bottom + gap + drive * height;



    return RECT { left, top, left + width, top + height };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkListActions::FillListAction
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkListActions::FillListAction (bool hasMessage)
{
    std::wstring               name;
    int                        slot         = 0;
    int                        drive        = 0;
    std::string                disconnected;
    LinkTreeEntry              entry;
    std::vector<std::wstring>  drives;
    bool                       parsed       = m_context.app.links != nullptr && TreeModel::TryParseLinkId (m_context.browser.GetLocation().path, name, slot, drive) && drive == 0;
    bool                       onLink       = hasMessage && parsed;
    bool                       server       = parsed && IsStoppedServer (TextEncoding::WideToNarrow (name));



    if (server || (onLink && m_context.app.links->IsHeld (TextEncoding::WideToNarrow (name))))
    {
        disconnected = TextEncoding::WideToNarrow (name);
    }

    //  The firewall first: until the port is allowed, no connection the other
    //  actions start arrives at this PC.
    m_listActionFirewall = onLink && disconnected.empty() && m_context.cassoLinkManager.TryGetConnectionEntry (name, entry) && !entry.firewall.empty();
    m_listActionConnects          = !m_listActionFirewall && !disconnected.empty();
    m_listActionConnection              = m_listActionFirewall ? std::string() : m_listActionConnects ? disconnected : hasMessage ? GetBootstrapConnection() : std::string();

    //  The driver using the virtual drives: their images as links, and no
    //  Bootstrap the Apple, as the Apple is running.
    if (onLink && !m_listActionFirewall && disconnected.empty() && IsUsingVirtualDrives (TextEncoding::WideToNarrow (name), drives))
    {
        m_listActionConnection.clear();
    }
    else
    {
        drives = {};
    }

    m_driveLinksShown = !drives.empty();

    m_context.listPane.AlignMessage (m_listActionFirewall || m_driveLinksShown);
    m_listAction->SetLabel      (m_listActionFirewall ? FirewallText::kAllowAction : server ? L"Start server" : m_listActionConnects ? L"Connect" : L"Bootstrap the Apple");
    m_listActionAlt->SetVisible (m_listActionFirewall && entry.firewallPublic);
    m_listAction->Layout        (GetListActionRect(), m_context.scaler);
    m_listActionAlt->Layout     (GetListActionAltRect(), m_context.scaler);
    m_listAction->SetVisible    (m_listActionFirewall || !m_listActionConnection.empty());

    for (size_t i = 0; i < m_driveLinks.size(); i++)
    {
        m_driveLinkPaths[i] = drives.empty() ? std::wstring() : drives[i];

        m_driveLinks[i]->SetLabel    (CassoLinkConnectionText::DescribeDriveImage (m_driveLinkPaths[i]));
        m_driveLinks[i]->SetEnabled  (!m_driveLinkPaths[i].empty());
        m_driveLinks[i]->Layout      (GetDriveLinkRect ((int) i), m_context.scaler);
        m_driveLinks[i]->SetVisible  (!drives.empty());
        m_driveLabels[i]->Layout     (GetDriveLabelRect ((int) i), m_context.scaler);
        m_driveLabels[i]->SetVisible (!drives.empty());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkListActions::PollFirewall
//
//  Every window polls the one monitor, so each compares the monitor's version
//  with the one it drew rather than relying on Update's result, which only
//  the first window to poll after a change would see.
//
//  The firewall read again on the drive poll, the views redrawn when it
//  changed, and the elevated helper's result once it exits.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkListActions::PollFirewall()
{
    HRESULT       result  = S_OK;
    std::wstring  message;



    if (m_context.app.firewall == nullptr)
    {
        return;
    }

    //  The helper changed the rules, which no notification reports.
    if (m_firewallHelper != nullptr && Win32FirewallHelper::TryFinish (m_firewallHelper, result))
    {
        m_context.app.firewall->Invalidate();

        if (FAILED (result))
        {
            message = std::format (L"Windows Firewall could not be changed: {}", ChangePrompt::DescribeError (result));
            m_context.window.ShowMessage (message, MB_ICONWARNING);
        }
    }

    m_context.app.firewall->Update();

    if (m_context.app.firewall->GetVersion() != m_firewallVersion)
    {
        m_firewallVersion = m_context.app.firewall->GetVersion();

        if (m_context.app.links != nullptr)
        {
            m_context.app.links->ApplyFirewall();
        }

        m_context.cassoLinkManager.RefreshCassoLinkViews();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkListActions::FixFirewall
//
//  Every connection's port is in the plan, not only the one the list is on,
//  so one UAC prompt allows them all. Declining the prompt changes nothing
//  and is not an error.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkListActions::FixFirewall (bool markPrivate)
{
    HRESULT                    hr = S_OK;
    std::vector<FirewallPort>  ports;
    FirewallPort               port;
    FirewallPlan               plan;



    BAIL_OUT_IF (m_context.app.firewall == nullptr || m_context.app.links == nullptr || m_firewallHelper != nullptr, S_OK);

    for (const CassoLinkSettings & settings : m_context.app.links->GetLinks())
    {
        if (CassoLinkConnections::GetInboundPort (settings, port) && std::find (ports.begin(), ports.end(), port) == ports.end())
        {
            ports.push_back (port);
        }
    }

    plan = FirewallRemedy::MakePlan (m_context.app.firewall->GetSnapshot(), m_context.app.firewall->GetProgram(), CassoLinkConnections::kFirewallTitle, ports, markPrivate);
    BAIL_OUT_IF (plan.ports.empty() && !plan.markPrivate, S_OK);

    hr = Win32FirewallHelper::Launch (m_context.window.GetHwnd(), plan, m_firewallHelper);
    BAIL_OUT_IF (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED), S_OK);
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        m_context.window.ShowMessage (std::format (L"Windows Firewall could not be changed: {}", ChangePrompt::DescribeError (hr)), MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkListActions::GetBootstrapConnection
//
//  The serial link the list has, when its Apple is not replying and no
//  bootstrap is under way; empty otherwise.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkListActions::GetBootstrapConnection() const
{
    std::wstring        name;
    int                 slot    = 0;
    int                 drive   = 0;
    std::string         link;
    CassoLinkSession  * session = nullptr;
    LinkState           state   = LinkState::Closed;
    bool                waiting = false;



    if (m_context.app.links == nullptr || !TreeModel::TryParseLinkId (m_context.browser.GetLocation().path, name, slot, drive) || drive != 0)
    {
        return std::string();
    }

    link    = TextEncoding::WideToNarrow (name);
    session = m_context.app.links->GetSession (link);
    state   = (session != nullptr) ? session->GetState() : LinkState::Closed;
    waiting = m_context.cassoLinkManager.IsSerialConnection (link) && !m_context.browser.GetTreeModel().IsLinkReplying (name)
              && state != LinkState::Bootstrapping && state != LinkState::LoadingProgram;

    return waiting ? link : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkListActions::IsStoppedServer
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkListActions::IsStoppedServer (const std::string & name) const
{
    const std::vector<CassoLinkSettings> &  links = m_context.app.links->GetLinks();



    return !m_context.app.links->IsStarted (name) && std::any_of (links.begin(), links.end(), [&name] (const CassoLinkSettings & settings)
    {
        return settings.name == name && settings.transport == LinkTransport::AdtProServer;
    });
}





