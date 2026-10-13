#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StateThumbnail
//
//  A machine state's thumbnail: the emulator's picture halved with a box
//  filter and encoded as PNG.
//
////////////////////////////////////////////////////////////////////////////////

class StateThumbnail
{
public:
    static constexpr int  kWidth  = 280;
    static constexpr int  kHeight = 192;

    static HRESULT  Encode (const std::vector<uint32_t> & framebuffer, int width, int height, std::vector<Byte> & outPng);
};
