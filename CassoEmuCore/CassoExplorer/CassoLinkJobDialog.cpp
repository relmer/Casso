#include "Pch.h"

#include "CassoExplorer/CassoLinkJobDialog.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobView::IsDiskJob
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkJobView::IsDiskJob (LinkOperationKind kind)
{
    return kind == LinkOperationKind::ReadDisk || kind == LinkOperationKind::CaptureDisk || kind == LinkOperationKind::WriteDisk;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobView::DescribeDrive
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkJobView::DescribeDrive (int drive)
{
    return (drive == 1 || drive == 2) ? std::format (L"Slot 6, Drive {}", drive) : std::format (L"Drive {}", drive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobView::DescribeConnection
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkJobView::DescribeConnection (const std::wstring & model, LinkTransport transport, const std::wstring & port)
{
    std::wstring  apple = model.empty() ? std::wstring (L"The Apple") : model;



    switch (transport)
    {
        case LinkTransport::Serial:       return port.empty() ? apple + L" over serial" : std::format (L"{} over serial on {}", apple, port);
        case LinkTransport::Tcp:          return apple + L" over TCP";
        case LinkTransport::WifiModem:    return apple + L" over a Wi-Fi modem";
        case LinkTransport::Emulated:     return apple + L" in Casso";
        case LinkTransport::Uthernet:     return apple + L" over Ethernet on an Uthernet II";
        case LinkTransport::Udp:          return apple + L" over Ethernet";
        case LinkTransport::AdtProServer: return apple + L" over ADTPro";
        case LinkTransport::Cassette:     return apple + L" over cassette audio";
    }

    return apple;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobView::DescribeTab
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkJobView::DescribeTab (const std::wstring & model, const std::string & link, LinkTransport transport, const std::wstring & port)
{
    std::wstring  apple = model.empty() ? TextEncoding::NarrowToWide (link) : model;
    std::wstring  how;



    switch (transport)
    {
        case LinkTransport::Serial:       how = port.empty() ? std::wstring (L"serial") : port; break;
        case LinkTransport::Tcp:          how = L"TCP";                                         break;
        case LinkTransport::WifiModem:    how = L"Wi-Fi modem";                                 break;
        case LinkTransport::Emulated:     how = L"Casso";                                       break;
        case LinkTransport::Uthernet:
        case LinkTransport::Udp:          how = L"Ethernet";                                    break;
        case LinkTransport::AdtProServer: how = L"ADTPro";                                      break;
        case LinkTransport::Cassette:     how = L"cassette";                                    break;
    }

    return how.empty() ? apple : std::format (L"{} ({})", apple, how);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobView::Build
//
//  The drive is the job's Full Send status, as the session's thread last
//  wrote it.
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkJobView CassoLinkJobView::Build (const LinkOperation & operation, const std::string & link, const std::wstring & model, LinkTransport transport, const std::wstring & port)
{
    CassoLinkJobView    view;
    LinkOperationState  state   = operation.state;
    std::wstring        drive   = DescribeDrive (operation.drive);
    bool                isRead  = operation.kind == LinkOperationKind::ReadDisk;
    bool                isWrite = operation.kind == LinkOperationKind::WriteDisk;



    view.link       = link;
    view.tabLabel   = DescribeTab (model, link, transport, port);
    view.connection = DescribeConnection (model, transport, port);
    view.imagePath  = TextEncoding::NarrowToWide (operation.imagePath);
    view.state      = state == LinkOperationState::Queued  ? State::Queued
                    : state == LinkOperationState::Running ? State::Running
                    : state == LinkOperationState::Done    ? State::Done
                    : state == LinkOperationState::Failed  ? State::Failed
                                                           : State::Canceled;

    switch (view.state)
    {
        case State::Queued:   view.heading = std::format (L"Waiting to {} {}", isWrite ? L"write to" : isRead ? L"read" : L"capture", drive);       break;
        case State::Running:  view.heading = std::format (L"{} {}", isWrite ? L"Writing to" : isRead ? L"Reading" : L"Capturing", drive);        break;
        case State::Done:     view.heading = std::format (L"{} {}", isWrite ? L"Wrote to" : isRead ? L"Read" : L"Captured", drive);              break;
        case State::Failed:   view.heading = std::format (L"Couldn't {} {}", isWrite ? L"write to" : isRead ? L"read" : L"capture", drive);     break;
        case State::Canceled: view.heading = std::format (L"Canceled {} {}", isWrite ? L"writing to" : isRead ? L"reading" : L"capturing", drive); break;
    }

    if (operation.status != nullptr)
    {
        view.drive.snapshot = operation.status->GetSnapshot();
    }

    view.drive.isJob   = true;
    view.drive.machine = model;

    if (view.state == State::Failed)
    {
        view.message = TextEncoding::NarrowToWide (operation.message);
    }

    if (view.IsRunning())
    {
        return view;
    }

    for (const auto & [label, value] : operation.summary)
    {
        view.summary.push_back ({ TextEncoding::NarrowToWide (label), TextEncoding::NarrowToWide (value) });
    }

    if (view.summary.empty() && isRead && view.state == State::Done)
    {
        view.summary.push_back ({ L"Sectors that couldn't be read", operation.unreadable.empty() ? std::wstring (L"None") : std::format (L"{}, written as zeros", operation.unreadable.size()) });
    }

    return view;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobPanel::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobPanel::Init (DxuiTooltip * tooltip)
{
    m_tabs.SetStyle (DxuiTabStripStyle::Standard);
    m_tabs.SetOnChange ([this] (int index)
    {
        if (index >= 0 && (size_t) index < m_views.size())
        {
            m_selected = m_views[(size_t) index].link;

            if (m_onSelect)
            {
                m_onSelect (m_selected);
            }
        }
    });
    m_tabs.SetOnClose ([this] (int index)
    {
        if (index >= 0 && (size_t) index < m_views.size() && m_onClose)
        {
            m_onClose (m_views[(size_t) index].link);
        }
    });

    m_drive.SetTooltip (tooltip);

    Adopt (m_tabs);
    Adopt (m_drive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobPanel::SetViews
//
//  A tab's label has a dot while its job runs, and a star once it ends
//  until its tab is selected.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobPanel::SetViews (std::vector<CassoLinkJobView> views, const std::string & selected)
{
    std::vector<DxuiTabStrip::Tab>  tabs;
    int                             index = 0;
    const CassoLinkJobView        * shown = nullptr;



    m_views    = std::move (views);
    m_selected = selected;

    for (size_t i = 0; i < m_views.size(); i++)
    {
        DxuiTabStrip::Tab  tab;

        tab.label = m_views[i].tabLabel + (m_views[i].IsRunning() ? (std::wstring (L" ") + s_kpszBullet) : m_views[i].isUnseen ? L" *" : L"");
        index     = (m_views[i].link == m_selected) ? (int) i : index;

        tabs.push_back (tab);
    }

    m_tabs.SetTabs (std::move (tabs));
    m_tabs.SetSelected (index);

    shown = GetSelected();

    if (shown != nullptr)
    {
        m_drive.SetView (shown->drive);
    }

    LayTabs();
    LayDrive();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobPanel::GetSelected
//
////////////////////////////////////////////////////////////////////////////////

const CassoLinkJobView * CassoLinkJobPanel::GetSelected() const
{
    for (const CassoLinkJobView & view : m_views)
    {
        if (view.link == m_selected)
        {
            return &view;
        }
    }

    return m_views.empty() ? nullptr : &m_views.front();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobPanel::OnMouse
//
//  A click on the image's link selects it in its folder.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkJobPanel::OnMouse (const DxuiMouseEvent & ev)
{
    POINT                      point = { (LONG) ev.positionDip.x, (LONG) ev.positionDip.y };
    const CassoLinkJobView   * view  = GetSelected();



    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Leave)
    {
        m_isOverLink = ev.kind == DxuiMouseEventKind::Move && PtInRect (&m_linkPx, point);
    }

    if (ev.kind == DxuiMouseEventKind::Up && ev.button == DxuiMouseButton::Left && PtInRect (&m_linkPx, point) && view != nullptr && m_onOpenFile)
    {
        m_onOpenFile (view->imagePath);
        return true;
    }

    return DxuiPanel::OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobPanel::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR CassoLinkJobPanel::GetCursorForPoint (POINT clientPx) const
{
    return PtInRect (&m_linkPx, clientPx) ? IDC_HAND : DxuiPanel::GetCursorForPoint (clientPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobPanel::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobPanel::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    RECT  tabs = boundsPx;



    SetBounds (boundsPx);

    m_scaler    = scaler;
    tabs.bottom = tabs.top + scaler.ToPx (kTabsDip);
    m_tabsPx    = tabs;

    LayTabs();
    LayDrive();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobPanel::LayTabs
//
//  The strip's tabs side by side from its left, each the same width.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobPanel::LayTabs()
{
    std::vector<DxuiTabStrip::Tab>  tabs  = m_tabs.GetTabs();
    int                             x     = m_tabsPx.left;
    int                             width = m_scaler.ToPx (kTabWidthDip);
    int                             index = m_tabs.GetSelected();



    for (DxuiTabStrip::Tab & tab : tabs)
    {
        tab.rect  = RECT { x, m_tabsPx.top, x + width, m_tabsPx.bottom };
        x        += width;
    }

    m_tabs.SetTabs     (std::move (tabs));
    m_tabs.SetSelected (index);
    m_tabs.Layout      (m_tabsPx, m_scaler);
    m_tabs.SetDpi      (m_scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobPanel::GetHeader
//
//  The height above the drive: the tabs, the status line, the link, the
//  connection, and once the job ends, what it found.
//
////////////////////////////////////////////////////////////////////////////////

float CassoLinkJobPanel::GetHeader (const CassoLinkJobView * view) const
{
    float  line = m_scaler.ToPxf (kLineDip);
    float  rows = 3.0f;



    if (view != nullptr)
    {
        rows += (float) view->summary.size() + (view->message.empty() ? 0.0f : 1.0f);
    }

    return m_scaler.ToPxf (kTabsDip) + m_scaler.ToPxf (kPadDip) + line * rows + m_scaler.ToPxf (kSectionGapDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobPanel::LayDrive
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobPanel::LayDrive()
{
    RECT  bounds = GetBounds();
    LONG  pad    = m_scaler.ToPx (kPadDip);
    RECT  drive  = { bounds.left + pad, bounds.top + (LONG) GetHeader (GetSelected()), bounds.right - pad, bounds.bottom };



    if (drive.bottom > drive.top)
    {
        m_drive.Layout (drive, m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobPanel::DrawLine
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobPanel::DrawLine (IDxuiTextRenderer & text, const std::wstring & line, float left, float top, float width, uint32_t color, float fontDip) const
{
    HRESULT  hr = S_OK;



    hr = text.DrawString (line.c_str(), left, top, width, m_scaler.ToPxf (kLineDip), color, m_scaler.ToPxf (fontDip), FullSendPanel::s_kpszFont,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobPanel::Paint
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT                       bounds = GetBounds();
    float                      pad    = m_scaler.ToPxf (kPadDip);
    float                      left   = (float) bounds.left + pad;
    float                      width  = (float) (bounds.right - bounds.left) - pad * 2.0f;
    float                      top    = (float) bounds.top + m_scaler.ToPxf (kTabsDip) + pad;
    float                      line   = m_scaler.ToPxf (kLineDip);
    float                      labelW = std::min (m_scaler.ToPxf (kLabelWidthDip), width / 2.0f);
    const CassoLinkJobView   * view   = GetSelected();



    DxuiPanel::Paint (painter, text, theme);

    if (view == nullptr)
    {
        return;
    }

    DrawLine (text, view->heading, left, top, width, theme.Foreground(), kStatusFontDip);
    top += line;

    PaintLink (painter, text, theme, view->imagePath, left, top, width);
    top += line;

    DrawLine (text, view->connection, left, top, width, theme.ForegroundMuted(), kFontDip);
    top += line;

    if (!view->message.empty())
    {
        DrawLine (text, view->message, left, top, width, theme.Foreground(), kFontDip);
        top += line;
    }

    for (const auto & [label, value] : view->summary)
    {
        DrawLine (text, label, left, top, labelW, theme.ForegroundMuted(), kFontDip);
        DrawLine (text, value, left + labelW, top, width - labelW, theme.Foreground(), kFontDip);
        top += line;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobPanel::PaintLink
//
//  The image's path in the link color, underlined under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobPanel::PaintLink (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const std::wstring & path, float left, float top, float width) const
{
    HRESULT  hr     = S_OK;
    float    textW  = 0.0f;
    float    textH  = 0.0f;
    float    fontPx = m_scaler.ToPxf (kFontDip);
    float    lineH  = m_scaler.ToPxf (kLineDip);



    m_linkPx = {};

    hr = text.MeasureString (path.c_str(), fontPx, FullSendPanel::s_kpszFont, textW, textH);
    CHR (hr);

    textW    = std::min (textW, width);
    m_linkPx = RECT { (LONG) left, (LONG) top, (LONG) std::ceil (left + textW), (LONG) std::ceil (top + lineH) };

    hr = text.DrawString (path.c_str(), left, top, width, lineH, theme.Accent(), fontPx, FullSendPanel::s_kpszFont,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    CHR (hr);

    if (m_isOverLink)
    {
        painter.FillRect (left, top + (lineH + textH) / 2.0f, textW, std::max (1.0f, std::floor (m_scaler.ToPxf (1.0f))), theme.Accent());
    }

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobDialog::Configure
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobDialog::Configure (const IDxuiTheme * theme, Actions actions)
{
    m_theme   = theme;
    m_actions = std::move (actions);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobDialog::Select
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobDialog::Select (const std::string & link)
{
    m_selected = link;

    if (m_actions.select)
    {
        m_actions.select (link);
    }

    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobDialog::RunOnSelected
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobDialog::RunOnSelected (const LinkFn & action)
{
    const CassoLinkJobView  * view = m_panel->GetSelected();



    if (view != nullptr && action)
    {
        action (view->link);
    }

    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobDialog::OnCreate()
{
    m_tooltip.SetPopupHost (GetPopupHost());
    m_tooltip.SetFollowPointer (true);

    if (m_theme != nullptr)
    {
        m_tooltip.SetTheme (*m_theme);
    }

    m_panel = CreateDialogContent<CassoLinkJobPanel>();
    m_panel->Init (&m_tooltip);
    m_panel->SetOnSelect ([this] (const std::string & link) { Select (link); });
    m_panel->SetOnClose ([this] (const std::string & link)
    {
        if (m_actions.closeTab)
        {
            m_actions.closeTab (link);
        }

        Refresh();
    });
    m_panel->SetOnOpenFile ([this] (const std::wstring & imagePath)
    {
        if (m_actions.showInFolder)
        {
            m_actions.showInFolder (imagePath);
        }
    });

    m_cancelButton = AddDialogButton (L"Cancel job", kCancelId, DxuiButtonRow::Anchor::Left);
    m_rawButton    = AddDialogButton (L"Capture raw", kRawId);
    m_againButton  = AddDialogButton (L"Read again", kAgainId);
    m_openButton   = AddDialogButton (L"Open", kOpenId);

    AddDialogButton (L"Close", IDCANCEL);

    m_cancelButton->SetOnClick ([this]() { RunOnSelected (m_actions.cancel); });
    m_rawButton->SetOnClick ([this]() { RunOnSelected (m_actions.captureRaw); });
    m_againButton->SetOnClick ([this]() { RunOnSelected (m_actions.readAgain); });
    m_openButton->SetOnClick ([this]()
    {
        const CassoLinkJobView  * view = m_panel->GetSelected();

        if (view != nullptr && m_actions.open)
        {
            m_actions.open (view->imagePath);
        }
    });

    SetDialogTickIntervalMs (kTipTickMs);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobDialog::OnDialogTick
//
//  The window is drawn each frame while the map grows or shrinks, and the
//  jobs are read again every kTickMs.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobDialog::OnDialogTick()
{
    int64_t  now    = (int64_t) GetTickCount64();
    UINT     tickMs = m_panel->IsMagnifying (now) ? kFrameMs : kTipTickMs;



    m_tooltip.Tick (now);

    if (tickMs != m_tickMs)
    {
        m_tickMs = tickMs;
        SetDialogTickIntervalMs (tickMs);
    }

    if (now - m_refreshedMs >= (int64_t) kTickMs)
    {
        m_refreshedMs = now;
        Refresh();
    }
    else if (tickMs == kFrameMs)
    {
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobDialog::Refresh
//
//  The window closes once no tab is left. Capture raw and Read again are
//  visible only while they are offered.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobDialog::Refresh()
{
    std::vector<CassoLinkJobView>  views  = m_actions.views ? m_actions.views() : std::vector<CassoLinkJobView>();
    const CassoLinkJobView       * shown  = nullptr;
    bool                           isDone = false;
    bool                           offer  = false;



    if (views.empty())
    {
        if (IsWindowVisible (GetHwnd()))
        {
            PostMessageW (GetHwnd(), WM_CLOSE, 0, 0);
        }

        return;
    }

    m_panel->SetViews (std::move (views), m_selected);

    shown  = m_panel->GetSelected();
    isDone = shown != nullptr && shown->state == CassoLinkJobView::State::Done && !shown->imagePath.empty();
    offer  = shown != nullptr && shown->hasOffer;

    m_cancelButton->SetEnabled (shown != nullptr && shown->IsRunning());
    m_rawButton->SetVisible (offer);
    m_againButton->SetVisible (offer);
    m_openButton->SetEnabled (isDone);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobDialog::Open
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<CassoLinkJobDialog> CassoLinkJobDialog::Open (HWND owner, const IDxuiTheme * theme, bool activate, Actions actions, const std::string & link)
{
    HRESULT                              hr     = S_OK;
    std::unique_ptr<CassoLinkJobDialog>  dialog = std::make_unique<CassoLinkJobDialog>();
    DxuiWindow::CreateParams             params;



    dialog->Configure (theme, std::move (actions));
    dialog->m_selected = link;

    params.title                    = L"CassoLink jobs";
    params.hInstance                = (owner != nullptr) ? (HINSTANCE) GetWindowLongPtrW (owner, GWLP_HINSTANCE) : GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { kWidthDip, kHeightDip };
    params.minSizeDip               = { kWidthDip, kHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;
    params.createNoActivate         = !activate;

    hr = dialog->Create (params);
    CHRA (hr);

    dialog->SetTheme (theme);
    dialog->ShowModelessDialog (IDCANCEL, activate);

Error:
    return SUCCEEDED (hr) ? std::move (dialog) : nullptr;
}
