#pragma once

#include "Pch.h"
#include "DxuiTextPosition.h"
#include "DxuiTextRow.h"


class DxuiTextLayout;
class DxuiTextViewFind;
class DxuiTextViewSelection;
class IDxuiPainter;
class IDxuiTextRenderer;
class IDxuiTheme;
struct DxuiFontHandle;
struct DxuiTextViewport;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewPainter
//
//  Draws a text view's lines for one paint: each line's gutter, its cells'
//  text in their colors and styles over the selection and find marks, the
//  underline of the link under the pointer, and a row's picture.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextViewPainter
{
public:
    using Row       = DxuiTextRow;
    using Line      = DxuiTextLine;
    using Span      = DxuiTextSpan;
    using Picture   = DxuiTextPicture;
    using Position  = DxuiTextPosition;
    using FindMatch = DxuiTextMatch;

    DxuiTextViewPainter (const DxuiTextLayout         & layout,
                         const DxuiTextViewport       & viewport,
                         const DxuiTextViewSelection  & selection,
                         const DxuiTextViewFind       & find) : m_layout (layout), m_viewport (viewport), m_selection (selection), m_find (find) {}

    void  PaintLineGutter (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int lineIndex);
    void  PaintLine       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int lineIndex, const DxuiFontHandle & font);

private:
    //  What a line's runs are drawn over and in: the selected range and its
    //  fill, each character's span color, and the characters of the find's
    //  matches with the colors of their text and of the current one's edge.
    struct LineMarks
    {
        bool                   selected = false;
        int                    selFrom  = 0;
        int                    selTo    = 0;
        uint32_t               selArgb  = 0;
        std::vector<uint32_t>  colors;
        std::vector<bool>      lit;
        uint32_t               litText  = 0;
        uint32_t               litEdge  = 0;
    };

    int                    GetLineY           (int lineIndex) const;
    bool                   GetCellLeftPx      (int row, int cell, float & outX) const;
    LineMarks              GetLineMarks       (const IDxuiTheme & theme, int row) const;
    void                   FillSpans          (IDxuiPainter & painter, const Line & line, int y) const;
    void                   PaintCellRun       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiFontHandle & font, int y, int row, int cell, int cellFrom,
                                               int flatStart, const std::wstring & chars, float trailPx, const LineMarks & marks) const;
    void                   PaintLinkUnderline (IDxuiPainter & painter, const IDxuiTheme & theme, int lineIndex, int y) const;
    void                   PaintPicture       (IDxuiTextRenderer & text, int lineIndex) const;
    std::vector<uint32_t>  GetRowColors       (const Row & row, const IDxuiTheme & theme) const;
    void                   FillHighlights     (IDxuiPainter & painter, const IDxuiTheme & theme, int y, int row, int cell, int cellFrom, int flatStart, int count, uint32_t edge) const;
    void                   FillSelectedRange  (IDxuiPainter & painter, int y, int row, int cell, int cellFrom, int flatStart, int count, float trailPx, int selFrom, int selTo, uint32_t argb) const;
    void                   DrawRun            (IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiFontHandle & font, int y, int row, int cell, int cellFrom,
                                               int flatStart, const std::wstring & chars, const LineMarks & marks) const;
    void                   DrawStretch        (IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiFontHandle & font, float x, int y, float width,
                                               const std::wstring & chars, uint32_t argb, uint8_t style) const;

    //  The line being painted's shift for its row's alignment, each
    //  character's style bits, and the painter an underline and an inverse
    //  fill are drawn with.
    const DxuiTextLayout         & m_layout;
    const DxuiTextViewport       & m_viewport;
    const DxuiTextViewSelection  & m_selection;
    const DxuiTextViewFind       & m_find;
    float                          m_paintShift  = 0.0f;
    std::vector<uint8_t>           m_paintStyles;
    IDxuiPainter                 * m_painter     = nullptr;
};
