#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuPalette, DxuiPopupMenuAnchoring
//
//  A popup menu's colors, and where a child hangs relative to its parent
//  row, or where a root hangs relative to what opened it.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiPopupMenuPalette
{
    uint32_t  bg       = 0;
    uint32_t  hover    = 0;
    uint32_t  text     = 0;
    uint32_t  accel    = 0;
    uint32_t  border   = 0;
    uint32_t  divider  = 0;
    uint32_t  disabled = 0;
};

enum class DxuiPopupMenuAnchoring
{
    Below,
    AtPoint,
    Beside,
};