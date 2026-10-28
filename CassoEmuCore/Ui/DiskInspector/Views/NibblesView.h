#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"
#include "Ui/DiskInspector/NibbleSections.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView
//
//  Every nibble of the selected track (FR-041, FR-042), in sections: a
//  header for each address field, data field, sync run and other stretch,
//  with the field's state symbol, its kind and sector and its facts, and
//  under an open header its nibbles in rows of a multiple of 8, each row
//  under the hex offset of its first nibble, in its kind's color, with each
//  sync nibble's width and any extra zero cells after a nibble in small
//  figures beside it. Sync runs are folded to their header line. A click on
//  a header's chevron opens or folds every section of that kind; a click on
//  the rest of a field's header selects its sector. A click on a nibble
//  selects it, a drag or Shift and a click a range of them (FR-043); a
//  selection made elsewhere scrolls its line a third of the way down.
//
////////////////////////////////////////////////////////////////////////////////

class NibblesView : public InspectorView
{
public:
    static constexpr int  kRowDip       = 20;
    static constexpr int  kHeaderDip    = 26;
    static constexpr int  kCellDip      = 34;
    static constexpr int  kOffsetDip    = 44;
    static constexpr int  kNibbleStep   = 8;

    explicit NibblesView (InspectorViewContext & context) : InspectorView (context) {}

    void  Paint      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse    (const DxuiMouseEvent & ev) override;
    bool  GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const override;
    bool  GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const override;

    //  Scrolls so the given nibble's line is about a third of the way down.
    void  ScrollTo (int nibble);

    static int  GetNibblesPerRow (float widthPx, float cellPx, float offsetPx);

    int   GetPerRow     () const;

    //  The header last clicked as one line of text, for Copy, or empty when
    //  a nibble was clicked since.
    std::wstring  GetSelectedHeaderText () const;

private:
    //  One line of the view: a section's header, or a row of its nibbles.
    struct Line
    {
        int   section     = 0;
        int   firstNibble = 0;
        int   count       = 0;
        bool  isHeader    = false;
    };

    void   Rebuild        () const;
    float  GetLineTop     (int line) const;
    float  GetLineHeight  (int line) const;
    int    GetVisibleLines () const;
    int    HitLine        (POINT pointPx) const;
    int    HitTest        (POINT pointPx) const;
    bool   IsOnChevron    (POINT pointPx) const;
    int    GetLastFirstLine () const;
    bool   IsInSelection  (const TrackAnalysis & track, int nibble) const;
    bool   IsInRange      (int nibble) const;
    void   PaintHeader    (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const NibbleSection & section, float y);
    void   PaintRow       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const Line & line, float y);
    static bool      IsOutsideTable (const TrackAnalysis & track, int nibble);
    static uint32_t  GetOwnCells    (const TrackAnalysis & track, int nibble);

    mutable vector<NibbleSection>    m_sections;
    mutable vector<Line>             m_lines;
    mutable const TrackAnalysis    * m_builtFor        = nullptr;
    mutable int                      m_builtPerRow     = 0;
    mutable uint32_t                 m_builtOpen       = 0;
    std::array<bool, static_cast<size_t> (NibbleSectionKind::Count)>  m_isOpen = { false, true, true, true };
    int                              m_firstLine       = 0;
    int                              m_selectedHeader  = -1;
    bool                             m_isDragging      = false;
};
