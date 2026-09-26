#include "Pch.h"

#include "Ui/Debugger/ConsoleHistory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleHistory::Add
//
//  A blank line is not kept. The oldest line goes first once the history is
//  full.
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleHistory::Add (const std::wstring & line)
{
    m_index.reset();

    if (line.find_first_not_of (L" \t") == std::wstring::npos)
    {
        return;
    }

    std::erase (m_lines, line);
    m_lines.push_back (line);

    if (m_lines.size() > kLimit)
    {
        m_lines.erase (m_lines.begin());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleHistory::GetOlder
//
//  The first step back keeps what was typed so the walk can return to it. At
//  the oldest line the walk stays there.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::wstring> ConsoleHistory::GetOlder (const std::wstring & typed)
{
    if (m_lines.empty())
    {
        return std::nullopt;
    }

    if (!m_index.has_value())
    {
        m_draft = typed;
        m_index = m_lines.size() - 1;
    }
    else if (*m_index > 0)
    {
        --*m_index;
    }

    return m_lines[*m_index];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleHistory::GetNewer
//
//  Past the most recent line the walk ends on what was typed before it began.
//  Outside a walk there is nothing newer.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::wstring> ConsoleHistory::GetNewer()
{
    if (!m_index.has_value())
    {
        return std::nullopt;
    }

    if (*m_index + 1 >= m_lines.size())
    {
        m_index.reset();
        return m_draft;
    }

    ++*m_index;
    return m_lines[*m_index];
}
