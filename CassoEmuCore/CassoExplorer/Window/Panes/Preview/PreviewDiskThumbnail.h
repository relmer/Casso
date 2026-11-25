#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/DiskThumbnailWorker.h"
#include "CassoExplorer/Model/FileSystemCheckScheduler.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Shell/BackgroundWorkQueue.h"
#include "Ui/DiskInspector/Blocks/BlockMapView.h"
#include "Ui/DiskInspector/InspectorView.h"
#include "Ui/DiskInspector/InspectorViewModel.h"
#include "Ui/DiskInspector/PlatterView.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailPlatterView
//
//  The inspector's platter at a fixed small size, without zoom, pan or
//  tilt. Its palette is resolved from Explorer's theme as it is drawn.
//
////////////////////////////////////////////////////////////////////////////////

class ThumbnailPlatterView : public PlatterView
{
public:
    ThumbnailPlatterView (InspectorViewContext & context, const DxuiTheme * const & theme);

    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override { (void) ev; return false; }

    //  The pointer is on the platter, which a click opens in the inspector:
    //  a ring in the accent is drawn around it.
    void  SetHot  (bool isHot) { m_isHot = isHot; }

private:
    const DxuiTheme * const  & m_theme;
    bool                       m_isHot = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailBlockMap
//
//  Block mode's map at the platter's height, with no current block outlined
//  and no pointer text. While the pointer is on it, a box in the accent is
//  drawn around it, as the ring is around the platter.
//
////////////////////////////////////////////////////////////////////////////////

class ThumbnailBlockMap : public BlockMapView
{
public:
    ThumbnailBlockMap () { SetOutlined (false); }

    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override { (void) ev; return false; }
    void  SetHot  (bool isHot) { m_isHot = isHot; }
    bool  IsHot   () const     { return m_isHot; }

private:
    bool  m_isHot = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailChips
//
//  The summary chips under the small platter, wrapped to the pane's width;
//  or one line of text in their place, such as "Analyzing" or the note for
//  a 3.5" disk.
//
////////////////////////////////////////////////////////////////////////////////

class ThumbnailChips : public IDxuiControl
{
public:
    static constexpr float  kRowDip = 28.0f;

    explicit ThumbnailChips (const DiskInspectorPalette & palette) : m_palette (palette) {}

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override { SetBounds (boundsDip); m_scaler = scaler; }
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    void  SetChips (vector<SummaryChip> chips)  { m_chips = std::move (chips); m_line.clear(); m_chipRects.clear(); }
    void  SetLine  (const std::wstring & line)  { m_line = line; m_chips.clear(); m_chipRects.clear(); }

    //  The info chip under a point, as the chips were last drawn, and where it
    //  is; false for none.
    bool  TryGetInfoChipAt (POINT pointPx, RECT & outRect) const;

    //  The rows the chips wrap onto in a width, two at the least.
    int   CountRows (IDxuiTextRenderer * text, const DxuiDpiScaler & scaler, int widthPx) const;

private:
    const DiskInspectorPalette  & m_palette;
    DxuiDpiScaler                 m_scaler;
    vector<SummaryChip>           m_chips;
    vector<RECT>                  m_chipRects;   // each chip's place, as last drawn
    std::wstring                  m_line;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailScrollbar
//
//  The preview pane's one vertical bar while a disk image's thumbnail is
//  above the view, in pixels of the thumbnail and the view together. The
//  pane sets its range and reads its position; the sync function, called as
//  the bar is drawn, sets the position from where the view has scrolled.
//
////////////////////////////////////////////////////////////////////////////////

class ThumbnailScrollbar : public IDxuiControl
{
public:
    using ScrollFn = std::function<void (int posPx)>;
    using SyncFn   = std::function<void ()>;

    static constexpr int  kWidthDip = 10;

    ThumbnailScrollbar();

    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    //  Over the bar, where it overlaps the window's resize edge, the pointer drags the bar, not the edge.
    bool  IsOverScrollbar (POINT pointPx) const override { return IsVisible() && m_bar.HitTest (pointPx.x, pointPx.y); }

