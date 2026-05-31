#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  InputDebugPanelLayout
//
////////////////////////////////////////////////////////////////////////////////

constexpr int  kInputCategoryCheckCount = 3;



struct InputPanelLayoutSlots
{
    RECT showLabel;
    RECT categoryChecks[kInputCategoryCheckCount];
    RECT pauseButton;
    RECT clearButton;
    RECT listView;
};



InputPanelLayoutSlots ComputeInputDebugPanelLayout (
    int   clientWidthPx,
    int   clientHeightPx,
    int   topOffsetPx,
    UINT  dpi) noexcept;
