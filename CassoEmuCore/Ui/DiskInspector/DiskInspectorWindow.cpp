#include "Pch.h"

#include "Ui/DiskInspector/DiskInspectorWindow.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Ui/DiskInspector/PlatterCells.h"





static constexpr LPCWSTR  s_kpszWindowTitle  = L"Disk inspector";
static constexpr LPCWSTR  s_kpszClassName    = L"CassoDiskInspector";
static constexpr LPCWSTR  s_kpszHint         = L"Scroll to zoom, drag to pan, double-click to fit";
static constexpr LPCWSTR  s_kpszNoDisk       = L"No disk";
static constexpr LPCWSTR  s_kpszAnalyzing    = L"Analyzing";

static constexpr int      s_kToolbarDip      = 40;
static constexpr int      s_kRowDip          = 28;
static constexpr int      s_kTabsDip         = 30;
static constexpr int      s_kMarginDip       = 8;
static constexpr int      s_kSplitterDip     = 6;
static constexpr int      s_kButtonDip       = 32;
static constexpr int      s_kTabWidthDip     = 96;
static constexpr int      s_kDragThreshold   = 4;
static constexpr double   s_kMinSplit        = 0.3;
static constexpr double   s_kMaxSplit        = 0.75;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::DiskInspectorWindow
//
//  Analysis results are taken on the window's own frame, so the scheduler
//  needs no wake-up of its own.
//
////////////////////////////////////////////////////////////////////////////////

DiskInspectorWindow::DiskInspectorWindow() :
    m_scheduler (nullptr)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::~DiskInspectorWindow
//
////////////////////////////////////////////////////////////////////////////////

