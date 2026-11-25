#pragma once

#include "Pch.h"

#include "Config/StockMachineList.h"

struct MachineConfig;





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
