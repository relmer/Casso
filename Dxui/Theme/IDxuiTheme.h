#pragma once

#include "Pch.h"



////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiTheme
//
//  Pure-virtual theme interface consumed by every Dxui widget. The
//  concrete implementation lives in the host application and supplies
//  whatever palette and typography that application uses. Widgets MUST
//  depend on this interface only -- never on a concrete host type --
//  so the Dxui framework stays portable to any host that supplies an
//  `IDxuiTheme` implementation.
//
//  All accessors are `const`, allocation-free, and state-free. Theme
//  objects are effectively immutable; a "theme change" swaps the
//  pointer / reference and broadcasts to listeners (Phase 6 wires the
//  broadcast). Colours are packed ARGB (UINT32) for Direct2D friendliness;
//  fonts are returned as opaque `DxuiFontHandle` values that consumers
//  hand to the text renderer without unwrapping.
//
////////////////////////////////////////////////////////////////////////////////



//
//  Font weight on the standard OpenType weight axis. Widgets and themes
//  speak this native enum so the public API never exposes DirectWrite;
//  DxuiTextRenderer maps it to DWRITE_FONT_WEIGHT internally.
//
enum class DxuiFontWeight : int
{
    Normal   = 400,
    SemiBold = 600,
    Bold     = 700,
};



//
//  Semantic text-color role. A text-bearing widget stores WHICH kind of
//  text it is -- not a resolved color -- and the theme maps the role to a
//  packed-ARGB value at paint time via TextColor(). This keeps color out
//  of widget state: a theme change (or a preview theme) simply repaints
//  and every widget re-resolves, so nothing caches a stale color or a
//  dangling theme pointer. (Foreground/background is a fill dichotomy that
//  does not describe text, so text roles are named by their SEMANTICS --
//  Body, Heading, Error, ... -- not by "foreground".)
//
enum class DxuiTextRole
{
    Body,       // primary body text   -> Foreground()
    Heading,    // title / column head -> HeadingForeground()
    Muted,      // secondary / accel   -> ForegroundMuted()
    Disabled,   // disabled-state text -> ForegroundDisabled()
    Error,      // invalid-input text  -> ErrorForeground()
    Link,       // hyperlink text      -> Accent()
};



//
//  Sentinel font size (DIP) meaning "resolve the default at paint time"
//  -- a text-bearing widget left at this size draws with the theme's body
//  font size (IDxuiTheme::BodyFont().sizeDip) rather than a hard-coded
//  value, so default typography lives in one place (the theme).
//
constexpr float kDxuiDefaultFontSizeDip = -1.0f;



//
//  Font descriptor: a face name, DIP size, and weight. Concrete themes
//  return these from the typography accessors; widgets pass them to the
//  text renderer's handle overloads instead of repeating literal face
//  names and sizes. `face` points at a string literal owned by the theme
//  (stable for the process lifetime); a null face means "renderer default".
//
struct DxuiFontHandle
{
    const wchar_t     * face    = nullptr;
    float               sizeDip = 13.0f;
    DxuiFontWeight      weight  = DxuiFontWeight::Normal;
};



class IDxuiTheme
{
public:
    virtual ~IDxuiTheme() = default;

    // Surface backgrounds (packed ARGB).
    virtual uint32_t  Background          () const = 0;  // primary panel fill
    virtual uint32_t  BackgroundElevated  () const = 0;  // popup / dropdown / text-input surface
    virtual uint32_t  HoverBackground     () const = 0;  // row / menu-item hover fill
    virtual uint32_t  PressedBackground   () const = 0;  // pressed-state fill
    virtual uint32_t  SelectionBackground () const = 0;  // selected text / row highlight

    // Foreground / text colours.
    virtual uint32_t  Foreground          () const = 0;  // primary body text
    virtual uint32_t  ForegroundMuted     () const = 0;  // secondary / accelerator text
    virtual uint32_t  ForegroundDisabled  () const = 0;  // disabled-state text
    virtual uint32_t  ErrorForeground     () const = 0;  // invalid-input / error text
    virtual uint32_t  HeadingForeground   () const = 0;  // column header / title text

    //
    //  Resolve a semantic text role to a packed-ARGB color. Default maps
    //  each role onto the accessors above; a theme may override for a
    //  bespoke mapping. Text-bearing widgets call this at paint time so
    //  color never lives in widget state.
    //
    virtual uint32_t  TextColor (DxuiTextRole role) const
    {
        switch (role)
        {
            case DxuiTextRole::Heading:  return HeadingForeground();
            case DxuiTextRole::Muted:    return ForegroundMuted();
            case DxuiTextRole::Disabled: return ForegroundDisabled();
            case DxuiTextRole::Error:    return ErrorForeground();
            case DxuiTextRole::Link:     return Accent();
            case DxuiTextRole::Body:     return Foreground();
        }

        return Foreground();
    }

    // Accent / focus / borders.
    virtual uint32_t  Accent              () const = 0;
    virtual uint32_t  FocusRing           () const = 0;
    virtual uint32_t  Border              () const = 0;
    virtual uint32_t  Divider             () const = 0;

    // Button palette. Default-styled buttons read these directly rather
    // than remapping from Background/Hover/Pressed because button
    // surfaces are typically tinted distinctly from generic panels.
    virtual uint32_t  ButtonIdle          () const = 0;
    virtual uint32_t  ButtonHover         () const = 0;
    virtual uint32_t  ButtonPressed       () const = 0;
    virtual uint32_t  ButtonBorder        () const = 0;
    virtual uint32_t  ButtonText          () const = 0;

    // Caption / system buttons (host-window chrome). `TitleBarTop` /    // `TitleBarBottom` feed the caption gradient; the system-button
    // hover / pressed pairs feed the Win11-style overlay (min and max
    // share the neutral pair; close uses the red pair).
    virtual uint32_t  CaptionBackground   () const = 0;
    virtual uint32_t  CaptionForeground   () const = 0;
    virtual uint32_t  TitleBarTop         () const = 0;
    virtual uint32_t  TitleBarBottom      () const = 0;
    virtual uint32_t  SystemButtonHover   () const = 0;
    virtual uint32_t  SystemButtonPressed () const = 0;
    virtual uint32_t  SystemCloseHover    () const = 0;
    virtual uint32_t  SystemClosePressed  () const = 0;

    // Tooltip surface (popup bubble: distinct from the elevated surface so
    // it reads above whatever it overlays). Border + text complete it.
    virtual uint32_t  TooltipBackground   () const = 0;
    virtual uint32_t  TooltipBorder       () const = 0;
    virtual uint32_t  TooltipForeground   () const = 0;

    // Typography. Returned handles are opaque to widgets.
    virtual DxuiFontHandle  BodyFont      () const = 0;
    virtual DxuiFontHandle  BodyBoldFont  () const = 0;
    virtual DxuiFontHandle  CaptionFont   () const = 0;
    virtual DxuiFontHandle  HeadingFont   () const = 0;
    virtual DxuiFontHandle  MonospaceFont () const = 0;

    // Metrics (device-independent pixels). `BodyLineHeightDip` feeds the
    // focus-manager row-epsilon heuristic in Phase 6.
    virtual float  BodyLineHeightDip () const = 0;
    virtual float  CornerRadiusDip   () const = 0;
    virtual float  FocusRingWidthDip () const = 0;
};
