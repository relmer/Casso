#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IVideoTiming
//
//  Cycle-accurate-enough video timing model. Source of IsInVblank() for
//  $C019 (RDVBLBAR).
//
//  NTSC parameters:
//    - 65 CPU cycles per scanline
//    - 262 scanlines per frame
//    - 17,030 CPU cycles per frame
//    - VBL active for scanlines 192..261 (cycle-in-frame >= 12,480)
//
////////////////////////////////////////////////////////////////////////////////

class IVideoTiming
{
public:
    virtual              ~IVideoTiming () = default;

    virtual void          Tick (uint32_t cpuCycles) = 0;
    virtual bool          IsInVblank () const = 0;
    virtual uint32_t      GetCycleInFrame () const = 0;
    virtual void          Reset () = 0;
};