DiskInspectorWindow::~DiskInspectorWindow()
{
    DestroyBackend();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskInspectorWindow::Create (HINSTANCE hInstance, HWND hwndOwner, const CassoTheme * theme, IDiskInspectorHost * host, bool activate)
{
    HRESULT                   hr     = S_OK;
    DxuiWindow::CreateParams  params;



    BAIL_OUT_IF (IsCreated(), S_OK);

    m_theme = theme;
    m_host  = (host != nullptr) ? host : &m_nullHost;

    params.title                    = s_kpszWindowTitle;
    params.hInstance                = hInstance;
    params.ownerHwnd                = hwndOwner;
    params.initialSizeDip           = { kOpeningWidthDip, kOpeningHeightDip };
    params.minSizeDip               = { kMinWidthDip, kMinHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::Standard;
    params.classNameOverride        = s_kpszClassName;
    params.createNoActivate         = !activate;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;
    params.fitToWorkArea            = true;

    hr = DxuiWindow::Create (params);
    CHR (hr);

    SetTheme (m_theme);
    Show (activate);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ShowDrive
//
//  Starts over on the disk in the drive: the window asks for a copy and
//  shows "Analyzing" until the copy arrives.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskInspectorWindow::ShowDrive (int drive)
{
    HRESULT  hr = S_OK;



    CBRA (drive >= 0);

    m_drive = drive;

    if (m_driveTabs != nullptr)
    {
        m_driveTabs->SetSelected (drive);
    }

    RequestCopy();
    UpdateLabels();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::RenderFrame
//
//  Once per host frame: replies from the host, then analysis results, then
//  a repaint when anything changed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskInspectorWindow::RenderFrame()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (!IsCreated() || !IsWindowVisible (GetHwnd()), S_OK);

    TakeReplies();
    TakeResults();
    UpdateRings();
    UpdateLabels();

    Invalidate();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OnCreate()
{
    m_driveTabs     = CreateChild<DxuiTabStrip>();
    m_diskLabel     = CreateChild<DxuiLabel> (s_kpszNoDisk, DxuiTextRole::Heading, DxuiTextHAlign::Left);
    m_summaryLabel  = CreateChild<DxuiLabel> (L"",          DxuiTextRole::Body,    DxuiTextHAlign::Left);
    m_platterVisual = CreateChild<DxuiCustomVisual>();
    m_zoomOut       = CreateChild<DxuiButton> (L"\x2212");
    m_zoomIn        = CreateChild<DxuiButton> (L"+");
    m_fit           = CreateChild<DxuiButton> (L"Fit");
    m_zoomLabel     = CreateChild<DxuiLabel> (L"",          DxuiTextRole::Body,    DxuiTextHAlign::Left);
    m_trackLabel    = CreateChild<DxuiLabel> (L"",          DxuiTextRole::Heading, DxuiTextHAlign::Left);
    m_trackTabs     = CreateChild<DxuiTabStrip>();

    m_driveTabs->SetOnChange ([this] (int index) { (void) ShowDrive (index); });
    m_zoomOut->SetOnClick    ([this] () { ZoomBy (1.0 / kWheelZoomStep, { (m_platterPx.left + m_platterPx.right) / 2, (m_platterPx.top + m_platterPx.bottom) / 2 }); });
    m_zoomIn->SetOnClick     ([this] () { ZoomBy (kWheelZoomStep,       { (m_platterPx.left + m_platterPx.right) / 2, (m_platterPx.top + m_platterPx.bottom) / 2 }); });
    m_fit->SetOnClick        ([this] () { m_model.Fit(); });

    m_platterVisual->SetDraw ([this] (const DxuiCustomDrawArgs & args) { DrawPlatter (args); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnWindowClose
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OnWindowClose()
{
    Hide();
    m_host->OnInspectorClosed();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::Layout
//
//  A toolbar across the top; under it the platter column on the left, kept
//  square, with its zoom row and hint below, and the track column on the
//  right past the splitter.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int                        margin = scaler.ToPx (s_kMarginDip);
    int                        row    = scaler.ToPx (s_kRowDip);
    int                        tab    = scaler.ToPx (s_kTabWidthDip);
    int                        button = scaler.ToPx (s_kButtonDip);
    int                        width  = boundsDip.right - boundsDip.left;
    int                        splitX = boundsDip.left + static_cast<int> (width * m_splitFraction);
    int                        top    = boundsDip.top + scaler.ToPx (s_kToolbarDip);
    int                        column = 0;
    int                        side   = 0;
    int                        x      = 0;
    int                        drives = std::max (1, m_host != nullptr ? m_host->GetDriveCount() : 1);
    vector<DxuiTabStrip::Tab>  tabs;
    int                        i      = 0;



    SetBounds (boundsDip);
    m_scaler = scaler;

    m_toolbarPx  = { boundsDip.left, boundsDip.top, boundsDip.right, top };
    m_splitterPx = { splitX, top, splitX + scaler.ToPx (s_kSplitterDip), boundsDip.bottom };
    m_rightPx    = { m_splitterPx.right + margin, top + margin, boundsDip.right - margin, boundsDip.bottom - margin };

    for (i = 0; i < drives; i++)
    {
        RECT  r = { boundsDip.left + margin + i * tab, boundsDip.top + margin / 2, boundsDip.left + margin + (i + 1) * tab, top - margin / 2 };

        tabs.push_back ({ r, std::format (L"Drive {}", i + 1) });
    }

    m_driveTabs->SetTabs (std::move (tabs));
    m_driveTabs->Layout  ({ boundsDip.left + margin, boundsDip.top, boundsDip.left + margin + drives * tab, top }, scaler);

    x = boundsDip.left + 2 * margin + drives * tab;
    m_diskLabel->Layout    ({ x, boundsDip.top, x + width / 3, top }, scaler);
    m_summaryLabel->Layout ({ x + width / 3, boundsDip.top, boundsDip.right - margin, top }, scaler);

    column = splitX - boundsDip.left - 2 * margin;
    side   = std::max (0, std::min (column, static_cast<int> (boundsDip.bottom - top - 2 * row - 3 * margin)));

    m_platterPx = { boundsDip.left + margin + (column - side) / 2, top + margin, boundsDip.left + margin + (column - side) / 2 + side, top + margin + side };
    m_platterVisual->Layout (m_platterPx, scaler);

    x = boundsDip.left + margin;
    m_zoomOut->Layout   ({ x,              m_platterPx.bottom + margin, x + button,     m_platterPx.bottom + margin + row }, scaler);
    m_zoomIn->Layout    ({ x + button,     m_platterPx.bottom + margin, x + 2 * button, m_platterPx.bottom + margin + row }, scaler);
    m_fit->Layout       ({ x + 2 * button, m_platterPx.bottom + margin, x + 4 * button, m_platterPx.bottom + margin + row }, scaler);
    m_zoomLabel->Layout ({ x + 4 * button + margin, m_platterPx.bottom + margin, splitX - margin, m_platterPx.bottom + margin + row }, scaler);

    m_trackLabel->Layout ({ m_rightPx.left, m_rightPx.top, m_rightPx.right, m_rightPx.top + row }, scaler);

    tabs.clear();

    for (LPCWSTR label : { L"Sector data", L"Nibbles", L"Fields", L"Flux timing" })
    {
        i = static_cast<int> (tabs.size());
        tabs.push_back ({ { m_rightPx.left + i * tab, m_rightPx.top + row, m_rightPx.left + (i + 1) * tab, m_rightPx.top + row + scaler.ToPx (s_kTabsDip) }, label });
    }

    m_trackTabs->SetTabs (std::move (tabs));
    m_trackTabs->Layout  ({ m_rightPx.left, m_rightPx.top + row, m_rightPx.right, m_rightPx.top + row + scaler.ToPx (s_kTabsDip) }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    painter.FillRect (static_cast<float> (m_boundsDip.left), static_cast<float> (m_boundsDip.top),
                      static_cast<float> (m_boundsDip.right - m_boundsDip.left), static_cast<float> (m_boundsDip.bottom - m_boundsDip.top),
                      theme.Background());

    painter.FillRect (static_cast<float> (m_toolbarPx.left), static_cast<float> (m_toolbarPx.top),
                      static_cast<float> (m_toolbarPx.right - m_toolbarPx.left), static_cast<float> (m_toolbarPx.bottom - m_toolbarPx.top),
                      theme.BackgroundElevated());

    painter.FillRect (static_cast<float> (m_splitterPx.left), static_cast<float> (m_splitterPx.top),
                      static_cast<float> (m_splitterPx.right - m_splitterPx.left), static_cast<float> (m_splitterPx.bottom - m_splitterPx.top),
                      theme.Divider());

    DxuiWindow::Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnMouse
//
//  Over the platter: the wheel zooms about the pointer, a drag pans, a
//  double-click returns to fit, and a press shorter than the drag threshold
//  is a click. On the splitter: a drag moves it. Everything else goes to the
//  controls.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::OnMouse (const DxuiMouseEvent & ev)
{
    bool     isHandled = false;
    POINT    p         = ev.positionDip;
    int64_t  now       = static_cast<int64_t> (GetTickCount64());
    double   size      = std::max (1L, m_platterPx.right - m_platterPx.left);
    int      width     = m_boundsDip.right - m_boundsDip.left;



    if (ev.kind == DxuiMouseEventKind::Wheel && !ev.wheelHorizontal && IsInPlatter (p) && m_hasDisk)
    {
        ZoomBy (std::pow (kWheelZoomStep, ev.wheelDelta), p);
        isHandled = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && PtInRect (&m_splitterPx, p))
    {
        m_isSplitting = true;
        isHandled     = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && IsInPlatter (p))
    {
        if (now - m_lastClickMs <= static_cast<int64_t> (GetDoubleClickTime()))
        {
            m_model.Fit();
        }

        m_lastClickMs = now;
        m_isDragging  = true;
        m_dragFrom    = p;
        isHandled     = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Move && m_isSplitting && width > 0)
    {
        m_splitFraction = std::clamp (static_cast<double> (p.x - m_boundsDip.left) / width, s_kMinSplit, s_kMaxSplit);
        Layout (m_boundsDip, m_scaler);
        isHandled = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Move && m_isDragging)
    {
        if (std::abs (p.x - m_dragFrom.x) + std::abs (p.y - m_dragFrom.y) >= s_kDragThreshold || !m_model.IsAtFit())
        {
            m_model.PanBy ({ 2.0 * (p.x - m_dragFrom.x) / size, 2.0 * (p.y - m_dragFrom.y) / size });
            m_dragFrom = p;
        }

        isHandled = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Up && (m_isDragging || m_isSplitting))
    {
        m_isDragging  = false;
        m_isSplitting = false;
        isHandled     = true;
    }

    if (!isHandled)
    {
        isHandled = DxuiWindow::OnMouse (ev);
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnKey
//
//  Plus and minus zoom about the platter's center and 0 returns to fit
//  (FR-025); Up and Down step through quarter tracks.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::OnKey (const DxuiKeyEvent & ev)
{
    bool   isHandled = false;
    POINT  center    = { (m_platterPx.left + m_platterPx.right) / 2, (m_platterPx.top + m_platterPx.bottom) / 2 };



    if (ev.kind == DxuiKeyEventKind::Down && !ev.ctrl && !ev.alt)
    {
        isHandled = true;

        switch (ev.vk)
        {
            case VK_OEM_PLUS:  case VK_ADD:      ZoomBy (kWheelZoomStep, center);       break;
            case VK_OEM_MINUS: case VK_SUBTRACT: ZoomBy (1.0 / kWheelZoomStep, center); break;
            case '0':          case VK_NUMPAD0:  m_model.Fit();                          break;
            case VK_UP:                          m_model.SelectQuarterTrack (m_model.GetQuarterTrack() - 1); break;
            case VK_DOWN:                        m_model.SelectQuarterTrack (m_model.GetQuarterTrack() + 1); break;
            default:                             isHandled = false;                      break;
        }
    }

    if (!isHandled)
    {
        isHandled = DxuiWindow::OnKey (ev);
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DiskInspectorWindow::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = IDC_ARROW;



    if (m_isSplitting || PtInRect (&m_splitterPx, clientPx))
    {
        cursor = IDC_SIZEWE;
    }
    else if (m_isDragging || (IsInPlatter (clientPx) && !m_model.IsAtFit()))
    {
        cursor = IDC_SIZEALL;
    }

    return cursor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::RequestCopy
//
//  A copy of whatever disk is in the drive; the reply says which disk it is.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::RequestCopy()
{
    InspectorRequest  request;



    request.kind     = InspectorRequestKind::CopyDisk;
    request.drive    = m_drive;
    m_pendingRequest = m_host->PostInspectorRequest (request);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::TakeReplies
//
//  Only the reply to the latest request counts; a copy of another disk
//  starts the view over.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::TakeReplies()
{
    vector<InspectorReply>  replies;
    int                     slot    = 0;



    m_host->TakeInspectorReplies (replies);

    for (InspectorReply & reply : replies)
    {
        if (reply.requestId != m_pendingRequest)
        {
            continue;
        }

        m_pendingRequest = 0;
        m_hasDisk        = (reply.disk != nullptr);
        m_changedSlots.clear();
        m_levels.fill (nullptr);

        if (m_hasDisk)
        {
            m_model.SetDisk (reply.disk->mediaId);
            m_analysis           = DiskAnalysis();
            m_analysis.mediaId   = reply.disk->mediaId;
            m_analysis.copy      = reply.disk;
            m_analysis.settings  = DecodeSettings::MakeStandard();
            m_analysis.headLimit = Disk2Controller::kMaxQuarterTrack;
            m_analysis.tracks.resize (reply.disk->tracks.size());
            DiskAnalyzer::Assemble (m_analysis);
            m_scheduler.Restart (reply.disk, m_analysis.settings);

            for (slot = 0; slot < static_cast<int> (reply.disk->tracks.size()); slot++)
            {
                m_changedSlots.insert (slot);
            }
        }
        else
        {
            m_model.SetDisk (0);
            m_analysis = DiskAnalysis();
        }

        m_model.SetAnalysis (m_hasDisk ? &m_analysis : nullptr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::TakeResults
//
//  Every result that arrived since the last frame is stored, then the disk
//  is rebuilt once.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::TakeResults()
{
    vector<RecordResult>  results;



    m_scheduler.TakeResults (results);

    for (RecordResult & result : results)
    {
        if (result.mediaId == m_analysis.mediaId)
        {
            DiskAnalyzer::Accept (result.copy, result.slot, std::move (result.analysis), m_analysis);
            m_changedSlots.insert (result.slot);
        }
    }

    if (!results.empty())
    {
        DiskAnalyzer::Assemble (m_analysis);
        m_model.SetAnalysis (&m_analysis);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::UpdateRings
//
//  The platter's rings for every record whose analysis or state changed. A
//  record's cells are built once and shared by the quarter tracks that play
//  it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::UpdateRings()
{
    const DiskCopy *  copy  = m_analysis.copy.get();
    int               qt    = 0;
    int               slot  = 0;
    vector<Byte>      cells;



    for (qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
    {
        slot = (copy != nullptr) ? copy->playedSlot[qt] : -1;

        if (copy == nullptr || slot < 0)
        {
            if (copy != nullptr && copy->mappedSlot[qt] >= 0 && copy->IsSlotDamaged (copy->mappedSlot[qt]))
            {
                m_platter.SetRing (qt, PlatterRingState::Damaged, nullptr);
            }
            else
            {
                m_platter.SetRing (qt, PlatterRingState::Nothing, nullptr);
            }

            continue;
        }

        if (!m_changedSlots.contains (slot) && !m_scheduler.IsPending (slot))
        {
            continue;
        }

        if (m_scheduler.IsPending (slot) || slot >= static_cast<int> (m_analysis.tracks.size()) || m_analysis.tracks[slot] == nullptr)
        {
            m_platter.SetRing (qt, PlatterRingState::Pending, nullptr);
            continue;
        }

        if (m_levels[slot] == nullptr)
        {
            auto  levels = std::make_shared<PlatterRenderer::Levels>();

            PlatterCells::BuildCells  (*m_analysis.tracks[slot], cells);
            PlatterCells::BuildLevels (cells, *levels);
            m_levels[slot] = levels;
        }

        m_platter.SetRing (qt, PlatterRingState::Data, m_levels[slot]);
    }

    m_changedSlots.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::UpdateLabels
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::UpdateLabels()
{
    HRESULT              hr    = S_OK;
    const DiskSummary &  s     = m_analysis.summary;
    std::wstring         name;
    std::wstring         track;



    BAIL_OUT_IF (m_diskLabel == nullptr, S_OK);

    if (m_hasDisk && m_analysis.copy != nullptr)
    {
        name = TextEncoding::Utf8ToWide (m_analysis.copy->fileName);
        name = name.substr (name.find_last_of (L"\\/") == std::wstring::npos ? 0 : name.find_last_of (L"\\/") + 1);
    }

    m_diskLabel->SetText (m_hasDisk ? (name.empty() ? L"Untitled disk" : name) : (m_pendingRequest != 0 ? s_kpszAnalyzing : s_kpszNoDisk));

    m_summaryLabel->SetText (m_hasDisk
        ? std::format (L"{} tracks with data {} {} of {} sectors good{}", s.tracksWithData, s_kchBullet, s.sectorsGood, s.sectorsFound,
                       m_scheduler.HasPending() ? std::wstring (L" ") + s_kchBullet + L" " + s_kpszAnalyzing : std::wstring())
        : std::wstring());

    m_zoomLabel->SetText (std::format (L"{:.0f}{}", m_model.GetZoom(), s_kpszMultiplyX));
    m_fit->SetEnabled    (!m_model.IsAtFit());
    m_zoomOut->SetVisible (m_hasDisk);
    m_zoomIn->SetVisible  (m_hasDisk);
    m_fit->SetVisible     (m_hasDisk);
    m_zoomLabel->SetVisible (m_hasDisk);

    if (m_hasDisk)
    {
        track = L"Track " + InspectorFormat::FormatQuarterTrack (m_model.GetQuarterTrack());

        if (m_model.GetSector() != nullptr)
        {
            track += L", sector " + InspectorFormat::FormatSector (m_model.GetSector()->sector);
        }
    }

    m_trackLabel->SetText (track);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::DrawPlatter
//
//  The renderer is made on the device the window draws with, the first time
//  it draws.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::DrawPlatter (const DxuiCustomDrawArgs & args)
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (!m_hasDisk, S_OK);

    if (!m_isPlatterReady)
    {
        hr = m_platter.Initialize (args.device);
        CHR (hr);

        m_isPlatterReady = true;
    }

    m_platter.SetPalette (DiskInspectorPalette::Resolve (*m_theme));

    hr = m_platter.Render (args, m_model.GetPlatterView (args.rectPx, 0.0));
    CHR (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ZoomBy
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ZoomBy (double factor, POINT anchorPx)
{
    m_model.ZoomAbout (m_model.GetZoom() * factor, ToViewPoint (anchorPx));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::IsInPlatter
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::IsInPlatter (POINT px) const
{
    return PtInRect (&m_platterPx, px) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ToViewPoint
//
//  A pixel as the view model's fit units: the platter's center is (0, 0)
//  and its edges are at -1 and 1.
//
////////////////////////////////////////////////////////////////////////////////

InspectorViewModel::Point DiskInspectorWindow::ToViewPoint (POINT px) const
{
    double                      half  = std::max (1.0, (m_platterPx.right - m_platterPx.left) / 2.0);
    InspectorViewModel::Point   point;



    point.x = (px.x - (m_platterPx.left + m_platterPx.right) / 2.0) / half;
    point.y = (px.y - (m_platterPx.top + m_platterPx.bottom) / 2.0) / half;

    return point;
}
