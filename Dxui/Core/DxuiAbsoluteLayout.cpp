#include "Pch.h"

#include "Core/DxuiAbsoluteLayout.h"
#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Arrange
//
//  No-op: children keep whatever bounds they were given at SetBounds.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiAbsoluteLayout::Arrange (
    const RECT                          & /*boundsDip*/,
    const DxuiDpiScaler                 & /*scaler*/,
    std::span<IDxuiControl * const>       /*children*/)
{
}
