#include "Pch.h"

#include "Ui/DiskInspector/DiskInspectorWindow.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Ui/DiskInspector/DecodeSettingsDialog.h"
#include "Ui/DiskInspector/FindingsTab.h"
#include "Ui/DiskInspector/InspectorTableView.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/FindPanel.h"
#include "Ui/DiskInspector/FluxTimingTab.h"
#include "Ui/DiskInspector/GoToDialog.h"
#include "Ui/DiskInspector/NibblesTab.h"
#include "Ui/DiskInspector/FluxTiming.h"
#include "Ui/DiskInspector/PlatterCells.h"
#include "Ui/DiskInspector/PlatterLegendView.h"
#include "Ui/DiskInspector/PlatterView.h"
#include "Ui/DiskInspector/SectorByteView.h"
#include "Ui/DiskInspector/SectorRowView.h"
#include "Ui/DiskInspector/TrackHeaderView.h"
#include "Ui/DiskInspector/TrackStripView.h"





static constexpr LPCWSTR  s_kpszWindowTitle = L"Disk inspector";
static constexpr LPCWSTR  s_kpszClassName   = L"CassoDiskInspector";
static constexpr LPCWSTR  s_kpszHint        = L"Scroll to zoom, drag to pan, double-click to fit";
static constexpr LPCWSTR  s_kpszStripHint   = L"Scroll to zoom, drag to pan, Shift+drag to select, double-click for the whole track";
static constexpr LPCWSTR  s_kpszNoDisk      = L"No disk";
static constexpr LPCWSTR  s_kpszAnalyzing   = L"Analyzing";

static constexpr int      s_kToolbarDip     = 40;
static constexpr int      s_kRowDip         = 28;
static constexpr int      s_kTabsDip        = 30;
static constexpr int      s_kStripDip       = 64;
static constexpr int      s_kHintRowDip     = 20;
static constexpr int      s_kWholeTrackDip  = 104;
static constexpr int      s_kReadoutDip     = 220;
static constexpr int      s_kSectorRowDip   = 76;
static constexpr int      s_kMarginDip      = 8;
static constexpr int      s_kSplitterDip    = 6;
static constexpr int      s_kButtonDip      = 32;
static constexpr int      s_kTabWidthDip    = 96;
static constexpr int      s_kFileNameDip    = 260;
static constexpr float    s_kChipPadDip     = 8.0f;
static constexpr float    s_kChipGapDip     = 6.0f;
static constexpr float    s_kChipHeightDip  = 22.0f;
static constexpr float    s_kChipTextDip    = 12.0f;
static constexpr float    s_kAnalyzingDip   = 90.0f;
static constexpr double   s_kMinSplit       = 0.3;
static constexpr double   s_kMaxSplit       = 0.7;
static constexpr double   s_kPanStep        = 0.1;
static constexpr double   s_kPlatterShare   = 0.55;
static constexpr int      s_kDecodeButtonDip = 140;
static constexpr int      s_kModeTabDip     = 80;
static constexpr int      s_kRangeDip       = 120;
static constexpr double   s_kRanges[]       = { 0.01, 0.02, 0.03, 0.05, 0.10, 0.15, 0.20, 0.25 };

enum TrackTab
{
    kTabSectorData,
    kTabNibbles,
    kTabFields,
    kTabFluxTiming,
};


enum DiskTab
{
    kTabTracks,
    kTabFindings,
    kTabFileMap,
    kTabImage,
};





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
    m_context.analysis           = &m_analysis;
    m_context.model              = &m_model;
    m_context.onSelectionChanged = [this] () { OnSelection(); };
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
    m_tooltip.SetTheme (*m_theme);
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
    UpdateControls();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::RenderFrame
