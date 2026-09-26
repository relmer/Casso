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
            entry.value = PeekWord (session, item.address);
        }

        data.entries.push_back (entry);
    }

    return data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::Add
//
//  ZP0-ZP7 and P0-P4 carry a slot in their name and fill it; the others
//  take the next id. The reply lists the entry.
//
////////////////////////////////////////////////////////////////////////////////

void WatchHandlers::Add (DebugSession & session, const DebugCommand & command, WatchListKind kind, Reply & reply)
{
    WatchTable     & table = GetTable (session, kind);
    WatchListData    list  = MakeList (session, kind);
    WatchListData    one;
    int              slot  = 0;
    int              id    = 0;



    if (!command.hasA1)
    {
        reply.data = list;
        return;
    }

    id   = TryGetSlot (command.sourceName, slot) ? table.AddAt (slot, command.a1) : table.Add (command.a1);
    list = MakeList (session, kind);
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
//  WatchHandlers::Clear
//
////////////////////////////////////////////////////////////////////////////////

void WatchHandlers::Clear (DebugSession & session, const DebugCommand & command, WatchListKind kind, Reply & reply)
{
    WatchTable  & table = GetTable (session, kind);
    int           id    = (int) command.count;



    if (command.text == "*")
    {
        table.ClearAll();
        reply.data = MakeList (session, kind);
        return;
    }

    if (!table.TryClear (id))
    {
        SetNoSuch (reply, kind, id);
        return;
    }

    reply.data = MessageData { { std::format ("{}{} #{} cleared.", (char) toupper ((unsigned char) GetNoun (kind)[0]), GetNoun (kind) + 1, id) } };
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

    if (!table.TrySetEnabled (id, enabled))
    {
        SetNoSuch (reply, kind, id);
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



    if (command.text == "*" || !session.GetBookmarks().TryFind ((int) command.count, item))
    {
        SetNoSuch (reply, WatchListKind::Bookmark, (int) command.count);
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
////////////////////////////////////////////////////////////////////////////////

Word WatchHandlers::PeekWord (DebugSession & session, Word address)
{
    static constexpr int  kByteBits = 8;
    Byte                  low       = 0;
    Byte                  high      = 0;



    session.TryPeek (address,              low);
    session.TryPeek ((Word) (address + 1), high);
    return (Word) (low | (high << kByteBits));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlers::SetNoSuch
//
////////////////////////////////////////////////////////////////////////////////

void WatchHandlers::SetNoSuch (Reply & reply, WatchListKind kind, int id)
{
    reply.SetError (CommandStatus::Error, std::string ("no such ") + GetNoun (kind),
                    std::format ("There is no {} #{}.", GetNoun (kind), id));
}
