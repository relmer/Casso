#pragma once

#include "Pch.h"
#include "Core/IDxuiLayout.h"



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAbsoluteLayout
//
//  Escape-hatch layout: leaves each child's pre-set bounds untouched.
//  Useful for free-form positioning where the host hand-computes
//  bounds for individual controls.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiAbsoluteLayout : public IDxuiLayout
{
public:
    void  Arrange  (const RECT                          & boundsDip,
                    const DxuiDpiScaler                 & scaler,
                    std::span<IDxuiControl * const>       children) override;
};
