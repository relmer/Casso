#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DialogDefinition
//
//  Pure value type consumed by `DialogPrimitive::Show`. Describes the
//  title, optional icon, wrapped body runs, action buttons, and
//  optional custom-body paint / input hooks for richer consumers
//  (startup-download progress, boot-disk picker list). All types are
//  intentionally Win32-free so the layout math in `DialogLayout` can
//  be unit-tested headlessly.
//
////////////////////////////////////////////////////////////////////////////////



class DxuiPainter;
class DxuiTextRenderer;
struct ChromeTheme;
struct DialogPaintContext;
struct DialogInputEvent;



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
    const ChromeTheme * theme          = nullptr;
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
};



struct DialogDefinition
{
    std::wstring title;
    DialogIcon   icon  = DialogIcon::None;

    // Per-dialog icon size override in DIPs. 0 = use the default
    // primitive icon size.
    float                      iconSizeOverrideDp = 0.0f;
    std::vector<DialogTextRun> body;
    std::vector<DialogButton>  buttons;

    // Custom-body hooks. When `onPaintCustomBody` is set the layout
    // reserves space between the body text and the button row equal
    // to `customBodyMinSizePx`; the primitive then calls the hook on
    // every render frame. `onInputCustomBody` receives input events
    // hit-tested into the custom body rect and may return a result
    // code to request that the dialog close with that value. The
    // primitive passes its own `this` so the hook can drive focus
    // (SetCustomBodyFocus), repaint, or update buttons in place.
    std::function<void (DialogPaintContext &)>                                            onPaintCustomBody;
    std::function<std::optional<int> (const DialogInputEvent &, class DialogPrimitive &)> onInputCustomBody;

    // Custom-body focus integration. When `customBodyFocusableCount > 0`
    // the dialog's Tab ring places that many custom-body stops AHEAD of
    // the action buttons, and focuses custom-body stop 0 on open instead
    // of the default button. `onCustomBodyFocusChanged` fires with the
    // focused stop index (0-based) while a custom-body stop holds focus,
    // or -1 when focus moves onto a button / hyperlink. The custom body
    // uses this to route typed text / edit keys to its focused sub-widget
    // (e.g. a search box vs. a list). Keyboard events still arrive through
    // `onInputCustomBody`; this only tells the body which stop is active.
    int                                  customBodyFocusableCount = 0;
    std::function<void (int focusIndex)> onCustomBodyFocusChanged;

    // Optional measurement hook fired during layout, giving consumers
    // access to a DxuiTextRenderer so they can size the custom body
    // based on string metrics. Returning a non-zero SIZE overrides
    // `customBodyMinSizePx` for that layout pass.
    std::function<SIZE (DxuiTextRenderer &, float dpiScale)> onMeasureCustomBody;
    SIZE                                                     customBodyMinSizePx = {};

    // Optional hook fired when a button is activated (mouse, default,
    // cancel). Receives the button index. Return true to close the
    // dialog with that button's resultCode; return false to keep the
    // dialog open (e.g. to start an async operation in-place). The
    // primitive passes its own `this` so the hook can call
    // SetButtonLabel / SetButtonEnabled / SetButtonVisible / Repaint
    // to update the dialog without closing it.
    std::function<bool (size_t buttonIdx, class DialogPrimitive &)> onButtonActivated;

    // Optional periodic tick. When `tickIntervalMs > 0` the primitive
    // installs a WM_TIMER and invokes `onTick` on every fire. Use to
    // poll worker-thread state and either repaint or call Close. The
    // primitive guarantees this fires only on the UI thread.
    std::function<void (class DialogPrimitive &)> onTick;
    unsigned int                                  tickIntervalMs   = 0;

    // When set, WM_CLOSE (title-bar X, Alt+F4) returns this result
    // code instead of clicking the cancel button. Use to distinguish
    // "user closed the window" from "user clicked Cancel/Skip".
    std::optional<int> closeBoxResult;
};
