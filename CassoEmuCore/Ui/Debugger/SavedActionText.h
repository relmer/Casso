#pragma once

struct DebuggerAction;





////////////////////////////////////////////////////////////////////////////////
//
//  SavedActionText
//
//  A watch edit's action as a debug project saves it: the verb of an
//  AddWatch, ClearWatch, EnterBytes, EnterWords or SetRegister action and its
//  fields, apart at dots, with any text among them as hex digits. The text of
//  an action of another kind is empty, and TryFromText returns false for text
//  that does not parse.
//
////////////////////////////////////////////////////////////////////////////////

class SavedActionText
{
public:
    static std::string  ToText      (const DebuggerAction & action);
    static bool         TryFromText (const std::string & text, DebuggerAction & action);
};
