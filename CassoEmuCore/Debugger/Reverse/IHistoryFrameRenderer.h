#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IHistoryFrameRenderer
//
//  Draws the screen a whole-machine snapshot held, as BGRA pixels, top row
//  first. Called on a worker thread, one snapshot at a time; never on the
//  thread that runs the machine.
//
////////////////////////////////////////////////////////////////////////////////

class IHistoryFrameRenderer
{
public:
    virtual          ~IHistoryFrameRenderer() = default;

    virtual HRESULT  Render (const std::vector<Byte>  & state,
                             std::vector<uint32_t>    & outBgra,
                             int                      & outWidth,
                             int                      & outHeight) = 0;
};
