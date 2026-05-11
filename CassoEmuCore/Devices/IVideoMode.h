#pragma once

#include "Pch.h"

class Framebuffer;





////////////////////////////////////////////////////////////////////////////////
//
//  IVideoMode
//
//  Composable video mode. Each mode renders into a shared framebuffer,
//  optionally over a row range (for MIXED + 80COL composition per FR-017a).
//  Apple80ColTextMode::RenderRowRange is the routine used by mixed-mode
//  bottom-4-rows when 80COL=1, satisfying FR-017a's "same composed path"
//  requirement.
//
////////////////////////////////////////////////////////////////////////////////

class IVideoMode
{
public:
    virtual              ~IVideoMode () = default;

    virtual HRESULT       RenderFullScreen (Framebuffer & fb) = 0;
    virtual HRESULT       RenderRowRange   (int firstRow, int lastRow, Framebuffer & fb) = 0;
};
