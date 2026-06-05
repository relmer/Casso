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
//  Opaque font handle. Concrete implementations may store an
//  `IDWriteTextFormat *` or a font-name+size pair behind the
//  `opaque` pointer; widgets never poke at the internals -- they
//  pass the handle to `DxuiTextRenderer::DrawText` / `MeasureString`.
//
struct DxuiFontHandle
{
    void * opaque = nullptr;
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
    virtual uint32_t  HeadingForeground   () const = 0;  // column header / title text

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

    // Caption / system buttons (host-window chrome). `TitleBarTop` /
    // `TitleBarBottom` feed the caption gradient; the system-button
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
