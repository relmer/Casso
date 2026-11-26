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
//  romBoot   What the model's ROM boots at power on.
//  has525    Whether the stock model has a 5.25-inch drive. The //c Plus's
//            built-in drive is a 3.5-inch one.
//
////////////////////////////////////////////////////////////////////////////////

enum class RomBoot
{
    None,         // the original ][ monitor, which stops at its * prompt
    DiskIiOnly,   // the Autostart and //e ROMs, which pass over a card without the Disk II's signature
    AnyCard,      // the enhanced //e and //c ROMs, which boot any card with a ProDOS signature
};


struct StockMachine
{
    const char     * id       = nullptr;
    const wchar_t  * label    = nullptr;
    bool             hasSlots = false;
    bool             is6502   = false;
    RomBoot          romBoot  = RomBoot::AnyCard;
    bool             has525   = true;
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
    { "Apple2",          L"Apple ][",           true,  true,  RomBoot::None,       true  },
    { "Apple2Plus",      L"Apple ][+",          true,  true,  RomBoot::DiskIiOnly, true  },
    { "Apple2e",         L"Apple //e",          true,  false, RomBoot::DiskIiOnly, true  },
    { "Apple2eEnhanced", L"Apple //e Enhanced", true,  false, RomBoot::AnyCard,    true  },
    { "Apple2c",         L"Apple //c",          false, false, RomBoot::AnyCard,    true  },
    { "Apple2cPlus",     L"Apple //c Plus",     false, false, RomBoot::AnyCard,    false },
};
