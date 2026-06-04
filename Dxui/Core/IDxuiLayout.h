#pragma once

#include "Pch.h"
#include "Core/DxuiDpiScaler.h"



////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiLayout
//
//  Container layout policy. A DxuiPanel owns one IDxuiLayout and calls
//  Arrange() to assign bounds to each child for the supplied parent
//  bounds. All sizes are DIPs (FR-022, FR-082).
//
//  Measure() is optional. The default returns {0, 0}, meaning "no
//  opinion -- the parent's bounds win." Concrete layouts override
//  Measure() when they can report a desired minimum size.
//
////////////////////////////////////////////////////////////////////////////////



class IDxuiControl;



class IDxuiLayout
{
public:
    virtual ~IDxuiLayout() = default;

    virtual void  Arrange  (const RECT                          & boundsDip,
                            const DxuiDpiScaler                 & scaler,
                            std::span<IDxuiControl * const>       children)     = 0;

    virtual SIZE  Measure  (const DxuiDpiScaler                 & scaler)       { (void) scaler; return SIZE{ 0, 0 }; }
};
