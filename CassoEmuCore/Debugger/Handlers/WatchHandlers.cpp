#include "Pch.h"

#include "Debugger/Handlers/WatchHandlers.h"

#include "Config/IFileSystem.h"
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
        script += (kind == WatchListKind::ZeroPage)
                ? std::format ("{}{} {:04X}\n", kAdd[(int) kind], item.id, item.address)
                : std::format ("{} {:04X}\n",   kAdd[(int) kind], item.address);
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

WatchListData WatchHandlers::MakeList (DebugSession & session, WatchListKind kind)
{
    WatchListData  data;



    data.kind = kind;

    for (const WatchItem & item : GetTable (session, kind).GetAll())
    {
        WatchEntry  entry;



        entry.id      = item.id;
        entry.address = item.address;
        entry.enabled = item.enabled;

        if (kind != WatchListKind::Bookmark)
        {
            entry.value = PeekWord (session, kind, item.address);
        }

        data.entries.push_back (entry);
    }

    return data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::Add
//
//  An address adds an entry, and the reply lists that entry alone. With no
//  address the reply is the whole list, under the list's own verb, so a
//  format that writes an add and a list differently writes a list.
//
////////////////////////////////////////////////////////////////////////////////

void WatchHandlers::Add (DebugSession & session, const DebugCommand & command, WatchListKind kind, Reply & reply)
{
    static constexpr DebugVerb    kListVerbs[] = { DebugVerb::ListWatches, DebugVerb::ListZeroPagePointers, DebugVerb::ListBookmarks };
    WatchTable                  & table        = GetTable (session, kind);
    int                           id           = 0;



    if (!command.hasA1)
    {
        reply.verb = kListVerbs[(int) kind];
        reply.data = MakeList (session, kind);
        return;
    }

    if (kind != WatchListKind::ZeroPage)
    {
        id = table.Add (command.a1);
    }
    else if (!TryAddPointer (table, command, reply, id))
    {
        return;
    }

    reply.data = MakeEntry (session, kind, id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::TryAddPointer
//
//  A ZERO-PAGE POINTER HAS ONE OF EIGHT SLOTS, as AppleWin's do. ZP0-ZP7 and
//  P0-P4 fill the slot their name carries, replacing what was there; ZP and
//  ZPA take the lowest free slot, and there is none once all eight are set.
//  That keeps every entry one ZPSAVE can write as a command that recreates
//  it.
//
//  The address is in zero page, where the pointer's two bytes are: a
//  pointer at $FF takes its high byte from $00, as the 6502 does.
//
////////////////////////////////////////////////////////////////////////////////

bool WatchHandlers::TryAddPointer (WatchTable & table, const DebugCommand & command, Reply & reply, int & id)
{
    int  slot = 0;



    if (command.a1 > kLastZeroPage)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments",
                        std::format ("${:04X} is not in zero page. A zero-page pointer is at an address from $00 to $FF.", command.a1));
        return false;
    }

    if (!TryGetSlot (command.sourceName, slot) && !TryFindFreeSlot (table, slot))
    {
        reply.SetError (CommandStatus::Error, "zero-page pointers full",
                        "All eight zero-page pointers are set. Clear one with ZPC, or replace one with ZP0 to ZP7.");
        return false;
    }

    id = table.AddAt (slot, command.a1);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::TryFindFreeSlot
//
////////////////////////////////////////////////////////////////////////////////

bool WatchHandlers::TryFindFreeSlot (const WatchTable & table, int & slot)
{
    WatchItem  item;



    for (int candidate = 0; candidate <= kLastSlot; ++candidate)
    {
        if (!table.TryFind (candidate, item))
        {
            slot = candidate;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::MakeEntry
//
//  The list holding only the entry with the given id, as an add or an enable
//  replies.
//
////////////////////////////////////////////////////////////////////////////////

WatchListData WatchHandlers::MakeEntry (DebugSession & session, WatchListKind kind, int id)
{
    WatchListData  list = MakeList (session, kind);
    WatchListData  one;



    one.kind = kind;

    for (const WatchEntry & entry : list.entries)
    {
        if (entry.id == id)
        {
            one.entries.push_back (entry);
        }
    }

    return one;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::Clear
//
////////////////////////////////////////////////////////////////////////////////

void WatchHandlers::Clear (DebugSession & session, const DebugCommand & command, WatchListKind kind, Reply & reply)
{
    WatchTable  & table = GetTable (session, kind);



    if (command.text == "*")
    {
        table.ClearAll();
        reply.data = MakeList (session, kind);
        return;
    }

    if (!table.TryClear ((int) command.count))
    {
        SetNoSuch (reply, kind, command.count);
        return;
    }

    reply.data = MessageData { { std::format ("{} #{} cleared.", Capitalize (GetNoun (kind)), command.count) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::Enable
//
////////////////////////////////////////////////////////////////////////////////

void WatchHandlers::Enable (DebugSession & session, const DebugCommand & command, WatchListKind kind, bool enabled, Reply & reply)
{
    WatchTable  & table = GetTable (session, kind);



    if (command.text == "*")
    {
        for (const WatchItem & item : table.GetAll())
        {
            table.TrySetEnabled (item.id, enabled);
        }

        reply.data = MakeList (session, kind);
        return;
    }

    if (!table.TrySetEnabled ((int) command.count, enabled))
    {
        SetNoSuch (reply, kind, command.count);
        return;
    }

    reply.data = MakeEntry (session, kind, (int) command.count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::GoTo
//
//  BMG # lists 20 lines of code at the bookmark. There is one listing, so
//  `*` is not an id here.
//
////////////////////////////////////////////////////////////////////////////////

void WatchHandlers::GoTo (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    WatchItem        item;
    DisassemblyData  data;



    if (command.text == "*")
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "BMG takes one bookmark id, not *.");
        return;
    }

    if (!session.GetBookmarks().TryFind ((int) command.count, item))
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
    size_t         count = GetTable (session, kind).GetAll().size();
    HRESULT        hr    = S_OK;



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

    reply.data = MessageData { { std::format ("Saved {} {} to {}.", count, count == 1 ? GetNoun (kind) : GetPlural (kind), command.text) } };
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
//  WatchHandlers::Capitalize
//
//  A noun that starts a sentence.
//
////////////////////////////////////////////////////////////////////////////////

std::string WatchHandlers::Capitalize (const std::string & text)
{
    std::string  result = text;



    if (!result.empty())
    {
        result[0] = (char) toupper ((unsigned char) result[0]);
    }

    return result;
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
//  WatchHandlers::PeekWord
//
//  The word at address, low byte first, read as the CPU would see it. A
//  zero-page pointer's high byte wraps within zero page. A word with a byte
//  the debugger cannot read without disturbing the machine -- an I/O address
//  -- has no value, rather than a zero that reads like one.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> WatchHandlers::PeekWord (DebugSession & session, WatchListKind kind, Word address)
{
    static constexpr int  kByteBits  = 8;
    Word                  next       = (kind == WatchListKind::ZeroPage) ? (Word) ((address + 1) & kLastZeroPage) : (Word) (address + 1);
    Byte                  low        = 0;
    Byte                  high       = 0;
    bool                  isLowRead  = session.TryPeek (address, low);
    bool                  isHighRead = session.TryPeek (next, high);



    if (!isLowRead || !isHighRead)
    {
        return std::nullopt;
    }

    return (Word) (low | (high << kByteBits));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::SetNoSuch
//
//  The id as it was typed, so an id above the largest int is not reported
//  as a negative number.
//
////////////////////////////////////////////////////////////////////////////////

void WatchHandlers::SetNoSuch (Reply & reply, WatchListKind kind, uint32_t id)
{
    reply.SetError (CommandStatus::Error, std::string ("no such ") + GetNoun (kind),
                    std::format ("There is no {} #{}.", GetNoun (kind), id));
}
