#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMetrics
//
//  Casso Explorer's padding, gaps and insets, by use. The window's are File
//  Explorer's, measured at the scales noted; Dp values are the toolbar's
//  units.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerMetrics
{
public:
    //  A dialog's form: a row for each field, its label beside it, and an
    //  error message under a box.
    static constexpr int    kFormRowHeightDip        = 30;
    static constexpr int    kFormRowGapDip           = 8;
    static constexpr int    kFormErrorTopDip         = 4;       // between a box and the message under it
    static constexpr int    kFormErrorHeightDip      = 20;      // the message's height before it is measured

    //  A dialog's stacked form: a label above its field.
    static constexpr int    kStackLabelHeightDip     = 22;
    static constexpr int    kStackGapDip             = 6;
    static constexpr int    kStackFieldHeightDip     = 30;

    //  The tabs.
    static constexpr int    kTabTopDip               = 4;
    static constexpr float  kTabLeadDip              = 8.67f;   // the first tab from the strip's left edge, measured at 150%
    static constexpr float  kNewTabGapDip            = 6.67f;   // the last tab to the + button

    //  The navigation bar's back, forward, up and refresh buttons are 48 dip
    //  apart, center to center, the first 34.67 dip from the window's left
    //  edge: measured at 150%. A 12 dip glyph, 16 dip either side and the
    //  toolbar's 4 dip gap. The command bar's padding follows.
    static constexpr float  kNavButtonPadDip         = 16.0f;
    static constexpr int    kNavBarPadDp             = 5;
    static constexpr int    kNavAddressGapDp         = 8;       // from the refresh button to the address box
    static constexpr float  kCommandBarPadDip        = 12.0f;
    static constexpr float  kCommandBarChevronGapDip = 6.0f;    // a drop-down's label to its chevron, measured at 150%
    static constexpr float  kCommandBarIconGapDip    = 6.33f;   // a button's icon to its label, measured at 150%
    static constexpr int    kCommandBarGroupGapDp    = 9;       // 14 px at 150%, with the separator centered in it
    static constexpr int    kCommandBarPadXDp        = 6;
    static constexpr float  kFindBoxRightInsetDip    = 5.33f;   // past the toolbar's padding
    static constexpr int    kFindBoxGapDip           = 8;

    //  The tree. Each level sits in from its parent by File Explorer's
    //  indent, measured at 125% as 10 pixels. A row's icon starts 18 dip past
    //  the middle of its twisty and its label 3.33 dip past the icon:
    //  measured at 150%.
    static constexpr int    kTreeIndentDip           = 8;
    static constexpr float  kTreeIconLeadDip         = 10.0f;
    static constexpr float  kTreeIconGapDip          = 3.33f;
    static constexpr float  kTreeLeftPadDip          = 8.0f;    // the top level from the pane's left edge

    //  The other panes.
    static constexpr int    kSearchMessageTopDip     = 18;      // a search's message below the list's top: 27 pixels at 150%
    static constexpr int    kListActionGapDip        = 12;      // between the two firewall actions
    static constexpr int    kListActionOffsetDip     = 36;      // the list's button below the middle of its message, past two lines of text
    static constexpr float  kStatusLeadDip           = 14.67f;  // the item count, from the band's left edge
    static constexpr int    kPictureInsetDip         = 16;      // room around a picture for its glow, which spills past its edges
    static constexpr int    kHexPaddingDip           = 6;
};
