#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTip
//
//  The tip a control has under a point, and how it opens and closes. The
//  window's hit test finds the one control under the pointer, that control
//  returns its tip from IDxuiControl::TryGetTip, and DxuiTipService displays
//  it in the window's one tooltip. The anchor is in the same pixels as the
//  point the control was given.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiTip
{
    //  Where the balloon opens.
    //
    //  AtPointer:     after the open delay, by the pointer where it came to
    //                 rest, and fixed there while the pointer is in the
    //                 anchor, as the system's tips are.
    //  AtAnchor:      after the open delay, against the anchor.
    //  FollowPointer: at once, against the pointer's pixel rather than the
    //                 anchor, and moved along with the pointer, as a hex
    //                 view's address is.
    enum class Placement
    {
        AtPointer,
        AtAnchor,
        FollowPointer,
    };

    //  When the balloon closes.
    //
    //  UntilLeave: when the pointer moves out of the anchor or the window.
    //  ReadTime:   also once it has been up long enough to read.
    //  Timed:      opened at once and closed after timedMs, for a notice that
    //              no pointer move dismisses.
    enum class Lifetime
    {
        UntilLeave,
        ReadTime,
        Timed,
    };

    std::wstring                        text;
    RECT                                anchor       = {};
    Placement                           placement    = Placement::AtPointer;
    Lifetime                            lifetime     = Lifetime::UntilLeave;
    int                                 timedMs      = 0;
    bool                                isMonospace  = false;

    //  What the tip lists is being read: a tip that is up stays as it is,
    //  and no other opens.
    bool                                isPending    = false;

    //  DxuiTooltip::SetLineColors, SetLineSwatches, SetLineScales and
    //  SetGradient; empty for none.
    std::vector<uint32_t>               lineColors;
    std::vector<uint32_t>               lineSwatches;
    std::vector<std::vector<uint32_t>>  lineScales;
    std::vector<uint32_t>               gradient;
    float                               gradientMark = -1.0f;

    //  The tip is what a color means, as a list cell's is. Placed under an
    //  in-place tip that is up, such as a cut-off cell's whole text.
    bool                                isColorKey   = false;
};
