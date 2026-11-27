#pragma once

#include "Pch.h"
#include "Widgets/TextView/DxuiTextLayout.h"

class   DxuiTextPictures;
class   IDxuiPainter;
class   IDxuiTextGeometry;
class   IDxuiTextRenderer;
class   IDxuiTextSelectionRange;
class   IDxuiTextViewport;
class   IDxuiTheme;
struct  DxuiFontHandle;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextPainter
//
//  Draws a text view's lines in sight: each line's selection fill, its
//  characters in their spans' colors and styles, a row's warning mark and
//  rule, and the rows' pictures.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextPainter
{
public:
    DxuiTextPainter (const DxuiTextLayout          & layout,
                     const IDxuiTextGeometry       & geometry,
                     const IDxuiTextViewport       & viewport,
                     const IDxuiTextSelectionRange & selection,
                     const DxuiTextPictures        & pictures)
        : m_layout (layout), m_geometry (geometry), m_viewport (viewport), m_selection (selection), m_pictures (pictures) {}

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiFontHandle & font,
                 const wchar_t * iconFace, float textStrength);

private:
    using Row      = DxuiTextLayout::Row;
    using Span     = DxuiTextLayout::Span;
    using Line     = DxuiTextLayout::Line;
    using Position = DxuiTextLayout::Position;

    float                  GetTextLeft       () const;
    void                   PaintLine         (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int lineIndex, const DxuiFontHandle & font);
    void                   FillSelectedRange (IDxuiPainter & painter, int y, int row, int cell, int cellFrom, int flatStart, int count, float trailPx, int selFrom, int selTo, uint32_t argb) const;
    void                   DrawRun           (IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiFontHandle & font, int y, int row, int cell, int cellFrom,
                                              int flatStart, const std::wstring & chars, bool selected, int selFrom, int selTo,
                                              const std::vector<uint32_t> & colors) const;
    std::vector<uint32_t>  GetRowColors      (const Row & row) const;

    const DxuiTextLayout           & m_layout;
    const IDxuiTextGeometry        & m_geometry;
    const IDxuiTextViewport        & m_viewport;
    const IDxuiTextSelectionRange  & m_selection;
    const DxuiTextPictures         & m_pictures;

    //  The line being painted: its shift for its row's alignment, each
    //  character's style bits, the painter an underline and an inverse
    //  fill are drawn with, the warning mark's face, and how far the text
    //  color goes from the background toward the theme's foreground.
    float                  m_paintShift    = 0.0f;
    std::vector<uint8_t>   m_paintStyles;
    IDxuiPainter         * m_paintPainter  = nullptr;
    const wchar_t        * m_paintIconFace = nullptr;
    float                  m_paintStrength = 1.0f;
};
