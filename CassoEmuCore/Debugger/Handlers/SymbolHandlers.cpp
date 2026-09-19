#include "Pch.h"

#include "Debugger/Handlers/SymbolHandlers.h"

#include "Config/IFileSystem.h"
#include "Debugger/DebugFileReader.h"
#include "Debugger/SymbolFileReader.h"
#include "Debugger/DebugSession.h"
#include "Debugger/ReplyJson.h"
#include "Sha1.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers::TryExecute
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolHandlers::TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    switch (command.verb)
    {
    case DebugVerb::LookupSymbol:   Lookup (session, command, reply); return true;
    case DebugVerb::ShowSymbolInfo: Info   (session, reply);          return true;
    case DebugVerb::ListSymbols:    List   (session, command, reply); return true;
    case DebugVerb::LoadSymbols:    Load   (session, command, reply); return true;
    case DebugVerb::SaveSymbols:    Save   (session, command, reply); return true;
    case DebugVerb::ClearSymbols:   Clear  (session, command, reply); return true;
    case DebugVerb::EnableSymbols:  Enable (session, command, reply); return true;
    case DebugVerb::AddSymbol:      Add    (session, command, reply); return true;
    case DebugVerb::RemoveSymbol:   Remove (session, command, reply); return true;
    default:                                                          return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers::Lookup
//
//  A hex address gives the name at it; anything else is a name to find.
//
////////////////////////////////////////////////////////////////////////////////

void SymbolHandlers::Lookup (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    const SymbolTable  & symbols  = session.GetSymbols();
    std::string          text     = Trim (command.text);
    SymbolTableId        table    = SymbolTableId::Main;
    bool                 hasTable = TryGetTable (command, table);
    bool                 isFound  = false;
    SymbolInfo           symbol;
    SymbolData           data;



    if (TryParseHex (text, symbol.address))
    {
        isFound      = hasTable ? symbols.TryFindNameIn (table, symbol.address, symbol.name) : symbols.TryFindName (symbol.address, symbol.name, symbol.table);
        symbol.table = hasTable ? table : symbol.table;
    }
    else
    {
        isFound = hasTable ? symbols.TryFindSymbolIn (table, text, symbol) : symbols.TryFindSymbol (text, symbol);
    }

    if (!isFound)
    {
        reply.SetError (CommandStatus::Error, "symbol not found",
                        std::format ("{} is not in {}.", text, hasTable ? ReplyJson::GetSymbolTableName (table) : "any enabled symbol table"));
        return;
    }

    data.symbols.push_back (symbol);
    reply.data = data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers::Info
//
//  One line per table: its name, count, and whether it is on.
//
////////////////////////////////////////////////////////////////////////////////

void SymbolHandlers::Info (DebugSession & session, Reply & reply)
{
    const SymbolTable  & symbols = session.GetSymbols();
    MessageData          message;



    for (int i = 0; i < SymbolTable::kTableCount; ++i)
    {
        SymbolTableId  table = (SymbolTableId) i;



        message.lines.push_back (std::format ("{:<7} {:>5} symbols, {}",
                                              ReplyJson::GetSymbolTableName (table),
                                              symbols.GetCount (table),
                                              symbols.IsEnabled (table) ? "on" : "off"));
    }

    reply.data = message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers::List
//
//  SYMLIST table lists one table; SYMLIST alone lists the User table.
//
////////////////////////////////////////////////////////////////////////////////

void SymbolHandlers::List (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    SymbolTableId  table = SymbolTableId::User;
    std::string    text  = Trim (command.text);
    SymbolData     data;



    if (!text.empty() && !SymbolTable::TryGetTableId (text, table))
    {
        reply.SetError (CommandStatus::Error, "invalid arguments",
                        "SYMLIST takes a table: MAIN, BASIC, ASM, USER, USER2, SRC, SRC2, DOS33 or PRODOS.");
        return;
    }

    session.GetSymbols().GetAll (table, data.symbols);
    reply.data = data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers::Load
//
//  SYM<table> LOAD "file"[,offset] replaces the table with the file's
//  symbols, each address moved by the offset.
//
////////////////////////////////////////////////////////////////////////////////

void SymbolHandlers::Load (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IFileSystem   * files  = session.GetFileSystem();
    SymbolTableId   table  = SymbolTableId::User;
    std::string     name;
    std::string     content;
    std::string     error;
    int             offset = 0;
    size_t          loaded = 0;
    HRESULT         hr     = S_OK;



    TryGetTable (command, table);

    if (!TryGetFileName (command.text, name, offset, error))
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", error);
        return;
    }

    if (files == nullptr)
    {
        reply.SetError (CommandStatus::Error, "no file access", "This session cannot read or write host files.");
        return;
    }

    hr = files->ReadAllText (session.ResolvePath (name), content);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not found", std::format ("{} could not be read.", name));
        return;
    }

    if (DebugFileReader::IsDebugFile (content))
    {
        LoadDebugFile (session, table, name, content, offset, false, reply);
        return;
    }

    //  A Merlin listing with assembled lines is its own source (FR-033b); one
    //  holding only a symbol table loads its symbols as before.
    if (SymbolFileReader::Detect (content) == SymbolFileFormat::MerlinListing && IsListingWithLines (content))
    {
        LoadDebugFile (session, table, name, content, offset, true, reply);
        return;
    }

    hr = session.GetSymbols().LoadFrom (table, content, offset, loaded, error);

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "not a symbol file", error);
        return;
    }

    reply.data = MessageData { { std::format ("Loaded {} symbols into {} from {}.", loaded, ReplyJson::GetSymbolTableName (table), name) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers::LoadDebugFile
//
//  A cc65 debug file gives the session its source lines as well as its
//  symbols. The offset moves every segment, and so every line, with the
//  symbols. A file with lines and no top-level symbols still loads. A Merlin
//  listing is read the same way, with the listing as its one source file.
//
////////////////////////////////////////////////////////////////////////////////

void SymbolHandlers::LoadDebugFile (DebugSession & session, SymbolTableId table, const std::string & name,
                                    const std::string & content, int offset, bool isListing, Reply & reply)
{
    DebugFile    file;
    std::string  error;
    size_t       loaded = 0;
    size_t       lines  = 0;
    size_t       slash  = name.find_last_of ("/\\");
    std::string  base   = (slash == std::string::npos) ? name : name.substr (slash + 1);
    HRESULT      hr     = isListing ? DebugFileReader::ReadMerlinListing (content, base, file, error)
                                    : DebugFileReader::Read (content, file, error);



    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "not a debug file", error);
        return;
    }

    for (DebugSegment & segment : file.segments)
    {
        segment.start = (uint32_t) (Word) (segment.start + offset);
    }

    lines = file.lines.size();
    session.SetDebugFile (std::move (file), session.ResolvePath (name), Sha1::ComputeTextHex (content));

    hr = session.GetSymbols().LoadFrom (table, content, offset, loaded, error);

    if (FAILED (hr))
    {
        session.GetSymbols().Clear (table);
        loaded = 0;
    }

    reply.data = MessageData { { std::format ("Loaded {} symbols into {} and {} source lines from {}.",
                                              loaded, ReplyJson::GetSymbolTableName (table), lines, name) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers::IsListingWithLines
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolHandlers::IsListingWithLines (const std::string & content)
{
    DebugFile    file;
    std::string  error;
    HRESULT      hr = DebugFileReader::ReadMerlinListing (content, "", file, error);



    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers::Save
//
////////////////////////////////////////////////////////////////////////////////

void SymbolHandlers::Save (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IFileSystem   * files  = session.GetFileSystem();
    SymbolTableId   table  = SymbolTableId::User;
    std::string     name;
    std::string     error;
    int             offset = 0;
    HRESULT         hr     = S_OK;



    TryGetTable (command, table);

    if (!TryGetFileName (command.text, name, offset, error))
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", error);
        return;
    }

    if (files == nullptr)
    {
        reply.SetError (CommandStatus::Error, "no file access", "This session cannot read or write host files.");
        return;
    }

    hr = files->WriteAllText (session.ResolvePath (name), session.GetSymbols().Format (table));

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not written", std::format ("{} could not be written.", name));
        return;
    }

    reply.data = MessageData { { std::format ("Saved {} symbols from {} to {}.", session.GetSymbols().GetCount (table), ReplyJson::GetSymbolTableName (table), name) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers::Clear
//
////////////////////////////////////////////////////////////////////////////////

void SymbolHandlers::Clear (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    SymbolTableId  table = SymbolTableId::User;



    TryGetTable (command, table);
    session.GetSymbols().Clear (table);
    reply.data = MessageData { { std::format ("Cleared {}.", ReplyJson::GetSymbolTableName (table)) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers::Enable
//
////////////////////////////////////////////////////////////////////////////////

void SymbolHandlers::Enable (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    SymbolTableId  table = SymbolTableId::User;
    bool           isOn  = command.count != 0;



    TryGetTable (command, table);
    session.GetSymbols().SetEnabled (table, isOn);
    reply.data = MessageData { { std::format ("{}: {}", ReplyJson::GetSymbolTableName (table), isOn ? "on" : "off") } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers::Add
//
////////////////////////////////////////////////////////////////////////////////

void SymbolHandlers::Add (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    SymbolTableId  table = SymbolTableId::User;
    SymbolData     data;



    TryGetTable (command, table);
    session.GetSymbols().Add (table, command.text, command.a1);
    data.symbols.push_back ({ command.text, command.a1, table });
    reply.data = data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers::Remove
//
////////////////////////////////////////////////////////////////////////////////

void SymbolHandlers::Remove (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    SymbolTableId  table = SymbolTableId::User;



    TryGetTable (command, table);

    if (!session.GetSymbols().TryRemove (table, command.text))
    {
        reply.SetError (CommandStatus::Error, "symbol not found",
                        std::format ("{} is not in {}.", command.text, ReplyJson::GetSymbolTableName (table)));
        return;
    }

    reply.data = MessageData { { std::format ("Removed {} from {}.", command.text, ReplyJson::GetSymbolTableName (table)) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers::TryGetTable
//
//  SYM<table> carries its table in the name; SYM alone carries none.
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolHandlers::TryGetTable (const DebugCommand & command, SymbolTableId & table)
{
    return SymbolTable::TryGetTableId (command.sourceName, table);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers::TryParseHex
//
//  Up to four hex digits, with or without $. A name made only of hex digits
//  is read as an address, as AppleWin reads it.
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolHandlers::TryParseHex (const std::string & text, Word & value)
{
    static constexpr size_t  kDigits = 4;
    std::string              digits  = text.starts_with ('$') ? text.substr (1) : text;



    if (digits.empty() || digits.size() > kDigits || digits.find_first_not_of ("0123456789ABCDEFabcdef") != std::string::npos)
    {
        return false;
    }

    value = (Word) std::stoul (digits, nullptr, 16);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers::TryGetFileName
//
//  "file" or file, then an optional ,offset in hex.
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolHandlers::TryGetFileName (const std::string & text, std::string & name, int & offset, std::string & error)
{
    std::string  trimmed   = Trim (text);
    size_t       comma     = trimmed.rfind (',');
    size_t       lastQuote = trimmed.find_last_of ("\"'");
    Word         value     = 0;



    offset = 0;

    if (comma != std::string::npos && (lastQuote == std::string::npos || comma > lastQuote))
    {
        if (!TryParseHex (Trim (trimmed.substr (comma + 1)), value))
        {
            error = "The offset after the file name is hex.";
            return false;
        }

        offset  = value;
        trimmed = Trim (trimmed.substr (0, comma));
    }

    if (trimmed.size() >= 2 && (trimmed.front() == '"' || trimmed.front() == '\'') && trimmed.back() == trimmed.front())
    {
        trimmed = trimmed.substr (1, trimmed.size() - 2);
    }

    if (trimmed.empty())
    {
        error = "Give the symbol file's name.";
        return false;
    }

    name = trimmed;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlers::Trim
//
////////////////////////////////////////////////////////////////////////////////

std::string SymbolHandlers::Trim (const std::string & text)
{
    size_t  first = text.find_first_not_of (" \t\r\n");
    size_t  last  = text.find_last_not_of  (" \t\r\n");



    return (first == std::string::npos) ? std::string() : text.substr (first, last - first + 1);
}
