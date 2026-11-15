#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/DiskThumbnailWorker.h"
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

    void  SetChips (vector<SummaryChip> chips)  { m_chips = std::move (chips); m_line.clear(); }
    void  SetLine  (const std::wstring & line)  { m_line = line; m_chips.clear(); }

private:
    const DiskInspectorPalette  & m_palette;
    DxuiDpiScaler                 m_scaler;
    vector<SummaryChip>           m_chips;
    std::wstring                  m_line;
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
//  platter, or above it in a narrow pane, and a line with its boot result.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewDiskThumbnail
{
public:
    static constexpr int  kPlatterDip     = 180;
    static constexpr int  kGapDip         = 8;
    static constexpr int  kScreenDip      = 240;  // the screen's picture, three quarters as tall
    static constexpr int  kInspectWideDip = 90;   // Inspect..., before its text is measured
    static constexpr int  kInspectHighDip = 28;

    explicit PreviewDiskThumbnail (ExplorerContext & context);

    void  Create     ();
    void  SetPath    (const std::wstring & path);
    void  TakeResult ();

    //  The picture of the image's screen and its result, read again: a
    //  picture that was missing has been made.
    void  RefreshScreen ();
    void  Layout        (const RECT & area);
    void  Hide          ();

    bool                  IsShown       () const { return !m_path.empty() && m_state != State::Failed; }
    int                   GetHeightPx   (int widthPx) const;
    bool                  IsOverPlatter (POINT pointPx) const;
    const std::wstring  & GetPath       () const { return m_path; }

    //  The screen's picture as it is laid out, empty when it isn't drawn, and
    //  the result line under it.
    bool                  IsScreenShown () const { return HasScreen(); }
    RECT                  GetScreenRect () const { return HasScreen() ? m_screen->GetBounds() : RECT {}; }
    const std::wstring  & GetResultLine () const { return m_bootLine->GetText(); }

    //  Inspect..., at the platter's bottom right or under the map, and the
    //  block map as it is laid out, empty when it isn't drawn; for a test.
    const DxuiButton    & GetInspectButton () const { return *m_inspect; }
    RECT                  GetBlockMapRect  () const { return m_blockMap->IsVisible() ? m_blockMap->GetBounds() : RECT {}; }
    bool                  IsBlockMapHot    () const { return m_blockMap->IsHot(); }

private:
    bool  HandlePlatterMouse (const DxuiMouseEvent & ev);
    bool  HandleScreenMouse  (const DxuiMouseEvent & ev);
    void  SetHot             (bool isHot);
    int   GetInspectWidthPx  () const;
    bool  IsInspectBeside    (int widthPx) const;

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

    void  ApplyResult ();
    bool  HasScreen   () const { return m_picture != nullptr && m_state == State::Ready; }
    bool  IsBlocks    () const { return m_result.blocks != nullptr; }
    bool  IsBeside    (int widthPx) const;

    ExplorerContext                       & m_context;
    InspectorViewContext                    m_view;
    InspectorViewModel                      m_model;
    DiskThumbnailWorker                     m_worker;
    DiskThumbnailResult                     m_result;
    ThumbnailPlatterView                  * m_platter  = nullptr;
    ThumbnailBlockMap                     * m_blockMap = nullptr;
    ThumbnailChips                        * m_chips    = nullptr;
    ThumbnailScreen                       * m_screen   = nullptr;
    DxuiLabel                             * m_bootLine = nullptr;   // the boot result, under the picture
    DxuiButton                            * m_inspect  = nullptr;   // Inspect..., at the platter area's bottom right
    std::shared_ptr<const DxuiIconImage>    m_picture;
    std::wstring                            m_path;
    State                                   m_state    = State::Analyzing;
};
