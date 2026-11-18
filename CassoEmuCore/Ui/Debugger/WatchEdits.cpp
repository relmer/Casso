#include "Pch.h"

#include "Ui/Debugger/WatchEdits.h"
#include "Ui/Debugger/MemoryActions.h"
#include "Ui/Debugger/WatchActions.h"

#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/WatchTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WatchEdits::GetWatchError
//
//  Text after BASIC must be a variable or an array element as a program
//  writes it. A BASIC variable, and anything once the machine is stopped in
//  a program, is left to the session; otherwise the text, less any `.b` or
//  `.w` suffix, must parse. A name no symbol has yet is a watch all the same,
//  which reads as the symbol once symbols define it.
//
////////////////////////////////////////////////////////////////////////////////

std::string WatchEdits::GetWatchError (const DebuggerViewSnapshot & snapshot, const std::string & typed)
{
    static constexpr std::string_view  kBasic  = "BASIC ";
    std::string                        text    = typed;
    std::string                        name;
    std::string                        error;
    BasicReference                     reference;
    Expression                         parsed;
    HRESULT                            hr      = S_OK;



    text.erase (0, std::min (text.size(), text.find_first_not_of (" \t")));
    text.erase (text.find_last_not_of (" \t") + 1);

    if (text.empty())
    {
        return "There is nothing to watch.";
    }

    if (text.size() > kBasic.size() && _strnicmp (text.c_str(), kBasic.data(), kBasic.size()) == 0)
    {
        name = text.substr (kBasic.size());
        std::erase (name, ' ');

        return ApplesoftProgram::TryParseReference (name, reference) ? std::string()
                                                                     : std::string ("BASIC takes a variable or an array element as a program writes it: A, B%, C$ or D(3,4).");
    }

    if (WatchItem::SplitWidth (text, text) != 0)
    {
        hr = DebugExpressionEvaluator::Parse (text, parsed, error);
        return FAILED (hr) ? error : std::string();
    }

    if (ApplesoftProgram::TryParseReference (text, reference) &&
        (text.find_first_of ("$%(") != std::string::npos || (snapshot.basic.has_value() && snapshot.basic->hasValues)))
    {
        return {};
    }

    hr = DebugExpressionEvaluator::Parse (text, parsed, error);
    return FAILED (hr) ? error : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchEdits::GetWatchEditActions
//
//  Each edit runs as the command anyone could have typed, and is echoed as
//  that command in the console:
//
//    manual watch, expression   the watch moved to the new address
//    manual watch, value        the word written to the watched address,
//                               or R <register> <value> for a register
//    automatic register         R <register> <value>
//    automatic flag             R P with that one bit set or cleared
//    automatic address          the byte written there
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DebuggerAction> WatchEdits::GetWatchEditActions (const DebuggerViewSnapshot & snapshot,
                                                             std::optional<int> watchId, std::optional<int> autoIndex,
                                                             int column, const std::string & typed, CommandMode mode)
{
    static constexpr std::pair<char, Byte>  kFlagBits[] =
    {
        { 'C', 0x01 }, { 'Z', 0x02 }, { 'I', 0x04 }, { 'D', 0x08 }, { 'V', 0x40 }, { 'N', 0x80 },
    };
    std::string  text     = typed;
    std::string  key;
    Word         address  = 0;
    auto         oneValue = [] (DebuggerAction action) { action.isOneValue = true; return action; };



    while (!text.empty() && isspace ((unsigned char) text.back()))  { text.pop_back(); }
    while (!text.empty() && isspace ((unsigned char) text.front())) { text.erase (0, 1); }

    if (text.empty())
    {
        return {};
    }

    if (watchId.has_value())
    {
        for (const DebuggerViewSnapshot::WatchLine & watch : snapshot.watches)
        {
            if (watch.id == *watchId)
            {
                //  A BASIC watch's value is the program's to change.
                return (column == 0)               ? std::vector<DebuggerAction> { WatchActions::GetClearWatch (watch.id, mode), WatchActions::GetAddWatch (text, mode) }
                     : !watch.basicName.empty() || !watch.isValueEditable ? std::vector<DebuggerAction> {}
                     : IsRegisterWatch (watch)      ? std::vector<DebuggerAction> { MemoryActions::GetSetRegister (GetUpper (watch.expression), text, mode) }
                     : IsByteWatch (watch)          ? std::vector<DebuggerAction> { oneValue (MemoryActions::GetEnterByte (watch.address, text, mode)) }
                     :                               std::vector<DebuggerAction> { oneValue (MemoryActions::GetEnterWord (watch.address, text, mode)) };
            }
        }

        return {};
    }

    if (!autoIndex.has_value() || *autoIndex < 0 || *autoIndex >= (int) snapshot.autoWatches.size() || column == 0)
    {
        return {};
    }

    key = snapshot.autoWatches[(size_t) *autoIndex].key;

    if (key.starts_with ("R:"))
    {
        return { MemoryActions::GetSetRegister (key.substr (2), text, mode) };
    }

    if (key.starts_with ("M:") && TryParseHexWord (key.substr (2), address))
    {
        return { oneValue (MemoryActions::GetEnterByte (address, text, mode)) };
    }

    //  A flag is 0 or 1 and nothing else; it is written as the whole status
    //  register with that one bit changed.
    if (key.starts_with ("F:") && (text == "0" || text == "1"))
    {
        for (const DebuggerViewSnapshot::RegisterRow & reg : snapshot.registers)
        {
            unsigned  p = 0;

            if (reg.name != "P" || std::from_chars (reg.value.data(), reg.value.data() + reg.value.size(), p, 16).ec != std::errc())
            {
                continue;
            }

            for (const auto & [letter, bit] : kFlagBits)
            {
                if (key[2] == letter)
                {
                    return { MemoryActions::GetSetRegister ("P", (Word) ((text == "1") ? (Byte) (p | bit) : (Byte) (p & ~bit)), mode) };
                }
            }
        }
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchEdits::IsRegisterWatch
//
////////////////////////////////////////////////////////////////////////////////

bool WatchEdits::IsRegisterWatch (const DebuggerViewSnapshot::WatchLine & watch)
{
    //  By the watch's kind, not its text: a symbol called X or PC is memory.
    return watch.isRegister;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchEdits::IsByteWatch
//
////////////////////////////////////////////////////////////////////////////////

bool WatchEdits::IsByteWatch (const DebuggerViewSnapshot::WatchLine & watch)
{
    std::string  base;



    return watch.basicName.empty() && WatchItem::SplitWidth (watch.expression, base) == 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchEdits::GetUpper
//
////////////////////////////////////////////////////////////////////////////////

std::string WatchEdits::GetUpper (const std::string & text)
{
    std::string  upper = text;



    std::ranges::transform (upper, upper.begin(), [] (char ch) { return (char) std::toupper ((unsigned char) ch); });
    return upper;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchEdits::GetWatchUndo
//
//  The value each kind of edit overwrote, written back the way the edit wrote
//  it. A value the pane could not read -- a soft switch shows "--" -- has
//  nothing to put back, and the edit is then not undoable.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<WatchEdits::WatchUndo> WatchEdits::GetWatchUndo (const DebuggerViewSnapshot & before,
                                                               std::optional<int> watchId,
                                                               std::optional<int> autoIndex, int column,
                                                               CommandMode mode)
{
    WatchUndo  undo;
    Word       value   = 0;
    Word       address = 0;



    if (watchId.has_value())
    {
        for (const DebuggerViewSnapshot::WatchLine & watch : before.watches)
        {
            if (watch.id != *watchId)
            {
                continue;
            }

            if (column == 0)
            {
                undo.restoreText = !watch.expression.empty() ? watch.expression
                                 : watch.basicName.empty()   ? std::format ("{:04X}", watch.address)
                                 :                             "BASIC " + watch.basicName;

                for (const DebuggerViewSnapshot::WatchLine & other : before.watches)
                {
                    undo.movedFromIds.push_back (other.id);
                }

                return undo;
            }

            if (!TryParseHexWord (watch.value, value))
            {
                return std::nullopt;
            }

            undo.actions = { IsRegisterWatch (watch) ? MemoryActions::GetSetRegister (GetUpper (watch.expression), value, mode)
                           : IsByteWatch (watch)     ? MemoryActions::GetEnterByte (watch.address, (Byte) value, mode)
                           :                           MemoryActions::GetEnterWord (watch.address, value, mode) };
            return undo;
        }

        return std::nullopt;
    }

    if (!autoIndex.has_value() || *autoIndex < 0 || *autoIndex >= (int) before.autoWatches.size() || column == 0)
    {
        return std::nullopt;
    }

    //  An automatic watch's edit and its undo are the same command with the
    //  old value in it -- a flag's, the whole status register as it was.
    const DebuggerViewSnapshot::AutoWatchLine & line = before.autoWatches[(size_t) *autoIndex];

    if (line.key.starts_with ("F:"))
    {
        for (const DebuggerViewSnapshot::RegisterRow & reg : before.registers)
        {
            if (reg.name == "P" && TryParseHexWord (reg.value, value))
            {
                undo.actions = { MemoryActions::GetSetRegister ("P", value, mode) };
                return undo;
            }
        }

        return std::nullopt;
    }

    if (!TryParseHexWord (line.value, value))
    {
        return std::nullopt;
    }

    if (line.key.starts_with ("R:"))
    {
        undo.actions = { MemoryActions::GetSetRegister (line.key.substr (2), value, mode) };
    }
    else if (line.key.starts_with ("M:") && TryParseHexWord (line.key.substr (2), address))
    {
        undo.actions = { MemoryActions::GetEnterByte (address, (Byte) value, mode) };
    }

    return undo.actions.empty() ? std::nullopt : std::optional<WatchUndo> (undo);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchEdits::TryParseHexWord
//
//  A value as the panes show it: hex digits and nothing else. A value the
//  pane could not read, such as a soft switch's "--", is not one.
//
////////////////////////////////////////////////////////////////////////////////

bool WatchEdits::TryParseHexWord (const std::string & text, Word & value)
{
    static constexpr unsigned  kMaxWord = 0xFFFF;
    unsigned                   parsed   = 0;
    auto                       result   = std::from_chars (text.data(), text.data() + text.size(), parsed, 16);



    if (text.empty() || result.ec != std::errc() || result.ptr != text.data() + text.size() || parsed > kMaxWord)
    {
        return false;
    }

    value = (Word) parsed;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchEdits::NoteMovedWatch
//
//  The engine numbers watches in order and never reuses a number, so the
//  watch a move made is the lowest id the snapshot holds that was not there
//  before. It is noted from the first snapshot that shows it, so a watch
//  removed and another added later is never taken for it.
//
////////////////////////////////////////////////////////////////////////////////

void WatchEdits::NoteMovedWatch (const DebuggerViewSnapshot & now, WatchUndo & undo)
{
    if (!undo.restoreText.has_value() || undo.movedToId.has_value())
    {
        return;
    }

    for (const DebuggerViewSnapshot::WatchLine & watch : now.watches)
    {
        bool  existed = std::find (undo.movedFromIds.begin(), undo.movedFromIds.end(), watch.id) != undo.movedFromIds.end();

        if (!existed && (!undo.movedToId.has_value() || watch.id < *undo.movedToId))
        {
            undo.movedToId = watch.id;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WatchEdits::GetWatchUndoActions
//
//  Nothing yet when the snapshot does not show the watch a move made, so the
//  undo is kept for a later try. A moved watch that has since been removed
//  has nothing left to put back.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::vector<DebuggerAction>> WatchEdits::GetWatchUndoActions (const DebuggerViewSnapshot & now, WatchUndo & undo, CommandMode mode)
{
    if (!undo.restoreText.has_value())
    {
        return undo.actions;
    }

    NoteMovedWatch (now, undo);

    if (!undo.movedToId.has_value())
    {
        return std::nullopt;
    }

    for (const DebuggerViewSnapshot::WatchLine & watch : now.watches)
    {
        if (watch.id == *undo.movedToId)
        {
            return std::vector<DebuggerAction> { WatchActions::GetClearWatch (watch.id, mode), WatchActions::GetAddWatch (*undo.restoreText, mode) };
        }
    }

    return std::vector<DebuggerAction> {};
}
