#include "Pch.h"

#include "Ui/Debugger/SymbolComments.h"
#include "Debugger/SymbolTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolComments::SetSource
//
////////////////////////////////////////////////////////////////////////////////

void SymbolComments::SetSource (const DebuggerViewSnapshot::SourceState & state)
{
    bool  isSame = m_state.has_value() && m_state->lineAddresses == state.lineAddresses && m_state->debugFilePath == state.debugFilePath;



    if (!isSame)
    {
        m_files.clear();
        m_comments.clear();
    }

    m_state = state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolComments::GetComment
//
////////////////////////////////////////////////////////////////////////////////

std::string SymbolComments::GetComment (Word address, const std::string & name, const FindFn & find)
{
    std::optional<std::pair<int, int>>  place;
    const std::vector<std::string>    * lines   = nullptr;
    std::string                         comment;



    if (!m_state.has_value() || m_state->lineAddresses == nullptr || !find)
    {
        return {};
    }

    if (m_comments.contains (address))
    {
        return m_comments.at (address);
    }

    place = FindLabelLine (address, find);
    lines = place.has_value() ? GetLines (place->first, find) : nullptr;

    if (lines != nullptr)
    {
        comment = GetDefiningComment (*lines, place->second, name);
    }

    m_comments[address] = comment;
    return comment;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolComments::GetLines
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<std::string> * SymbolComments::GetLines (int fileId, const FindFn & find)
{
    SourceLookup              lookup;
    std::vector<std::string>  lines;
    std::string               line;



    if (m_files.contains (fileId))
    {
        return &m_files.at (fileId);
    }

    for (const DebugSourceFile & record : m_state->files)
    {
        if (record.id == fileId)
        {
            lookup = find (record, m_state->debugFilePath, m_state->programKey);
        }
    }

    {
        std::istringstream  text (lookup.text);

        while (std::getline (text, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            lines.push_back (line);
        }
    }

    m_files[fileId] = std::move (lines);
    return &m_files.at (fileId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolComments::FindDefiningLine
//
//  The earliest line in the line table whose code starts at the address,
//  preferring one isLabelLine accepts.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::pair<int, int>> SymbolComments::FindDefiningLine (
    const std::map<std::pair<int, int>, Word>          & lineAddresses,
    Word                                                 address,
    const std::function<bool (int fileId, int line)>   & isLabelLine)
{
    std::optional<std::pair<int, int>>  found;
    std::optional<std::pair<int, int>>  labeled;



    //  The map is ordered by place, so the first of each kind is the earliest.
    for (const auto & [place, first] : lineAddresses)
    {
        if (first != address)
        {
            continue;
        }

        found = found.has_value() ? found : place;

        if (!labeled.has_value() && isLabelLine && isLabelLine (place.first, place.second))
        {
            labeled = place;
        }
    }

    return labeled.has_value() ? labeled : found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolComments::FindLabelLine
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::pair<int, int>> SymbolComments::FindLabelLine (Word address, const FindFn & find)
{
    if (!m_state.has_value() || m_state->lineAddresses == nullptr)
    {
        return std::nullopt;
    }

    return FindDefiningLine (*m_state->lineAddresses, address, [this, &find] (int fileId, int line)
    {
        const std::vector<std::string>  * lines = GetLines (fileId, find);



        return lines != nullptr && line >= 1 && (size_t) line <= lines->size() && IsLabelLine ((*lines)[(size_t) line - 1]);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolComments::IsLabelLine
//
////////////////////////////////////////////////////////////////////////////////

bool SymbolComments::IsLabelLine (const std::string & text)
{
    return !text.empty() && text[0] != ' ' && text[0] != '\t' && text[0] != ';' && text[0] != '*';
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolComments::GetDefiningComment
//
//  Lines count from 1. A label alone on the line above, as in
//
//      LOOP     ; wait for a key
//               LDA KBD
//
//  is where the label is defined, so its comment comes first.
//
////////////////////////////////////////////////////////////////////////////////

std::string SymbolComments::GetDefiningComment (const std::vector<std::string> & lines, int line, const std::string & name)
{
    std::string  above;
    std::string  code;



    if (line < 1 || line > (int) lines.size())
    {
        return {};
    }

    if (line >= 2)
    {
        above = lines[(size_t) line - 2];
        code  = above.substr (0, above.find (';'));

        while (!code.empty() && (code.back() == ' ' || code.back() == '\t' || code.back() == ':'))
        {
            code.pop_back();
        }

        if (!name.empty() && SymbolTable::ToUpper (code) == SymbolTable::ToUpper (name) && !GetLineComment (above).empty())
        {
            return GetLineComment (above);
        }
    }

    return GetLineComment (lines[(size_t) line - 1]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolComments::GetLineComment
//
//  A semicolon in a quoted string or character constant is part of it.
//
////////////////////////////////////////////////////////////////////////////////

std::string SymbolComments::GetLineComment (const std::string & line)
{
    char    quote = 0;
    size_t  at    = std::string::npos;
    size_t  first = 0;



    for (size_t i = 0; i < line.size() && at == std::string::npos; i++)
    {
        char  ch = line[i];

        if (quote != 0)
        {
            quote = (ch == quote) ? 0 : quote;
        }
        else if (ch == '"' || ch == '\'')
        {
            quote = ch;
        }
        else if (ch == ';')
        {
            at = i;
        }
    }

    if (at == std::string::npos)
    {
        return {};
    }

    first = line.find_first_not_of (" \t;", at);

    if (first == std::string::npos)
    {
        return {};
    }

    return line.substr (first, line.find_last_not_of (" \t") - first + 1);
}