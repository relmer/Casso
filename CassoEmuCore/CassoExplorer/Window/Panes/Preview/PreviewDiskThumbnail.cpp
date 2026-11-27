#include "Pch.h"

#include "CassoExplorer/Window/Panes/Preview/PreviewDiskThumbnail.h"
#include "Seams/IDiskPictures.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/InspectorWindowSet.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerImageTools.h"
#include "CassoExplorer/Window/Managers/ExplorerInput.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewOutput.h"
#include "CassoExplorer/Window/Managers/ExplorerTips.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "CassoExplorer/Window/Managers/ExplorerThumbnails.h"
#include "Core/AppMessages.h"
#include "Core/AppTimerIds.h"
#include "Core/TextEncoding.h"
#include "CassoExplorer/Window/Managers/ExplorerTabManager.h"
#include "CassoExplorer/Window/Panes/ExplorerStatusBar.h"
#include "CassoExplorer/Window/Panes/Preview/PreviewListingView.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"
#include "Devices/Disk/Inspector/FindingFormatter.h"
#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"

#include "Ui/DiskInspector/DiskInspectorPalette.h"
#include "Ui/DiskInspector/PlatterCells.h"





static constexpr float     s_kChipPadDip  = 8.0f;
static constexpr float     s_kChipGapDip  = 6.0f;
static constexpr float     s_kChipHighDip = 22.0f;
static constexpr float     s_kChipTextDip = 12.0f;
static constexpr float     s_kHotRingDip  = 2.0f;
static constexpr float     s_kHotBoxDip   = 6.0f;      // the corner of the box around a hot block map
static constexpr LPCWSTR   s_kpszWaiting  = L"Analyzing";
static constexpr LPCWSTR   s_kpszBooting  = L"Getting thumbnail...";
static constexpr LPCWSTR   s_kpszNote35   = L"Casso does not analyze 3.5\" disks";





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailPlatterView::ThumbnailPlatterView
//
////////////////////////////////////////////////////////////////////////////////

ThumbnailPlatterView::ThumbnailPlatterView (InspectorViewContext & context, const DxuiTheme * const & theme) :
    PlatterView (context),
    m_theme     (theme)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailPlatterView::Paint
//
//  The palette follows a theme change in Explorer (FR-078). While the
//  pointer is on it, a ring in the accent is drawn around it, since a click
//  opens it in the inspector.
//
////////////////////////////////////////////////////////////////////////////////

