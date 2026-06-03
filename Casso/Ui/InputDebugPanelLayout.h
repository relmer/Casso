#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  InputDebugPanelLayout
//
////////////////////////////////////////////////////////////////////////////////

struct InputPanelLayoutSlots
{
    RECT emuLabel;
    RECT allCheck;
    RECT emuKeyboardCheck;
    RECT joystickCheck;
    RECT paddleCheck;
    RECT hostLabel;
    RECT hostKeyboardCheck;
    RECT pairLabel[2];
    RECT pairDropdown[2];
    RECT pauseButton;
    RECT clearButton;
    RECT copyButton;
    RECT listView;
};



InputPanelLayoutSlots ComputeInputDebugPanelLayout (
    int   clientWidthPx,
    int   clientHeightPx,
    int   topOffsetPx,
    bool  showJoystickCheck,
    bool  showPaddleCheck,
    UINT  dpi) noexcept;
