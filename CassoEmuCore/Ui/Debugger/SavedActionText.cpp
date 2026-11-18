#include "Pch.h"

#include "Ui/Debugger/SavedActionText.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/UndoText.h"





//  The verbs a watch edit's action can have, each by the AppleWin command it
//  is, which a saved action has in place of the verb's number.
static constexpr std::pair<DebugVerb, const char *>  s_kSavedVerbs[] =
{
    { DebugVerb::AddWatch,             "W"   },
    { DebugVerb::ClearWatch,           "WC"  },
    { DebugVerb::AddZeroPagePointer,   "ZP"  },
    { DebugVerb::ClearZeroPagePointer, "ZPC" },
    { DebugVerb::EnterBytes,           "MEB" },
    { DebugVerb::EnterWords,           "MEW" },
    { DebugVerb::SetRegister,          "R"   },
};





////////////////////////////////////////////////////////////////////////////////
//
//  SavedActionText::ToText
//
////////////////////////////////////////////////////////////////////////////////

std::string SavedActionText::ToText (const DebuggerAction & action)
{
    const DebugCommand  & command = action.command;



    for (const auto & [verb, name] : s_kSavedVerbs)
    {
        if (verb == command.verb)
        {
            return std::format ("{}.{:X}.{}.{:X}.{}.{}.{}.{}.{}", name, command.a1, command.hasA1 ? 1 : 0, command.count,
                                UndoText::FormatBytes (command.values), UndoText::FormatBytes (command.mask),
                                UndoText::FormatText (action.operand), UndoText::FormatText (command.text), UndoText::FormatText (action.echo));
        }
    }

    return std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SavedActionText::TryFromText
//
////////////////////////////////////////////////////////////////////////////////

bool SavedActionText::TryFromText (const std::string & text, DebuggerAction & action)
{
    std::vector<std::string>  fields = UndoText::Split (text, '.');
    const auto              * found  = std::ranges::end (s_kSavedVerbs);
    uint32_t                  a1     = 0;
    uint32_t                  count  = 0;
    size_t                    start  = 0;



    action = {};

    if (fields.size() != 9)
    {
        return false;
    }

    found = std::ranges::find_if (s_kSavedVerbs, [&fields] (const auto & each) { return fields[0] == each.second; });

    if (found == std::ranges::end (s_kSavedVerbs) || !UndoText::TryParseHex (fields[1], a1) || a1 > 0xFFFF ||
        (fields[2] != "0" && fields[2] != "1") || !UndoText::TryParseHex (fields[3], count) ||
        !UndoText::TryParseBytes (fields[4], action.command.values) || !UndoText::TryParseBytes (fields[5], action.command.mask) ||
        !UndoText::TryParseText (fields[6], action.operand) || !UndoText::TryParseText (fields[7], action.command.text) ||
        !UndoText::TryParseText (fields[8], action.echo) || action.echo.empty())
    {
        action = {};
        return false;
    }

    start                     = action.echo.starts_with ("/") ? 1 : 0;
    action.command.verb       = found->first;
    action.command.sourceName = action.echo.substr (start, action.echo.find (' ', start) - start);
    action.command.a1         = (Word) a1;
    action.command.hasA1      = fields[2] == "1";
    action.command.count      = count;

    //  A typed watch is the only add with its text in the operand.
    action.command.isTypedWatch = action.command.verb == DebugVerb::AddWatch && !action.operand.empty();
    return true;
}
