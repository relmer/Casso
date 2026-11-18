#pragma once

#include "Debugger/DebugCommand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandCompletion
//
//  What the console's command box offers as the user types, as PowerShell
//  does (FR-128, FR-131):
//
//  - A suggestion from the last reply -- another mode's word answered with
//    this mode's equivalent, or the closest command -- shows in gray after
//    the caret; Tab takes it, and any change to the box drops it.
//  - Otherwise the newest earlier line that starts with the typed text shows
//    in gray, and Right arrow at the end of the line takes it.
//  - Tab completes the command word from the current mode's commands, and
//    pressing it again steps through the other matches; Shift+Tab steps back.
//  - A word after the command word completes from the symbols the same way,
//    and with no earlier line to show, the first symbol it starts shows in
//    gray, which Right arrow at the end of the line takes as well.
//  - F8 steps back through earlier lines starting with the typed text.
//
//  The history is passed in, oldest first, as ConsoleHistory keeps it.
//
////////////////////////////////////////////////////////////////////////////////

class CommandCompletion
{
public:
    void  SetMode        (CommandMode mode);

    //  The symbol names a word after the command word completes from.
    void  SetSymbols     (std::vector<std::wstring> names);

    //  The suggestion is kept while the box holds what it held when offered.
    void  OfferSuggestion (const std::wstring & suggestion, const std::wstring & boxText);

    //  The gray text after the caret for the box as it is now; empty for none.
    //  A box changed since the suggestion was offered drops it.
    std::wstring  GetGhost (const std::wstring & text, bool caretAtEnd, const std::vector<std::wstring> & history);

    bool  TryAcceptSuggestion (const std::wstring & text, std::wstring & result);
    bool  TryAcceptHistory    (const std::wstring & text, const std::vector<std::wstring> & history, std::wstring & result) const;
    bool  TryComplete         (const std::wstring & text, bool forward, std::wstring & result);
    bool  TrySearchHistory    (const std::wstring & text, const std::vector<std::wstring> & history, std::wstring & result);

    static std::vector<std::wstring>  GetCommandNames (CommandMode mode);

private:
    static std::wstring  FindHistoryMatch (const std::wstring & text, const std::vector<std::wstring> & history);

    //  Where the word at the end of the text starts, when it follows the
    //  command word; npos for the command word itself.
    static size_t        FindArgumentStart (const std::wstring & text);

    //  The first symbol the word at the end of the text starts, past that
    //  word; empty for none.
    std::wstring         FindSymbolRest   (const std::wstring & text) const;
    static bool          StartsWithNoCase (const std::wstring & text, const std::wstring & prefix);
    static std::wstring  ToLower          (const std::wstring & text);

    CommandMode                m_mode = CommandMode::AppleWin;
    std::vector<std::wstring>  m_names;
    std::vector<std::wstring>  m_symbols;

    std::wstring               m_suggestion;
    std::wstring               m_suggestionBase;

    std::vector<std::wstring>  m_matches;
    size_t                     m_matchIndex = 0;
    std::wstring               m_matchHead;
    std::wstring               m_matchRest;
    std::wstring               m_lastCompletion;

    std::wstring               m_searchPrefix;
    size_t                     m_searchIndex = 0;
    std::wstring               m_lastSearch;
};
