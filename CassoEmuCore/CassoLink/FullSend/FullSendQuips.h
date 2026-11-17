#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendQuips
//
//  The order of the prompt lines on the Apple's Full Send screen.
//
////////////////////////////////////////////////////////////////////////////////

class FullSendQuips
{
public:
    FullSendQuips() = default;
    explicit FullSendQuips (uint32_t seed) : m_random (seed) {}

    Byte  Pick();

private:
    std::mt19937       m_random { std::random_device{}() };
    std::vector<Byte>  m_left;          // the lines left before any repeats
    Byte               m_last   = 0;
};
