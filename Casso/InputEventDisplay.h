#pragma once

#include "Pch.h"

#include "../CassoEmuCore/Devices/InputEvent.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InputGamePortClass
//
//  Projection-time classification of a guest game-port access by which
//  analog axis pair it belongs to. The Joystick-vs-Paddle interpretation
//  is applied later at filter time (it depends on the user's per-pair view
//  dropdowns), so the projection only records the stable pair identity.
//
//      None    -- not a game-port access (keyboard read, host, or system).
//      Pair0   -- axes 0/1: PADDL0/PADDL1 ($C064/$C065), buttons PB0/PB1
//                  ($C061/$C062).
//      Pair1   -- axes 2/3: PADDL2/PADDL3 ($C066/$C067), button PB2 ($C063).
//      Global  -- the $C070 PTRIG strobe, which arms every axis at once.
//
////////////////////////////////////////////////////////////////////////////////

enum class InputGamePortClass : uint8_t
{
    None   = 0,
    Pair0  = 1,
    Pair1  = 2,
    Global = 3,
};




////////////////////////////////////////////////////////////////////////////////
//
//  InputEventDisplay
//
////////////////////////////////////////////////////////////////////////////////

struct InputEventDisplay
{
    InputEventCategory        category  = InputEventCategory::System;
    InputEventType            type      = InputEventType::EventsLost;
    InputGamePortClass        gamePort  = InputGamePortClass::None;
    uint64_t                  cycle     = 0;

    std::array<wchar_t, 16>   wallStr   {};
    std::array<wchar_t, 12>   uptimeStr {};
    std::array<wchar_t, 24>   cycleStr  {};

    std::wstring              source;
    std::wstring              address;
    std::wstring              value;
    std::wstring              meaning;
};
