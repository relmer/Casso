#include "Pch.h"

#include "Ui/DiskInspector/Components/ComparisonPart.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/CompareDialog.h"
#include "Ui/DiskInspector/ComparisonText.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorMetrics.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/DecodeSettingsDialog.h"
#include "Ui/DiskInspector/DifferencesTab.h"
#include "Ui/DiskInspector/InspectorTableView.h"
#include "Ui/DiskInspector/TrackStripView.h"
#include "Ui/DiskInspector/Views/FilesView.h"





static constexpr LPCWSTR  s_kpszAnalyzing  = L"Analyzing";
static constexpr int      s_kBarButtonDip  = 136;
static constexpr int      s_kStopButtonDip = 116;





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::ComparisonPart
//
////////////////////////////////////////////////////////////////////////////////

ComparisonPart::ComparisonPart (InspectorSession & session, IInspectorComponentHost & host, DifferencesTab & diffsTab, TrackStripView & stripB, FilesView & files) :
    m_session  (session),
    m_host     (host),
    m_diffsTab (diffsTab),
    m_stripB   (stripB),
    m_files    (files)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::Create
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::Create()
{
    DxuiPanel &  parent = m_host.GetControlParent();



    m_prevDiff        = parent.CreateChild<DxuiButton> (L"Previous difference");
    m_nextDiff        = parent.CreateChild<DxuiButton> (L"Next difference");
    m_swapButton      = parent.CreateChild<DxuiButton> (L"Swap A and B");
    m_bSettingsButton = parent.CreateChild<DxuiButton> (L"B's decode settings...");
    m_stopButton      = parent.CreateChild<DxuiButton> (L"Stop comparing");

    m_prevDiff->SetOnClick        ([this] () { StepDifference (-1); });
    m_nextDiff->SetOnClick        ([this] () { StepDifference (1); });
    m_swapButton->SetOnClick      ([this] () { SwapSides(); });
    m_bSettingsButton->SetOnClick ([this] () { OpenBSettings(); });
    m_stopButton->SetOnClick      ([this] () { StopComparing(); });
    m_diffsTab.SetOnSelect        ([this] (const TableRow & row) { if (const Difference * d = m_diffsTab.GetDifference (row)) { m_diffIndex = row.finding; SelectDifference (*d); } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::Layout
//
//  The bar's buttons from the right; its sources fill the rest.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::Layout (const RECT & bar, const DxuiDpiScaler & scaler)
{
    int  margin = scaler.ToPx (InspectorMetrics::kMarginDip);
    int  x      = bar.right - margin;



    m_barPx = bar;

    for (DxuiButton * barButton : { m_stopButton, m_bSettingsButton, m_swapButton, m_nextDiff, m_prevDiff })
    {
        int  w = scaler.ToPx (barButton == m_stopButton ? s_kStopButtonDip : s_kBarButtonDip);

        barButton->Layout ({ x - w, bar.top + margin / 2, x, bar.bottom - margin / 2 }, scaler);
        x -= w + margin / 2;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::Update
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::Update()
{
    bool  isComparing = m_session.comparison.IsComparing();



    m_prevDiff->SetVisible        (isComparing);
    m_nextDiff->SetVisible        (isComparing);
    m_swapButton->SetVisible      (isComparing);
    m_bSettingsButton->SetVisible (isComparing);
    m_stopButton->SetVisible      (isComparing);
    m_prevDiff->SetEnabled        (!m_session.comparison.GetListed().empty());
    m_nextDiff->SetEnabled        (!m_session.comparison.GetListed().empty());
    m_swapButton->SetEnabled      (m_session.context.hasDisk && m_session.comparison.GetB().copy != nullptr);
    m_bSettingsButton->SetEnabled (m_session.comparison.GetB().copy != nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::OnDiskStarting
//
//  A comparison of the window's drive ends once its disk is ejected or
//  replaced.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::OnDiskStarting (const DiskChange & change)
{
    if (m_session.comparison.IsComparing() && m_session.IsWindowDiskA() && change.lastMediaId != 0 && change.isNewDisk)
    {
        EndComparison (std::format (L"The comparison ended. The disk in drive {} was ejected or replaced.", m_session.drive + 1));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::PaintUnder
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::PaintUnder (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (m_session.comparison.IsComparing())
    {
        PaintComparisonBar (painter, text, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::GetTip
//
//  Each bar button's tip, or for one that is unavailable, why.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ComparisonPart::GetTip (POINT pointPx, RECT & outAnchorPx) const
{
    struct ButtonTip
    {
        const IDxuiControl  * control;
        LPCWSTR               text;
        LPCWSTR               whyUnavailable;
    };

    const ButtonTip  tips[] =
    {
        { m_prevDiff,        L"Go to the previous difference",             L"There are no differences to step through" },
        { m_nextDiff,        L"Go to the next difference",                 L"There are no differences to step through" },
        { m_swapButton,      L"Make A the disk B is, and B the disk A is", L"B is still being read" },
        { m_bSettingsButton, L"Give B decode settings of its own",         L"B is still being read" },
        { m_stopButton,      L"End the comparison",                        nullptr },
    };



    std::wstring  tip;



    for (const ButtonTip & entry : tips)
    {
        RECT  bounds = entry.control->GetBounds();

        if (tip.empty() && entry.control->IsVisible() && PtInRect (&bounds, pointPx))
        {
            tip         = (entry.control->IsEnabled() || entry.whyUnavailable == nullptr) ? entry.text : entry.whyUnavailable;
            outAnchorPx = bounds;
        }
    }

    return tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::TakeComparison
//
//  Once a frame, after A's results: the comparison takes its reads and B's
//  results, passes A's disk to the window when A was read from another source, and
//  compares once both sides are analyzed. A new result rebuilds the tables.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::TakeComparison()
{
    vector<LoadedDisk>  loads;
    bool                isAReady = m_session.context.hasDisk && m_session.pendingRequest == 0 && !m_session.scheduler->HasPending();



    m_session.comparison.Tick (m_session.analysis, isAReady, loads);

    for (LoadedDisk & loaded : loads)
    {
        m_host.StartDisk (loaded.copy, loaded.reason);
    }

    m_session.context.comparison = (m_session.comparison.IsComparing() && m_session.comparison.HasResult()) ? &m_session.comparison.GetResult() : nullptr;
    m_session.context.diffCells  = (m_session.comparison.IsComparing() && !m_session.diffCells.empty()) ? &m_session.diffCells : nullptr;
    m_session.context.analysisB  = m_session.comparison.IsComparing() ? &m_session.comparison.GetB() : nullptr;

    if (m_session.comparison.GetVersion() != m_comparisonVersion)
    {
        m_comparisonVersion     = m_session.comparison.GetVersion();
        m_diffIndex             = std::min (m_diffIndex, static_cast<int> (m_session.comparison.GetListed().size()) - 1);
        m_session.isTablesDirty = true;
    }

    CheckComparedSources();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::CheckComparedSources
//
//  Ends the comparison when B is the other drive and its disk is ejected or
//  replaced. When a compared image file is deleted or renamed, a note
//  appears in the status band and the comparison is not cleared. File
//  existence is checked once a second.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::CheckComparedSources()
{
    static constexpr int64_t  kCheckMs = 1000;



    int64_t                   now   = static_cast<int64_t> (GetTickCount64());
    const ComparisonSource  & b     = m_session.comparison.GetSourceB();
    uint64_t                  media = m_session.comparison.GetB().mediaId;
    std::error_code           error;



    if (!m_session.comparison.IsComparing())
    {
        return;
    }

    if (b.kind == ComparisonSourceKind::DriveNow && b.drive != m_session.drive && media != 0)
    {
        std::shared_ptr<const DriveHeadState>  state = m_session.host->GetHeadState (b.drive);

        if (state != nullptr && (!state->hasDisk || state->mediaId != media))
        {
            EndComparison (std::format (L"The comparison ended. The disk in drive {} was ejected or replaced.", b.drive + 1));
        }
    }

    if (m_session.comparison.IsComparing() && now - m_sourceCheckMs >= kCheckMs)
    {
        m_sourceCheckMs = now;

        for (const ComparisonSource & source : { m_session.sourceA, b })
        {
            if (source.kind == ComparisonSourceKind::ImageFile && !source.path.empty() && !std::filesystem::exists (source.path, error))
            {
                m_host.SetStatusNote (L"The compared file " + std::filesystem::path (source.path).filename().wstring() + L" is gone. This comparison is from before it was removed.");
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::EndComparison
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::EndComparison (const std::wstring & why)
{
    StopComparing();
    m_host.SetStatusNote (why);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::SyncSideB
//
//  Once a frame while comparing: B's view of the selected quarter track,
//  the nibble hunks and alignment of that track (again whenever either
//  side's record changes), and the two strips' zoom and pan linked, the
//  strip moved since the last frame leading (FR-121).
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::SyncSideB()
{
    const DiskAnalysis &   b        = m_session.comparison.GetB();
    int                    qt       = m_session.model.GetQuarterTrack();
    const TrackAnalysis *  trackA   = m_session.context.hasDisk ? m_session.model.GetTrack() : nullptr;
    const TrackAnalysis *  trackB   = nullptr;
    bool                   isShown  = m_session.comparison.IsComparing() && b.copy != nullptr;
    bool                   isBMoved = false;
    int                    rotation = 0;



    m_session.contextB.palette      = m_session.context.palette;
    m_session.contextB.isTimingMode = m_session.context.isTimingMode;
    m_session.contextB.timingRange  = m_session.context.timingRange;
    m_session.contextB.hasDisk      = isShown;
    m_session.contextB.analysis     = &b;
    m_session.contextB.comparison   = m_session.context.comparison;

    if (isShown)
    {
        if (m_modelBDisk != b.mediaId)
        {
            m_session.modelB.SetDisk (b.mediaId);
            m_modelBDisk = b.mediaId;
            m_linkStartB = -1.0;
        }

        m_session.modelB.SetAnalysis (&b);

        if (m_session.modelB.GetQuarterTrack() != qt)
        {
            m_session.modelB.SelectQuarterTrack (qt);
        }

        trackB = m_session.modelB.GetTrack();
    }

    if (trackA != m_diffsOfA || trackB != m_diffsOfB)
    {
        m_nibbleDiffs.clear();
        m_bOffset = 0.0;

        if (trackA != nullptr && trackB != nullptr && !trackA->framed.nibbles.empty() && !trackB->framed.nibbles.empty())
        {
            DiskComparer::ListNibbles (*trackA, *trackB, qt, m_nibbleDiffs);

            rotation  = DiskComparer::Align (*trackA, *trackB);
            m_bOffset = TrackAnalyzer::GetAngle (*trackB, trackB->framed.nibbles[static_cast<size_t> (rotation) % trackB->framed.nibbles.size()].startCell) -
                        TrackAnalyzer::GetAngle (*trackA, trackA->framed.nibbles[0].startCell);
        }

        m_diffsOfA   = trackA;
        m_diffsOfB   = trackB;
        m_linkStartB = -1.0;
    }

    m_session.context.trackB       = trackB;
    m_session.context.nibbleDiffs  = (trackB != nullptr) ? &m_nibbleDiffs : nullptr;
    m_session.contextB.nibbleDiffs = m_session.context.nibbleDiffs;

    if (isShown)
    {
        isBMoved = m_linkStartB >= 0.0 && (m_session.modelB.GetStripStart() != m_linkStartB || m_session.modelB.GetStripSpan() != m_linkSpanB);

        if (isBMoved)
        {
            m_session.model.SetStrip (m_session.modelB.GetStripStart() - m_bOffset, m_session.modelB.GetStripSpan());
        }
        else
        {
            m_session.modelB.SetStrip (m_session.model.GetStripStart() + m_bOffset, m_session.model.GetStripSpan());
        }

        m_linkStartB = m_session.modelB.GetStripStart();
        m_linkSpanB  = m_session.modelB.GetStripSpan();
    }

    m_stripB.SetVisible (isShown);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::OpenCompare
//
//  "Compare with..." (FR-117): the dialog returns A's and B's sources. When
//  A is this window's drive, its disk's analysis is reused, or the disk is
//  read again after a swap; any other A is read by the comparison and drawn
//  in its place.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::OpenCompare()
{
    HRESULT                   hr      = S_OK;
    CompareDialog             dialog;
    DxuiWindow::CreateParams  params;
    ComparisonSource          b       = m_session.comparison.IsComparing() ? m_session.comparison.GetSourceB() : ComparisonSource { ComparisonSourceKind::ImageFile, 0 };
    ComparisonSource          own     = { ComparisonSourceKind::DriveNow, m_session.drive };
    bool                      isSame  = false;



    dialog.Configure (m_session.theme, std::max (1, m_session.host->GetDriveCount()), m_session.sourceA, b);

    params.title                    = L"Compare with";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = m_host.GetOwnerHwnd();
    params.initialSizeDip           = CompareDialog::kSizeDip;
    params.minSizeDip               = CompareDialog::kSizeDip;
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (m_session.theme);
    dialog.ShowModalDialog (IDOK);
    BAIL_OUT_IF (!dialog.IsChosen(), S_OK);

    isSame    = dialog.GetSource (ComparisonSession::kSideA) == m_session.sourceA && m_session.context.hasDisk;
    m_session.sourceA = dialog.GetSource (ComparisonSession::kSideA);
    m_diffIndex = -1;

    m_session.comparison.Begin (m_session.sourceA, dialog.GetSource (ComparisonSession::kSideB), !isSame && !(m_session.sourceA == own), m_session.analysis.settings, *m_session.host);

    if (!isSame && m_session.sourceA == own)
    {
        m_host.RequestCopy();
    }

    m_session.isTablesDirty = true;
    m_host.ShowDiskTab (kTabDifferences);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::StopComparing
//
//  The window returns to the drive's disk, read again when A was another.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::StopComparing()
{
    bool  isOwn = m_session.IsWindowDiskA();



    m_session.comparison.End();
    m_session.diffCells.clear();
    m_session.context.comparison = nullptr;
    m_session.context.analysisB  = nullptr;
    m_diffIndex                  = -1;
    m_session.isTablesDirty      = true;

    if (!isOwn)
    {
        (void) m_host.ShowDrive (m_session.drive);
    }

    m_host.ShowDiskTab (m_session.diskTab == kTabDifferences ? kTabTracks : m_session.diskTab);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::SwapSides
//
//  "Swap A and B": the views switch to what was B, and its rings are built anew.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::SwapSides()
{
    int  slot = 0;



    m_session.comparison.Swap (m_session.analysis, m_session.scheduler, m_session.sourceA);

    m_session.context.hasDisk = m_session.analysis.copy != nullptr;
    m_session.model.SetDisk     (m_session.analysis.mediaId);
    m_session.model.SetAnalysis (m_session.context.hasDisk ? &m_session.analysis : nullptr);
    m_session.levels.fill (nullptr);
    m_session.timingLevels.fill (nullptr);
    m_session.changedSlots.clear();

    for (slot = 0; slot < static_cast<int> (m_session.analysis.tracks.size()); slot++)
    {
        m_session.changedSlots.insert (slot);
    }

    m_diffIndex             = -1;
    m_session.isTablesDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::OpenBSettings
//
//  Separate decode settings for B (FR-117); a later change to the window's
//  settings doesn't change them.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::OpenBSettings()
{
    HRESULT                   hr     = S_OK;
    DecodeSettingsDialog      dialog;
    DxuiWindow::CreateParams  params;



    dialog.Configure (m_session.theme, m_session.comparison.GetB().settings, m_session.model.GetQuarterTrack() / DiskImage::kQuarterTracksPerWholeTrack);

    params.title                    = L"B's decode settings";
    params.hInstance                = GetModuleHandle (nullptr);
    params.ownerHwnd                = m_host.GetOwnerHwnd();
    params.initialSizeDip           = DecodeSettingsDialog::kSizeDip;
    params.minSizeDip               = DecodeSettingsDialog::kSizeDip;
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);
    CHRA (hr);

    dialog.SetTheme (m_session.theme);
    dialog.ShowModalDialog (IDOK);

    if (dialog.GetOutcome() != DecodeSettingsDialog::Outcome::Cancelled)
    {
        m_session.comparison.ApplyOwnSettings (dialog.GetSettings());
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::StepDifference
//
//  "Next difference" and "Previous difference" step through the listed
//  differences across the disk, wrapping at either end (FR-121).
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::StepDifference (int step)
{
    const vector<Difference> &  listed = m_session.comparison.GetListed();
    int                         count  = static_cast<int> (listed.size());



    if (count > 0)
    {
        m_diffIndex = (m_diffIndex < 0) ? (step > 0 ? 0 : count - 1) : (m_diffIndex + step + count) % count;

        SelectDifference (listed[m_diffIndex]);
        m_diffsTab.SelectRowWhere ([this] (const TableRow & row) { return row.finding == m_diffIndex; });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::SelectDifference
//
//  A difference goes to what it is about on A: its nibbles, its sector, its
//  quarter track, or its file in the file map.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::SelectDifference (const Difference & difference)
{
    const TrackAnalysis *  track  = nullptr;
    const FileMap *        map    = m_session.GetFileMap();
    int                    sector = -1;
    int                    file   = -1;
    size_t                 i      = 0;



    m_session.diffCells.clear();

    if (difference.quarterTrack >= 0)
    {
        m_session.model.SelectQuarterTrack (difference.quarterTrack);
        track = m_session.model.GetTrack();

        for (i = 0; track != nullptr && sector < 0 && difference.sector >= 0 && i < track->sectors.size(); i++)
        {
            sector = (track->sectors[i].sector == difference.sector) ? static_cast<int> (i) : -1;
        }

        if (difference.firstNibbleA >= 0 && difference.nibbleCountA > 0)
        {
            m_session.model.SelectNibbles (difference.quarterTrack, difference.firstNibbleA, difference.nibbleCountA);
        }
        else if (sector >= 0)
        {
            m_session.model.SelectSector (difference.quarterTrack, sector);
        }

        m_host.OnSelection();
    }
    else if (!difference.path.empty() && map != nullptr)
    {
        for (i = 0; file < 0 && i < map->files.size(); i++)
        {
            file = (map->files[i].path == difference.path && !map->files[i].isDeleted) ? static_cast<int> (i) : -1;
        }

        //  A file pair marks A's sectors that differ from B's (FR-120).
        for (const FilePair & pair : m_session.comparison.GetResult().files)
        {
            m_session.diffCells = (file >= 0 && pair.fileA == file) ? pair.differingCells : m_session.diffCells;
        }

        if (file >= 0)
        {
            m_files.ChooseFile (file);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::PaintComparisonBar
//
//  A's and B's sources and the names of their files (FR-121), with the reason
//  a side could not be read in place of its file's name.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::PaintComparisonBar (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float             margin = m_session.scaler.ToPxf (static_cast<float> (InspectorMetrics::kMarginDip));
    float             left   = static_cast<float> (m_barPx.left) + margin;
    float             right  = static_cast<float> (m_prevDiff->GetBounds().left) - margin;
    float             half   = std::max (0.0f, (right - left) / 2.0f);
    float             height = static_cast<float> (m_barPx.bottom - m_barPx.top);
    const DiskCopy  * copyB  = m_session.comparison.GetB().copy.get();
    std::wstring      nameB  = (copyB != nullptr) ? std::filesystem::path (TextEncoding::Utf8ToWide (copyB->fileName)).filename().wstring() : std::wstring();
    std::wstring   sideA  = std::format (L"A  {}  {}  {}", ComparisonText::FormatSource (m_session.comparison.GetSourceA()), s_kpszMiddleDot,
                                         m_session.context.hasDisk ? m_session.GetFileName() : m_session.comparison.GetAError());
    std::wstring   sideB  = std::format (L"B  {}  {}  {}", ComparisonText::FormatSource (m_session.comparison.GetSourceB()), s_kpszMiddleDot,
                                         m_session.comparison.GetBError().empty() ? (nameB.empty() ? std::wstring (s_kpszAnalyzing) : nameB) : m_session.comparison.GetBError());



    painter.FillRect (static_cast<float> (m_barPx.left), static_cast<float> (m_barPx.top), static_cast<float> (m_barPx.right - m_barPx.left), height,
                      theme.SelectionBackground());

    text.DrawString (sideA.c_str(), left, static_cast<float> (m_barPx.top), half, height, theme.Foreground(), m_session.scaler.ToPxf (InspectorView::kTextDip),
                     DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    text.DrawString (sideB.c_str(), left + half, static_cast<float> (m_barPx.top), half, height,
                     m_session.comparison.GetBError().empty() ? theme.Foreground() : theme.ErrorForeground(), m_session.scaler.ToPxf (InspectorView::kTextDip),
                     DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
}
