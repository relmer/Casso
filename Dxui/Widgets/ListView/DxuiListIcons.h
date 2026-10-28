#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListIcons
//
//  How an icon is drawn in a list's rows and items and in the tree: ghosted
//  for a hidden item, framed when it is a small picture, and badged when the
//  item cannot be opened.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListIcons
{
public:
    //  How opaque a ghosted icon is: Explorer's hidden items, measured.
    static constexpr float     s_kGhostedIconAlpha = 0.5f;
    static constexpr uint32_t  s_kBrokenBadgeArgb  = 0xFFD13438;   // Windows' error red

    //  The badge on an item that cannot be opened, over its icon's lower-left
    //  corner. On the text layer, which icons are drawn on, so it goes over
    //  its icon.
    static void      PaintBrokenBadge   (IDxuiTextRenderer & text, float iconX, float iconY, float iconPx, float minPx);
    static void      PaintSmallArtFrame (IDxuiPainter & painter, float x, float y, float sizePx, uint32_t bg);
    static uint32_t  ShiftGray          (uint32_t argb, int delta);
};
