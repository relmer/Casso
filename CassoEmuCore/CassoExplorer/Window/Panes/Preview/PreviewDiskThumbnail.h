#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/DiskThumbnailWorker.h"
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

private:
    const DxuiTheme * const  & m_theme;
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
//  PreviewDiskThumbnail
//
//  The small platter and the summary chips above the catalog or details
//  of a disk image selected in a folder (FR-072). The catalog appears at
//  once. The thumbnail is built off the UI thread, with "Analyzing" in its
//  place until it arrives, and a result for a selection that moved on is
//  dropped. A 3.5" WOZ has a note in its place, and an image that does not
//  open has no thumbnail. A click on the platter opens the inspector.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewDiskThumbnail
{
public:
    static constexpr int  kPlatterDip = 180;
    static constexpr int  kGapDip     = 8;

    explicit PreviewDiskThumbnail (ExplorerContext & context);

    void  Create     ();
    void  SetPath    (const std::wstring & path);
    void  TakeResult ();
    void  Layout     (const RECT & area);
    void  Hide       ();

    bool                  IsShown       () const { return !m_path.empty() && m_state != State::Failed; }
    int                   GetHeightPx   () const;
    bool                  IsOverPlatter (POINT pointPx) const;
    const std::wstring  & GetPath       () const { return m_path; }

private:
    enum class State
    {
        Analyzing,
        Ready,
        Note35,
        Failed,
    };

    void  ApplyResult ();

    ExplorerContext       & m_context;
    InspectorViewContext    m_view;
    InspectorViewModel      m_model;
    DiskThumbnailWorker     m_worker;
    DiskThumbnailResult     m_result;
    ThumbnailPlatterView  * m_platter = nullptr;
    ThumbnailChips        * m_chips   = nullptr;
    std::wstring            m_path;
    State                   m_state   = State::Analyzing;
};
