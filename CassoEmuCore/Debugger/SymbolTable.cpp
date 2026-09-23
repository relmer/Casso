#include "Pch.h"

#include "Debugger/SymbolTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kSymbolTableNames
//
//  The table each SYM<name> command and bare table name selects.
//
////////////////////////////////////////////////////////////////////////////////

struct SymbolTableName
{
    const char     * name;
    SymbolTableId    id;
};

static constexpr SymbolTableName s_kSymbolTableNames[] =
{
    { "MAIN",   SymbolTableId::Main   }, { "BASIC",  SymbolTableId::Basic  }, { "ASM",   SymbolTableId::Asm   },
    { "USER",   SymbolTableId::User   }, { "USER2",  SymbolTableId::User2  }, { "SRC",   SymbolTableId::Src   },
    { "SRC2",   SymbolTableId::Src2   }, { "DOS33",  SymbolTableId::Dos33  }, { "DOS",   SymbolTableId::Dos33 },
    { "PRODOS", SymbolTableId::ProDos }, { "PRO",    SymbolTableId::ProDos },
};





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::SymbolTable
//
//  Main, Basic and User start enabled, as AppleWin loads them at startup.
//
////////////////////////////////////////////////////////////////////////////////

SymbolTable::SymbolTable()
{
    m_enabled[(int) SymbolTableId::Main]  = true;
    m_enabled[(int) SymbolTableId::Basic] = true;
    m_enabled[(int) SymbolTableId::User]  = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::Add
//
//  A name already in the table takes the new address.
//
////////////////////////////////////////////////////////////////////////////////

void SymbolTable::Add (SymbolTableId table, const std::string & name, Word address)
{
    std::vector<Entry> & entries = m_tables[(int) table];
    std::string          upper   = ToUpper (name);



    for (Entry & entry : entries)
    {
        if (entry.upper == upper)
        {
            entry.address = address;
            return;
        }
    }

    entries.push_back ({ name, upper, address });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::TryRemove
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolTable::TryRemove (SymbolTableId table, const std::string & name)
{
    std::string  upper   = ToUpper (name);
    size_t       removed = std::erase_if (m_tables[(int) table], [&upper] (const Entry & entry) { return entry.upper == upper; });



    return removed > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::Clear
//
////////////////////////////////////////////////////////////////////////////////

void SymbolTable::Clear (SymbolTableId table)
{
    m_tables[(int) table].clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::SetEnabled
//
////////////////////////////////////////////////////////////////////////////////

void SymbolTable::SetEnabled (SymbolTableId table, bool enabled)
{
    m_enabled[(int) table] = enabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::IsEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolTable::IsEnabled (SymbolTableId table) const
{
    return m_enabled[(int) table];
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::GetCount
//
////////////////////////////////////////////////////////////////////////////////

size_t SymbolTable::GetCount (SymbolTableId table) const
{
    return m_tables[(int) table].size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::GetAll
//
//  In address order.
//
////////////////////////////////////////////////////////////////////////////////

void SymbolTable::GetAll (SymbolTableId table, std::vector<SymbolInfo> & symbols) const
{
    for (const Entry & entry : m_tables[(int) table])
    {
        symbols.push_back ({ entry.name, entry.address, table });
    }

    std::stable_sort (symbols.begin(), symbols.end(), [] (const SymbolInfo & a, const SymbolInfo & b) { return a.address < b.address; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::TryResolve
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolTable::TryResolve (const std::string & name, Word & address, SymbolTableId & table) const
{
    for (int i = 0; i < kTableCount; ++i)
    {
        if (m_enabled[i] && TryResolveIn ((SymbolTableId) i, name, address))
        {
            table = (SymbolTableId) i;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::TryFindName
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolTable::TryFindName (Word address, std::string & name, SymbolTableId & table) const
{
    for (int i = 0; i < kTableCount; ++i)
    {
        if (m_enabled[i] && TryFindNameIn ((SymbolTableId) i, address, name))
        {
            table = (SymbolTableId) i;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::FindNames
//
//  Every name an address carries, rather than the first one found.
//
////////////////////////////////////////////////////////////////////////////////

void SymbolTable::FindNames (Word address, std::vector<std::string> & names) const
{
    for (int i = 0; i < kTableCount; i++)
    {
        if (!m_enabled[i])
        {
            continue;
        }

        for (const Entry & entry : m_tables[i])
        {
            if (entry.address == address)
            {
                names.push_back (entry.name);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::TryResolveIn
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolTable::TryResolveIn (SymbolTableId table, const std::string & name, Word & address) const
{
    std::string  upper = ToUpper (name);



    for (const Entry & entry : m_tables[(int) table])
    {
        if (entry.upper == upper)
        {
            address = entry.address;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::TryFindSymbol
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolTable::TryFindSymbol (const std::string & name, SymbolInfo & symbol) const
{
    for (int i = 0; i < kTableCount; ++i)
    {
        if (m_enabled[i] && TryFindSymbolIn ((SymbolTableId) i, name, symbol))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::TryFindSymbolIn
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolTable::TryFindSymbolIn (SymbolTableId table, const std::string & name, SymbolInfo & symbol) const
{
    std::string  upper = ToUpper (name);



    for (const Entry & entry : m_tables[(int) table])
    {
        if (entry.upper == upper)
        {
            symbol = { entry.name, entry.address, table };
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::TryFindNameIn
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolTable::TryFindNameIn (SymbolTableId table, Word address, std::string & name) const
{
    for (const Entry & entry : m_tables[(int) table])
    {
        if (entry.address == address)
        {
            name = entry.name;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::LoadFrom
//
//  The table is replaced by the file's symbols.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SymbolTable::LoadFrom (SymbolTableId table, const std::string & content, int offset, size_t & loaded, std::string & error)
{
    HRESULT                       hr     = S_OK;
    std::vector<SymbolFileEntry>  symbols;
    SymbolFileFormat              format = SymbolFileFormat::Unknown;



    loaded = 0;

    hr = SymbolFileReader::Read (content, symbols, format, error);
    CHR (hr);

    Clear (table);

    for (const SymbolFileEntry & symbol : symbols)
    {
        Add (table, symbol.name, (Word) (symbol.address + offset));
    }

    loaded = symbols.size();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::Format
//
//  NAME=$ADDR lines in address order, which the reader takes back.
//
////////////////////////////////////////////////////////////////////////////////

std::string SymbolTable::Format (SymbolTableId table) const
{
    std::vector<SymbolInfo>  symbols;
    std::string              text = "; by address\n";



    GetAll (table, symbols);

    for (const SymbolInfo & symbol : symbols)
    {
        text += std::format ("{}=${:04X}\n", symbol.name, symbol.address);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::TryGetTableId
//
//  SYMMAIN, SYMBASIC, SYMASM, SYMUSER, SYMUSER2, SYMSRC, SYMSRC2, SYMDOS33,
//  SYMDOS, SYMPRODOS and SYMPRO, or a bare table name such as MAIN.
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolTable::TryGetTableId (const std::string & commandName, SymbolTableId & table)
{
    std::string  upper = ToUpper (commandName);



    if (upper.starts_with ("SYM"))
    {
        upper = upper.substr (3);
    }

    for (const SymbolTableName & candidate : s_kSymbolTableNames)
    {
        if (upper == candidate.name)
        {
            table = candidate.id;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable::ToUpper
//
////////////////////////////////////////////////////////////////////////////////

std::string SymbolTable::ToUpper (const std::string & text)
{
    std::string  upper (text);



    for (char & ch : upper)
    {
        ch = (char) toupper ((unsigned char) ch);
    }

    return upper;
}
