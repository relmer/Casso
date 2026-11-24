#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"
#include "Ui/DiskInspector/Blocks/BlockHexSource.h"

struct HeaderItem;





////////////////////////////////////////////////////////////////////////////////
//
//  SectorsView
//
//  The Sector data tab: the selected sector's header on fixed lines, the
//  edit bar's row, then its 256 bytes in an editable hex view, with zero
//  bytes muted and bad data marked (FR-040); while comparing, B's paired
//  sector in a second hex view below, the bytes that differ marked in both.
//  The hex views draw the edit look and take typing while the sector is
//  edited (FR-097). The view model has the byte selection, and the main hex
//  view follows it.
//
////////////////////////////////////////////////////////////////////////////////

class SectorsView : public InspectorView
{
public:
    static constexpr int  kRowDip      = 18;
    static constexpr int  kHeaderDip   = 22;
    static constexpr int  kHeaderLines = 4;
    static constexpr int  kBytesPerRow = 16;

    explicit SectorsView (InspectorViewContext & context);

    //  The width a sector's bytes take at full size.
    static int  GetFullWidthPx (const DxuiDpiScaler & scaler);

    void  Layout     (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse    (const DxuiMouseEvent & ev) override;
    bool  GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const override;

    //  Editing (FR-097): typing goes to the selected sector's pending edit at
    //  the caret, in the column last clicked.
    void  SetEditing         (bool isEditing);
    bool  IsEditing          () const { return m_isEditing; }
    bool  OnEditKey          (const DxuiKeyEvent & ev);
    bool  GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const override;

    //  Tab arrives at the hex column and Shift+Tab at the text column.
    void  OnFocusEntered     (bool forward) override;

    //  Pixels at the right of the header's first line reserved for the
    //  window's "Edit" and "Copy sector" buttons.
    void  SetHeaderReserve (int px) { m_headerReservePx = static_cast<float> (px); }

    //  Called as the view is laid out, with the row under the header that the
    //  edit bar goes in. Returns the row's height, which the bytes start below.
    using EditRowFn = std::function<int (const RECT & rowPx)>;
    void  SetEditRow (EditRowFn fn) { m_editRow = std::move (fn); }

    //  Called with the byte double-clicked.
    using OpenFn = std::function<void (int offset)>;
    void  SetOnOpen  (OpenFn fn)    { m_onOpen = std::move (fn); }

    //  The byte the caret is on, moved into view.
    void  GoToByte   (int offset);

private:
    void   PaintHeader     (IDxuiTextRenderer & text, const IDxuiTheme & theme, const AnalyzedSector & sector, const TrackAnalysis & track);
    void   PaintHeaderItem (IDxuiTextRenderer & text, const IDxuiTheme & theme, const HeaderItem & item, float x, float y, float rowH) const;
    int    GetEdit         () const;
    void   ShowBytes       (const AnalyzedSector & sector, const TrackAnalysis & track);
    void   PlaceBytes      (bool isBad, bool isB);
    void   FollowSelection ();
    void   TakeSelection   ();
    int    HitTest         (POINT pointPx) const;
    bool   PickStyle       (const BlockHexSource & source, uint64_t offset, uint8_t mark, bool isSelected, DxuiHexViewTypes::ByteStyle & outStyle) const;

    std::span<const Byte>  GetPairedBytes (const TrackAnalysis & track) const;

    DxuiHexView     m_hexA;
    DxuiHexView     m_hexB;
    BlockHexSource  m_sourceA;
    BlockHexSource  m_sourceB;
    DxuiTheme       m_panelTheme;
    EditRowFn       m_editRow;
    OpenFn          m_onOpen;
    int64_t         m_pressMs         = 0;
    int             m_pressAt         = -1;
    float           m_headerReservePx = 0.0f;
    int             m_bytesTopPx      = 0;     // where the bytes start, below the header and the edit row
    float           m_rowPx           = 0.0f;  // a row of the bytes at full size, once the hex view has measured it
    bool            m_isDragging      = false;
    bool            m_isEditing       = false;
    bool            m_isFollowing     = false; // the main hex view is being set from the view model
    bool            m_isBad           = false;
    bool            m_isB             = false;
};
