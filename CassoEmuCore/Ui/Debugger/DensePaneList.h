#pragma once

class DxuiListView;





////////////////////////////////////////////////////////////////////////////////
//
//  DensePaneList
//
//  The look every debugger pane's list has: its font, row and header
//  heights, cell padding, and keyboard selection.
//
////////////////////////////////////////////////////////////////////////////////

class DensePaneList
{
public:
    //  The pane font and rows at the usual text size.
    static constexpr float  kFontDip   = 12.0f;
    static constexpr int    kRowDip    = 16;
    static constexpr int    kHeaderDip = 22;
    static constexpr int    kPadDip    = 4;

    static void  MakeDense (DxuiListView * list);
};
