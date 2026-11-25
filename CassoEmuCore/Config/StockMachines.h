#pragma once

#include "Pch.h"

struct MachineConfig;





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
//  StockMachines
//
//  The models `--launch` copies. A temporary machine starts from the model's
//  embedded configuration, without the user's saved changes to that model.
//
////////////////////////////////////////////////////////////////////////////////

class StockMachines
{
public:
    static std::span<const StockMachine>  GetAll        ();
    static const StockMachine          &  GetDefault    ();

    // The model with this id, matched without regard to case, or null.
    static const StockMachine          *  Find          (std::string_view id);

    // The error for an id Find doesn't match.
    static std::wstring                   FormatUnknown (std::string_view id);

    static HRESULT                        LoadConfig    (HINSTANCE                      hInstance,
                                                         const StockMachine           & machine,
                                                         const std::vector<fs::path>  & romSearchPaths,
                                                         MachineConfig                & outConfig,
                                                         std::string                  & outError);
};
