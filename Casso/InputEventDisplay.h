#pragma once

#include "Pch.h"

#include "../CassoEmuCore/Devices/InputEvent.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InputEventDisplay
//
////////////////////////////////////////////////////////////////////////////////

struct InputEventDisplay
{
    InputEventCategory        category  = InputEventCategory::System;
    InputEventType            type      = InputEventType::EventsLost;
    uint64_t                  cycle     = 0;

    std::array<wchar_t, 16>   wallStr   {};
    std::array<wchar_t, 12>   uptimeStr {};
    std::array<wchar_t, 24>   cycleStr  {};

    std::wstring              source;
    std::wstring              address;
    std::wstring              value;
    std::wstring              meaning;
};
