#pragma once

#include "Pch.h"
#include "Core/DxuiDpiScaler.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewport
//
//  Where a text view's layout is drawn, taken from the view for one paint
//  or one query: the bounds and the edges of the text, the top line, and
//  the settings and marks the painter draws.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiTextViewport
{
    RECT             bounds         = {};
    DxuiDpiScaler    scaler;
    int              padPx          = 0;
    int              textLeft       = 0;      // the text's first column, moved left by a sideways scroll
    int              textBaseLeft   = 0;      // the text area's left, which does not scroll
    int              textAreaRight  = 0;      // short of the vertical scroll bar when it is visible
    float            textWidthPx    = 0.0f;
    int              gutterPx       = 0;
    int              topLine        = 0;
    int              textTopPx      = 0;
    float            textStrength   = 1.0f;
    int              gutterIconDip  = 0;
    float            glyphCenterDip = 0.0f;
    int              bandLeftDip    = 0;
    int              bandWidthDip   = 0;
    const wchar_t  * iconFace       = nullptr;
    int              focusRow       = -1;     // the row drawn as the focused row, -1 for none
    int              highlightRow   = -1;
    int              highlightCell  = 0;
    int              hoverRow       = -1;     // the link under the pointer, -1 for none
    int              hoverSpan      = -1;
};
