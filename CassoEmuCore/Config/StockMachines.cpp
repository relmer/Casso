#include "Pch.h"

#include "Config/StockMachines.h"
#include "Core/MachineConfig.h"
#include "Core/MachineConfigSource.h"
#include "Core/TextEncoding.h"





static constexpr StockMachine  s_kStockMachines[] =
{
    { "Apple2",          L"Apple ][",           true,  true  },
    { "Apple2Plus",      L"Apple ][+",          true,  true  },
    { "Apple2e",         L"Apple //e",          true,  false },
    { "Apple2eEnhanced", L"Apple //e Enhanced", true,  false },
    { "Apple2c",         L"Apple //c",          false, false },
    { "Apple2cPlus",     L"Apple //c Plus",     false, false },
};

static constexpr size_t  s_kDefaultIndex = 3;





////////////////////////////////////////////////////////////////////////////////
//
//  StockMachines::GetAll
//
////////////////////////////////////////////////////////////////////////////////

std::span<const StockMachine> StockMachines::GetAll()
{
    return s_kStockMachines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StockMachines::GetDefault
//
////////////////////////////////////////////////////////////////////////////////

const StockMachine & StockMachines::GetDefault()
{
    return s_kStockMachines[s_kDefaultIndex];
}





////////////////////////////////////////////////////////////////////////////////
//
//  StockMachines::Find
//
////////////////////////////////////////////////////////////////////////////////

const StockMachine * StockMachines::Find (std::string_view id)
{
    const StockMachine  * found = nullptr;



    for (const StockMachine & machine : s_kStockMachines)
    {
        if (id.size() == strlen (machine.id) && _strnicmp (id.data(), machine.id, id.size()) == 0)
        {
            found = &machine;
            break;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StockMachines::FormatUnknown
//
//  "--machine takes Apple2, Apple2Plus, ... or Apple2cPlus."
//
////////////////////////////////////////////////////////////////////////////////

std::wstring StockMachines::FormatUnknown()
{
    std::wstring  text  = L"--machine takes ";
    size_t        count = std::size (s_kStockMachines);



    for (size_t i = 0; i < count; i++)
    {
        if (i > 0)
        {
            text += (i + 1 == count) ? L" or " : L", ";
        }

        text += TextEncoding::NarrowToWide (s_kStockMachines[i].id);
    }

    text += L".";
    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StockMachines::LoadConfig
//
//  Parses the model's embedded configuration. The user's saved changes to the
//  model and the copy in the Machines folder aren't read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT StockMachines::LoadConfig (
    HINSTANCE                      hInstance,
    const StockMachine           & machine,
    const std::vector<fs::path>  & romSearchPaths,
    MachineConfig                & outConfig,
    std::string                  & outError)
{
    HRESULT      hr       = S_OK;
    std::string  jsonText;



    hr = MachineConfigSource::GetEmbeddedText (hInstance, TextEncoding::NarrowToWide (machine.id), jsonText);
    CHR (hr);

    hr = MachineConfigLoader::Load (jsonText, machine.id, romSearchPaths, outConfig, outError);
    CHR (hr);

Error:
    return hr;
}