void ThumbnailPlatterView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (m_theme != nullptr)
    {
        m_context.palette = DiskInspectorPalette::Resolve (*m_theme);
    }

    PlatterView::Paint (painter, text, theme);

    if (m_isHot)
    {
        RECT     bounds = GetBounds();
        float    stroke = m_scaler.ToPxf (s_kHotRingDip);
        float    radius = static_cast<float> ((std::min) (bounds.right - bounds.left, bounds.bottom - bounds.top)) / 2.0f - stroke;
        HRESULT  hr     = text.DrawEllipse ((bounds.left + bounds.right) / 2.0f, (bounds.top + bounds.bottom) / 2.0f, radius, radius, stroke, theme.Accent());

        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailBlockMap::Paint
//
//  While the pointer is on it, a box in the accent is drawn around the map,
//  since a click opens it in the inspector.
//
////////////////////////////////////////////////////////////////////////////////

void ThumbnailBlockMap::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT   bounds = GetBounds();
    float  stroke = GetScaler().ToPxf (s_kHotRingDip);



    BlockMapView::Paint (painter, text, theme);

    if (m_isHot && IsVisible())
    {
        painter.OutlineRoundedRect (bounds.left + stroke / 2.0f, bounds.top + stroke / 2.0f, (bounds.right - bounds.left) - stroke, (bounds.bottom - bounds.top) - stroke,
                                    GetScaler().ToPxf (s_kHotBoxDip), stroke, theme.Accent());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailChips::Paint
//
//  Each chip as the status band draws it, wrapped onto the next row where
//  it does not fit; a bad chip in the bad sector color, and an info chip in
//  the accent.
//
////////////////////////////////////////////////////////////////////////////////

void ThumbnailChips::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float  pad    = m_scaler.ToPxf (s_kChipPadDip);
    float  gap    = m_scaler.ToPxf (s_kChipGapDip);
    float  chipH  = m_scaler.ToPxf (s_kChipHighDip);
    float  rowH   = m_scaler.ToPxf (kRowDip);
    float  textPx = m_scaler.ToPxf (s_kChipTextDip);
    float  left   = static_cast<float> (m_boundsDip.left);
    float  right  = static_cast<float> (m_boundsDip.right);
    float  x      = left;
    float  y      = static_cast<float> (m_boundsDip.top) + (rowH - chipH) / 2.0f;
    float  w      = 0.0f;
    float  h      = 0.0f;



    if (!m_line.empty())
    {
        text.DrawString (m_line.c_str(), left, static_cast<float> (m_boundsDip.top), right - left, rowH, theme.ForegroundMuted(), textPx,
                         DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }

    m_chipRects.clear();

    for (const SummaryChip & chip : m_chips)
    {
        uint32_t  ink = chip.isBad ? m_palette.colors.sectorBad : chip.isInfo ? theme.Accent() : theme.Foreground();

        text.MeasureString (chip.text.c_str(), textPx, DxuiTheme::kBodyFace, w, h);

        if (x > left && x + w + 2 * pad > right)
        {
            x  = left;
            y += rowH;
        }

        painter.FillRoundedRect    (x, y, w + 2 * pad, chipH, chipH / 2, theme.ButtonIdle());
        painter.OutlineRoundedRect (x, y, w + 2 * pad, chipH, chipH / 2, m_scaler.ToPxf (1.0f), (chip.isBad || chip.isInfo) ? ink : theme.ButtonBorder());
        text.DrawString (chip.text.c_str(), x, y, w + 2 * pad, chipH, ink, textPx,
                         DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        m_chipRects.push_back (RECT { static_cast<LONG> (x), static_cast<LONG> (y), static_cast<LONG> (x + w + 2 * pad), static_cast<LONG> (y + chipH) });
        x += w + 2 * pad + gap;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailChips::TryGetInfoChipAt
//
////////////////////////////////////////////////////////////////////////////////

bool ThumbnailChips::TryGetInfoChipAt (POINT pointPx, RECT & outRect) const
{
    bool  isFound = false;



    outRect = {};

    for (size_t i = 0; i < m_chipRects.size() && i < m_chips.size() && !isFound; i++)
    {
        isFound = m_chips[i].isInfo && PtInRect (&m_chipRects[i], pointPx) != FALSE;
        outRect = isFound ? m_chipRects[i] : outRect;
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailChips::CountRows
//
//  Wraps as Paint wraps.
//
////////////////////////////////////////////////////////////////////////////////

int ThumbnailChips::CountRows (IDxuiTextRenderer * text, const DxuiDpiScaler & scaler, int widthPx) const
{
    float  pad    = scaler.ToPxf (s_kChipPadDip);
    float  gap    = scaler.ToPxf (s_kChipGapDip);
    float  textPx = scaler.ToPxf (s_kChipTextDip);
    float  x      = 0.0f;
    float  w      = 0.0f;
    float  h      = 0.0f;
    int    rows   = 1;



    if (text == nullptr)
    {
        return 2;
    }

    for (const SummaryChip & chip : m_chips)
    {
        text->MeasureString (chip.text.c_str(), textPx, DxuiTheme::kBodyFace, w, h);

        if (x > 0.0f && x + w + 2 * pad > static_cast<float> (widthPx))
        {
            x = 0.0f;
            rows++;
        }

        x += w + 2 * pad + gap;
    }

    return (std::max) (2, rows);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::FormatVerdict
//
//  The verdict and the finding as a sentence, as the infobar of an image of
//  blocks has them, then the pointer to Inspect...; nothing for OK or a disk
//  with no standard file system.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PreviewDiskThumbnail::FormatVerdict (FileSystemVerdict verdict, const std::wstring & finding)
{
    bool  isProblem = verdict == FileSystemVerdict::Bad || verdict == FileSystemVerdict::Suspect;



    return isProblem ? FileSystemCheckScheduler::FormatInfobar ({ verdict, finding }) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailScrollbar::ThumbnailScrollbar
//
//  A move of the bar is passed on as its position.
//
////////////////////////////////////////////////////////////////////////////////

ThumbnailScrollbar::ThumbnailScrollbar()
{
    m_bar.SetOnScroll ([this] (int, int pos)
    {
        if (m_onScroll)
        {
            m_onScroll (pos);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailScrollbar::Layout
//
//  The bar fills the bounds; an arrow click moves it a catalog row.
//
////////////////////////////////////////////////////////////////////////////////

void ThumbnailScrollbar::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  width = scaler.ToPx (kWidthDip);



    SetBounds (boundsDip);
    m_scaler = scaler;

    m_bar.Configure (DxuiScrollbar::Orientation::Vertical, width, width, scaler.ToPx (PreviewListingView::kRowHeightDip));
    m_bar.SetTrack  (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailScrollbar::Paint
//
////////////////////////////////////////////////////////////////////////////////

void ThumbnailScrollbar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    (void) text;

    if (m_onSync)
    {
        m_onSync();
    }

    m_bar.Paint (painter, theme.ForegroundMuted());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailScrollbar::OnMouse
//
//  A press on the bar, a drag of its thumb and the release.
//
////////////////////////////////////////////////////////////////////////////////

bool ThumbnailScrollbar::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isHandled = false;



    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && m_bar.HitTest (ev.positionDip.x, ev.positionDip.y))
    {
        isHandled = m_bar.OnMouseDown (ev.positionDip.x, ev.positionDip.y);
    }
    else if (ev.kind == DxuiMouseEventKind::Move && m_bar.IsDragging())
    {
        isHandled = m_bar.OnMouseMove (ev.positionDip.x, ev.positionDip.y);
    }
    else if (ev.kind == DxuiMouseEventKind::Up && m_bar.IsDragging())
    {
        isHandled = m_bar.OnMouseUp();
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailScrollbar::SetRange
//
////////////////////////////////////////////////////////////////////////////////

void ThumbnailScrollbar::SetRange (int rangePx, int pagePx, int posPx)
{
    DxuiScrollInfo  info;



    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;
    info.nMax  = (std::max) (0, rangePx + pagePx - 1);
    info.nPage = static_cast<UINT> ((std::max) (1, pagePx));
    info.nPos  = std::clamp (posPx, 0, (std::max) (0, rangePx));

    m_bar.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailScreen::Paint
//
////////////////////////////////////////////////////////////////////////////////

void ThumbnailScreen::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT  hr     = S_OK;
    RECT     bounds = GetBounds();



    (void) painter;
    (void) theme;

    if (m_picture != nullptr && m_picture->width > 0 && m_picture->height > 0)
    {
        hr = text.DrawIconBitmap (m_picture->bgraPremul.data(), m_picture->width, m_picture->height, (float) bounds.left, (float) bounds.top,
                                  (float) (bounds.right - bounds.left), (float) (bounds.bottom - bounds.top));
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::PreviewDiskThumbnail
//
//  A finished thumbnail is posted to the window, which takes it on the UI
//  thread. So is a finished file-system check, which runs on the check
//  queue's thread and reads the image through the browser's file I/O, as the
//  thumbnail does. Only an image of blocks is checked: the thumbnail's
//  analysis of any other image has its verdict.
//
////////////////////////////////////////////////////////////////////////////////

PreviewDiskThumbnail::PreviewDiskThumbnail (ExplorerContext & context) :
    m_context (context),
    m_worker  ([this] () { PostMessageW (m_context.window.GetHwnd(), (UINT) AppMessage::ExplorerThumbnail, 0, 0); },
               [this] (const std::wstring & path) { return BuildThumbnail (path); }),
    m_checks  ([] () { return CassoExplorerWindow::GetNowMs(); },
               [this] (FileSystemCheckScheduler::Job job) { m_checkQueue.Post (std::move (job)); },
               [this] (const FileSystemCheckKey & key, const std::atomic<bool> & isCanceled, FileSystemCheckOutcome & outOutcome)
               {
                   IDiskFileIo  & fileIo   = m_context.browser.GetOperations().GetFileIo();
                   bool           isBlocks = BlockImageFile::IsBlockImage (fileIo, TextEncoding::WideToUtf8 (key.path));

                   return isBlocks && FileSystemCheckScheduler::CheckBlockImage (fileIo, key, isCanceled, outOutcome);
               },
               [this] () { PostMessageW (m_context.window.GetHwnd(), (UINT) AppMessage::ExplorerFileSystemCheck, 0, 0); })
{
    m_view.model = &m_model;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::BuildThumbnail
//
//  The image is read through the browser's file I/O, which reads a partition
//  of a card that is open. A file in neither form has no thumbnail.
//
////////////////////////////////////////////////////////////////////////////////

DiskThumbnailResult PreviewDiskThumbnail::BuildThumbnail (const std::wstring & path)
{
    IDiskFileIo          & fileIo   = m_context.browser.GetOperations().GetFileIo();
    bool                   isBlocks = BlockImageFile::IsBlockImage (fileIo, TextEncoding::WideToUtf8 (path));
    DiskThumbnailResult    result;



    result.path = path;

    if (isBlocks)
    {
        result = DiskThumbnailWorker::BuildBlocks (fileIo, path);
    }
    else if (InspectorWindowSet::HasTrackFormat (path))
    {
        result = DiskThumbnailWorker::Build (path);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::Create
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::Create()
{
    m_header    = m_context.window.CreateChild<DxuiScrollPanel>();
    m_scrollbar = m_context.window.CreateChild<ThumbnailScrollbar>();
    m_platter  = m_header->CreateChild<ThumbnailPlatterView> (m_view, m_context.theme);
    m_blockMap = m_header->CreateChild<ThumbnailBlockMap>();
    m_chips    = m_header->CreateChild<ThumbnailChips> (m_view.palette);
    m_screen   = m_header->CreateChild<ThumbnailScreen>();
    m_bootLine = m_header->CreateChild<DxuiLabel> (L"", DxuiTextRole::Muted, DxuiTextHAlign::Left, DxuiTextVAlign::Top);
    m_verdict  = m_header->CreateChild<DxuiInfoBanner>();
    m_inspect  = m_header->CreateChild<DxuiButton> (L"Inspect...");

    m_header->SetScrollbarShown (false);
    m_inspect->SetOnClick       ([this] () { m_context.imageTools.InspectDiskImages ({ m_path }); });

    m_context.window.SetMouseHandler (m_header, [this] (const DxuiMouseEvent & ev) { return HandleHeaderMouse (ev); });

    m_platter->SetSelectionShown (false);
    Hide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::SetPath
//
//  Another file starts over with "Analyzing"; an empty path is no
//  thumbnail.
//
//  The infobar's space stays, empty, while the next image is read after one
//  that had an infobar, so the catalog under it doesn't jump down when the
//  next one's infobar arrives. Its text stays in the hidden infobar to
//  measure the space.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::SetPath (const std::wstring & path)
{
    if (path == m_path)
    {
        return;
    }

    m_path    = path;
    m_isStale = !path.empty() && m_state == State::Ready;

    m_isVerdictReserved = !path.empty() && (HasVerdict() || m_isVerdictReserved);
    m_blockMap->SetModel (nullptr);

    m_isVerdictShown = false;
    m_chips->SetLine (s_kpszWaiting);

    //  After a ready image, a blank disk is shown in the same place until
    //  the next one is read, so the pane doesn't flash its background.
    if (m_isStale)
    {
        for (int qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
        {
            m_platter->GetRenderer().SetRing (qt, PlatterRingState::Nothing, nullptr);
        }

        m_verdict->SetVisible (false);
    }
    else
    {
        m_state         = State::Analyzing;
        m_result        = DiskThumbnailResult();
        m_view.hasDisk  = false;
        m_view.analysis = nullptr;
    }

    if (path.empty())
    {
        m_worker.Cancel();
        Hide();
    }
    else if (FindRecent (path, m_result))
    {
        m_worker.Cancel();
        m_isStale = false;
        ApplyResult();
    }
    else
    {
        (void) m_worker.Request (path);
    }

    StartCheck();
    RefreshScreen();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::MovePath
//
//  The result read from the image moves to its new path, so the pane isn't
//  laid out again when the renamed file is selected. An image that is being
//  read is read again under the new path by SetPath.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::MovePath (const std::wstring & from, const std::wstring & to)
{
    if (m_path.empty() || m_state == State::Analyzing || m_isStale || _wcsicmp (m_path.c_str(), from.c_str()) != 0)
    {
        return;
    }

    m_path        = to;
    m_result.path = to;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::RefreshScreen
//
//  An image with no picture yet is queued to be booted by the read, and the
//  window is posted a message when its picture is made. A partition of a
//  card isn't booted.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::RefreshScreen()
{
    std::wstring                          result;
    bool                                  found = false;
    std::shared_ptr<const DxuiIconImage>  picture;



    if (!m_path.empty() && m_context.app.pictures != nullptr && !PhysicalCards::IsOnCard (m_path))
    {
        found = m_context.app.pictures->Find (m_path, picture, result);
    }

    //  A boot for the picture takes several seconds, so a waiting line
    //  replaces the result line until it is done.
    m_isBooting = !found && !m_path.empty() && m_context.app.pictures != nullptr && m_context.app.pictures->IsBooting (m_path);
    result      = m_isBooting ? std::wstring (s_kpszBooting) : result;

    m_picture = found ? picture : nullptr;
    m_screen->SetPicture (m_picture);
    m_bootLine->SetText (result);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::TakeResult
//
//  The block map's model is released only once a result for this selection
//  replaces it.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::TakeResult()
{
    DiskThumbnailResult  result;



    if (m_worker.TakeResult (result) && result.path == m_path)
    {
        m_blockMap->SetModel (nullptr);
        m_result  = std::move (result);
        m_isStale = false;
        KeepRecent (m_result);
        ApplyResult();
        RefreshScreen();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::TickCheck
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::TickCheck()
{
    ArmCheckTimer (m_checks.Tick());
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::TakeCheck
//
//  A check of an image that is no longer selected is cached, and the infobar
//  changes only for the selected one.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDiskThumbnail::TakeCheck()
{
    bool  isSelected = m_checks.TakeFinished();



    return isSelected && ShowCheck();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::StartCheck
//
//  The selected image's check, by its path, length and modified time, starts
//  after the debounce delay. No image, or one whose length and time can't
//  be read, has no check.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::StartCheck()
{
    HRESULT             hr      = S_OK;
    FileSystemCheckKey  key;
    FileStamp           stamp;
    bool                hasPath = !m_path.empty();



    m_checks.Clear();
    ArmCheckTimer (-1);
    CBR (hasPath);

    hr = m_context.browser.GetOperations().GetFileIo().Stat (TextEncoding::WideToUtf8 (m_path), stamp);
    CHR (hr);

    key.path         = m_path;
    key.sizeBytes    = stamp.sizeBytes;
    key.modifiedUnix = stamp.modifiedUnix;

    m_checks.Select (key);
    ArmCheckTimer (m_checks.Tick());

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::ArmCheckTimer
//
//  A negative delay stops the timer.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::ArmCheckTimer (int64_t delayMs)
{
    HWND  hwnd = m_context.window.GetHwnd();



    if (hwnd == nullptr)
    {
        return;
    }

    if (delayMs < 0)
    {
        KillTimer (hwnd, (UINT_PTR) AppTimerId::ExplorerFileSystemCheck);
    }
    else
    {
        SetTimer (hwnd, (UINT_PTR) AppTimerId::ExplorerFileSystemCheck, (UINT) std::max<int64_t> (delayMs, USER_TIMER_MINIMUM), nullptr);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::ShowCheck
//
//  The check of the selected image of blocks, in the infobar once its block
//  map is drawn: Bad as an error, Possible inconsistencies and no standard
//  file system as info, with no button, since Inspect... is under the map,
//  and nothing for OK.
//  Returns whether the infobar is visible.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDiskThumbnail::ShowCheck()
{
    FileSystemCheckOutcome  outcome;
    std::wstring            text;
    bool                    hasOutcome = m_state == State::Ready && IsBlocks() && m_checks.TryGetOutcome (outcome);



    text             = hasOutcome ? FileSystemCheckScheduler::FormatInfobar (outcome) : std::wstring();
    m_isVerdictShown = !text.empty();

    if (m_isVerdictShown)
    {
        m_verdict->SetText     (text);
        m_verdict->SetSeverity ((outcome.verdict == FileSystemVerdict::Bad) ? DxuiInfoBanner::Severity::Error : DxuiInfoBanner::Severity::Info);
    }

    return m_isVerdictShown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::ApplyResult
//
//  Each quarter track's ring set from its record's levels, as the
//  inspector sets them, with nothing selected. An image of blocks has its
//  block map read from the model.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::ApplyResult()
{
    const DiskCopy  * copy    = (m_result.analysis != nullptr) ? m_result.analysis->copy.get() : nullptr;
    int               qt      = 0;
    int               slot    = 0;



    m_isVerdictReserved = false;

    if (m_result.isOpened && IsBlocks())
    {
        m_state = State::Ready;
        m_blockMap->SetModel (m_result.blocks.get());
        m_blockMap->Refresh();
        m_chips->SetChips (m_result.chips);
        (void) ShowCheck();
        return;
    }

    if (!m_result.isOpened || copy == nullptr)
    {
        m_state = State::Failed;
        Hide();
        return;
    }

    if (m_result.is35)
    {
        m_state = State::Note35;
        m_chips->SetLine (s_kpszNote35);
        m_platter->SetVisible (false);
        return;
    }

    m_state         = State::Ready;
    m_view.analysis = m_result.analysis.get();
    m_view.hasDisk  = true;
    m_model.SetDisk (m_result.analysis->mediaId);
    m_model.SetAnalysis (m_result.analysis.get());
    m_model.ClearSelection();

    for (qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
    {
        slot = copy->playedSlot[qt];

        if (slot < 0 || slot >= static_cast<int> (m_result.levels.size()) || m_result.levels[slot] == nullptr)
        {
            m_platter->GetRenderer().SetRing (qt, PlatterRingState::Nothing, nullptr);
        }
        else
        {
            m_platter->GetRenderer().SetRing (qt, PlatterRingState::Data, m_result.levels[slot], nullptr, PlatterCells::IsEchoRing (copy->playedSlot, qt),
                                              m_result.edges[slot]);
        }
    }

    m_chips->SetChips (GetChips());
    m_platter->SetVisible (m_chips->IsVisible());

    ApplyVerdict();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::ApplyVerdict
//
//  The analysis's verdict in the infobar, or with none to show, the
//  listing's damage as a warning.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::ApplyVerdict()
{
    const Finding            * finding  = FileSystemCheck::GetVerdictFinding (*m_result.analysis);
    std::wstring               text     = FormatVerdict (m_result.analysis->verdict, finding != nullptr ? FindingFormatter::Format (*finding) : std::wstring());
    DxuiInfoBanner::Severity   severity = (m_result.analysis->verdict == FileSystemVerdict::Bad) ? DxuiInfoBanner::Severity::Error : DxuiInfoBanner::Severity::Info;



    if (text.empty() && !m_listingWarning.empty())
    {
        text     = FormatListingWarning (m_listingWarning);
        severity = DxuiInfoBanner::Severity::Warning;
    }

    m_isVerdictShown = !text.empty();

    m_verdict->SetText     (text);
    m_verdict->SetActions  ({});
    m_verdict->SetSeverity (severity);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::FormatListingWarning
//
//  The listing's damage as the infobar of a damaged file system has its
//  finding, with the pointer to Inspect... on the next line.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PreviewDiskThumbnail::FormatListingWarning (const std::wstring & warning)
{
    return warning.empty() ? std::wstring() : FileSystemCheckScheduler::FormatInfobar ({ FileSystemVerdict::Suspect, warning });
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::SetListingWarning
//
//  The damage the image's listing reports, such as a CP/M directory's
//  entries that aren't valid. A disk analyzed already has its infobar
//  updated.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::SetListingWarning (const std::wstring & warning)
{
    if (warning == m_listingWarning)
    {
        return;
    }

    m_listingWarning = warning;

    if (m_state == State::Ready && !IsBlocks() && m_result.analysis != nullptr)
    {
        ApplyVerdict();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::GetStamp
//
//  The file's size and modified time, through the browser's file I/O. False
//  for a path that isn't a file, such as a card's partition.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDiskThumbnail::GetStamp (const std::wstring & path, FileStamp & outStamp) const
{
    HRESULT  hr = S_OK;



    outStamp = FileStamp();

    if (PhysicalCards::IsOnCard (path))
    {
        return false;
    }

    hr = m_context.browser.GetOperations().GetFileIo().Stat (TextEncoding::WideToUtf8 (path), outStamp);

    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::FindRecent
//
//  A result read for the path while the file had the size and modified time
//  it has now.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDiskThumbnail::FindRecent (const std::wstring & path, DiskThumbnailResult & outResult) const
{
    FileStamp  stamp;
    bool       isFound = false;



    if (m_recent.empty() || !GetStamp (path, stamp))
    {
        return false;
    }

    for (const auto & [kept, result] : m_recent)
    {
        if (!isFound && _wcsicmp (result.path.c_str(), path.c_str()) == 0 && kept.sizeBytes == stamp.sizeBytes && kept.modifiedUnix == stamp.modifiedUnix)
        {
            outResult = result;
            isFound   = true;
        }
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::KeepRecent
//
//  The result first in the list, in place of an earlier one for its path,
//  with the oldest dropped past kRecentCount.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::KeepRecent (const DiskThumbnailResult & result)
{
    FileStamp  stamp;



    if (!GetStamp (result.path, stamp))
    {
        return;
    }

    m_recent.remove_if ([&result] (const KeptResult & kept) { return _wcsicmp (kept.second.path.c_str(), result.path.c_str()) == 0; });
    m_recent.emplace_front (stamp, result);

    while (m_recent.size() > kRecentCount)
    {
        m_recent.pop_back();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::GetFit
//
//  The screen's picture and the platter side by side, both at full size
//  where they fit in the width and scaled down together where they don't.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::GetFit (int widthPx, int & outScreenPx, int & outPlatterPx) const
{
    const DxuiDpiScaler  & scaler  = m_context.scaler;
    int                    gap     = scaler.ToPx (kGapDip);
    int                    screen  = HasScreen() ? scaler.ToPx (kScreenDip) : 0;
    int                    platter = scaler.ToPx (kPlatterDip);
    int                    room    = (std::max) (1, widthPx - (HasScreen() ? 3 : 2) * gap);
    float                  share   = (std::min) (1.0f, static_cast<float> (room) / static_cast<float> (screen + platter));



    outScreenPx  = static_cast<int> (screen * share);
    outPlatterPx = static_cast<int> (platter * share);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::GetHeightPx
//
//  The platter and the rows the chips wrap onto, two at the least, or one
//  row for a line in their place.
//  With the screen's picture, two rows for its result as well, and the
//  picture's height where it is above the platter.
//
////////////////////////////////////////////////////////////////////////////////

int PreviewDiskThumbnail::GetHeightPx (int widthPx) const
{
    const DxuiDpiScaler  & scaler = m_context.scaler;
    int                    rows   = scaler.ToPx (static_cast<int> (ThumbnailChips::kRowDip));
    int                    gap    = scaler.ToPx (kGapDip);
    int                    screen = 0;
    int                    height = 0;



    GetFit (widthPx, screen, height);
    height += m_chips->CountRows (m_context.window.GetTextRenderer(), scaler, widthPx - 2 * gap) * rows + 2 * gap;

    if (!IsShown())
    {
        return 0;
    }

    if (m_state == State::Note35)
    {
        return rows + gap;
    }

    if (HasScreen())
    {
        height += 2 * rows + gap;
    }

    //  Inspect...'s row under the platter, where there's no room beside it.
    if (!IsInspectBeside (widthPx))
    {
        height += scaler.ToPx (kInspectHighDip) + gap;
    }

    return height + GetVerdictHeightPx (widthPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::Layout
//
//  The platter centered across the area's top, and the chips under it. The
//  screen's picture is to the platter's left where both fit, and above it
//  otherwise, and its result is between them and the chips.
//
////////////////////////////////////////////////////////////////////////////////

int PreviewDiskThumbnail::Layout (const RECT & area, int scrollPx, int bottomPx, int rightPx)
{
    const DxuiDpiScaler  & scaler    = m_context.scaler;
    int                    platter   = 0;
    int                    screenW   = 0;
    int                    gap       = scaler.ToPx (kGapDip);
    int                    rows      = scaler.ToPx (static_cast<int> (ThumbnailChips::kRowDip));
    int                    width     = static_cast<int> (area.right - area.left);
    bool                   hasScreen = HasScreen();
    int                    screenH   = 0;
    int                    group     = 0;
    int                    left      = 0;
    int                    top       = area.top + gap;
    int                    chipsTop  = area.top + gap;
    int                    shown     = 0;
    int                    inspectW  = GetInspectWidthPx();
    int                    inspectH  = scaler.ToPx (kInspectHighDip);



    if (!IsShown())
    {
        Hide();
        return area.top;
    }

    GetFit (width, screenW, platter);
    screenH = screenW * 3 / 4;
    group   = platter + (hasScreen ? screenW + gap : 0);
    left    = area.left + (std::max) (0, (width - group) / 2);

    if (HasVerdict() || m_isVerdictReserved)
    {
        PlaceVerdict (area, top, GetVerdictHeightPx (width) - gap);
        top     += GetVerdictHeightPx (width);
        chipsTop = top;
    }

    if (hasScreen)
    {
        m_header->PlaceChild (*m_screen, RECT { left, top + (platter - screenH) / 2, left + screenW, top + (platter - screenH) / 2 + screenH });
        left += screenW + gap;
    }

    //  A block map is as wide as the pane, as a volume of blocks isn't round.
    if (m_state != State::Note35)
    {
        m_header->PlaceChild (*m_platter,  RECT { left, top, left + platter, top + platter });
        m_header->PlaceChild (*m_blockMap, RECT { hasScreen ? left : area.left + gap, top, area.right - gap, top + platter });
        chipsTop = top + platter + gap;

        //  Inspect... at the platter area's bottom right, or under the
        //  platter or the map at the right where it would overlap it.
        if (IsInspectBeside (width))
        {
            m_header->PlaceChild (*m_inspect, RECT { area.right - gap - inspectW, top + platter - inspectH, area.right - gap, top + platter });
        }
        else
        {
            m_header->PlaceChild (*m_inspect, RECT { area.right - gap - inspectW, chipsTop, area.right - gap, chipsTop + inspectH });
            chipsTop += inspectH + gap;
        }
    }

    if (HasBootLine())
    {
        m_header->PlaceChild (*m_bootLine, RECT { area.left + gap, chipsTop, area.right - gap, chipsTop + 2 * rows });
        chipsTop += 2 * rows + gap;
    }

    m_header->PlaceChild (*m_chips, RECT { area.left + gap, chipsTop, area.right - gap, chipsTop + m_chips->CountRows (m_context.window.GetTextRenderer(), scaler, width - 2 * gap) * rows });

    m_header->SetVisible  (true);
    m_chips->SetVisible   (true);
    m_platter->SetVisible  (m_state == State::Ready && !IsBlocks());
    m_blockMap->SetVisible (m_state == State::Ready && IsBlocks());
    m_inspect->SetVisible  (m_state == State::Ready);
    m_screen->SetVisible  (hasScreen);
    m_bootLine->SetVisible  (HasBootLine());
    m_verdict->SetVisible (HasVerdict());

    //  Laid out whole to measure it, then in the part not scrolled away.
    m_header->Layout (area, scaler);
    shown = std::clamp ((std::min) (static_cast<int> (area.bottom - area.top), m_header->GetContentHeightPx()) - scrollPx, 0, (std::max) (0, bottomPx - static_cast<int> (area.top)));

    m_header->Layout         (RECT { area.left, area.top, (std::min) (static_cast<int> (area.right), rightPx), area.top + shown }, scaler);
    m_header->SetScrollPosPx (scrollPx);

    return area.top + shown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::Hide
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::Hide()
{
    m_header->SetVisible  (false);
    m_inspect->SetVisible (false);
    m_scrollbar->SetVisible (false);
    m_platter->SetVisible (false);
    m_blockMap->SetVisible (false);
    m_chips->SetVisible   (false);
    m_screen->SetVisible  (false);
    m_bootLine->SetVisible  (false);
    m_verdict->SetVisible   (false);

    SetHot (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::IsOverPlatter
//
//  Over the platter, or over the block map in its place.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDiskThumbnail::IsOverPlatter (POINT pointPx) const
{
    RECT  platter = m_platter->GetBounds();
    RECT  map     = m_blockMap->GetBounds();
    bool  isOver  = (m_platter->IsVisible() && PtInRect (&platter, pointPx) != FALSE) || (m_blockMap->IsVisible() && PtInRect (&map, pointPx) != FALSE);



    return m_state == State::Ready && isOver;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::IsOverVerdict
//
//  Whether a point is on the file system's infobar, which opens the
//  inspector on a click, as Inspect... does.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDiskThumbnail::IsOverVerdict (POINT pointPx) const
{
    RECT  verdict = m_verdict->GetBounds();



    return HasVerdict() && m_verdict->IsVisible() && PtInRect (&verdict, pointPx) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::HandleHeaderMouse
//
//  The platter's and the picture's clicks, then the infobar's action. The
//  wheel is the pane's, so the header doesn't scroll by itself.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDiskThumbnail::HandleHeaderMouse (const DxuiMouseEvent & ev)
{
    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Leave)
    {
        SetHot (ev.kind == DxuiMouseEventKind::Move && IsOverPlatter (ev.positionDip));
        ShowChipTip (ev);
    }

    if (ev.kind == DxuiMouseEventKind::Wheel)
    {
        return true;
    }

    return HandlePlatterMouse (ev) || HandleScreenMouse (ev) || m_header->OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::GetInspectWidthPx
//
//  Inspect...'s text, measured where the window has a text renderer, and a
//  button's padding either side.
//
////////////////////////////////////////////////////////////////////////////////

int PreviewDiskThumbnail::GetInspectWidthPx() const
{
    static constexpr float  kFontDip = 13.0f;
    static constexpr int    kRoomDip = 32;



    IDxuiTextRenderer  * text  = m_context.window.GetTextRenderer();
    float                width = 0.0f;
    float                high  = 0.0f;



    if (text != nullptr)
    {
        text->MeasureString (L"Inspect...", m_context.scaler.ToPxf (kFontDip), DxuiTheme::GetUiFace(), width, high);
    }

    return (width > 0.0f) ? static_cast<int> (std::ceil (width)) + m_context.scaler.ToPx (kRoomDip) : m_context.scaler.ToPx (kInspectWideDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::IsInspectBeside
//
//  Whether Inspect... fits at the platter area's right without overlapping
//  the platter, which is centered, or with the screen's picture beside it.
//  A block map is as wide as the pane, so Inspect... goes under it.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDiskThumbnail::IsInspectBeside (int widthPx) const
{
    const DxuiDpiScaler  & scaler  = m_context.scaler;
    int                    gap     = scaler.ToPx (kGapDip);
    int                    screen  = 0;
    int                    platter = 0;



    GetFit (widthPx, screen, platter);

    int  group = platter + (HasScreen() ? screen + gap : 0);

    return !IsBlocks() && (widthPx - group) / 2 >= GetInspectWidthPx() + 2 * gap;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::SetHot
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::SetHot (bool isHot)
{
    m_platter->SetHot  (isHot);
    m_blockMap->SetHot (isHot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::GetChips
//
////////////////////////////////////////////////////////////////////////////////

vector<SummaryChip> PreviewDiskThumbnail::GetChips() const
{
    vector<SummaryChip>  chips  = m_result.chips;
    bool                 isCpm  = m_context.browser.GetOperations().IsReadAsCpmByChoice (TextEncoding::WideToNarrow (m_path));



    if (isCpm)
    {
        chips.push_back (InspectorText::MakeReadAsCpmChip());
    }

    return chips;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::ShowChipTip
//
//  The tip of the chip under the pointer, for the chip of a disk read as
//  CP/M by the user's choice; hidden once the pointer is off it.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::ShowChipTip (const DxuiMouseEvent & ev)
{
    RECT  chip   = {};
    bool  isOver = ev.kind == DxuiMouseEventKind::Move && m_chips->IsVisible() && m_chips->TryGetInfoChipAt (ev.positionDip, chip);



    if (isOver && !m_isChipTipShown)
    {
        m_context.tips.ShowHoverTip (ExplorerTips::TipOwner::Preview, chip, InspectorText::kpszReadAsCpmTip);
    }
    else if (!isOver && m_isChipTipShown)
    {
        m_context.tips.HideHoverTip (ExplorerTips::TipOwner::Preview);
    }

    m_isChipTipShown = isOver;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::HandlePlatterMouse
//
//  A click on the small platter, the block map or the file system's infobar
//  opens the inspector.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDiskThumbnail::HandlePlatterMouse (const DxuiMouseEvent & ev)
{
    if (!IsOverPlatter (ev.positionDip) && !IsOverVerdict (ev.positionDip))
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Up && ev.button == DxuiMouseButton::Left)
    {
        m_context.imageTools.InspectDiskImages ({ m_path });
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::GetVerdictHeightPx
//
//  The infobar as tall as its text wrapped to the pane's width, measured
//  where the window has a text renderer, and a gap; nothing for none. The
//  last image's, hidden, while its space is reserved for the next image.
//
////////////////////////////////////////////////////////////////////////////////

int PreviewDiskThumbnail::GetVerdictHeightPx (int widthPx) const
{
    const DxuiDpiScaler  & scaler = m_context.scaler;
    IDxuiTextRenderer    * text   = m_context.window.GetTextRenderer();
    int                    gap    = scaler.ToPx (kGapDip);
    float                  width  = static_cast<float> ((std::max) (0, widthPx - 2 * gap));
    float                  height = 0.0f;



    if (!HasVerdict() && !m_isVerdictReserved)
    {
        return 0;
    }

    height = (text != nullptr) ? m_verdict->GetMeasuredHeightPx (*text, width, scaler) : m_verdict->GetPreferredHeightPx (width, scaler);

    return static_cast<int> (std::ceil (height)) + gap;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::PlaceVerdict
//
//  The infobar across the area's top.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewDiskThumbnail::PlaceVerdict (const RECT & area, int top, int height)
{
    int  gap = m_context.scaler.ToPx (kGapDip);



    m_header->PlaceChild (*m_verdict, RECT { area.left + gap, top, area.right - gap, top + height });
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::GetFileSystemText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PreviewDiskThumbnail::GetFileSystemText() const
{
    std::wstring  text;



    if (m_state != State::Ready || m_isStale || m_result.analysis == nullptr || m_result.analysis->verdict == FileSystemVerdict::None || m_result.analysis->fileMaps.size() != 1)
    {
        return text;
    }

    switch (m_result.analysis->fileMaps[0].fileSystem)
    {
        case MapFileSystem::Dos33:  text = L"DOS 3.3";      break;
        case MapFileSystem::ProDos: text = L"ProDOS";       break;
        case MapFileSystem::Pascal: text = L"Apple Pascal"; break;
        case MapFileSystem::Cpm:    text = L"CP/M";         break;
        default:                                            break;
    }

    if (!text.empty() && m_result.analysis->verdict == FileSystemVerdict::Bad)
    {
        text += L", damaged";
    }
    else if (!text.empty() && m_result.analysis->verdict == FileSystemVerdict::Suspect)
    {
        text += L", possible inconsistencies";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail::HandleScreenMouse
//
//  The booted screen's picture has the thumbnail's menu.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewDiskThumbnail::HandleScreenMouse (const DxuiMouseEvent & ev)
{
    RECT                            bounds = m_screen->GetBounds();
    std::vector<DxuiPopupMenuItem>  items;



    if (!HasScreen() || !m_screen->IsVisible() || PtInRect (&bounds, ev.positionDip) == FALSE)
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Right)
    {
        m_context.input.SetFocusPane (ExplorerPane::Preview);
        m_context.input.ReleasePressed();
        m_context.menus.GetMenuCommands().clear();
        ExplorerThumbnails::AddPreviewMenu (m_context, items, m_path);
        DxuiContextMenu::Show (*m_context.window.GetPopupHost(), ev.positionDip.x, ev.positionDip.y, std::move (items));
    }

    return true;
}
