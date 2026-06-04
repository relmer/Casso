# Contract: `IDxuiViewportInputSink`

```cpp
// Dxui/Core/IDxuiViewportInputSink.h
#pragma once

#include "Pch.h"
#include "DxuiEvents.h"


// Consumer-implemented interface that receives keyboard and mouse events
// forwarded from a focused, input-consuming DxuiViewport.
//
// Casso implements this to route to EmulatorShell / the Apple ][ keyboard
// controller. The viewport itself never paints; it is purely a participation
// hook in the layout tree.
class IDxuiViewportInputSink
{
public:
    virtual ~IDxuiViewportInputSink() = default;

    // Return true if the sink consumed the event; false to let Dxui process it.
    // (In practice, the sink consumes everything except Dxui-reserved chords,
    // which the viewport never forwards in the first place.)
    virtual bool OnViewportKey   (const DxuiKeyEvent & ev)   = 0;
    virtual bool OnViewportMouse (const DxuiMouseEvent & ev) = 0;
};
```

## Reserved chords (NOT forwarded — FR-034)

`DxuiViewport::OnKey` consumes (returns `true`) for every key when focused with `SetConsumesInput(true)`, **except**:

- `Tab` — focus navigation
- `Shift+Tab` — focus navigation reverse
- `Esc` — focus scope / dialog cancel
- `Alt` — menu activation
- `F10` — Dxui-level shortcut

These five always stay in Dxui. Everything else forwards to the sink.

Mouse events follow the same model: when consuming input and the event is inside the viewport rect, forward to the sink. No reserved mouse gestures in v1.
