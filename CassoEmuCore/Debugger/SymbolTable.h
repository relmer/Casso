#pragma once

#include "Debugger/Reply.h"
#include "Debugger/SymbolFileReader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolTable
//
//  Nine tables of name-to-address pairs, each enabled on its own. A name is
//  looked up without regard to case, across the enabled tables in AppleWin's
//  order: Main, Basic, Asm, User, User2, Src, Src2, Dos33, ProDos. An
//  address looks up the same way and gives the first name that holds it.
//
//  A constant, an assembler's equate, resolves by name like any symbol, but
//  its value is not an address: an address never looks up to it, so it
//  labels nothing in a disassembly, and a load offset leaves it where it is.
//
////////////////////////////////////////////////////////////////////////////////

class SymbolTable
{
public:
    static constexpr int  kTableCount = 9;

    SymbolTable();

    void     Add          (SymbolTableId table, const std::string & name, Word address, bool isConstant = false, Word size = 0);
    bool     TryRemove    (SymbolTableId table, const std::string & name);
    void     Clear        (SymbolTableId table);
    void     SetEnabled   (SymbolTableId table, bool enabled);
    bool     IsEnabled    (SymbolTableId table) const;
    size_t   GetCount     (SymbolTableId table) const;
    void     GetAll       (SymbolTableId table, std::vector<SymbolInfo> & symbols) const;

    bool     TryResolve   (const std::string & name, Word & address, SymbolTableId & table) const;
    bool     TryFindName  (Word address, std::string & name, SymbolTableId & table) const;

    //  Every enabled table's name for one address, in table order. One
    //  address carries two names wherever reading it and writing it operate
    //  different soft switches ($C000 is KBD read, 80STOREOFF written).
    void     FindNames    (Word address, std::vector<std::string> & names) const;

    // In one table only, whether or not it is enabled.
    bool     TryResolveIn (SymbolTableId table, const std::string & name, Word & address) const;
    bool     TryFindNameIn (SymbolTableId table, Word address, std::string & name) const;

    // The symbol as stored, with its own spelling of the name.
    bool     TryFindSymbol   (const std::string & name, SymbolInfo & symbol) const;
    bool     TryFindSymbolIn (SymbolTableId table, const std::string & name, SymbolInfo & symbol) const;

    // How many bytes a symbol spans, where its file said: cc65's debug
    // files give a size for a label on data. False when none was given.
    bool     TryGetSize      (const std::string & name, Word & size) const;

    // A number that changes with every change to the tables, their enabled
    // state or their origins, and is never given to two tables' states.
    uint64_t  GetRevision    () const { return m_revision; }

    // A file in any format the reader knows, each address moved by offset.
    HRESULT  LoadFrom     (SymbolTableId table, const std::string & content, int offset, size_t & loaded, std::string & error);

    // The files a table was loaded from, by file name, in load order; a
    // table filled any other way, such as a built-in one, has none.
    void     AddOrigin    (SymbolTableId table, const std::string & fileName);
    const std::vector<std::string> &  GetOrigins (SymbolTableId table) const { return m_origins[(int) table]; }

    // The table in the Casso debug file format.
    std::string  Format   (SymbolTableId table) const;

    static bool  TryGetTableId (const std::string & commandName, SymbolTableId & table);
    static std::string  ToUpper (const std::string & text);

private:
    struct Entry
    {
        std::string  name;
        std::string  upper;
        Word         address    = 0;
        bool         isConstant = false;
        Word         size       = 0;
    };

    static uint64_t  TakeRevision ();
    void             NoteChange   () { m_revision = TakeRevision(); }

    std::vector<Entry>        m_tables[kTableCount];
    std::vector<std::string>  m_origins[kTableCount];
    bool                      m_enabled[kTableCount] = {};
    uint64_t                  m_revision             = TakeRevision();
};
