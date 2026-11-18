#pragma once

#include "Pch.h"
#include "IDxuiTextLayoutSource.h"


class IDxuiPainter;
class IDxuiTextRenderer;
class IDxuiTheme;
struct DxuiFontHandle;





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

    explicit DxuiTextViewPainter (const IDxuiTextLayoutSource & view) : m_view (view) {}

    //  Takes the view's look for the lines painted after it.
    void  Begin           () { m_look = m_view.GetLayoutLook(); }
    void  PaintLineGutter (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int lineIndex);
    void  PaintLine       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int lineIndex, const DxuiFontHandle & font);

private:
    void                   PaintLinkUnderline (IDxuiPainter & painter, const IDxuiTheme & theme, int lineIndex, int y) const;
    void                   PaintPicture       (IDxuiTextRenderer & text, int lineIndex) const;
    std::vector<uint32_t>  GetRowColors       (const Row & row, const IDxuiTheme & theme) const;
    void                   FillHighlights     (IDxuiPainter & painter, const IDxuiTheme & theme, int y, int row, int cell, int cellFrom, int flatStart, int count, uint32_t edge) const;
    void                   FillSelectedRange  (IDxuiPainter & painter, int y, int row, int cell, int cellFrom, int flatStart, int count, float trailPx, int selFrom, int selTo, uint32_t argb) const;
    void                   DrawRun            (IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiFontHandle & font, int y, int row, int cell, int cellFrom,
                                               int flatStart, const std::wstring & chars, bool selected, int selFrom, int selTo,
                                               const std::vector<uint32_t> & colors, const std::vector<bool> & lit, uint32_t litArgb) const;

    //  The view's look from Begin, the line being painted's shift for its
    //  row's alignment, each character's style bits, and the painter an
    //  underline and an inverse fill are drawn with.
    const IDxuiTextLayoutSource  & m_view;
    IDxuiTextLayoutSource::Look    m_look;
    float                          m_paintShift  = 0.0f;
    std::vector<uint8_t>           m_paintStyles;
    IDxuiPainter                 * m_painter     = nullptr;
};
