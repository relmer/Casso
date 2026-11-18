#pragma once

#include "Ui/Debugger/Panes/HeatMapCellText.h"
#include "Ui/Debugger/Panes/HeatMapGrid.h"

class HeatMapCells;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPainter
//
//  What the heat map draws over and around its picture of the cells: the
//  note in the row of views, the row labels and the range headers, each
//  cell's value or instruction where the cells are large enough, the
//  outlines of the marks and the labeled ranges, the sections' boxes, and
//  the frame around the cell the mouse picks. The cells are read from
//  HeatMapCells and placed by HeatMapGrid.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapPainter
{
public:
    //  The note in the bar while the heat is rebuilt after a move through
    //  history, and the note in place of an empty set.
    static constexpr const wchar_t  * kpszRebuildingNote = L"Rebuilding";
    static constexpr const wchar_t  * kpszNoRangesNote   = L"No ranges in this set. Include some with Edit ranges.";

    //  Any labeled range other than a section is outlined once cells are
    //  kRegionCellPx; an outline's lowest contrast against the page is
    //  WCAG's minimum for a control's border.
    static constexpr int    kRegionCellPx    = 4;
    static constexpr float  kMinLineContrast = 3.0f;

    //  A section's label's size.
    static constexpr float  kSectionLabelDip = 9.0f;

    HeatMapPainter (const HeatMapGrid & grid, const HeatMapCells & cells) : m_grid (grid), m_cells (cells) {}

    void   SetRebuilding  (bool isRebuilding) { m_isRebuilding = isRebuilding; }
    bool   IsRebuilding   () const            { return m_isRebuilding; }
    void   SetNoteRect    (const RECT & rect) { m_noteRect = rect; }

    void   PaintBar       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void   PaintRowLabels (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void   PaintHeaders   (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void   PaintOverlays  (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void   PaintSections  (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void   PaintHover     (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;

private:
    using Band             = HeatMapGrid::Band;
    using Place            = HeatMapGrid::Place;
    using Placed           = HeatMapGrid::Placed;
    using Merged           = HeatMapGrid::Merged;
    using Detail           = HeatMapCellText::Detail;
    using InstructionLines = HeatMapCellText::InstructionLines;

    static constexpr int    kAddressCount     = HeatMapGrid::kAddressCount;
    static constexpr int    kStreetPx         = HeatMapGrid::kStreetPx;
    static constexpr float  kLabelLine        = HeatMapGrid::kLabelLine;
    static constexpr int    kHeaderDip        = HeatMapGrid::kHeaderDip;
    static constexpr int    kSectionPadDip    = HeatMapGrid::kSectionPadDip;
    static constexpr float  kMinDetailFontDip = HeatMapCellText::kMinDetailFontDip;
    static constexpr int    kDetailPadDip     = HeatMapCellText::kDetailPadDip;
    static constexpr int    kValueChars       = HeatMapCellText::kValueChars;
    static constexpr int    kDetailChars      = HeatMapCellText::kDetailChars;
    static constexpr float  kAdvancePerEm     = HeatMapCellText::kAdvancePerEm;
    static constexpr float  kLinePerEm        = HeatMapCellText::kLinePerEm;

    void   PaintDetail      (IDxuiTextRenderer & text, const IDxuiTheme & theme, const Place & place, Word address) const;
    void   PaintRegions     (IDxuiTextRenderer & text) const;
    void   PaintInstruction (IDxuiTextRenderer & text, const IDxuiTheme & theme, const Merged & merged) const;
    void   PaintOutlines    (IDxuiTextRenderer & text, const std::vector<RECT> & rects, Word address) const;

    const HeatMapGrid   & m_grid;
    const HeatMapCells  & m_cells;
    bool                  m_isRebuilding = false;
    RECT                  m_noteRect     = {};
};
