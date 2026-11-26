#include "Pch.h"

#include "Ui/DiskInspector/Components/ComparisonPart.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Devices/Disk/Inspector/DiskCopy.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/CompareDialog.h"
#include "Ui/DiskInspector/ComparisonText.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorMetrics.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/DecodeSettingsDialog.h"
#include "Ui/DiskInspector/Views/DifferencesView.h"
#include "Ui/DiskInspector/InspectorMarquee.h"
#include "Ui/DiskInspector/OrphanFields.h"
#include "Ui/DiskInspector/InspectorTableView.h"
#include "Ui/DiskInspector/TrackStripView.h"
#include "Ui/DiskInspector/Views/FilesView.h"





static constexpr LPCWSTR  s_kpszAnalyzing  = L"Analyzing";
static constexpr LPCWSTR  s_kpszComparing  = L"Comparing";
static constexpr int      s_kBarButtonDip  = 136;
static constexpr int      s_kStopButtonDip = 116;
static constexpr double   s_kStripFill     = 0.75;
static constexpr int64_t  s_kSectorsNoteMs = 20000;    // the sector data infobar's time up, at least





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::ComparisonPart
//
////////////////////////////////////////////////////////////////////////////////

ComparisonPart::ComparisonPart (InspectorSession & session, IInspectorComponentHost & host, DifferencesView & diffsTab, TrackStripView & stripB, FilesView & files) :
    m_session  (session),
    m_host     (host),
    m_diffsView (diffsTab),
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
    m_swapButton      = parent.CreateChild<DxuiButton> (L"");
    m_stopButton      = parent.CreateChild<DxuiButton> (L"Stop comparing");

    m_prevDiff->SetOnClick        ([this] () { StepDifference (-1); });
    m_nextDiff->SetOnClick        ([this] () { StepDifference (1); });
    m_swapButton->SetOnClick      ([this] () { SwapSides(); });
    m_stopButton->SetOnClick      ([this] () { StopComparing(); });
    m_diffsView.SetOnSelect        ([this] (const TableRow & row) { if (const Difference * d = m_diffsView.GetDifference (row)) { m_diffIndex = row.finding; SelectDifference (*d); } });
    m_diffsView.SetOnActivate      ([this] (const TableRow & row) { if (const Difference * d = m_diffsView.GetDifference (row)) { m_diffIndex = row.finding; SelectDifference (*d); } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::Layout
//
//  The bar's buttons from the right; its sources fill the rest. The
//  infobar, while shown, is in the row under the bar.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::Layout (const RECT & bar, const RECT & notice, const DxuiDpiScaler & scaler)
{
    int  margin = scaler.ToPx (InspectorMetrics::kMarginDip);
    int  x      = bar.right - margin;



    m_barPx = bar;

    for (DxuiButton * barButton : { m_stopButton })
    {
        int  w = scaler.ToPx (barButton == m_stopButton ? s_kStopButtonDip : s_kBarButtonDip);

        barButton->Layout ({ x - w, bar.top + margin / 2, x, bar.bottom - margin / 2 }, scaler);
        x -= w + margin / 2;
    }

    m_sectorsNote.Layout (notice, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::GetNoticeHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int ComparisonPart::GetNoticeHeightPx (int widthPx, const DxuiDpiScaler & scaler) const
{
    int  height = 0;



    if (m_sectorsNote.IsVisible())
    {
        height = scaler.ToPx (InspectorMetrics::kMarginDip) + static_cast<int> (std::ceil (m_sectorsNote.GetPreferredHeightPx (static_cast<float> (widthPx), scaler)));
    }

    return height;
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
    m_stopButton->SetVisible      (isComparing);
    m_prevDiff->SetEnabled        (!m_session.comparison.GetListed().empty());
    m_nextDiff->SetEnabled        (!m_session.comparison.GetListed().empty());
    m_swapButton->SetEnabled      (m_session.context.hasDisk && m_session.comparison.GetB().copy != nullptr);
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
        EndComparison (m_session.host->GetDriveCount() == 0 ? std::wstring (L"The comparison ended. The image file changed.")
                                                            : std::format (L"The comparison ended. The disk in drive {} was ejected or replaced.", m_session.drive + 1));
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
//  ComparisonPart::PaintOver
//
//  While both strips are shown: a border around the side that has focus,
//  and one line at the pointer's position across both strips.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::PaintOver (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    const InspectorView *  strips[] = { m_session.views.strip, &m_stripB };
    const InspectorView *  focused  = strips[m_focusedSide];
    RECT                   bounds   = focused->GetBounds();
    float                  ring     = std::max (1.0f, m_session.scaler.ToPxf (theme.FocusRingWidthDip()));



    if (m_session.comparison.IsComparing() && m_swapButton->IsVisible())
    {
        PaintSwapArrow (painter, theme);
    }

    PaintSectorsNote (painter, text, theme);

    if (!m_session.comparison.IsComparing() || !m_stripB.IsVisible())
    {
        return;
    }

    painter.OutlineRect (static_cast<float> (bounds.left), static_cast<float> (bounds.top), static_cast<float> (bounds.right - bounds.left),
                         static_cast<float> (bounds.bottom - bounds.top), ring, theme.FocusAccent());

    //  Each strip's image name, at its right end above the bar, on a fill
    //  that covers the strip's labels under it.
    painter.FlushLayer();

    for (int side = 0; side < 2; side++)
    {
        std::wstring  name   = GetImageName (side);
        float         size   = m_session.scaler.ToPxf (InspectorView::kTextDip);
        float         pad    = m_session.scaler.ToPxf (4.0f);
        float         w      = 0.0f;
        float         h      = 0.0f;
        RECT          strip  = strips[side]->GetBounds();
        float         x      = 0.0f;
        float         y      = static_cast<float> (strip.top) + ring;

        (void) text.MeasureStringWeighted (name.c_str(), size, DxuiTheme::kBodyFace, DxuiFontWeight::Bold, w, h);

        x = static_cast<float> (strip.right) - w - pad * 3.0f;

        painter.FillRect (x - pad, y, w + pad * 2.0f, h, theme.Background());
        text.DrawString (name.c_str(), x, y, w + 1.0f, h, side == m_focusedSide ? theme.Accent() : theme.Foreground(), size, DxuiTheme::kBodyFace,
                         DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Bold, false);
    }

    if (m_cursorX >= 0.0f)
    {
        for (const InspectorView * strip : strips)
        {
            bounds = strip->GetBounds();
            painter.DrawLine (m_cursorX, static_cast<float> (bounds.top), m_cursorX, static_cast<float> (bounds.bottom), m_session.scaler.ToPxf (1.0f), theme.Accent());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::PaintSectorsNote
//
//  The sector data infobar in its row under the comparison bar.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::PaintSectorsNote (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (m_sectorsNote.IsVisible())
    {
        m_sectorsNote.Paint (painter, text, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::PaintSwapArrow
//
//  A double-ended arrow drawn across the swap button, larger than the font's
//  glyph.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::PaintSwapArrow (IDxuiPainter & painter, const IDxuiTheme & theme)
{
    RECT      bounds = m_swapButton->GetBounds();
    float     cx     = (bounds.left + bounds.right) / 2.0f;
    float     cy     = (bounds.top + bounds.bottom) / 2.0f;
    float     half   = (bounds.right - bounds.left) * 0.26f;
    float     head   = (bounds.bottom - bounds.top) * 0.187f;
    float     line   = m_session.scaler.ToPxf (1.5f);
    uint32_t  color  = m_swapButton->IsEnabled() ? theme.Foreground() : theme.ForegroundDisabled();



    painter.DrawLine (cx - half, cy, cx + half, cy, line, color);
    painter.DrawLine (cx - half, cy, cx - half + head, cy - head, line, color);
    painter.DrawLine (cx - half, cy, cx - half + head, cy + head, line, color);
    painter.DrawLine (cx + half, cy, cx + half - head, cy - head, line, color);
    painter.DrawLine (cx + half, cy, cx + half - head, cy + head, line, color);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::OnMouse
//
//  A press in either strip moves focus to that side, and a press anywhere
//  else moves it to the window's image. The pointer's position in either strip
//  is the cursor of both. Only a click on the sector data infobar is
//  consumed.
//
////////////////////////////////////////////////////////////////////////////////

bool ComparisonPart::OnMouse (const DxuiMouseEvent & ev)
{
    RECT   boundsA  = m_session.views.strip->GetBounds();
    RECT   boundsB  = m_stripB.GetBounds();
    bool   isOnB    = m_stripB.IsVisible() && PtInRect (&boundsB, ev.positionDip);
    bool   isOnA    = PtInRect (&boundsA, ev.positionDip) != FALSE;
    float  cursor   = (isOnA || isOnB) ? static_cast<float> (ev.positionDip.x) : -1.0f;



    if (m_sectorsNote.IsVisible() && m_sectorsNote.OnMouse (ev))
    {
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down)
    {
        m_focusedSide = isOnB ? ComparisonSession::kSideB : ComparisonSession::kSideA;
    }

    m_cursorX = (ev.kind == DxuiMouseEventKind::Leave) ? -1.0f : cursor;

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::GetPrefs
//
//  A window maximized for a comparison is saved at the size it had before,
//  so an inspector opened later outside a comparison isn't maximized.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::GetPrefs (DiskInspectorPrefs & inOut) const
{
    if (m_isMaximizedHere)
    {
        inOut.isMaximized = false;
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

    std::wstring     nameB       = GetImageName (ComparisonSession::kSideB);
    std::wstring     reading     = nameB + L" is still being read";
    const ButtonTip  tips[] =
    {
        { m_prevDiff,        L"Go to the previous difference",          L"There are no differences to step through" },
        { m_nextDiff,        L"Go to the next difference",              L"There are no differences to step through" },
        { m_swapButton,      L"Swap sides",                             reading.c_str() },
        { m_stopButton,      L"End the comparison",                     nullptr },
    };



    static constexpr size_t  kWrapChars = 50;



    std::wstring  tip;
    std::wstring  pathA   = GetPath (ComparisonSession::kSideA);
    std::wstring  pathB   = GetPath (ComparisonSession::kSideB);
    RECT          names   = { m_barPx.left, m_barPx.top, m_labelPx[ComparisonSession::kSideB].right, m_barPx.bottom };
    RECT          swap    = m_swapButton->GetBounds();
    bool          isWraps = pathA.size() > kWrapChars || pathB.size() > kWrapChars;



    //  Both images' full paths, one per line, since two images can have the
    //  same file name in different folders. A blank line separates paths
    //  long enough to wrap.
    if (m_session.comparison.IsComparing() && PtInRect (&names, pointPx) && !PtInRect (&swap, pointPx))
    {
        tip         = pathA + (isWraps ? L"\n\n" : L"\n") + pathB;
        outAnchorPx = names;
    }

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

    //  The first difference is selected once the first result arrives.
    if (!m_isFirstShown && m_session.comparison.HasResult() && !m_session.comparison.GetListed().empty())
    {
        m_isFirstShown = true;
        StepDifference (1);
    }

    DefaultFilesOverlay();
    CheckComparedSources();
    UpdateSectorsNote();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::UpdateSectorsNote
//
//  Once per comparison of a sector image with a nibble image, an infobar
//  docked under the comparison bar for the default duration: only sector
//  data is compared. The window is laid out again whenever it appears or
//  goes.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::UpdateSectorsNote()
{
    int64_t           now         = static_cast<int64_t> (GetTickCount64());
    const DiskCopy  * copies[]    = { m_session.context.hasDisk ? m_session.analysis.copy.get() : nullptr, m_session.comparison.GetB().copy.get() };
    bool              isSector[2] = {};
    bool              wasShown    = m_sectorsNote.IsVisible();
    int               side        = 0;



    if (!m_session.comparison.IsComparing())
    {
        m_sectorsNote.Dismiss();
    }

    if (m_session.comparison.IsComparing() && !m_isSectorsNoted && m_session.comparison.HasResult() && m_session.comparison.GetResult().isSectorsOnly && copies[0] != nullptr && copies[1] != nullptr)
    {
        for (side = 0; side < 2; side++)
        {
            isSector[side] = copies[side]->format == DiskFormat::Dsk || copies[side]->format == DiskFormat::Do || copies[side]->format == DiskFormat::Po;
        }

        m_isSectorsNoted = true;

        if (isSector[0] != isSector[1])
        {
            std::wstring  extension = std::filesystem::path (GetPath (isSector[0] ? 0 : 1)).extension().wstring();

            std::transform (extension.begin(), extension.end(), extension.begin(), ::towlower);

            m_sectorsNote.SetSeverity    (DxuiInfoBar::Severity::Info);
            m_sectorsNote.SetDurationMs  (std::max (s_kSectorsNoteMs, DxuiInfoBar::ResolveDefaultDurationMs()));
            m_sectorsNote.SetDismissible (true);
            m_sectorsNote.SetOnDismiss   ([this] () { m_sectorsNote.Dismiss(); m_host.Relayout(); });
            m_sectorsNote.Show           (std::format (L"Comparing sector data only because {} files do not contain sync, gaps, volume numbers, or timing information.", extension), now);
        }
    }

    m_sectorsNote.Tick (now);

    if (m_sectorsNote.IsVisible() != wasShown)
    {
        m_host.Relayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::DefaultFilesOverlay
//
//  A comparison turns the Files overlay on once either image has a
//  recognized file system. After that, the overlay is on or off as the user sets it.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::DefaultFilesOverlay()
{
    const vector<FileMap> &  mapsB  = m_session.comparison.GetB().fileMaps;
    const FileMap *          mapA   = m_session.GetFileMap();
    bool                     hasFsA = mapA != nullptr && mapA->notMapped == NotMappedReason::None;
    bool                     hasFsB = !mapsB.empty() && mapsB.front().notMapped == NotMappedReason::None;



    if (m_session.comparison.IsComparing() && !m_isFilesDefaulted && (hasFsA || hasFsB))
    {
        m_session.context.isFilesOverlay = true;
        m_isFilesDefaulted               = true;
    }
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
            m_modelBDisk  = b.mediaId;
            m_modelBTrack = nullptr;
            m_linkStartB  = -1.0;
            m_session.modelB.SetAnalysis (&b);
        }

        //  Only when B's analysis of the track changes: taking it again
        //  every frame would select B's first sector whenever B has nothing
        //  selected, as when it lacks A's sector, and A would follow.
        if (m_session.modelB.GetTrack() != m_modelBTrack)
        {
            m_session.modelB.SetAnalysis (&b);
            m_modelBTrack = m_session.modelB.GetTrack();
        }

        //  B follows A to a new track. The selection that comes with the new
        //  track on B is not one made on B, so A's selection leads.
        if (m_session.modelB.GetQuarterTrack() != qt)
        {
            m_session.modelB.SelectQuarterTrack (qt);
            m_modelBTrack = m_session.modelB.GetTrack();
            m_linkedSecB  = m_session.modelB.GetSectorIndex();
            m_linkedSelB  = { m_session.modelB.GetFirstNibble(), m_session.modelB.GetNibbleCount() };
            m_linkedSecA  = -2;
            m_linkedSelA  = { -2, -2 };
        }

        trackB = m_session.modelB.GetTrack();
    }

    if (trackA != m_diffsOfA || trackB != m_diffsOfB)
    {
        m_nibbleDiffs.clear();
        m_bOffset  = 0.0;
        m_rotation = 0;

        if (trackA != nullptr && trackB != nullptr && !trackA->framed.nibbles.empty() && !trackB->framed.nibbles.empty())
        {
            DiskComparer::ListNibbles (*trackA, *trackB, qt, m_nibbleDiffs);

            rotation   = DiskComparer::Align (*trackA, *trackB);
            m_rotation = rotation;
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

        LinkSelections (trackA, trackB);
    }

    m_stripB.SetVisible (isShown);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::LinkSelections
//
//  A run of nibbles selected in either strip is selected in the other at
//  the aligned nibbles, the strip changed since the last frame leading.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::LinkSelections (const TrackAnalysis * trackA, const TrackAnalysis * trackB)
{
    InspectorViewModel &  a      = m_session.model;
    InspectorViewModel &  b      = m_session.modelB;
    int                   qt     = a.GetQuarterTrack();
    int                   sizeA  = (trackA != nullptr) ? static_cast<int> (trackA->framed.nibbles.size()) : 0;
    int                   sizeB  = (trackB != nullptr) ? static_cast<int> (trackB->framed.nibbles.size()) : 0;
    std::pair<int, int>   selA   = { a.GetFirstNibble(), a.GetNibbleCount() };
    std::pair<int, int>   selB   = { b.GetFirstNibble(), b.GetNibbleCount() };
    bool                  isB    = false;
    int                   sector = -1;



    //  The selected sector, by its number, the side changed since the last
    //  frame leading. A side without that sector has nothing selected, so no
    //  other sector looks like the match. A run of nibbles is linked by its
    //  nibbles below instead: selecting the whole sector on the other side
    //  would move both strips to the sector's start.
    if (trackA != nullptr && trackB != nullptr && b.GetSectorIndex() != m_linkedSecB && b.GetSector() != nullptr && b.GetNibbleCount() == 0)
    {
        sector = FindSector (*trackA, b.GetSector()->sector);

        if (sector >= 0)
        {
            a.SelectSector (qt, sector);
        }
        else
        {
            a.ClearSelection();
        }

        isB = true;
    }
    else if (trackA != nullptr && trackB != nullptr && a.GetSectorIndex() != m_linkedSecA && a.GetSector() != nullptr && a.GetNibbleCount() == 0)
    {
        sector = FindSector (*trackB, a.GetSector()->sector);

        if (sector >= 0)
        {
            b.SelectSector (qt, sector);
        }
        else
        {
            b.ClearSelection();
        }
    }

    if (sizeA > 0 && sizeB > 0 && selB != m_linkedSelB && selB.second > 0)
    {
        a.SelectNibbles (qt, ((selB.first - m_rotation) % sizeA + sizeA) % sizeA, std::min (selB.second, sizeA));
        isB = true;
    }
    else if (sizeA > 0 && sizeB > 0 && selA != m_linkedSelA && selA.second > 0)
    {
        b.SelectNibbles (qt, (selA.first + m_rotation) % sizeB, std::min (selA.second, sizeB));
    }

    //  Recorded after the nibbles are linked: a sector that linking the
    //  nibbles selects on the other side is not a selection made there.
    m_linkedSecA = a.GetSectorIndex();
    m_linkedSecB = b.GetSectorIndex();
    m_linkedSelA = { a.GetFirstNibble(), a.GetNibbleCount() };
    m_linkedSelB = { b.GetFirstNibble(), b.GetNibbleCount() };

    //  A selection made on B's strip is shown on A's views, and the platter
    //  flies to it as it does for one made on A's strip.
    if (isB)
    {
        m_linkedSecA = a.GetSectorIndex();
        m_host.OnSelection();
        a.StartFlight (InspectorMarquee::GetClockMs());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::ShowSelectionOnStrip
//
//  The strip centered on the selection, zoomed so it fills kStripFill of
//  the strip, but no further in than the mouse wheel zooms.
//  B's strip follows A's.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::ShowSelectionOnStrip()
{
    InspectorViewModel &  a     = m_session.model;
    double                end   = 0;
    double                start = a.GetSelectionTurn (end);
    double                span  = 0;



    if (start < 0)
    {
        return;
    }

    end  = (end < start) ? end + 1.0 : end;
    span = std::max ((end - start) / s_kStripFill, m_session.views.strip->GetMinSpan());
    a.SetStrip ((start + end) / 2 - span / 2, span);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::FindSector
//
//  The index of the track's sector with this number, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int ComparisonPart::FindSector (const TrackAnalysis & track, int sector)
{
    int  found = -1;
    int  i     = 0;



    for (i = 0; found < 0 && i < static_cast<int> (track.sectors.size()); i++)
    {
        found = (track.sectors[i].sector == sector) ? i : -1;
    }

    return found;
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



    dialog.Configure (m_session.theme, m_session.host->GetDriveCount(), m_session.sourceA, b);

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

    StartComparison (dialog.GetSource (ComparisonSession::kSideA), dialog.GetSource (ComparisonSession::kSideB));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::CompareWithFile
//
//  Casso Explorer's "Compare disk images" (FR-117): the window's image as A
//  and another image file as B, without the dialog.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::CompareWithFile (const std::wstring & path)
{
    StartComparison ({ ComparisonSourceKind::DriveNow, m_session.drive }, { ComparisonSourceKind::ImageFile, 0, path });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::StartComparison
//
//  When A is the window's disk, its analysis is reused, or the disk is read
//  again when A was another source. Any other A is read by the comparison.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::StartComparison (const ComparisonSource & a, const ComparisonSource & b)
{
    ComparisonSource  windowDisk = { ComparisonSourceKind::DriveNow, m_session.drive };
    bool              isSame     = a == m_session.sourceA && m_session.context.hasDisk;
    bool              isWindowA  = a == windowDisk;



    if (!m_session.comparison.IsComparing())
    {
        MaximizeForCompare();
    }

    m_session.sourceA                = a;
    m_session.context.isDiffsOverlay = true;
    m_diffIndex                      = -1;
    m_focusedSide                    = ComparisonSession::kSideA;
    m_isFilesDefaulted               = false;
    m_isFirstShown                   = false;
    m_isSectorsNoted                 = false;

    m_session.comparison.Begin (a, b, !isSame && !isWindowA, m_session.analysis.settings, *m_session.host);

    if (!isSame && isWindowA)
    {
        m_host.RequestCopy();
    }

    m_session.isTablesDirty = true;
    m_host.ShowDiskTab (kTabDifferences);
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
    m_cursorX                    = -1.0f;
    m_session.isTablesDirty      = true;

    RestoreAfterCompare();

    if (!isOwn)
    {
        (void) m_host.ShowDrive (m_session.drive);
    }

    m_host.ShowDiskTab (m_session.diskTab == kTabDifferences ? kTabTracks : m_session.diskTab);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::MaximizeForCompare
//
//  A comparison has two of everything on screen, so the window is maximized while
//  it lasts. A window that was maximized already is left as it is.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::MaximizeForCompare()
{
    HWND  hwnd = m_host.GetOwnerHwnd();



    if (hwnd != nullptr && !IsZoomed (hwnd))
    {
        ShowWindow (hwnd, SW_MAXIMIZE);
        m_isMaximizedHere = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::RestoreAfterCompare
//
//  The window returns to its size from before the comparison, unless the
//  user restored or resized it in the meantime.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::RestoreAfterCompare()
{
    HWND  hwnd = m_host.GetOwnerHwnd();



    if (m_isMaximizedHere && hwnd != nullptr && IsZoomed (hwnd))
    {
        ShowWindow (hwnd, SW_RESTORE);
    }

    m_isMaximizedHere = false;
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
    int                 slot   = 0;
    InspectorSelection  selA   = m_session.model.GetSelection();
    InspectorSelection  selB   = m_session.modelB.GetSelection();
    double              startA = m_session.model.GetStripStart();
    double              spanA  = m_session.model.GetStripSpan();
    double              startB = m_session.modelB.GetStripStart();
    double              spanB  = m_session.modelB.GetStripSpan();



    m_session.comparison.Swap (m_session.analysis, m_session.scheduler, m_session.sourceA);

    //  Each side's view moves to the other image, so the zoom, the track and
    //  the place on it remain where they were.
    m_session.context.hasDisk = m_session.analysis.copy != nullptr;
    m_session.model.Rebind      (m_session.analysis.mediaId);
    m_session.model.SetAnalysis (m_session.context.hasDisk ? &m_session.analysis : nullptr);
    m_session.model.SetStrip    (startB, spanB);
    m_session.model.RestoreSelection (selB);

    m_modelBDisk = m_session.comparison.GetB().mediaId;
    m_session.modelB.Rebind      (m_modelBDisk);
    m_session.modelB.SetAnalysis (&m_session.comparison.GetB());
    m_session.modelB.SetStrip    (startA, spanA);
    m_session.modelB.RestoreSelection (selA);

    std::swap (m_linkedSelA, m_linkedSelB);
    std::swap (m_linkedSecA, m_linkedSecB);
    m_focusedSide = 1 - m_focusedSide;
    m_diffsOfA    = nullptr;
    m_diffsOfB    = nullptr;

    m_session.levels.fill (nullptr);
    m_session.timingLevels.fill (nullptr);
    m_session.edgeLevels.fill (nullptr);
    m_session.changedSlots.clear();

    for (slot = 0; slot < static_cast<int> (m_session.analysis.tracks.size()); slot++)
    {
        m_session.changedSlots.insert (slot);
    }

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

    params.title                    = L"Decode settings for " + GetImageName (ComparisonSession::kSideB);
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
        m_diffsView.SelectRowWhere ([this] (const TableRow & row) { return row.finding == m_diffIndex; });
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
    const TrackAnalysis *  trackB = nullptr;
    int                    sector = -1;
    int                    orphan = -1;
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

        //  A sector this image lacks: its data field with no address field,
        //  when one is most likely that sector.
        for (int f : (track != nullptr && sector < 0 && difference.sector >= 0) ? OrphanFields::Find (*track) : vector<int>())
        {
            orphan = (orphan < 0 && OrphanFields::GuessSector (*track, f) == difference.sector) ? f : orphan;
        }

        //  Nibbles only in the other image select the nibble they come before.
        if (difference.firstNibbleA >= 0)
        {
            m_session.model.SelectNibbles (difference.quarterTrack, difference.firstNibbleA, std::max (difference.nibbleCountA, 1));
        }
        else if (sector >= 0)
        {
            m_session.model.SelectSector (difference.quarterTrack, sector);
        }
        else if (orphan >= 0)
        {
            m_session.model.SelectNibbles (difference.quarterTrack, track->fields[static_cast<size_t> (orphan)].firstNibble, track->fields[static_cast<size_t> (orphan)].nibbleCount);
        }
        else if (difference.sector >= 0)
        {
            //  Neither: the other image's sector, which A's views follow.
            m_session.modelB.SelectQuarterTrack (difference.quarterTrack);
            trackB = m_session.modelB.GetTrack();

            for (i = 0; trackB != nullptr && i < trackB->sectors.size(); i++)
            {
                if (trackB->sectors[i].sector == difference.sector)
                {
                    m_session.modelB.SelectSector (difference.quarterTrack, static_cast<int> (i));
                    break;
                }
            }
        }

        m_host.OnSelection();
        m_session.model.StartFlight (InspectorMarquee::GetClockMs());
        ShowSelectionOnStrip();
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
//  "Comparing <left> [swap] <right>" (FR-121), with the reason an image
//  could not be read in place of its file name. The side with focus is in
//  bold. The swap button sits between the two names.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonPart::PaintComparisonBar (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float         margin   = m_session.scaler.ToPxf (static_cast<float> (InspectorMetrics::kMarginDip));
    float         gap      = margin / 2.0f;
    float         x        = static_cast<float> (m_barPx.left) + margin;
    float         top      = static_cast<float> (m_barPx.top);
    float         height   = static_cast<float> (m_barPx.bottom - m_barPx.top);
    float         size     = m_session.scaler.ToPxf (InspectorView::kTextDip);
    float         button   = height - margin;
    float         stepW    = m_session.scaler.ToPxf (static_cast<float> (s_kBarButtonDip));
    std::wstring  errors[] = { m_session.context.hasDisk ? std::wstring() : m_session.comparison.GetAError(), m_session.comparison.GetBError() };
    std::wstring  names[2];
    float         w        = 0.0f;
    float         h        = 0.0f;
    float         slotW    = 0.0f;
    float         left     = 0.0f;
    int           side     = 0;



    painter.FillRect (static_cast<float> (m_barPx.left), top, static_cast<float> (m_barPx.right - m_barPx.left), height, theme.SelectionBackground());

    (void) text.MeasureString (s_kpszComparing, size, DxuiTheme::kBodyFace, w, h);
    text.DrawString (s_kpszComparing, x, top, w + 1.0f, height, theme.Foreground(), size, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    x += w + gap;

    //  Both names get a slot as wide as the wider one in bold, so a swap
    //  moves neither the swap button nor the difference buttons.
    for (side = 0; side < 2; side++)
    {
        names[side] = errors[side].empty() ? GetImageName (side) : errors[side];
        names[side] = names[side].empty() ? std::wstring (s_kpszAnalyzing) : names[side];

        (void) text.MeasureStringWeighted (names[side].c_str(), size, DxuiTheme::kBodyFace, DxuiFontWeight::Bold, w, h);
        slotW = std::max (slotW, w);
    }

    for (side = 0; side < 2; side++)
    {
        DxuiFontWeight  weight = (side == m_focusedSide) ? DxuiFontWeight::Bold : DxuiFontWeight::Normal;

        (void) text.MeasureStringWeighted (names[side].c_str(), size, DxuiTheme::kBodyFace, weight, w, h);

        left = (side == ComparisonSession::kSideA) ? x + slotW - w : x;

        text.DrawString (names[side].c_str(), left, top, w + 1.0f, height, errors[side].empty() ? theme.Foreground() : theme.ErrorForeground(), size,
                         DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, weight, false);

        m_labelPx[side] = { static_cast<LONG> (left), m_barPx.top, static_cast<LONG> (left + w), m_barPx.bottom };
        x              += slotW + margin * 1.5f;

        if (side == ComparisonSession::kSideA)
        {
            m_swapButton->Layout ({ static_cast<LONG> (x), static_cast<LONG> (top + margin / 2.0f), static_cast<LONG> (x + button * 1.5f), static_cast<LONG> (top + margin / 2.0f + button) },
                                  m_session.scaler);
            x += button * 1.5f + margin * 1.5f;
        }
    }

    //  The difference buttons follow the right image's name.
    for (DxuiButton * step : { m_prevDiff, m_nextDiff })
    {
        step->Layout ({ static_cast<LONG> (x), static_cast<LONG> (top + margin / 2.0f), static_cast<LONG> (x + stepW), static_cast<LONG> (top + margin / 2.0f + button) }, m_session.scaler);
        x += stepW + gap;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::GetPath
//
//  The full path of a side's image, or its source's path before it is
//  read; empty for a drive whose disk isn't read yet.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ComparisonPart::GetPath (int side) const
{
    const DiskCopy *          copy   = (side == ComparisonSession::kSideA) ? (m_session.context.hasDisk ? m_session.analysis.copy.get() : nullptr)
                                                                          : m_session.comparison.GetB().copy.get();
    const ComparisonSource &  source = (side == ComparisonSession::kSideA) ? m_session.comparison.GetSourceA() : m_session.comparison.GetSourceB();



    return (copy != nullptr && !copy->fileName.empty()) ? TextEncoding::Utf8ToWide (copy->fileName) : source.path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::GetImageName
//
//  A side's name in the comparison's text once there is a result, its file
//  name before.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ComparisonPart::GetImageName (int side) const
{
    return m_session.comparison.HasResult() ? m_session.comparison.GetResult().names.Get (side) : GetLeafName (GetPath (side));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonPart::GetLeafName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ComparisonPart::GetLeafName (const std::wstring & path)
{
    return std::filesystem::path (path).filename().wstring();
}