    void  SetRange    (int rangePx, int pagePx, int posPx);
    int   GetPos      () const { return m_bar.GetScrollPos(); }
    void  SetOnScroll (ScrollFn scroll) { m_onScroll = std::move (scroll); }
    void  SetOnSync   (SyncFn sync)     { m_onSync = std::move (sync); }
    bool  SetHover    (POINT pointPx)   { return m_bar.SetHover (IsVisible() && m_bar.HitTest (pointPx.x, pointPx.y), pointPx); }
    bool  TickBar     (int64_t nowMs)   { return m_bar.Tick (nowMs); }

private:
    DxuiScrollbar  m_bar;
    DxuiDpiScaler  m_scaler;
    ScrollFn       m_onScroll;
    SyncFn         m_onSync;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ThumbnailScreen
//
//  The picture of a disk image's screen once booted.
//
////////////////////////////////////////////////////////////////////////////////

class ThumbnailScreen : public IDxuiControl
{
public:
    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override { SetBounds (boundsDip); (void) scaler; }
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    void  SetPicture (std::shared_ptr<const DxuiIconImage> picture) { m_picture = std::move (picture); }

private:
    std::shared_ptr<const DxuiIconImage>  m_picture;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDiskThumbnail
//
//  The small platter and the summary chips above the catalog or details
//  of a disk image selected in a folder (FR-072). The catalog appears at
//  once. The thumbnail is built off the UI thread, with "Analyzing" in its
//  place until it arrives, and a result for a selection that moved on is
//  dropped. A 3.5" WOZ has a note in its place, and an image that does not
//  open has no thumbnail. A click on the platter or on Inspect... opens
//  the inspector.
//
//  An image of blocks, such as a 32 MB volume or a partition of a card, has
//  its block map in place of the platter, and a click on the map opens the
//  inspector in block mode.
//
//  An image that has been booted has the picture of its screen beside the
//  platter, both scaled down in a narrow pane, and a line with its boot result.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewDiskThumbnail
{
public:
    static constexpr int  kPlatterDip     = 180;
    static constexpr int  kGapDip         = 8;
    static constexpr int  kScreenDip      = 240;   // the screen's picture, three quarters as tall
    static constexpr int  kInspectWideDip = 90;   // Inspect..., before its text is measured
    static constexpr int  kInspectHighDip = 28;

    explicit PreviewDiskThumbnail (ExplorerContext & context);

    void  Create     ();
    void  SetPath    (const std::wstring & path);
    void  TakeResult ();

    //  The file-system check of an image of blocks: its timer, and its result
    //  posted to the window. TakeCheck returns whether the infobar changed.
    void  TickCheck  ();
    bool  TakeCheck  ();

    //  The image in the pane, renamed from one path to the other.
    void  MovePath   (const std::wstring & from, const std::wstring & to);

    //  The picture of the image's screen and its result, read again: a
    //  picture that was missing has been made.
    void  RefreshScreen ();
    void  Hide          ();

    //  Laid out in the area, as tall as GetHeightPx, with its top scrolled
    //  away by scrollPx and clipped there, at bottomPx, the pane's bottom,
    //  and at rightPx, the pane's right edge when it's narrower than the
    //  area. Returns the bottom of the visible part.
    int   Layout        (const RECT & area, int scrollPx, int bottomPx, int rightPx);


    bool                  IsShown       () const { return !m_path.empty() && m_state != State::Failed; }
    bool                  IsReady       () const { return m_state == State::Ready && !m_isStale; }
    int                   GetHeightPx   (int widthPx) const;
    bool                  IsOverPlatter (POINT pointPx) const;
    bool                  IsOverVerdict (POINT pointPx) const;
    const std::wstring  & GetPath       () const { return m_path; }

    //  The screen's picture as it is laid out, empty when it isn't drawn, and
    //  the result line under it.
    bool                  IsScreenShown () const { return HasScreen(); }
    RECT                  GetScreenRect () const { return HasScreen() ? m_screen->GetBounds() : RECT {}; }
    const std::wstring  & GetResultLine () const { return m_bootLine->GetText(); }

    //  The verdict's infobar above the thumbnail, the pane's bar, and
    //  Inspect... at the platter's bottom right; for a test.
    const DxuiInfoBanner  & GetVerdict       () const { return *m_verdict; }
    ThumbnailScrollbar    & GetScrollbar     () const { return *m_scrollbar; }
    const DxuiButton      & GetInspectButton () const { return *m_inspect; }
    RECT                    GetPlatterRect   () const { return m_platter->GetBounds(); }
    RECT                    GetBlockMapRect  () const { return m_blockMap->IsVisible() ? m_blockMap->GetBounds() : RECT {}; }
    bool                    IsBlockMapHot    () const { return m_blockMap->IsHot(); }

    //  A disk image's file-system verdict above its thumbnail: Bad as an
    //  error infobar with its first finding of damage, Suspect as an info
    //  infobar with its first suspect finding, since a suspect finding can be
    //  normal for the disk; each finding on the line under the verdict, and a
    //  pointer to Inspect..., where the findings and Repair are; nothing for
    //  OK or a disk with no standard file system.
    static std::wstring  FormatVerdict (FileSystemVerdict verdict, const std::wstring & finding);

    //  The damage the image's listing reports, shown as a warning infobar when
    //  the analysis has no verdict to show, and its text there.
    void                 SetListingWarning    (const std::wstring & warning);
    static std::wstring  FormatListingWarning (const std::wstring & warning);

    //  The file system the analysis found and its verdict, as "DOS 3.3,
    //  damaged", for the preview's details of an image that doesn't mount,
    //  as one with a lost VTOC header; empty before the analysis or for an
    //  image with no standard file system.
    std::wstring  GetFileSystemText () const;

    //  The repaired copy written beside the image and selected, after the
    //  confirmation.


private:
    bool  HandleHeaderMouse  (const DxuiMouseEvent & ev);
    int   GetInspectWidthPx  () const;
    bool  IsInspectBeside    (int widthPx) const;
    bool  HandlePlatterMouse (const DxuiMouseEvent & ev);
    bool  HandleScreenMouse  (const DxuiMouseEvent & ev);
    void  ShowChipTip        (const DxuiMouseEvent & ev);
    void  SetHot             (bool isHot);

    using KeptResult = std::pair<FileStamp, DiskThumbnailResult>;

    static constexpr size_t  kRecentCount = 8;

    enum class State
    {
        Analyzing,
        Ready,
        Note35,
        Failed,
    };

    //  On the worker's thread: the block map of an image of blocks, and the
    //  platter of any other.
    DiskThumbnailResult  BuildThumbnail (const std::wstring & path);

    void  ApplyResult        ();
    void  StartCheck         ();
    void  ArmCheckTimer      (int64_t delayMs);
    bool  ShowCheck          ();
    void  ApplyVerdict       ();
    bool  GetStamp           (const std::wstring & path, FileStamp & outStamp) const;
    bool  FindRecent         (const std::wstring & path, DiskThumbnailResult & outResult) const;
    void  KeepRecent         (const DiskThumbnailResult & result);

    //  The result's chips, and the chip of a disk read as CP/M by the
    //  user's choice.
    vector<SummaryChip>  GetChips () const;

    bool  IsBlocks           () const { return m_result.blocks != nullptr; }
    //  The picture comes from the thumbnail store, not the analysis, so it is
    //  drawn while the analysis runs and the pane isn't laid out again when
    //  the analysis finishes.
    bool  HasScreen          () const { return m_picture != nullptr && (m_state == State::Ready || m_state == State::Analyzing); }
    void  GetFit             (int widthPx, int & outScreenPx, int & outPlatterPx) const;
    bool  HasVerdict         () const { return m_state == State::Ready && m_isVerdictShown; }
    int   GetVerdictHeightPx (int widthPx) const;
    void  PlaceVerdict       (const RECT & area, int top, int height);

    ExplorerContext                       & m_context;
    InspectorViewContext                    m_view;
    InspectorViewModel                      m_model;
    DiskThumbnailWorker                     m_worker;
    BackgroundWorkQueue                     m_checkQueue;                    // before m_checks, so a check is canceled before the queue is joined
    FileSystemCheckScheduler                m_checks;
    DiskThumbnailResult                     m_result;
    std::list<KeptResult>                   m_recent;   // the results read last, newest first, so a pane opened again isn't read again
    DxuiScrollPanel                       * m_header            = nullptr;   // the controls below, clipped where the pane scrolls them away
    ThumbnailPlatterView                  * m_platter           = nullptr;
    ThumbnailBlockMap                     * m_blockMap          = nullptr;
    ThumbnailChips                        * m_chips             = nullptr;
    ThumbnailScreen                       * m_screen            = nullptr;
    DxuiLabel                             * m_bootLine          = nullptr;   // the boot result, under the picture
    DxuiInfoBanner                        * m_verdict           = nullptr;
    DxuiButton                            * m_inspect           = nullptr;   // Inspect..., at the platter area's bottom right
    ThumbnailScrollbar                    * m_scrollbar         = nullptr;   // a window child after the views, so it's offered the mouse first
    bool                                    m_isVerdictShown    = false;
    std::wstring                            m_listingWarning;
    bool                                    m_isVerdictReserved = false;     // the last image's infobar space is reserved while the next is read
    bool                                    m_isChipTipShown    = false;

    std::shared_ptr<const DxuiIconImage>    m_picture;
    std::wstring                            m_path;
    State                                   m_state          = State::Analyzing;
    bool                                    m_isStale        = false;   // the last image is shown until the next one is read
};
