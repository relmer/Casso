#include "Pch.h"

#include "CassoExplorer/CassoLinkJobDialog.h"
#include "CassoExplorer/FullSendDialog.h"
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
//  CassoLinkJobView::Build
//
//  A running job's tracks before its share done are read. Once a read ends,
//  its unreadable sectors' tracks are marked, and a capture's tracks with
//  weak bits are listed for the map.
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkJobView CassoLinkJobView::Build (const LinkOperation & operation, const std::string & link, const std::wstring & model, double elapsed)
{
    CassoLinkJobView    view;
    LinkOperationState  state   = operation.state;
    std::wstring        drive   = DescribeDrive (operation.drive);
    std::wstring        file    = std::filesystem::path (TextEncoding::NarrowToWide (operation.imagePath)).filename().wstring();
    std::wstring        name    = TextEncoding::NarrowToWide (link);
    int                 done    = 0;
    bool                isRead  = operation.kind == LinkOperationKind::ReadDisk;
    bool                isWrite = operation.kind == LinkOperationKind::WriteDisk;



    view.link      = link;
    view.tabLabel  = model.empty() ? name : model + L", " + name;
    view.imagePath = TextEncoding::NarrowToWide (operation.imagePath);
    view.percent   = std::clamp (operation.progress.load(), 0, 100);
    view.elapsed   = elapsed;
    view.hasMap    = IsDiskJob (operation.kind);
    view.step      = TextEncoding::NarrowToWide (operation.GetDetail());
    view.state     = state == LinkOperationState::Queued  ? State::Queued
                   : state == LinkOperationState::Running ? State::Running
                   : state == LinkOperationState::Done    ? State::Done
                   : state == LinkOperationState::Failed  ? State::Failed
                                                          : State::Canceled;

    switch (view.state)
    {
        case State::Queued:   view.heading = std::format (L"Waiting to {} {}",  isWrite ? L"write to" : isRead ? L"read" : L"capture", drive);                       break;
        case State::Running:  view.heading = isWrite ? std::format (L"Writing {} to {}", file, drive) : std::format (L"{} {} to {}", isRead ? L"Reading" : L"Capturing", drive, file); break;
        case State::Done:     view.heading = isWrite ? std::format (L"Wrote {} to {}", file, drive)   : std::format (L"{} {} to {}", isRead ? L"Read" : L"Captured", drive, file);     break;
        case State::Failed:   view.heading = std::format (L"Couldn't {} {}", isWrite ? L"write to" : isRead ? L"read" : L"capture", drive);                           break;
        case State::Canceled: view.heading = std::format (L"Canceled {} {}", isWrite ? L"writing to" : isRead ? L"reading" : L"capturing", drive);                    break;
    }

    done = (view.state == State::Done) ? kTracks : view.percent * kTracks / 100;

    for (int track = 0; track < done && track < kTracks; track++)
    {
        view.tracks[(size_t) track] = FullSendTrackMark::Read;
    }

    for (const SectorMark & mark : operation.marks)
    {
        if (mark.track >= 0 && mark.track < kTracks)
        {
            view.tracks[(size_t) mark.track] = mark.mark == 'X' ? FullSendTrackMark::Unreadable : FullSendTrackMark::Unreliable;
        }
    }

    for (const auto & [track, sector] : operation.unreadable)
    {
        if (track >= 0 && track < kTracks)
        {
            view.tracks[(size_t) track] = FullSendTrackMark::Unreadable;
        }
    }

    if (view.IsRunning() && view.percent > 0)
    {
        view.left = elapsed * (100 - view.percent) / view.percent;
    }

    if (view.state == State::Failed)
    {
        view.message = TextEncoding::NarrowToWide (operation.message);
    }

    if (view.IsRunning())
    {
        return view;
    }

    for (int position : operation.weakPositions)
    {
        view.weakTracks.push_back (position / 4);
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

void CassoLinkJobPanel::Init()
{
    m_tabs.SetStyle (DxuiTabStripStyle::Explorer);
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

    Adopt (m_tabs);
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
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkJobPanel::OnMouse (const DxuiMouseEvent & ev)
{
    return DxuiPanel::OnMouse (ev);
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

    m_tabs.Layout (tabs, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobPanel::DrawLine
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobPanel::DrawLine (IDxuiTextRenderer & text, const std::wstring & line, float left, float top, float width, uint32_t color, float fontDip, DxuiFontWeight weight) const
{
    HRESULT  hr = S_OK;



    hr = text.DrawString (line.c_str(), left, top, width, m_scaler.ToPxf (kLineDip), color, m_scaler.ToPxf (fontDip), FullSendPanel::s_kpszFont,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, weight, false);
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
    const CassoLinkJobView   * view   = GetSelected();
    std::wstring               times;



    DxuiPanel::Paint (painter, text, theme);

    if (view == nullptr)
    {
        return;
    }

    DrawLine (text, view->heading, left, top, width, theme.Foreground(), kHeadingFontDip, DxuiFontWeight::SemiBold);
    top += line + m_scaler.ToPxf (4.0f);

    if (view->IsRunning() && !view->step.empty())
    {
        DrawLine (text, view->step, left, top, width, theme.ForegroundMuted(), kFontDip, DxuiFontWeight::Normal);
    }

    top += line + m_scaler.ToPxf (kSectionGapDip);

    if (view->hasMap)
    {
        top = PaintMap (painter, text, theme, *view, left, top, width);
    }

    painter.FillRect (left, top, width, m_scaler.ToPxf (kBarDip), theme.Divider());
    painter.FillRect (left, top, width * (float) (view->state == CassoLinkJobView::State::Done ? 100 : view->percent) / 100.0f, m_scaler.ToPxf (kBarDip), FullSendPanel::kBarArgb);
    top += m_scaler.ToPxf (kBarDip) + m_scaler.ToPxf (6.0f);

    times = FullSendPanel::FormatSeconds (view->elapsed) + (view->IsRunning() ? L" elapsed" : L" in all");

    if (view->left >= 0.0)
    {
        times += L", about " + FullSendPanel::FormatSeconds (view->left) + L" left";
    }

    DrawLine (text, std::format (L"{}%   {}", view->state == CassoLinkJobView::State::Done ? 100 : view->percent, times), left, top, width, theme.ForegroundMuted(), kFontDip, DxuiFontWeight::Normal);
    top += line + m_scaler.ToPxf (kSectionGapDip);

    if (!view->message.empty())
    {
        DrawLine (text, view->message, left, top, width, theme.Foreground(), kFontDip, DxuiFontWeight::Normal);
        top += line;
    }

    PaintSummary (text, theme, *view, left, top, width);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobPanel::PaintMap
//
//  A cell a track, in Full Send's colors; a track with weak bits has a
//  stripe across its foot. Returns the bottom.
//
////////////////////////////////////////////////////////////////////////////////

float CassoLinkJobPanel::PaintMap (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const CassoLinkJobView & view, float left, float top, float width) const
{
    float    gap   = m_scaler.ToPxf (kMapGapDip);
    float    cellW = (width - gap * (float) (CassoLinkJobView::kTracks - 1)) / (float) CassoLinkJobView::kTracks;
    float    cellH = m_scaler.ToPxf (kMapCellDip);
    float    foot  = std::max (2.0f, std::floor (cellH / 4.0f));
    HRESULT  hr    = S_OK;



    for (int track = 0; track < CassoLinkJobView::kTracks; track++)
    {
        float  x = left + (cellW + gap) * (float) track;

        painter.FillRect (x, top, cellW, cellH, FullSendPanel::GetColor (view.tracks[(size_t) track], theme));

        if (std::find (view.weakTracks.begin(), view.weakTracks.end(), track) != view.weakTracks.end())
        {
            painter.FillRect (x, top + cellH - foot, cellW, foot, kWeakArgb);
        }
    }

    top += cellH + m_scaler.ToPxf (4.0f);

    DrawLine (text, L"Track 0", left, top, width, theme.ForegroundMuted(), kFontDip, DxuiFontWeight::Normal);
    hr = text.DrawString (L"34", left, top, width, m_scaler.ToPxf (kLineDip), theme.ForegroundMuted(), m_scaler.ToPxf (kFontDip), FullSendPanel::s_kpszFont,
                          DxuiTextHAlign::Right, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    return top + m_scaler.ToPxf (kLineDip) + m_scaler.ToPxf (kSectionGapDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobPanel::PaintSummary
//
//  A row a finding: its label, then its value. Returns the bottom.
//
////////////////////////////////////////////////////////////////////////////////

float CassoLinkJobPanel::PaintSummary (IDxuiTextRenderer & text, const IDxuiTheme & theme, const CassoLinkJobView & view, float left, float top, float width) const
{
    float  labelW = std::min (m_scaler.ToPxf (kLabelWidthDip), width / 2.0f);
    float  line   = m_scaler.ToPxf (kLineDip);



    for (const auto & [label, value] : view.summary)
    {
        DrawLine (text, label, left, top, labelW, theme.ForegroundMuted(), kFontDip, DxuiFontWeight::Normal);
        DrawLine (text, value, left + labelW, top, width - labelW, theme.Foreground(), kFontDip, DxuiFontWeight::Normal);
        top += line;
    }

    return top;
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
//  CassoLinkJobDialog::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobDialog::OnCreate()
{
    m_panel = CreateDialogContent<CassoLinkJobPanel>();
    m_panel->Init();
    m_panel->SetOnSelect ([this] (const std::string & link) { Select (link); });
    m_panel->SetOnClose ([this] (const std::string & link)
    {
        if (m_actions.closeTab)
        {
            m_actions.closeTab (link);
        }

        Refresh();
    });

    m_cancelButton = AddDialogButton (L"Cancel job", kCancelId);
    m_folderButton = AddDialogButton (L"Show in folder", kFolderId);
    m_openButton   = AddDialogButton (L"Open", kOpenId);

    AddDialogButton (L"Close", IDCANCEL);

    m_cancelButton->SetOnClick ([this]()
    {
        const CassoLinkJobView  * view = m_panel->GetSelected();

        if (view != nullptr && m_actions.cancel)
        {
            m_actions.cancel (view->link);
        }
    });
    m_folderButton->SetOnClick ([this]()
    {
        const CassoLinkJobView  * view = m_panel->GetSelected();

        if (view != nullptr && m_actions.showInFolder)
        {
            m_actions.showInFolder (view->imagePath);
        }
    });
    m_openButton->SetOnClick ([this]()
    {
        const CassoLinkJobView  * view = m_panel->GetSelected();

        if (view != nullptr && m_actions.open)
        {
            m_actions.open (view->imagePath);
        }
    });

    SetDialogTickIntervalMs (kTickMs);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobDialog::OnDialogTick
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobDialog::OnDialogTick()
{
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobDialog::Refresh
//
//  The window closes once no tab is left.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobDialog::Refresh()
{
    std::vector<CassoLinkJobView>  views   = m_actions.views ? m_actions.views() : std::vector<CassoLinkJobView>();
    const CassoLinkJobView       * shown   = nullptr;
    bool                           isDone  = false;



    if (views.empty())
    {
        PostMessageW (GetHwnd(), WM_CLOSE, 0, 0);
        return;
    }

    m_panel->SetViews (std::move (views), m_selected);

    shown  = m_panel->GetSelected();
    isDone = shown != nullptr && shown->state == CassoLinkJobView::State::Done && !shown->imagePath.empty();

    m_cancelButton->SetEnabled (shown != nullptr && shown->IsRunning());
    m_folderButton->SetEnabled (isDone);
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
    params.resizable                = false;
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
