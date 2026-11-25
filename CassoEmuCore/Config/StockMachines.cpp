#include "Pch.h"

#include "Config/StockMachines.h"
#include "Core/MachineConfig.h"
#include "Core/MachineConfigSource.h"
#include "Core/TextEncoding.h"





static constexpr size_t  s_kDefaultIndex = 3;





////////////////////////////////////////////////////////////////////////////////
//
//  StockMachines::GetAll
//
////////////////////////////////////////////////////////////////////////////////

std::span<const StockMachine> StockMachines::GetAll()
{
    return kStockMachines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StockMachines::GetDefault
//
////////////////////////////////////////////////////////////////////////////////

const StockMachine & StockMachines::GetDefault()
{
    return kStockMachines[s_kDefaultIndex];
}





////////////////////////////////////////////////////////////////////////////////
//
//  StockMachines::Find
//
////////////////////////////////////////////////////////////////////////////////

const StockMachine * StockMachines::Find (std::string_view id)
{
    const StockMachine  * found = nullptr;



    for (const StockMachine & machine : kStockMachines)
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
//  "Unknown machine 'Foo'. Expected Apple2, Apple2Plus, ... or Apple2cPlus."
//
////////////////////////////////////////////////////////////////////////////////

std::wstring StockMachines::FormatUnknown (std::string_view id)
{
    std::wstring  text  = std::format (L"Unknown machine '{}'. Expected ", TextEncoding::NarrowToWide (std::string (id)));
    size_t        count = std::size (kStockMachines);



    for (size_t i = 0; i < count; i++)
    {
        if (i > 0)
        {
            text += (i + 1 == count) ? L" or " : L", ";
        }

        text += TextEncoding::NarrowToWide (kStockMachines[i].id);
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
