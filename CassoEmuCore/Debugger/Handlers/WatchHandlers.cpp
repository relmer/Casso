#include "Pch.h"

#include "Debugger/Handlers/WatchHandlers.h"

#include "Config/IFileSystem.h"
#include "Debugger/AppleWinParser.h"
#include "Debugger/Basic/ApplesoftProgram.h"
#include "Debugger/Basic/BasicStopMonitor.h"
#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/DataDirectiveHandlers.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::TryExecute
//
////////////////////////////////////////////////////////////////////////////////

bool WatchHandlers::TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    switch (command.verb)
    {
    case DebugVerb::AddWatch:               Add    (session, command, WatchListKind::Watch, reply);           return true;
    case DebugVerb::ClearWatch:             Clear  (session, command, WatchListKind::Watch, reply);           return true;
    case DebugVerb::DisableWatch:           Enable (session, command, WatchListKind::Watch, false, reply);    return true;
    case DebugVerb::EnableWatch:            Enable (session, command, WatchListKind::Watch, true,  reply);    return true;
    case DebugVerb::ListWatches:            reply.data = MakeList (session, WatchListKind::Watch);            return true;
    case DebugVerb::SaveWatches:            Save   (session, command, WatchListKind::Watch, reply);           return true;

    case DebugVerb::AddZeroPagePointer:     Add    (session, command, WatchListKind::ZeroPage, reply);        return true;
    case DebugVerb::ClearZeroPagePointer:   Clear  (session, command, WatchListKind::ZeroPage, reply);        return true;
    case DebugVerb::DisableZeroPagePointer: Enable (session, command, WatchListKind::ZeroPage, false, reply); return true;
    case DebugVerb::EnableZeroPagePointer:  Enable (session, command, WatchListKind::ZeroPage, true,  reply); return true;
    case DebugVerb::ListZeroPagePointers:   reply.data = MakeList (session, WatchListKind::ZeroPage);         return true;
    case DebugVerb::SaveZeroPagePointers:   Save   (session, command, WatchListKind::ZeroPage, reply);        return true;

    case DebugVerb::AddBookmark:            Add    (session, command, WatchListKind::Bookmark, reply);        return true;
    case DebugVerb::ClearBookmark:          Clear  (session, command, WatchListKind::Bookmark, reply);        return true;
    case DebugVerb::ListBookmarks:          reply.data = MakeList (session, WatchListKind::Bookmark);         return true;
    case DebugVerb::GoToBookmark:           GoTo   (session, command, reply);                                 return true;
    case DebugVerb::SaveBookmarks:          Save   (session, command, WatchListKind::Bookmark, reply);        return true;

    default:                                                                                                   return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::MakeScript
//
//  A clear line, one add per entry with its slot for zero-page pointers,
//  then a disable line for each disabled entry.
//
////////////////////////////////////////////////////////////////////////////////

std::string WatchHandlers::MakeScript (DebugSession & session, WatchListKind kind)
{
    static constexpr const char * kClear[]   = { "WC *", "ZPC *", "BMC *" };
    static constexpr const char * kAdd[]     = { "WA", "ZP", "BMA" };
    static constexpr const char * kDisable[] = { "WD", "ZPD", nullptr };
    const WatchTable            & table      = GetTable (session, kind);
    std::string                   script     = std::string (kClear[(int) kind]) + "\n";
    size_t                        index      = 0;



    for (const WatchItem & item : table.GetAll())
    {
        script += (kind == WatchListKind::ZeroPage) ? std::format ("{}{} {:04X}\n", kAdd[(int) kind], item.id, item.address)
                : !item.basicName.empty()           ? std::format ("{} BASIC {}\n", kAdd[(int) kind], item.basicName)
                : !item.expression.empty()          ? std::format ("{} EXPR {}\n",  kAdd[(int) kind], item.expression)
                :                                     std::format ("{} {:04X}\n",   kAdd[(int) kind], item.address);
    }

    for (const WatchItem & item : table.GetAll())
    {
        if (!item.enabled && kDisable[(int) kind] != nullptr)
        {
            script += std::format ("{} {}\n", kDisable[(int) kind], kind == WatchListKind::ZeroPage ? item.id : (int) index);
        }

        ++index;
    }

    return script;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::MakeList
//
////////////////////////////////////////////////////////////////////////////////

WatchListData WatchHandlers::MakeList (DebugSession & session, WatchListKind kind, bool isReadingBasic)
{
    WatchListData  data;



    data.kind = kind;

    for (const WatchItem & item : GetTable (session, kind).GetAll())
    {
        WatchEntry  entry;



        entry.id         = item.id;
        entry.address    = item.address;
        entry.enabled    = item.enabled;
        entry.basicName  = item.basicName;
        entry.expression = item.expression;

        if (!item.basicName.empty())
        {
            entry.basicValue = isReadingBasic ? ReadBasicValue (session, item.basicName) : std::nullopt;
        }
        else if (!item.expression.empty())
        {
            ReadTyped (session, item, entry);
        }
        else if (kind != WatchListKind::Bookmark)
        {
            entry.value = PeekWord (session, item.address, kind);
        }

        data.entries.push_back (entry);
    }

    return data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::Add
//
//  ZP0-ZP7 and P0-P4 carry a slot in their name and fill it. ZP and ZPA
//  take the lowest free slot, since only ZP0-ZP7 exist to replay a saved
//  pointer; watches and bookmarks take the next id. The reply lists the
//  entry. With no address nothing is added, and the reply is the list; a
//  watch list then carries the verb a list of watches has.
//
////////////////////////////////////////////////////////////////////////////////

void WatchHandlers::Add (DebugSession & session, const DebugCommand & command, WatchListKind kind, Reply & reply)
{
    WatchTable     & table   = GetTable (session, kind);
    WatchListData    list    = MakeList (session, kind);
    WatchListData    one;
    int              slot    = 0;
    int              id      = 0;
    Word             address = command.a1;
    std::string      name;
    std::string      error;



    if (!command.hasA1)
    {
        if (kind == WatchListKind::Watch)
        {
            reply.verb = DebugVerb::ListWatches;
        }

        reply.data = list;
        return;
    }

    //  A watch's text is resolved here, where the symbols and the program
    //  are: what a typed watch is, a BASIC variable, or the address the text
    //  evaluates to.
    if (kind == WatchListKind::Watch && command.isTypedWatch)
    {
        WatchItem  item;

        if (!TryResolveTyped (session, command.text, item, error))
        {
            reply.SetError (CommandStatus::Error, "invalid arguments", error);
            return;
        }

        id = table.AddTyped (item);
    }
    else if (kind == WatchListKind::Watch && !command.text.empty() && TryGetBasicName (session, command.text, name))
    {
        id = table.AddBasic (name);
    }
    else if (kind == WatchListKind::Watch && !command.text.empty() && !AppleWinParser::TryEvaluate (command.text, session, address, error))
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", error);
        return;
    }
    else if (TryGetSlot (command.sourceName, slot))
    {
        id = table.AddAt (slot, address);
    }
    else if (kind != WatchListKind::ZeroPage)
    {
        id = table.Add (address);
    }
    else if (TryGetFreeSlot (table, slot))
    {
        id = table.AddAt (slot, address);
    }
    else
    {
        reply.SetError (CommandStatus::Error, "no free zero-page pointer", "All eight zero-page pointers are in use.");
        return;
    }

    list     = MakeList (session, kind);
    one.kind = kind;

    for (const WatchEntry & entry : list.entries)
    {
        if (entry.id == id)
        {
            one.entries.push_back (entry);
        }
    }

    reply.data = one;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::TryResolveTyped
//
//  The kind of watch the typed text is, resolved here once: a BASIC
//  variable; a symbol, before a register of the same name, as in an
//  expression; a register; a hex number, an address; or else an expression,
//  which must evaluate now. A name is never read again as something else.
//
////////////////////////////////////////////////////////////////////////////////

bool WatchHandlers::TryResolveTyped (DebugSession & session, const std::string & text, WatchItem & item, std::string & error)
{
    static constexpr const char *  kRegisters[] = { "A", "X", "Y", "P", "S", "PC" };
    std::string                    trimmed      = text;
    std::string                    upper;
    std::string                    name;
    Expression                     parsed;
    Word                           address      = 0;
    int32_t                        value        = 0;
    HRESULT                        hr           = S_OK;



    trimmed.erase (0, std::min (trimmed.size(), trimmed.find_first_not_of (" \t")));
    trimmed.erase (trimmed.find_last_not_of (" \t") + 1);
    upper = trimmed;
    std::ranges::transform (upper, upper.begin(), [] (char ch) { return (char) std::toupper ((unsigned char) ch); });

    if (trimmed.empty())
    {
        error = "There is nothing to watch.";
        return false;
    }

    if (TryGetBasicName (session, trimmed, name))
    {
        item.kind      = WatchItem::Kind::Basic;
        item.basicName = name;
        return true;
    }

    if (session.TryResolveSymbol (trimmed, address))
    {
        item.kind       = WatchItem::Kind::Symbol;
        item.expression = trimmed;
        item.address    = address;
        return true;
    }

    if (std::ranges::any_of (kRegisters, [&upper] (const char * reg) { return upper == reg || upper == std::string ("@") + reg; }))
    {
        item.kind       = WatchItem::Kind::Register;
        item.expression = upper.starts_with ("@") ? upper.substr (1) : upper;
        return true;
    }

    if (trimmed.find_first_not_of ("$0123456789ABCDEFabcdef") == std::string::npos && AppleWinParser::TryEvaluate (trimmed, session, address, error))
    {
        item.kind    = WatchItem::Kind::Address;
        item.address = address;
        return true;
    }

    hr = DebugExpressionEvaluator::Parse (trimmed, parsed, error);

    if (SUCCEEDED (hr))
    {
        hr = DebugExpressionEvaluator::Evaluate (parsed, session, value, error, EvaluationMode::Check);
    }

    if (FAILED (hr))
    {
        return false;
    }

    item.kind       = WatchItem::Kind::Expression;
    item.expression = trimmed;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::ReadTyped
//
//  A register's value, two hex digits or four for PC; a symbol's word at
//  wherever it now is; an expression's value, two digits for a byte read
//  through *, else four. A name no longer known reads as nothing.
//
////////////////////////////////////////////////////////////////////////////////

void WatchHandlers::ReadTyped (DebugSession & session, const WatchItem & item, WatchEntry & entry)
{
    Word         address = 0;
    Word         reg     = 0;
    int32_t      value   = 0;
    std::string  error;
    HRESULT      hr      = S_OK;



    entry.isValueEditable = item.kind == WatchItem::Kind::Symbol;

    switch (item.kind)
    {
    case WatchItem::Kind::Symbol:
        if (session.TryResolveSymbol (item.expression, address))
        {
            entry.address = address;
            entry.value   = PeekWord (session, address, WatchListKind::Watch);
        }

        break;

    case WatchItem::Kind::Register:
        if (session.TryGetRegister (item.expression, reg))
        {
            entry.valueText = (item.expression == "PC") ? std::format ("{:04X}", reg) : std::format ("{:02X}", reg & 0xFF);
        }

        break;

    case WatchItem::Kind::Expression:
        hr = DebugExpressionEvaluator::ParseAndEvaluate (item.expression, session, value, error);

        if (SUCCEEDED (hr))
        {
            entry.valueText = item.expression.starts_with ("*") ? std::format ("{:02X}", value & 0xFF) : std::format ("{:04X}", value & 0xFFFF);
        }

        break;

    default:
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::TryGetBasicName
//
////////////////////////////////////////////////////////////////////////////////

bool WatchHandlers::TryGetBasicName (DebugSession & session, const std::string & text, std::string & name)
{
    static constexpr std::string_view  kBasic   = "BASIC ";
    BasicReference                     reference;
    std::string                        trimmed  = text;
    Word                               address  = 0;
    bool                               isForced = false;
    bool                               isMarked = false;
    ApplesoftProgram                   program ([&session] (Word at) { Byte value = 0; session.TryPeek (at, value); return value; });



    trimmed.erase (0, std::min (trimmed.size(), trimmed.find_first_not_of (" \t")));
    trimmed.erase (trimmed.find_last_not_of (" \t") + 1);

    isForced = trimmed.size() > kBasic.size() && _strnicmp (trimmed.c_str(), kBasic.data(), kBasic.size()) == 0;
    name     = isForced ? trimmed.substr (kBasic.size()) : trimmed;
    name.erase (0, std::min (name.size(), name.find_first_not_of (" \t")));
    std::erase (name, ' ');

    if (!ApplesoftProgram::TryParseReference (name, reference))
    {
        return false;
    }

    std::ranges::transform (name, name.begin(), [] (char ch) { return (char) std::toupper ((unsigned char) ch); });

    if (isForced)
    {
        return true;
    }

    if (session.TryResolveSymbol (name, address))
    {
        return false;
    }

    isMarked = name.find_first_of ("$%(") != std::string::npos;

    return isMarked || (IsProgramRunning (session, program) && program.HasValue (reference));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::ReadBasicValue
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::string> WatchHandlers::ReadBasicValue (DebugSession & session, const std::string & name)
{
    BasicReference    reference;
    std::string       value;
    ApplesoftProgram  program ([&session] (Word at) { Byte peeked = 0; session.TryPeek (at, peeked); return peeked; });



    if (!ApplesoftProgram::TryParseReference (name, reference) || !IsProgramRunning (session, program))
    {
        return std::nullopt;
    }

    value = program.GetValueText (reference);
    return (reference.type == BasicType::String) ? "\"" + value + "\"" : value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::IsProgramRunning
//
////////////////////////////////////////////////////////////////////////////////

bool WatchHandlers::IsProgramRunning (DebugSession & session, const ApplesoftProgram & program)
{
    return BasicStopMonitor::IsApplesoftVisible (session) && program.GetCurrentLine().has_value() &&
           !program.IsAtPrompt (session.GetTarget().GetRegisters().sp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::Clear
//
////////////////////////////////////////////////////////////////////////////////

void WatchHandlers::Clear (DebugSession & session, const DebugCommand & command, WatchListKind kind, Reply & reply)
{
    WatchTable  & table = GetTable (session, kind);
    int           id    = (int) command.count;
    WatchItem     item;



    if (command.text == "*")
    {
        table.ClearAll();
        reply.data = MakeList (session, kind);
        return;
    }

    if (command.count > INT_MAX || !table.TryFind (id, item) || !table.TryClear (id))
    {
        SetNoSuch (reply, kind, command.count);
        return;
    }

    reply.data = MessageData { { item.basicName.empty() ? std::format ("Removed {} {} ({:04X})", GetNoun (kind), id, item.address)
                                                        : std::format ("Removed {} {} ({})", GetNoun (kind), id, item.basicName) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::Enable
//
////////////////////////////////////////////////////////////////////////////////

void WatchHandlers::Enable (DebugSession & session, const DebugCommand & command, WatchListKind kind, bool enabled, Reply & reply)
{
    WatchTable     & table = GetTable (session, kind);
    int              id    = (int) command.count;
    WatchListData    list;
    WatchListData    one;



    if (command.text == "*")
    {
        for (const WatchItem & item : table.GetAll())
        {
            table.TrySetEnabled (item.id, enabled);
        }

        reply.data = MakeList (session, kind);
        return;
    }

    if (command.count > INT_MAX || !table.TrySetEnabled (id, enabled))
    {
        SetNoSuch (reply, kind, command.count);
        return;
    }

    list     = MakeList (session, kind);
    one.kind = kind;

    for (const WatchEntry & entry : list.entries)
    {
        if (entry.id == id)
        {
            one.entries.push_back (entry);
        }
    }

    reply.data = one;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::GoTo
//
//  BMG # lists 20 lines of code at the bookmark.
//
////////////////////////////////////////////////////////////////////////////////

void WatchHandlers::GoTo (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    WatchItem        item;
    DisassemblyData  data;



    if (command.count > INT_MAX || !session.GetBookmarks().TryFind ((int) command.count, item))
    {
        SetNoSuch (reply, WatchListKind::Bookmark, command.count);
        return;
    }

    DataDirectiveHandlers::Disassemble (session, item.address, std::nullopt, kListLines, data);
    reply.data = data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::Save
//
////////////////////////////////////////////////////////////////////////////////

void WatchHandlers::Save (DebugSession & session, const DebugCommand & command, WatchListKind kind, Reply & reply)
{
    IFileSystem  * files = session.GetFileSystem();
    HRESULT        hr    = S_OK;
    size_t         count = 0;



    if (command.text.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", std::format ("{} takes a file name.", command.sourceName));
        return;
    }

    if (files == nullptr)
    {
        reply.SetError (CommandStatus::Error, "no file access", "This session cannot read or write host files.");
        return;
    }

    hr = files->WriteAllText (session.ResolvePath (command.text), MakeScript (session, kind));

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not written", std::format ("{} could not be written.", command.text));
        return;
    }

    count      = GetTable (session, kind).GetAll().size();
    reply.data = MessageData { { std::format ("Saved {} {}{}.", count, count == 1 ? GetNoun (kind) : GetPlural (kind), session.GetPathEcho (command.text, "to")) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::GetPlural
//
////////////////////////////////////////////////////////////////////////////////

const char * WatchHandlers::GetPlural (WatchListKind kind)
{
    static constexpr const char * kPlurals[] = { "watches", "zero-page pointers", "bookmarks" };



    return kPlurals[(int) kind];
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::GetTable
//
////////////////////////////////////////////////////////////////////////////////

WatchTable & WatchHandlers::GetTable (DebugSession & session, WatchListKind kind)
{
    switch (kind)
    {
    case WatchListKind::ZeroPage: return session.GetZeroPage();
    case WatchListKind::Bookmark: return session.GetBookmarks();
    default:                      return session.GetWatches();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::GetNoun
//
////////////////////////////////////////////////////////////////////////////////

const char * WatchHandlers::GetNoun (WatchListKind kind)
{
    static constexpr const char * kNouns[] = { "watch", "zero-page pointer", "bookmark" };



    return kNouns[(int) kind];
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::TryGetSlot
//
//  A name ending in a digit, ZP3 or P1, carries a slot.
//
////////////////////////////////////////////////////////////////////////////////

bool WatchHandlers::TryGetSlot (const std::string & sourceName, int & slot)
{
    char  last = sourceName.empty() ? '\0' : sourceName.back();



    if (!isdigit ((unsigned char) last))
    {
        return false;
    }

    slot = last - '0';
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::TryGetFreeSlot
//
//  The lowest of the eight zero-page pointer slots that holds nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool WatchHandlers::TryGetFreeSlot (const WatchTable & table, int & slot)
{
    static constexpr int  kZeroPageSlots = 8;
    WatchItem             item;



    for (slot = 0; slot < kZeroPageSlots; ++slot)
    {
        if (!table.TryFind (slot, item))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::PeekWord
//
//  No value when either byte cannot be read. A zero-page pointer takes its
//  high byte from within page zero, as (zp) addressing does, so the pointer
//  at $FF reads $FF and $00.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> WatchHandlers::PeekWord (DebugSession & session, Word address, WatchListKind kind)
{
    static constexpr int  kByteBits = 8;
    Byte                  low       = 0;
    Byte                  high      = 0;
    Word                  next      = (Word) (address + 1);



    if (kind == WatchListKind::ZeroPage && address <= 0xFF)
    {
        next = (Word) (next & 0xFF);
    }

    if (!session.TryPeek (address, low) || !session.TryPeek (next, high))
    {
        return std::nullopt;
    }

    return (Word) (low | (high << kByteBits));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::SetNoSuch
//
////////////////////////////////////////////////////////////////////////////////

void WatchHandlers::SetNoSuch (Reply & reply, WatchListKind kind, uint32_t id)
{
    reply.SetError (CommandStatus::Error, std::string ("no such ") + GetNoun (kind),
                    std::format ("There is no {} #{}.", GetNoun (kind), id));
}
