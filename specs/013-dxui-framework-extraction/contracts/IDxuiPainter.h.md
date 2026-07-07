# Contract: `IDxuiPainter`

```cpp
// Dxui/Render/IDxuiPainter.h
#pragma once

#include "Pch.h"


class IDxuiPainter
{
public:
    virtual ~IDxuiPainter() = default;

    // Frame lifecycle (called by DxuiHostWindow / DxuiPopupHost, not widgets)
    virtual HRESULT BeginFrame    (UINT widthPx, UINT heightPx, float dpiScale) = 0;
    virtual HRESULT EndFrame      ()                                            = 0;

    // Primitives (DIPs everywhere; painter scales internally via dpiScale)
    virtual HRESULT Clear         (UINT32 argb)                                                   = 0;
    virtual HRESULT FillRect      (RECT rectDip, UINT32 argb)                                     = 0;
    virtual HRESULT StrokeRect    (RECT rectDip, UINT32 argb, float strokeWidthDip)               = 0;
    virtual HRESULT FillRounded   (RECT rectDip, float radiusDip, UINT32 argb)                    = 0;
    virtual HRESULT StrokeRounded (RECT rectDip, float radiusDip, UINT32 argb, float widthDip)    = 0;
    virtual HRESULT FillGradient  (RECT rectDip, UINT32 argbTop, UINT32 argbBottom)               = 0;
    virtual HRESULT OutlineRect   (RECT rectDip, UINT32 argb)                                     = 0;
    virtual HRESULT FillCircleApprox (POINT centerDip, float radiusDip, UINT32 argb)              = 0;
    virtual HRESULT DrawImage     (RECT rectDip, void * imageOpaque, float opacity)               = 0;

    // Clipping
    virtual HRESULT PushClip      (RECT rectDip)  = 0;
    virtual HRESULT PopClip       ()              = 0;
};
```

## Concrete & mock

- `DxuiPainter` (in `Dxui/Render/DxuiPainter.{h,cpp}`) implements via D3D11 + the existing HLSL shader pipeline. Renamed from `DxUiPainter` in Phase 3; derives from `IDxuiPainter` in Phase 6.
- `MockDxuiPainter` (in `UnitTest/Dxui/MockDxuiPainter.{h,cpp}`) implements every method by appending a `RecordedPaintCall` to an internal vector. `Calls()` accessor exposes the recorded sequence to tests. No D3D device. Returns `S_OK` from everything unless a test explicitly seeds failure.

## Contract notes

- Every method returns `HRESULT` so widgets can use `CHR`/`CHRA` per the EHM pattern.
- DIPs in, painter scales to pixels internally using the `dpiScale` from `BeginFrame`. Widgets do not call `DxuiDpiScaler::Px(...)` per-method (FR-022).
- `DrawImage` takes a `void *` opaque handle to keep the interface decoupled from WIC/Direct2D bitmap types; concrete painter / concrete asset loader collaborate via reinterpret-cast under a sealed contract.
