#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDiskDriveBus
//
//  A drive's lines on the IWM side: four phase lines, a sense line, and a bit
//  stream one cell at a time at the drive's cell time. `cellEndNs` is the
//  time the cell ends on the IWM's clock. A mechanism ignores it; a bus
//  between two IWMs lines their cells up with it.
//
//  GetQuietUntil returns the time up to which every cell that ends reads 0
//  and reading it changes nothing in the drive, so the IWM can pass over
//  those cells without reading them. It returns kQuietForever when that is
//  true until the drive's lines change, and 0 when it isn't true now.
//
////////////////////////////////////////////////////////////////////////////////

class IDiskDriveBus
{
public:
    static constexpr uint64_t  kQuietForever = UINT64_MAX;

    virtual ~IDiskDriveBus() = default;

    virtual void      SetPhases  (bool ph0, bool ph1, bool ph2, bool ph3) = 0;
    virtual bool      GetSense   () const = 0;
    virtual Byte      ReadCell   (uint64_t cellEndNs) = 0;
    virtual void      WriteCell  (Byte bit, uint64_t cellEndNs) = 0;
    virtual void      Advance    (uint64_t elapsedNs) = 0;
    virtual uint32_t  GetCellNs  () const = 0;

    virtual uint64_t  GetQuietUntil () const { return 0; }
};
