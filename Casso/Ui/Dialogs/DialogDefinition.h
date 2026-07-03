#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DialogDefinition
//
//  Pure value type describing a modal dialog: title, optional icon,
//  wrapped body runs (plain + hyperlink), action buttons, and the
//  window-close result code. Consumed by EmulatorShell::ShowModalDialog,
//  which hosts it as a MessageDialog (a DxuiWindow shown via ShowDialog).
//  All types are intentionally Win32-light so the value can be built
//  headlessly.
//
////////////////////////////////////////////////////////////////////////////////



class DxuiPainter;
class DxuiTextRenderer;
struct CassoTheme;



enum class DialogIcon
{
    None,
    AppPhotoreal,    // IDI_CASSO_PHOTOREAL
    AppFlat,         // IDI_CASSO_FLAT
    Info,
    Warning,
    Error
};



struct DialogTextRun
{
    std::wstring  text;
    bool          isHyperlink   = false;
    std::wstring  hyperlinkUrl;   // ignored unless isHyperlink == true
};



struct DialogButton
{
    std::wstring  label;
    int           resultCode = 0;
    bool          isDefault  = false;
    bool          isCancel   = false;
};



struct DialogPaintContext
{
    DxuiPainter       * painter        = nullptr;
    DxuiTextRenderer  * text           = nullptr;
    const CassoTheme * theme          = nullptr;
    RECT                customBodyRect = {};
    float               dpiScale       = 1.0f;
};



struct DialogInputEvent
{
    enum class Kind { MouseMove, LeftButtonDown, LeftButtonUp, KeyDown, Char, Wheel };
    Kind     kind       = Kind::MouseMove;
    int      xPx        = 0;
    int      yPx        = 0;
    int      vkCode     = 0;     // valid for KeyDown
    wchar_t  ch         = 0;     // valid for Char
    int      wheelDelta = 0;     // valid for Wheel (WHEEL_DELTA multiples, +up / -down)
    bool     wheelHorz  = false; // Wheel: true = horizontal (WM_MOUSEHWHEEL), false = vertical
};



struct DialogDefinition
{
    std::wstring title;
    DialogIcon   icon  = DialogIcon::None;

    // Per-dialog icon size override in DIPs. 0 = use the default
    // icon size.
    float                      iconSizeOverrideDp = 0.0f;
    std::vector<DialogTextRun> body;
    std::vector<DialogButton>  buttons;

    // When set, a window-close gesture (title-bar X, Alt+F4, Escape)
    // returns this result code instead of the cancel button's. Use to
    // distinguish "user closed the window" from "user clicked Cancel".
    std::optional<int> closeBoxResult;
};