//
//  Once per host frame: replies from the host, then analysis results, then
//  a repaint. The palette is resolved each frame, so a theme change shows
//  without reopening (FR-077).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskInspectorWindow::RenderFrame()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (!IsCreated() || !IsWindowVisible (GetHwnd()), S_OK);

    m_context.palette = DiskInspectorPalette::Resolve (*m_theme);
    m_tooltip.SetTheme (*m_theme);

    TakeReplies();
    TakeResults();
    UpdateRings();
    UpdateControls();
    RefreshTables();

    m_tooltip.Tick     (static_cast<int64_t> (GetTickCount64()));
    m_tracksTab->Tick   (static_cast<int64_t> (GetTickCount64()));
    m_findingsTab->Tick (static_cast<int64_t> (GetTickCount64()));
    m_fieldsTab->Tick   (static_cast<int64_t> (GetTickCount64()));
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
    m_driveTabs   = CreateChild<DxuiTabStrip>();
    m_platterView = CreateChild<PlatterView> (m_context);
    m_zoomOut     = CreateChild<DxuiButton> (s_kpszMinus);
    m_zoomIn      = CreateChild<DxuiButton> (L"+");
    m_fit         = CreateChild<DxuiButton> (L"Fit");
    m_zoomLabel   = CreateChild<DxuiLabel> (L"",         DxuiTextRole::Body,    DxuiTextHAlign::Left);
    m_hintLabel   = CreateChild<DxuiLabel> (s_kpszHint,  DxuiTextRole::Muted  , DxuiTextHAlign::Left);
    m_headerView  = CreateChild<TrackHeaderView> (m_context);
    m_stripView   = CreateChild<TrackStripView>  (m_context);
    m_stripOut    = CreateChild<DxuiButton> (s_kpszMinus);
    m_stripIn     = CreateChild<DxuiButton> (L"+");
    m_stripWhole  = CreateChild<DxuiButton> (L"Whole track");
    m_stripReadout = CreateChild<DxuiLabel> (L"", DxuiTextRole::Body, DxuiTextHAlign::Left);
    m_stripHint   = CreateChild<DxuiLabel> (s_kpszStripHint, DxuiTextRole::Muted, DxuiTextHAlign::Left);
    m_sectorRow   = CreateChild<SectorRowView>   (m_context);
    m_trackTabs   = CreateChild<DxuiTabStrip>();
    m_byteView    = CreateChild<SectorByteView>  (m_context);
    m_nibblesTab  = CreateChild<NibblesTab>      (m_context);
    m_fieldsTab   = CreateChild<InspectorTableView> (m_context);
    m_fluxTab     = CreateChild<FluxTimingTab>   (m_context);
    m_diskTabs    = CreateChild<DxuiTabStrip>();
    m_decodeButton = CreateChild<DxuiButton> (L"Decode settings...");
    m_alignmentCheck = CreateChild<DxuiCheckbox> (L"Alignment");
    m_modeTabs     = CreateChild<DxuiTabStrip>();
    m_rangeDown    = CreateChild<DxuiButton> (s_kpszMinus);
    m_rangeUp      = CreateChild<DxuiButton> (L"+");
    m_rangeLabel   = CreateChild<DxuiLabel> (L"", DxuiTextRole::Body, DxuiTextHAlign::Center);
    m_legend       = CreateChild<PlatterLegendView> (m_context);
    m_tracksTab   = CreateChild<InspectorTableView> (m_context);
    m_findingsTab = CreateChild<FindingsTab>     (m_context);

    m_driveTabs->SetOnChange ([this] (int index) { (void) ShowDrive (index); });
    m_trackTabs->SetOnChange ([this] (int index) { ShowTrackTab (index); });
    m_diskTabs->SetOnChange  ([this] (int index) { ShowDiskTab (index); });
    m_tracksTab->SetColumns  (InspectorTables::GetTrackColumns());
    m_fieldsTab->SetColumns  (InspectorTables::GetFieldColumns());
    m_tracksTab->SetOnSelect   ([this] (const TableRow & row) { SelectFromRow (row); });
    m_findingsTab->SetOnSelect ([this] (const TableRow & row) { SelectFromRow (row); });
    m_fieldsTab->SetOnSelect   ([this] (const TableRow & row) { SelectFromRow (row); });
    m_zoomOut->SetOnClick    ([this] () { m_platterView->ZoomAboutCenter (1.0 / kZoomStep); });
    m_zoomIn->SetOnClick     ([this] () { m_platterView->ZoomAboutCenter (kZoomStep); });
    m_fit->SetOnClick        ([this] () { m_model.Fit(); });
    m_stripOut->SetOnClick   ([this] () { m_stripView->ZoomAboutCenter (1.0 / kZoomStep); });
    m_stripIn->SetOnClick    ([this] () { m_stripView->ZoomAboutCenter (kZoomStep); });
    m_stripWhole->SetOnClick ([this] () { m_stripView->ShowWholeTrack(); });
    m_decodeButton->SetOnClick ([this] () { OpenDecodeSettings(); });
    m_alignmentCheck->SetOnChange ([this] (bool isChecked) { m_platterView->SetAlignmentShown (isChecked); });
    m_modeTabs->SetOnChange  ([this] (int index) { m_context.isTimingMode = (index == 1); });
    m_rangeDown->SetOnClick  ([this] () { StepRange (-1); });
    m_rangeUp->SetOnClick    ([this] () { StepRange (1); });

    m_tooltip.SetPopupHost (GetPopupHost());

    ShowTrackTab (kTabSectorData);
    ShowDiskTab  (kTabTracks);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnWindowClose
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OnWindowClose()
{
    m_tooltip.HideImmediate();
    Hide();
    m_host->OnInspectorClosed();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::Layout
//
//  A toolbar across the top; under it the platter column on the left, the
//  platter kept square, with its zoom row and hint below; past the splitter
//  the track column: header, strip, sector row, tabs and the tab's content.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int                        margin  = scaler.ToPx (s_kMarginDip);
    int                        row     = scaler.ToPx (s_kRowDip);
    int                        tab     = scaler.ToPx (s_kTabWidthDip);
    int                        button  = scaler.ToPx (s_kButtonDip);
    int                        width   = boundsDip.right - boundsDip.left;
    int                        splitX  = boundsDip.left + static_cast<int> (width * m_splitFraction);
    int                        top     = boundsDip.top + scaler.ToPx (s_kToolbarDip);
    int                        drives  = std::max (1, m_host != nullptr ? m_host->GetDriveCount() : 1);
    int                        column  = splitX - boundsDip.left - 2 * margin;
    int                        side    = std::max (0, std::min (column, static_cast<int> ((boundsDip.bottom - top) * s_kPlatterShare)));
    int                        x       = boundsDip.left + margin;
    int                        y       = 0;
    int                        i       = 0;
    int                        header  = scaler.ToPx (TrackHeaderView::kLineDip * TrackHeaderView::kLines);
    RECT                       right   = {};
    RECT                       platter = {};
    vector<DxuiTabStrip::Tab>  tabs;



    SetBounds (boundsDip);
    m_scaler = scaler;
    m_tooltip.SetDpi (scaler.GetDpi());
    m_tooltip.SetViewportSize (width, boundsDip.bottom - boundsDip.top);

    m_toolbarPx  = { boundsDip.left, boundsDip.top, boundsDip.right, top };
    m_splitterPx = { splitX, top, splitX + scaler.ToPx (s_kSplitterDip), boundsDip.bottom };
    right        = { m_splitterPx.right + margin, top + margin, boundsDip.right - margin, boundsDip.bottom - margin };

    for (i = 0; i < drives; i++)
    {
        tabs.push_back ({ { x + i * tab, boundsDip.top + margin / 2, x + (i + 1) * tab, top - margin / 2 }, std::format (L"Drive {}", i + 1) });
    }

    m_driveTabs->SetTabs (std::move (tabs));
    m_driveTabs->SetSelected (m_drive);
    m_driveTabs->Layout  ({ x, boundsDip.top, x + drives * tab, top }, scaler);

    m_fileNamePx = { x + drives * tab + margin, boundsDip.top, std::min (x + drives * tab + margin + scaler.ToPx (s_kFileNameDip), static_cast<int> (boundsDip.right)), top };
    x = boundsDip.right - margin - scaler.ToPx (s_kDecodeButtonDip) - margin - scaler.ToPx (s_kRangeDip) - scaler.ToPx (2 * s_kModeTabDip);
    m_chipsPx = { m_fileNamePx.right + margin, boundsDip.top, x - margin, top };

    tabs.clear();
    tabs.push_back ({ { x, boundsDip.top + margin / 2, x + scaler.ToPx (s_kModeTabDip), top - margin / 2 }, L"Structure" });
    tabs.push_back ({ { x + scaler.ToPx (s_kModeTabDip), boundsDip.top + margin / 2, x + scaler.ToPx (2 * s_kModeTabDip), top - margin / 2 }, L"Timing" });
    m_modeTabs->SetTabs     (std::move (tabs));
    m_modeTabs->SetSelected (m_context.isTimingMode ? 1 : 0);
    m_modeTabs->Layout      ({ x, boundsDip.top, x + scaler.ToPx (2 * s_kModeTabDip), top }, scaler);

    x += scaler.ToPx (2 * s_kModeTabDip);
    m_rangeDown->Layout  ({ x,                                     boundsDip.top + margin, x + button,                            top - margin }, scaler);
    m_rangeLabel->Layout ({ x + button,                            boundsDip.top + margin, x + scaler.ToPx (s_kRangeDip) - button, top - margin }, scaler);
    m_rangeUp->Layout    ({ x + scaler.ToPx (s_kRangeDip) - button, boundsDip.top + margin, x + scaler.ToPx (s_kRangeDip),          top - margin }, scaler);
    x = boundsDip.left + margin;
    m_decodeButton->Layout ({ boundsDip.right - margin - scaler.ToPx (s_kDecodeButtonDip), boundsDip.top + margin / 2, boundsDip.right - margin, top - margin / 2 }, scaler);

    platter = { boundsDip.left + margin + (column - side) / 2, top + margin, boundsDip.left + margin + (column - side) / 2 + side, top + margin + side };
    m_platterView->Layout (platter, scaler);

    y = platter.bottom + margin;
    m_zoomOut->Layout   ({ x,              y, x + button,     y + row }, scaler);
    m_zoomIn->Layout    ({ x + button,     y, x + 2 * button, y + row }, scaler);
    m_fit->Layout       ({ x + 2 * button, y, x + 4 * button, y + row }, scaler);
    m_zoomLabel->Layout ({ x + 4 * button + margin, y, x + 6 * button, y + row }, scaler);
    m_alignmentCheck->Layout ({ x + 6 * button + margin, y, splitX - margin, y + row }, scaler);
    m_hintLabel->Layout ({ x, y + row, splitX - margin, y + 2 * row }, scaler);

    y += 2 * row;
    m_legend->Layout ({ x, y, splitX - margin, y + scaler.ToPx (PlatterLegendView::kRowDip * PlatterLegendView::kRows) }, scaler);
    y += scaler.ToPx (PlatterLegendView::kRowDip * PlatterLegendView::kRows);
    tabs.clear();

    for (LPCWSTR label : { L"Tracks", L"Findings", L"File map", L"Image" })
    {
        i = static_cast<int> (tabs.size());
        tabs.push_back ({ { x + i * tab, y, x + (i + 1) * tab, y + scaler.ToPx (s_kTabsDip) }, label });
    }

    m_diskTabs->SetTabs     (std::move (tabs));
    m_diskTabs->SetSelected (m_diskTab);
    m_diskTabs->Layout      ({ x, y, splitX - margin, y + scaler.ToPx (s_kTabsDip) }, scaler);
    y += scaler.ToPx (s_kTabsDip) + margin / 2;

    m_tracksTab->Layout   ({ x, y, splitX - margin, boundsDip.bottom - margin }, scaler);
    m_findingsTab->Layout ({ x, y, splitX - margin, boundsDip.bottom - margin }, scaler);

    y = right.top;
    m_headerView->Layout ({ right.left, y, right.right, y + header }, scaler);
    y += header;
    m_stripView->Layout ({ right.left, y, right.right, y + scaler.ToPx (s_kStripDip) }, scaler);
    y += scaler.ToPx (s_kStripDip) + margin / 2;
    m_stripOut->Layout     ({ right.left,          y, right.left + button,     y + row }, scaler);
    m_stripIn->Layout      ({ right.left + button, y, right.left + 2 * button, y + row }, scaler);
    m_stripWhole->Layout   ({ right.left + 2 * button, y, right.left + 2 * button + scaler.ToPx (s_kWholeTrackDip), y + row }, scaler);
    m_stripReadout->Layout ({ right.left + 2 * button + scaler.ToPx (s_kWholeTrackDip) + margin, y, right.right, y + row }, scaler);
    y += row;
    m_stripHint->Layout    ({ right.left, y, right.right, y + scaler.ToPx (s_kHintRowDip) }, scaler);
    y += scaler.ToPx (s_kHintRowDip) + margin / 2;
    m_sectorRow->Layout ({ right.left, y, right.right, y + scaler.ToPx (s_kSectorRowDip) }, scaler);
    y += scaler.ToPx (s_kSectorRowDip);

    tabs.clear();

    for (LPCWSTR label : { L"Sector data", L"Nibbles", L"Fields", L"Flux timing" })
    {
        i = static_cast<int> (tabs.size());
        tabs.push_back ({ { right.left + i * tab, y, right.left + (i + 1) * tab, y + scaler.ToPx (s_kTabsDip) }, label });
    }

    m_trackTabs->SetTabs (std::move (tabs));
    m_trackTabs->SetSelected (m_trackTab);
    m_trackTabs->Layout  ({ right.left, y, right.right, y + scaler.ToPx (s_kTabsDip) }, scaler);
    y += scaler.ToPx (s_kTabsDip) + margin;

    m_byteView->Layout   ({ right.left, y, right.right, right.bottom }, scaler);
    m_nibblesTab->Layout ({ right.left, y, right.right, right.bottom }, scaler);
    m_fieldsTab->Layout  ({ right.left, y, right.right, right.bottom }, scaler);
    m_fluxTab->Layout    ({ right.left, y, right.right, right.bottom }, scaler);
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

    painter.FillRect (static_cast<float> (m_splitterPx.left), static_cast<float> (m_splitterPx.top),
                      static_cast<float> (m_splitterPx.right - m_splitterPx.left), static_cast<float> (m_splitterPx.bottom - m_splitterPx.top),
                      theme.Divider());

    PaintToolbar (painter, text, theme);

    DxuiWindow::Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::PaintToolbar
//
//  The file name, clipped to its room, then the summary chips that fit
//  (FR-018); the name's tooltip gives it whole.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::PaintToolbar (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float         pad     = m_scaler.ToPxf (s_kChipPadDip);
    float         gap     = m_scaler.ToPxf (s_kChipGapDip);
    float         chipH   = m_scaler.ToPxf (s_kChipHeightDip);
    float         textPx  = m_scaler.ToPxf (s_kChipTextDip);
    float         x       = static_cast<float> (m_chipsPx.left);
    float         y       = (m_chipsPx.top + m_chipsPx.bottom - chipH) / 2.0f;
    float         w       = 0;
    float         h       = 0;
    bool          isFull  = false;
    std::wstring  name    = m_context.hasDisk ? GetFileName() : std::wstring (m_pendingRequest != 0 ? s_kpszAnalyzing : s_kpszNoDisk);



    painter.FillRect (static_cast<float> (m_toolbarPx.left), static_cast<float> (m_toolbarPx.top),
                      static_cast<float> (m_toolbarPx.right - m_toolbarPx.left), static_cast<float> (m_toolbarPx.bottom - m_toolbarPx.top),
                      theme.BackgroundElevated());

    text.DrawString (name.c_str(), static_cast<float> (m_fileNamePx.left), static_cast<float> (m_fileNamePx.top),
                     static_cast<float> (m_fileNamePx.right - m_fileNamePx.left), static_cast<float> (m_fileNamePx.bottom - m_fileNamePx.top),
                     theme.HeadingForeground(), m_scaler.ToPxf (InspectorView::kTextDip + 1.0f), DxuiTheme::kBodyFace,
                     DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);

    if (m_context.hasDisk)
    {
        for (const SummaryChip & chip : InspectorText::BuildChips (m_analysis.summary))
        {
            text.MeasureString (chip.text.c_str(), textPx, DxuiTheme::kBodyFace, w, h);
            isFull = isFull || x + w + 2 * pad > m_chipsPx.right;

            if (!isFull)
            {
                painter.FillRoundedRect (x, y, w + 2 * pad, chipH, chipH / 2, theme.ButtonIdle());
                painter.OutlineRoundedRect (x, y, w + 2 * pad, chipH, chipH / 2, m_scaler.ToPxf (1.0f),
                                            chip.isBad ? m_context.palette.colors.sectorBad : theme.ButtonBorder());
                text.DrawString (chip.text.c_str(), x, y, w + 2 * pad, chipH, chip.isBad ? m_context.palette.colors.sectorBad : theme.Foreground(), textPx,
                                 DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

                x += w + 2 * pad + gap;
            }
        }

        if (!m_analysis.settings.IsStandard() && !isFull)
        {
            text.MeasureString (L"Custom decode settings", textPx, DxuiTheme::kBodyFace, w, h);
            painter.FillRoundedRect (x, y, w + 2 * pad, chipH, chipH / 2, theme.SelectionBackground());
            text.DrawString (L"Custom decode settings", x, y, w + 2 * pad, chipH, theme.Foreground(), textPx, DxuiTheme::kBodyFace,
                             DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            x += w + 2 * pad + gap;
        }

        if (m_scheduler.HasPending())
        {
            text.DrawString (s_kpszAnalyzing, x, y, m_scaler.ToPxf (s_kAnalyzingDip), chipH, theme.ForegroundMuted(), textPx, DxuiTheme::kBodyFace,
                             DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnMouse
//
//  The splitter drags here; everything else goes to the views and controls,
//  and the tooltip follows the pointer.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::OnMouse (const DxuiMouseEvent & ev)
{
    bool   isHandled = false;
    POINT  p         = ev.positionDip;
    int    width     = m_boundsDip.right - m_boundsDip.left;



    //  The keys go to the strip after a press on it or its buttons, and to the
    //  platter after a press anywhere else (FR-029, FR-034).
    if (ev.kind == DxuiMouseEventKind::Down)
    {
        m_isStripKeys = IsInside (m_stripView->GetBounds(), p) || IsInside (m_stripOut->GetBounds(), p) || IsInside (m_stripIn->GetBounds(), p)
                     || IsInside (m_stripWhole->GetBounds(), p);
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && PtInRect (&m_splitterPx, p))
    {
        m_isSplitting = true;
        isHandled     = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Move && m_isSplitting && width > 0)
    {
        m_splitFraction = std::clamp (static_cast<double> (p.x - m_boundsDip.left) / width, s_kMinSplit, s_kMaxSplit);
        Layout (m_boundsDip, m_scaler);
        isHandled = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Up && m_isSplitting)
    {
        m_isSplitting = false;
        isHandled     = true;
    }

    if (!isHandled)
    {
        isHandled = DxuiWindow::OnMouse (ev);
    }

    if (ev.kind == DxuiMouseEventKind::Move)
    {
        UpdateTooltip (p);
    }
    else if (ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Wheel || ev.kind == DxuiMouseEventKind::Leave)
    {
        m_tooltip.RequestHide (static_cast<int64_t> (GetTickCount64()));
        m_tooltipText.clear();
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnKey
//
//  Up and Down step a quarter track, Page Up and Page Down go to the next
//  whole track, Left and Right step through the sectors in passing order
//  wrapping at the index, Home and End go to the first and last (FR-029);
//  plus and minus zoom, 0 returns to fit (FR-025), and Ctrl with an arrow
//  pans the zoomed platter. Ctrl+F finds, F3 and Shift+F3 step through the
//  hits, and Ctrl+G goes to a target. After a press on the strip, plus and minus zoom
//  it, 0 shows the whole track, and Left and Right pan it (FR-034).
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::OnKey (const DxuiKeyEvent & ev)
{
    static constexpr int  kPerTrack = DiskImage::kQuarterTracksPerWholeTrack;



    bool  isHandled = false;
    int   qt        = m_model.GetQuarterTrack();



    if (ev.kind == DxuiKeyEventKind::Down && !ev.alt && m_context.hasDisk)
    {
        isHandled = true;

        if (ev.ctrl)
        {
            switch (ev.vk)
            {
                case VK_LEFT:  m_model.PanBy ({  s_kPanStep, 0.0 }); break;
                case VK_RIGHT: m_model.PanBy ({ -s_kPanStep, 0.0 }); break;
                case VK_UP:    m_model.PanBy ({ 0.0,  s_kPanStep }); break;
                case VK_DOWN:  m_model.PanBy ({ 0.0, -s_kPanStep }); break;
                case 'F':      OpenFind();                           break;
                case 'G':      OpenGoTo();                           break;
                default:       isHandled = false;                    break;
            }
        }
        else if (m_isStripKeys && IsStripKey (ev.vk))
        {
            switch (ev.vk)
            {
                case VK_OEM_PLUS:  case VK_ADD:      m_stripView->ZoomAboutCenter (kZoomStep);       break;
                case VK_OEM_MINUS: case VK_SUBTRACT: m_stripView->ZoomAboutCenter (1.0 / kZoomStep); break;
                case VK_LEFT:                        m_stripView->PanBy (-s_kPanStep);               break;
                case VK_RIGHT:                       m_stripView->PanBy (s_kPanStep);                break;
                default:                             m_stripView->ShowWholeTrack();                  break;
            }
        }
        else
        {
            switch (ev.vk)
            {
                case VK_OEM_PLUS:  case VK_ADD:      m_platterView->ZoomAboutCenter (kZoomStep);       break;
                case VK_OEM_MINUS: case VK_SUBTRACT: m_platterView->ZoomAboutCenter (1.0 / kZoomStep); break;
                case '0':          case VK_NUMPAD0:  m_model.Fit();                                    break;
                case VK_UP:    m_model.SelectQuarterTrack (qt - 1);                           OnSelection(); break;
                case VK_DOWN:  m_model.SelectQuarterTrack (qt + 1);                           OnSelection(); break;
                case VK_PRIOR: m_model.SelectQuarterTrack ((qt - 1) / kPerTrack * kPerTrack); OnSelection(); break;
                case VK_NEXT:  m_model.SelectQuarterTrack ((qt / kPerTrack + 1) * kPerTrack); OnSelection(); break;
                case VK_LEFT:  StepSector (-1, false); break;
                case VK_RIGHT: StepSector ( 1, false); break;
                case VK_HOME:  StepSector (-1, true);  break;
                case VK_F3:    StepFind (ev.shift ? -1 : 1); break;
                case VK_END:   StepSector ( 1, true);  break;
                default:       isHandled = false;      break;
            }
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
    else if (m_platterView != nullptr && (m_platterView->IsDragging() || (IsInside (m_platterView->GetBounds(), clientPx) && !m_model.IsAtFit())))
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
//  Only the reply to the latest request counts. Its disk starts the view
//  over unless it is the same disk; every record waits for its analysis.
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

        m_pendingRequest  = 0;
        m_context.hasDisk = (reply.disk != nullptr);
        m_changedSlots.clear();
        m_levels.fill (nullptr);
        m_timingLevels.fill (nullptr);

        if (m_context.hasDisk)
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

        m_model.SetAnalysis (m_context.hasDisk ? &m_analysis : nullptr);
        m_isTablesDirty = true;
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
            m_levels[result.slot]       = nullptr;
            m_timingLevels[result.slot] = nullptr;
        }
    }

    if (!results.empty())
    {
        DiskAnalyzer::Assemble (m_analysis);
        m_model.SetAnalysis (&m_analysis);
        m_isTablesDirty = true;
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
    const DiskCopy *   copy     = m_analysis.copy.get();
    PlatterRenderer &  renderer = m_platterView->GetRenderer();
    int                qt       = 0;
    int                slot     = 0;
    bool               damaged  = false;
    vector<Byte>       cells;



    for (qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
    {
        slot    = (copy != nullptr) ? copy->playedSlot[qt] : -1;
        damaged = copy != nullptr && slot < 0 && copy->mappedSlot[qt] >= 0 && copy->IsSlotDamaged (copy->mappedSlot[qt]);

        if (slot < 0)
        {
            renderer.SetRing (qt, damaged ? PlatterRingState::Damaged : PlatterRingState::Nothing, nullptr);
        }
        else if (m_scheduler.IsPending (slot) || slot >= static_cast<int> (m_analysis.tracks.size()) || m_analysis.tracks[slot] == nullptr)
        {
            renderer.SetRing (qt, PlatterRingState::Pending, nullptr);
        }
        else if (m_changedSlots.contains (slot) || m_levels[slot] == nullptr)
        {
            if (m_levels[slot] == nullptr)
            {
                auto  levels = std::make_shared<PlatterRenderer::Levels>();

                PlatterCells::BuildCells  (*m_analysis.tracks[slot], cells);
                PlatterCells::BuildLevels (cells, *levels);
                m_levels[slot] = levels;

                if (m_analysis.tracks[slot]->framed.isFlux)
                {
                    auto  timing = std::make_shared<PlatterRenderer::Levels>();

                    FluxTiming::BuildDeviations (*m_analysis.tracks[slot], cells);
                    FluxTiming::BuildLevels     (cells, *timing);
                    m_timingLevels[slot] = timing;
                }
            }

            renderer.SetRing (qt, PlatterRingState::Data, m_levels[slot], m_timingLevels[slot]);
        }
    }

    m_changedSlots.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::UpdateControls
//
//  The zoom controls are hidden with no disk, and "Fit" is unavailable at
//  fit (FR-025).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::UpdateControls()
{
    if (m_zoomLabel != nullptr)
    {
        m_zoomLabel->SetText    (std::format (L"{:.0f}{}", m_model.GetZoom(), s_kpszMultiplyX));
        m_fit->SetEnabled       (!m_model.IsAtFit());
        m_zoomOut->SetVisible   (m_context.hasDisk);
        m_decodeButton->SetVisible (m_context.hasDisk);
        m_zoomIn->SetVisible    (m_context.hasDisk);
        m_fit->SetVisible       (m_context.hasDisk);
        m_zoomLabel->SetVisible (m_context.hasDisk);
        m_hintLabel->SetVisible (m_context.hasDisk);
        m_stripOut->SetVisible     (m_context.hasDisk);
        m_stripIn->SetVisible      (m_context.hasDisk);
        m_stripWhole->SetVisible   (m_context.hasDisk);
        m_stripWhole->SetEnabled   (m_model.GetStripSpan() < 1.0);
        m_stripReadout->SetVisible (m_context.hasDisk);
        m_stripReadout->SetText    (m_stripView->GetReadout());
        m_stripHint->SetVisible    (m_context.hasDisk);
        m_alignmentCheck->SetVisible (m_context.hasDisk);
        m_modeTabs->SetVisible   (m_context.hasDisk);
        m_rangeDown->SetVisible  (m_context.hasDisk && m_context.isTimingMode);
        m_rangeUp->SetVisible    (m_context.hasDisk && m_context.isTimingMode);
        m_rangeLabel->SetVisible (m_context.hasDisk && m_context.isTimingMode);
        m_rangeLabel->SetText    (std::wstring (s_kpszPlusMinus) + InspectorFormat::FormatPercent (m_context.timingRange).substr (1));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OnSelection
//
//  After any view changes the selection, the Nibbles tab scrolls to it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OnSelection()
{
    const TrackAnalysis *   track  = m_model.GetTrack();
    const AnalyzedSector *  sector = m_model.GetSector();
    int                     nibble = m_model.GetFirstNibble();



    if (nibble < 0 && track != nullptr && sector != nullptr)
    {
        nibble = track->fields[sector->addressField].firstNibble;
    }

    m_nibblesTab->ScrollTo (nibble);
    SyncTables();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ShowTrackTab
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ShowTrackTab (int tab)
{
    m_trackTab = tab;
    m_byteView->SetVisible   (tab == kTabSectorData);
    m_nibblesTab->SetVisible (tab == kTabNibbles);
    m_fieldsTab->SetVisible  (tab == kTabFields);
    m_fluxTab->SetVisible    (tab == kTabFluxTiming);
    m_trackTabs->SetSelected (tab);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::UpdateTooltip
//
//  The first view with a tooltip for the point gives it; the file name gives
//  its full text.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::UpdateTooltip (POINT pointPx)
{
    std::wstring                          tip;
    RECT                                  anchor = {};
    int64_t                               now    = static_cast<int64_t> (GetTickCount64());
    const std::array<InspectorView *, 5>  views  = { m_platterView, m_stripView, m_sectorRow, m_nibblesTab, m_fluxTab };



    for (InspectorView * view : views)
    {
        if (tip.empty() && view != nullptr && view->IsVisible() && IsInside (view->GetBounds(), pointPx))
        {
            view->GetTooltip (pointPx, tip, anchor);
        }
    }

    if (tip.empty() && m_context.hasDisk && PtInRect (&m_fileNamePx, pointPx))
    {
        tip    = GetFileName();
        anchor = m_fileNamePx;
    }

    if (tip.empty())
    {
        m_tooltip.RequestHide (now);
    }
    else if (tip != m_tooltipText)
    {
        m_tooltip.RequestShow (anchor, tip, now);
    }

    m_tooltipText = tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::StepSector
//
//  The previous or next sector in passing order, wrapping at the index, or
//  the first or last.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::StepSector (int delta, bool isToEnd)
{
    const TrackAnalysis *  track = m_model.GetTrack();
    vector<int>            order = (track != nullptr) ? SectorRowView::GetPassingOrder (*track) : vector<int>();
    int                    count = static_cast<int> (order.size());
    int                    at    = 0;



    if (count > 0)
    {
        at = static_cast<int> (std::find (order.begin(), order.end(), m_model.GetSectorIndex()) - order.begin());
        at = isToEnd ? (delta < 0 ? 0 : count - 1) : (at + delta + count) % count;

        m_model.SelectSector (m_model.GetQuarterTrack(), order[at]);
        OnSelection();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::GetFileName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskInspectorWindow::GetFileName() const
{
    std::wstring  name;
    size_t        slash = 0;



    if (m_analysis.copy != nullptr)
    {
        name  = TextEncoding::Utf8ToWide (m_analysis.copy->fileName);
        slash = name.find_last_of (L"\\/");
        name  = (slash == std::wstring::npos) ? name : name.substr (slash + 1);
    }

    return name.empty() ? std::wstring (L"Untitled disk") : name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::IsInside
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::IsInside (const RECT & rect, POINT pointPx)
{
    return PtInRect (&rect, pointPx) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ShowDiskTab
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ShowDiskTab (int tab)
{
    m_diskTab = tab;
    m_tracksTab->SetVisible   (tab == kTabTracks);
    m_findingsTab->SetVisible (tab == kTabFindings);
    m_diskTabs->SetSelected   (tab);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::RefreshTables
//
//  The Tracks and Findings tabs after the analysis changes, and the Fields
//  tab when its track or that track's analysis changes.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::RefreshTables()
{
    const TrackAnalysis *  track = m_model.GetTrack();
    int                    qt    = m_model.GetQuarterTrack();



    if (m_isTablesDirty)
    {
        m_tracksTab->SetRows    (m_context.hasDisk ? InspectorTables::BuildTracks (m_analysis) : vector<TableRow>());
        m_tracksTab->SetCaption (m_context.hasDisk && InspectorTables::IsAlignmentNoteShown (m_analysis)
                                     ? L"This image's INFO says its tracks were not imaged in sync, so their alignment to one another was not kept"
                                     : L"");
        m_findingsTab->Refresh();
        Layout (m_boundsDip, m_scaler);
    }

    if (m_isTablesDirty || qt != m_fieldsOf || track != m_fieldsTrack)
    {
        m_fieldsTab->SetRows (m_context.hasDisk ? InspectorTables::BuildFields (m_analysis, qt) : vector<TableRow>());
        m_fieldsOf    = qt;
        m_fieldsTrack = track;
        SyncTables();
    }

    m_isTablesDirty = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SyncTables
//
//  Each table selects the row for the window's selection.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SyncTables()
{
    int  qt     = m_model.GetQuarterTrack();
    int  sector = m_model.GetSectorIndex();



    m_tracksTab->SelectRowWhere ([qt] (const TableRow & row) { return row.quarterTrack == qt; });
    m_fieldsTab->SelectRowWhere ([sector] (const TableRow & row) { return sector >= 0 && row.sectorIndex == sector; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SelectFromRow
//
//  A row goes to what it refers to: a field's nibbles, a sector, or a
//  quarter track.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SelectFromRow (const TableRow & row)
{
    const TrackAnalysis *  track = nullptr;



    if (row.quarterTrack >= 0)
    {
        m_model.SelectQuarterTrack (row.quarterTrack);
        track = m_model.GetTrack();

        if (row.sectorIndex >= 0)
        {
            m_model.SelectSector (row.quarterTrack, row.sectorIndex);
        }
        else if (track != nullptr && row.field >= 0 && row.field < static_cast<int> (track->fields.size()))
        {
            m_model.SelectNibbles (row.quarterTrack, track->fields[row.field].firstNibble, track->fields[row.field].nibbleCount);
        }

        OnSelection();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OpenDecodeSettings
//
//  The dialog opens on the settings in force for the selected track.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OpenDecodeSettings()
{
    HRESULT                   hr     = S_OK;
    DecodeSettingsDialog      dialog;
    DxuiWindow::CreateParams  params;



    dialog.Configure (m_theme, m_analysis.settings, m_model.GetQuarterTrack() / DiskImage::kQuarterTracksPerWholeTrack);

    params.title                    = L"Decode settings";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = GetHwnd();
    params.initialSizeDip           = DecodeSettingsDialog::kSizeDip;
    params.minSizeDip               = DecodeSettingsDialog::kSizeDip;
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (m_theme);
    dialog.ShowModalDialog (IDOK);

    if (dialog.GetOutcome() != DecodeSettingsDialog::Outcome::Cancelled)
    {
        ApplySettings (dialog.GetSettings());
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::ApplySettings
//
//  Every record is analyzed again under the new settings; each record's old
//  result shows until its new one arrives.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::ApplySettings (const DecodeSettings & settings)
{
    if (m_context.hasDisk && m_analysis.copy != nullptr)
    {
        m_analysis.settings = settings;
        m_scheduler.Restart (m_analysis.copy, settings);
        m_isTablesDirty = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::StepRange
//
//  The timing range steps through ±1% to ±25% (FR-024).
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::StepRange (int delta)
{
    m_rangeStep           = std::clamp (m_rangeStep + delta, 0, static_cast<int> (std::size (s_kRanges)) - 1);
    m_context.timingRange = s_kRanges[m_rangeStep];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::IsStripKey
//
////////////////////////////////////////////////////////////////////////////////

bool DiskInspectorWindow::IsStripKey (WPARAM vk)
{
    return vk == VK_OEM_PLUS || vk == VK_ADD || vk == VK_OEM_MINUS || vk == VK_SUBTRACT || vk == VK_LEFT || vk == VK_RIGHT || vk == '0' || vk == VK_NUMPAD0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OpenFind
//
//  The panel opens on the last query and its hits; a hit chosen there is
//  selected, and the hits stay for F3 either way.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OpenFind()
{
    HRESULT                   hr     = S_OK;
    FindPanel                 panel;
    DxuiWindow::CreateParams  params;



    panel.Configure (m_theme, m_context, m_model.GetQuarterTrack(), m_findQuery, m_findHits);

    params.title                    = L"Find";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = GetHwnd();
    params.initialSizeDip           = FindPanel::kSizeDip;
    params.minSizeDip               = FindPanel::kSizeDip;
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = panel.Create (params);
    CHRA (hr);

    panel.SetTheme (m_theme);
    panel.ShowModalDialog (IDOK);

    m_findQuery = panel.GetQuery();
    m_findHits  = panel.GetHits();
    m_findIndex = panel.GetChosen();

    if (m_findIndex >= 0)
    {
        SelectHit (m_findHits[m_findIndex]);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::OpenGoTo
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::OpenGoTo()
{
    HRESULT                   hr     = S_OK;
    GoToDialog                dialog;
    DxuiWindow::CreateParams  params;
    GoToTarget                target;



    dialog.Configure (m_theme, m_analysis, m_model.GetQuarterTrack(), m_goToKind);

    params.title                    = L"Go to";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = GetHwnd();
    params.initialSizeDip           = GoToDialog::kSizeDip;
    params.minSizeDip               = GoToDialog::kSizeDip;
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (m_theme);
    dialog.ShowModalDialog (IDOK);

    m_goToKind = dialog.GetKind();
    target     = dialog.GetTarget();

    if (dialog.IsChosen() && target.firstNibble >= 0)
    {
        m_model.SelectNibbles (target.quarterTrack, target.firstNibble, 1);
        OnSelection();
    }
    else if (dialog.IsChosen() && target.sectorIndex >= 0)
    {
        m_model.SelectSector (target.quarterTrack, target.sectorIndex);
        OnSelection();
    }
    else if (dialog.IsChosen())
    {
        m_model.SelectQuarterTrack (target.quarterTrack);
        OnSelection();
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::StepFind
//
//  Find next and Find previous, wrapping around the hits.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::StepFind (int step)
{
    int  count = static_cast<int> (m_findHits.size());



    if (count > 0)
    {
        m_findIndex = (m_findIndex < 0) ? (step > 0 ? 0 : count - 1) : (m_findIndex + step + count) % count;
        SelectHit (m_findHits[m_findIndex]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorWindow::SelectHit
//
//  A sector hit selects its sector; a nibble hit its nibbles, and one off
//  the framed nibbles the nibble its first cell falls in.
//
////////////////////////////////////////////////////////////////////////////////

void DiskInspectorWindow::SelectHit (const SearchHit & hit)
{
    const TrackAnalysis *  track  = nullptr;
    int                    nibble = hit.firstNibble;



    if (hit.sectorIndex >= 0)
    {
        m_model.SelectSector (hit.quarterTrack, hit.sectorIndex);
    }
    else
    {
        m_model.SelectQuarterTrack (hit.quarterTrack);
        track  = m_model.GetTrack();
        nibble = (!hit.isAligned && track != nullptr) ? PlatterGeometry::GetNibbleAt (*track, hit.cell) : nibble;
        m_model.SelectNibbles (hit.quarterTrack, nibble, hit.nibbleCount);
    }

    OnSelection();
}
