#pragma once

#include "Pch.h"




////////////////////////////////////////////////////////////////////////////////
//
//  ICycleSink
//
//  Generic per-instruction cycle fan-out seam. EmuCpu::AddCycles ticks an
//  optional sink alongside IVideoTiming so devices that must observe CPU
//  progress between bus accesses (the //c IOU mouse: VBL-edge interrupts +
//  paced movement-interrupt delivery) stay phase-locked without polling.
//  Null-safe: machines that need no sink simply leave it unset.
//
////////////////////////////////////////////////////////////////////////////////

class ICycleSink
{
public:
    virtual         ~ICycleSink() = default;

    virtual void     Tick (uint32_t cpuCycles) = 0;
};
