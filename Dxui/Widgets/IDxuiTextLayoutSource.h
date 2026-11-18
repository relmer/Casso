#pragma once

#include "Pch.h"
#include "DxuiTextPosition.h"
#include "DxuiTextRow.h"


class DxuiDpiScaler;
class DxuiTextViewFind;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiTextLayoutSource
//
//  A text view's layout, read by its painter and its hit tests: its rows
//  and lines, the selection and find marks, its settings, and where each
//  part of a row lands.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiTextLayoutSource
{
public:
    using Row      = DxuiTextRow;
    using Line     = DxuiTextLine;
    using Position = DxuiTextPosition;

    //  The view's settings and state.
    struct Look
    {
        float            cellAdvance    = 0.0f;
        int              cellWidthPx    = 0;
        int              cellHeightPx   = 0;
        int              textTopPx      = 0;
        int              padPx          = 0;
        int              topLine        = 0;
        bool             proportional   = false;
        float            textStrength   = 1.0f;
        int              gutterIconDip  = 0;
        float            glyphCenterDip = 0.0f;
        int              bandLeftDip    = 0;
        int              bandWidthDip   = 0;
        const wchar_t  * iconFace       = nullptr;
        int              highlightRow   = -1;
        int              highlightCell  = 0;
        int              hoverRow       = -1;
        int              hoverSpan      = -1;
    };

    virtual                            ~IDxuiTextLayoutSource() = default;

    virtual const std::vector<Row>   & GetLayoutRows        () const = 0;
    virtual const std::vector<Line>  & GetLayoutLines       () const = 0;
    virtual const DxuiDpiScaler      & GetLayoutScaler      () const = 0;
    virtual RECT                       GetLayoutBounds      () const = 0;
    virtual Position                   GetLayoutAnchor      () const = 0;
    virtual Position                   GetLayoutCaret       () const = 0;
    virtual const DxuiTextViewFind   & GetLayoutFind        () const = 0;
    virtual Look                       GetLayoutLook        () const = 0;
    virtual bool                       HasSelection         () const = 0;
    virtual bool                       IsRowSelectionShown  (int row) const = 0;
    virtual bool                       IsScrollbarVisible   () const = 0;
    virtual int                        GetGutterPx          () const = 0;
    virtual int                        GetTextLeft          () const = 0;
    virtual int                        GetTextBaseLeft      () const = 0;
    virtual int                        GetTextAreaRightPx   () const = 0;
    virtual float                      GetTextWidthPx       (bool scrollbar) const = 0;
    virtual int                        GetLineHeightPx      (int lineIndex) const = 0;
    virtual int                        GetLineOffsetPx      (int lineIndex) const = 0;
    virtual float                      GetLineShift         (int lineIndex) const = 0;
    virtual int                        GetRowLength         (const Row & row) const = 0;
    virtual int                        GetCellBase          (const Row & row, int cell) const = 0;
    virtual float                      GetCellX             (const Row & row, int cell) const = 0;
    virtual bool                       GetCellLeftPx        (int row, int cell, float & outX) const = 0;
    virtual float                      GetCharsPx           (int row, int cell, int from, int to) const = 0;
    virtual float                      GetPartPx            (int row, int cell, int cellFrom, int index) const = 0;
    virtual bool                       GetLinePart          (const Row & row, const Line & line, int cell, int & outFlat, int & outLength, int & outCellFrom) const = 0;
    virtual std::vector<uint8_t>       GetRowStyles         (const Row & row) const = 0;
    virtual int                        GetCellStart         (const Row & row, int cell) const = 0;
    virtual int                        GetLineColumn        (const Row & row, const Line & line) const = 0;
    virtual int                        GetNearestChar       (int row, int cell, int from, int to, float x) const = 0;
    virtual bool                       IsOverScrollbar      (POINT pt) const = 0;
};
