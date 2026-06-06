#pragma once

#include "Pch.h"
#include "Core/IDxuiLayout.h"
#include "Core/IDxuiControl.h"



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockLayout
//
//  Dock-anchored container layout. Each child is tagged with a
//  `DxuiDock` side (`Top` / `Bottom` / `Left` / `Right` / `Fill`).
//  Arrange peels Top/Bottom/Left/Right children off the parent rect
//  in registration order, consuming a slab equal to each child's
//  natural Bounds() extent on the docked axis. The remaining region
//  is given to the first `Fill` child; later `Fill` children collapse
//  to zero size (v1 supports a single fill).
//
//  Children without an explicit `SetDock` call default to `Fill`.
//
//  Constraint: non-fill children must report a fixed natural size in
//  their existing Bounds() (i.e. they measure themselves outside the
//  layout). Wrap-content / flexible non-fill children are unsupported
//  in v1 -- use a nested DxuiStackLayout if you need that.
//
//  ContainerSizeForFill is the inverse: given a desired Fill-region
//  size and the natural sizes of the non-fill children, returns the
//  container rect size required so the Fill slot ends up at exactly
//  the requested dimensions. Used by EmulatorShell to size the host
//  window from the Apple ][ pixel grid outward.
//
////////////////////////////////////////////////////////////////////////////////



enum class DxuiDock
{
    Top,
    Bottom,
    Left,
    Right,
    Fill,
};



class DxuiDockLayout : public IDxuiLayout
{
public:
    DxuiDockLayout  () = default;
    ~DxuiDockLayout () override = default;

    void  SetDock  (IDxuiControl & child, DxuiDock side);
    void  ClearDock(IDxuiControl & child);

    DxuiDock  DockOf (const IDxuiControl & child) const;

    void  Arrange  (const RECT                          & boundsDip,
                    const DxuiDpiScaler                 & scaler,
                    std::span<IDxuiControl * const>       children) override;

    SIZE  ContainerSizeForFill (SIZE                                          desiredFillDip,
                                std::span<IDxuiControl * const>               nonFillChildren) const;

private:
    DxuiDock  LookupDock (const IDxuiControl * child) const;

    std::unordered_map<const IDxuiControl *, DxuiDock>  m_docks;
};
