# Contract: `IDxuiTheme`

```cpp
// Dxui/Theme/IDxuiTheme.h
#pragma once

#include "Pch.h"


// Opaque font handle. Concrete impls (Casso's ChromeTheme) hand out
// IDWriteTextFormat* under the hood; widgets don't unwrap.
struct DxuiFontHandle
{
    void * opaque = nullptr;
};


class IDxuiTheme
{
public:
    virtual ~IDxuiTheme() = default;

    // Surface colours (ARGB packed UINT32)
    virtual UINT32 Background           () const = 0;
    virtual UINT32 BackgroundElevated   () const = 0;
    virtual UINT32 HoverBackground      () const = 0;
    virtual UINT32 PressedBackground    () const = 0;
    virtual UINT32 SelectionBackground  () const = 0;

    // Foreground / text colours
    virtual UINT32 Foreground           () const = 0;
    virtual UINT32 ForegroundMuted      () const = 0;
    virtual UINT32 ForegroundDisabled   () const = 0;

    // Accent / focus
    virtual UINT32 Accent               () const = 0;
    virtual UINT32 FocusRing            () const = 0;

    // Borders / dividers
    virtual UINT32 Border               () const = 0;
    virtual UINT32 Divider              () const = 0;

    // Chrome-specific (caption bar / system buttons)
    virtual UINT32 CaptionBackground    () const = 0;
    virtual UINT32 CaptionForeground    () const = 0;
    virtual UINT32 SystemButtonHover    () const = 0;
    virtual UINT32 SystemCloseHover     () const = 0;  // typically red

    // Typography
    virtual DxuiFontHandle BodyFont     () const = 0;
    virtual DxuiFontHandle BodyBoldFont () const = 0;
    virtual DxuiFontHandle CaptionFont  () const = 0;
    virtual DxuiFontHandle HeadingFont  () const = 0;
    virtual DxuiFontHandle MonospaceFont() const = 0;

    // Metrics (DIP)
    virtual float  BodyLineHeightDip    () const = 0;
    virtual float  CornerRadiusDip      () const = 0;
    virtual float  FocusRingWidthDip    () const = 0;
};
```

## Contract notes

- Casso's existing `ChromeTheme` (under `Casso/Ui/Chrome/ChromeTheme.h`) becomes the concrete implementation in Phase 5 (FR-092). Skeuomorphic palette + scanline tint stay in `ChromeTheme`; `IDxuiTheme` exposes only what widgets need.
- Colours are `UINT32` ARGB to keep the interface allocation-free and Direct2D-friendly.
- `DxuiFontHandle` is intentionally opaque — widgets pass it to `IDxuiTextRenderer::DrawText` / `Measure` and don't poke at its internals.
- All methods are `const`. Theme objects are effectively immutable; theme **changes** = swap the theme pointer and broadcast `OnThemeChanged` via `IDxuiControl` (FR-033).
