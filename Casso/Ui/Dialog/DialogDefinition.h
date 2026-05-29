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



class DxUiPainter;
class DwriteTextRenderer;
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
    DxUiPainter         * painter        = nullptr;
    DwriteTextRenderer  * text           = nullptr;
    const ChromeTheme   * theme          = nullptr;
    RECT                  customBodyRect = {};
    float                 dpiScale       = 1.0f;
};



struct DialogInputEvent
{
    enum class Kind { MouseMove, LeftButtonDown, LeftButtonUp, KeyDown };
    Kind   kind    = Kind::MouseMove;
    int    xPx     = 0;
    int    yPx     = 0;
    int    vkCode  = 0;     // valid for KeyDown
};



struct DialogDefinition
{
    std::wstring                                       title;
    DialogIcon                                         icon = DialogIcon::None;
    // Per-dialog icon size override in DIPs. 0 = use the default
    // primitive icon size.
    float                                              iconSizeOverrideDp = 0.0f;
    std::vector<DialogTextRun>                         body;
    std::vector<DialogButton>                          buttons;

    // Custom-body hooks. When `onPaintCustomBody` is set the layout
    // reserves space between the body text and the button row equal
    // to `customBodyMinSizePx`; the primitive then calls the hook on
    // every render frame. `onInputCustomBody` receives input events
    // hit-tested into the custom body rect and may return a result
    // code to request that the dialog close with that value.
    std::function<void (DialogPaintContext &)>                          onPaintCustomBody;
    std::function<std::optional<int> (const DialogInputEvent &)>        onInputCustomBody;
    SIZE                                               customBodyMinSizePx = {};
};
