#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StockMachine
//
//  One model a disk image can be opened on in a temporary machine.
//
//  id        The model's directory name under Resources/Machines.
//  label     The model's row in the Open on menus.
//  hasSlots  Whether the model takes cards.
//  is6502    Whether the model's CPU is the 6502 rather than the 65C02.
//
////////////////////////////////////////////////////////////////////////////////

struct StockMachine
{
    const char     * id       = nullptr;
    const wchar_t  * label    = nullptr;
    bool             hasSlots = false;
    bool             is6502   = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  kStockMachines
//
//  In the order the Open on menus list them. The table is in a header so the
//  shell extension DLL gets it without linking the machine configuration
//  code.
//
////////////////////////////////////////////////////////////////////////////////

inline constexpr StockMachine  kStockMachines[] =
{
    { "Apple2",          L"Apple ][",           true,  true  },
    { "Apple2Plus",      L"Apple ][+",          true,  true  },
    { "Apple2e",         L"Apple //e",          true,  false },
    { "Apple2eEnhanced", L"Apple //e Enhanced", true,  false },
    { "Apple2c",         L"Apple //c",          false, false },
    { "Apple2cPlus",     L"Apple //c Plus",     false, false },
};
