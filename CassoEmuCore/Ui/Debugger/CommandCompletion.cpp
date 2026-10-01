#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/CommandModeHelp.h"
#include "Ui/Debugger/CommandCompletion.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandCompletion::SetMode
//
//  A change of mode changes the words Tab completes, and ends any cycle.
//
////////////////////////////////////////////////////////////////////////////////

void CommandCompletion::SetMode (CommandMode mode)
{
    if (mode == m_mode && !m_names.empty())
    {
        return;
    }

    m_mode  = mode;
    m_names = GetCommandNames (mode);
    m_matches.clear();
    m_lastCompletion.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandCompletion::OfferSuggestion
//
////////////////////////////////////////////////////////////////////////////////

void CommandCompletion::OfferSuggestion (const std::wstring & suggestion, const std::wstring & boxText)
{
    m_suggestion     = suggestion;
    m_suggestionBase = boxText;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandCompletion::GetGhost
//
//  The reply's suggestion comes first: the rest of it past what is typed, or
//  all of it in an empty box. Otherwise the rest of the newest earlier line
//  that starts with the typed text.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CommandCompletion::GetGhost (const std::wstring & text, bool caretAtEnd, const std::vector<std::wstring> & history)
{
    std::wstring  match;



    if (!m_suggestion.empty() && text != m_suggestionBase)
    {
        m_suggestion.clear();
    }

    if (!caretAtEnd)
    {
        return std::wstring();
    }

    if (!m_suggestion.empty())
    {
        return StartsWithNoCase (m_suggestion, text) ? m_suggestion.substr (text.size()) : std::wstring();
    }

    match = FindHistoryMatch (text, history);
    return match.empty() ? std::wstring() : match.substr (text.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandCompletion::TryAcceptSuggestion
//
////////////////////////////////////////////////////////////////////////////////

bool CommandCompletion::TryAcceptSuggestion (const std::wstring & text, std::wstring & result)
{
    if (m_suggestion.empty() || text != m_suggestionBase || !StartsWithNoCase (m_suggestion, text))
    {
        m_suggestion.clear();
        return false;
    }

    result = m_suggestion;
    m_suggestion.clear();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandCompletion::TryAcceptHistory
//
////////////////////////////////////////////////////////////////////////////////

bool CommandCompletion::TryAcceptHistory (const std::wstring & text, const std::vector<std::wstring> & history, std::wstring & result) const
{
    std::wstring  match;



    if (!m_suggestion.empty())
    {
        return false;
    }

    match = FindHistoryMatch (text, history);

    if (match.empty())
    {
        return false;
    }

    result = text + match.substr (text.size());
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandCompletion::TryComplete
//
//  Only the command word completes; what follows it is kept. A box that still
//  holds the last completion steps to the next match, so repeated presses
//  cycle. A word typed in lowercase completes in lowercase.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandCompletion::TryComplete (const std::wstring & text, bool forward, std::wstring & result)
{
    size_t        start   = text.find_first_not_of (L" \t");
    size_t        end     = 0;
    std::wstring  prefix;
    bool          isLower = false;



    if (!m_matches.empty() && text == m_lastCompletion)
    {
        m_matchIndex = forward ? (m_matchIndex + 1) % m_matches.size() : (m_matchIndex + m_matches.size() - 1) % m_matches.size();
        result           = m_matches[m_matchIndex] + m_matchRest;
        m_lastCompletion = result;
        return true;
    }

    m_matches.clear();

    //  Before the first snapshot gives a mode, the words are the default mode's.
    if (m_names.empty())
    {
        m_names = GetCommandNames (m_mode);
    }

    if (start == std::wstring::npos)
    {
        return false;
    }

    end    = text.find_first_of (L" \t", start);
    end    = (end == std::wstring::npos) ? text.size() : end;
    prefix = text.substr (start, end - start);

    isLower = std::ranges::none_of (prefix, [] (wchar_t c) { return iswupper (c) != 0; });

    for (const std::wstring & name : m_names)
    {
        if (StartsWithNoCase (name, prefix))
        {
            m_matches.push_back (isLower ? ToLower (name) : name);
        }
    }

    if (m_matches.empty())
    {
        return false;
    }

    m_matchIndex     = forward ? 0 : m_matches.size() - 1;
    m_matchRest      = text.substr (end);
    result           = m_matches[m_matchIndex] + m_matchRest;
    m_lastCompletion = result;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandCompletion::TrySearchHistory
//
//  The first press takes the typed text as the prefix; while the box holds
//  the line the last press returned, each press steps to an older one. The
//  oldest match stays put.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandCompletion::TrySearchHistory (const std::wstring & text, const std::vector<std::wstring> & history, std::wstring & result)
{
    if (m_lastSearch.empty() || text != m_lastSearch)
    {
        m_searchPrefix = text;
        m_searchIndex  = history.size();
    }

    for (size_t i = m_searchIndex; i > 0; i--)
    {
        const std::wstring & line = history[i - 1];



        if (line != text && StartsWithNoCase (line, m_searchPrefix))
        {
            m_searchIndex = i - 1;
            m_lastSearch  = line;
            result        = line;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandCompletion::GetCommandNames
//
//  The words a mode runs, sorted without regard to case: AppleWin's table in
//  the AppleWin and Casso modes; otherwise the mode's own words, then the
//  Casso commands it reaches, behind its marker.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CommandCompletion::GetCommandNames (CommandMode mode)
{
    std::vector<std::wstring>  names;
    std::string                marker     = CommandModeHelp::GetMarker (mode);
    bool                       isAppleWin = (mode == CommandMode::AppleWin || mode == CommandMode::Casso);



    for (const CommandModeHelp::Entry & entry : CommandModeHelp::GetEntries (mode))
    {
        if (entry.word[0] != '\0')
        {
            names.push_back (TextEncoding::NarrowToWide (entry.word));
        }
    }

    for (const AppleWinCommand & command : AppleWinCommandTable::GetAll())
    {
        if (command.verb == DebugVerb::None || (!isAppleWin && !CommandModeHelp::IsCassoCommandReachable (mode, command.name)))
        {
            continue;
        }

        names.push_back (TextEncoding::NarrowToWide (marker + command.name));
    }

    std::ranges::sort (names, [] (const std::wstring & a, const std::wstring & b) { return ToLower (a) < ToLower (b); });
    names.erase (std::unique (names.begin(), names.end(), [] (const std::wstring & a, const std::wstring & b) { return ToLower (a) == ToLower (b); }), names.end());

    return names;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandCompletion::FindHistoryMatch
//
//  The newest earlier line longer than the typed text that starts with it.
//  An empty box matches nothing.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CommandCompletion::FindHistoryMatch (const std::wstring & text, const std::vector<std::wstring> & history)
{
    if (text.empty())
    {
        return std::wstring();
    }

    for (auto it = history.rbegin(); it != history.rend(); ++it)
    {
        if (it->size() > text.size() && it->starts_with (text))
        {
            return *it;
        }
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandCompletion::StartsWithNoCase
//
////////////////////////////////////////////////////////////////////////////////

bool CommandCompletion::StartsWithNoCase (const std::wstring & text, const std::wstring & prefix)
{
    return text.size() >= prefix.size() && ToLower (text.substr (0, prefix.size())) == ToLower (prefix);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandCompletion::ToLower
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CommandCompletion::ToLower (const std::wstring & text)
{
    std::wstring  lower = text;



    std::ranges::transform (lower, lower.begin(), [] (wchar_t c) { return (wchar_t) towlower (c); });
    return lower;
}
