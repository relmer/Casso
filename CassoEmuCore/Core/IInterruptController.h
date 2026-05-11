#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IInterruptController
//
//  Aggregates per-source IRQ assertions and drives the CPU's IRQ line.
//  Up to 32 sources (single uint32_t bitmask). Assert / Clear recompute the
//  aggregate and call ICpu::SetInterruptLine(kMaskable, ...). Level-sensitive:
//  aggregate=true keeps the line asserted continuously.
//
////////////////////////////////////////////////////////////////////////////////

using IrqSourceId = uint8_t;

class IInterruptController
{
public:
    virtual              ~IInterruptController () = default;

    virtual HRESULT       RegisterSource (IrqSourceId & outId) = 0;
    virtual void          Assert         (IrqSourceId source) = 0;
    virtual void          Clear          (IrqSourceId source) = 0;
    virtual bool          IsAnyAsserted  () const = 0;
};
