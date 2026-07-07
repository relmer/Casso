# Contract: `IDxuiLayout` and concrete layouts

```cpp
// Dxui/Layout/IDxuiLayout.h
#pragma once

#include "Pch.h"


class IDxuiControl;


class IDxuiLayout
{
public:
    virtual ~IDxuiLayout() = default;

    // Required: assign bounds to each child for the supplied parent bounds.
    virtual void Arrange  (const RECT & bounds,
                           const DxuiDpiScaler & scaler,
                           std::span<IDxuiControl * const> children) = 0;

    // Optional: report desired minimum size for a child/container.
    // Default {0,0} means "I have no opinion; the parent's bounds win."
    virtual SIZE Measure  (const DxuiDpiScaler & scaler) { return {0, 0}; }
};
```

## Concretes

### `DxuiStackLayout` (FR-021)

```cpp
class DxuiStackLayout : public IDxuiLayout
{
public:
    enum class Orientation { Horizontal, Vertical };
    enum class Align       { Start, Center, End, Stretch };

    DxuiStackLayout (Orientation orientation,
                     float spacingDip,
                     Align crossAxisAlign);

    // Per-child weight (0 = natural size, >0 = take a share of leftover space).
    void SetWeight  (IDxuiControl & child, float weight);

    void Arrange    (const RECT & bounds,
                     const DxuiDpiScaler & scaler,
                     std::span<IDxuiControl * const> children) override;
    SIZE Measure    (const DxuiDpiScaler & scaler) override;
};
```

### `DxuiGridLayout` (FR-021)

```cpp
class DxuiGridLayout : public IDxuiLayout
{
public:
    DxuiGridLayout (int rows, int cols, float gapDip);
    void SetCell   (IDxuiControl & child, int row, int col, int rowSpan = 1, int colSpan = 1);
    void Arrange   (const RECT & bounds,
                    const DxuiDpiScaler & scaler,
                    std::span<IDxuiControl * const> children) override;
};
```

### `DxuiFormLayout` (FR-021)

```cpp
class DxuiFormLayout : public IDxuiLayout
{
public:
    DxuiFormLayout (float labelColumnDip,
                    float rowHeightDip,
                    float rowGapDip,
                    float sectionGapDip,
                    float subRowIndentDip);

    void AddRow         (IDxuiControl & label, IDxuiControl & field);
    void AddSubRow      (IDxuiControl & label, IDxuiControl & field);
    void AddSectionGap  ();
    void Arrange        (const RECT & bounds,
                         const DxuiDpiScaler & scaler,
                         std::span<IDxuiControl * const> children) override;
};
```

### `DxuiDockLayout` (FR-021, FR-093, SC-013)

```cpp
class DxuiDockLayout : public IDxuiLayout
{
public:
    enum class Edge { Top, Bottom, Left, Right, Fill };

    void SetDock              (IDxuiControl & child, Edge edge);
    void Arrange              (const RECT & bounds,
                               const DxuiDpiScaler & scaler,
                               std::span<IDxuiControl * const> children) override;

    // Inverse fill: given the desired bounds of the FILL child, compute the
    // parent's container size. Used by the emulator to size the Apple ][
    // pixel grid from the inside out.
    static SIZE ContainerSizeForFill (SIZE fillSizeDip,
                                      const std::vector<SIZE> & edgeBands);
};
```

### `DxuiAbsoluteLayout` (FR-021)

```cpp
class DxuiAbsoluteLayout : public IDxuiLayout
{
public:
    // Escape hatch: respect children's pre-set bounds; no rearrangement.
    void Arrange (const RECT & bounds,
                  const DxuiDpiScaler & scaler,
                  std::span<IDxuiControl * const> children) override;
};
```

## Contract notes

- All parameters in DIPs; `Dip` suffix on identifiers (FR-022, FR-082).
- Layouts are stateless w.r.t. children except via the `Set*` helpers; the helpers store per-child metadata in an internal `std::unordered_map<IDxuiControl *, …>`.
- `DxuiDockLayout::ContainerSizeForFill` is `static`; non-fill children passed to it must have fixed `Measure()` results in v1. Flexible/wrap-content non-fill children are unsupported.
