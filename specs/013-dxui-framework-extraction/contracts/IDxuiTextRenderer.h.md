# Contract: `IDxuiTextRenderer`

```cpp
// Dxui/Render/IDxuiTextRenderer.h
#pragma once

#include "Pch.h"
#include "IDxuiTheme.h"


enum class DxuiTextAlign     { Left,  Center, Right };
enum class DxuiTextVerticalAlign { Top, Center, Bottom };


class IDxuiTextRenderer
{
public:
    virtual ~IDxuiTextRenderer() = default;

    // Measure
    virtual HRESULT Measure   (const std::wstring & text,
                               DxuiFontHandle font,
                               float maxWidthDip,
                               SIZE & outSizeDip) = 0;

    // Draw
    virtual HRESULT DrawText  (RECT rectDip,
                               const std::wstring & text,
                               UINT32 argb,
                               DxuiFontHandle font,
                               DxuiTextAlign h = DxuiTextAlign::Left,
                               DxuiTextVerticalAlign v = DxuiTextVerticalAlign::Top) = 0;
};
```

## Concrete & mock

- `DxuiTextRenderer` (in `Dxui/Render/DxuiTextRenderer.{h,cpp}`) implements via Direct2D + DirectWrite. Renamed from `DwriteTextRenderer` in Phase 3; derives from `IDxuiTextRenderer` in Phase 6.
- `MockDxuiTextRenderer` records each `DrawText` invocation; `Measure` returns a canned `SIZE` derived from `text.length() * canonicalGlyphWidthDip` (e.g., 7.0f) and `lineHeightDip` (e.g., 16.0f). Deterministic, allocation-free in the steady state.

## Contract notes

- All text is `std::wstring` (FR-080).
- DIPs in, scaled internally — same rule as `IDxuiPainter` (FR-022).
- `Measure` reports the wrapped block size if `maxWidthDip > 0`, else the single-line size. Widgets that need word-by-word metrics should fold them in widget-side; the interface stays small.
- No font enumeration / discovery in the interface — that lives in the concrete renderer's construction.
