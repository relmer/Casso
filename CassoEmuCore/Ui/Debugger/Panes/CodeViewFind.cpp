#include "Pch.h"

#include "Ui/Debugger/Panes/CodeViewFind.h"

#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViewFind::CodeViewFind
//
////////////////////////////////////////////////////////////////////////////////

CodeViewFind::CodeViewFind (DxuiListView & list, const DisassemblyOptions & options, LinesFn lines, WhereFn where, SelectFn select, SourceFn source) :
    m_list    (list),
    m_options (options),
    m_lines   (std::move (lines)),
    m_where   (std::move (where)),
    m_select  (std::move (select)),
    m_source  (std::move (source))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViewFind::FindLines
//
//  Each line's searched columns as the view writes them, with the address's
//  colon and the instruction's symbols as the options choose; a line matches
//  when one of them does.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Word> CodeViewFind::FindLines (const Lines & lines, const DisassemblyOptions & options, const Visible & visible, const SourceFn & source,
                                           const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex)
{
    bool                 symbols    = options.IsOn (DisassemblyOptions::Option::Symbols);
    bool                 colon      = options.IsOn (DisassemblyOptions::Option::AddressColon);
    bool                 isSource   = options.IsOn (DisassemblyOptions::Option::Source) && source != nullptr;
    std::pair<int, int>  lastSource = { -1, 0 };
    std::vector<Word>    found;



    if (needle.empty())
    {
        return found;
    }

    for (const DebuggerViewSnapshot::CodeLine & line : lines)
    {
        bool  isMatch = false;
        auto  matches = [&] (size_t column, const std::wstring & text)
        {
            return visible[column] && !DxuiTextFind::FindAll (text, needle, matchCase, wholeWord, isRegex).empty();
        };

        isMatch = matches (DisassemblyCells::kAddressColumn,     std::format (L"{:04X}{}", line.address, colon ? L":" : L"")) ||
                  matches (DisassemblyCells::kBytesColumn,       TextEncoding::NarrowToWide (line.bytes))                  ||
                  matches (DisassemblyCells::kLabelColumn,       TextEncoding::NarrowToWide (line.label))                  ||
                  matches (DisassemblyCells::kInstructionColumn, TextEncoding::NarrowToWide (DisassemblyOptions::GetInstructionText (line, symbols)));

        //  A source line is above the first instruction it produced alone,
        //  as the view writes it.
        if (isSource && line.sourceFileId >= 0 && std::make_pair (line.sourceFileId, line.sourceLine) != lastSource)
        {
            std::wstring  text;

            lastSource = { line.sourceFileId, line.sourceLine };
            isMatch    = isMatch || (source (line.sourceFileId, line.sourceLine, text) &&
                                     !DxuiTextFind::FindAll (text, needle, matchCase, wholeWord, isRegex).empty());
        }

        if (isMatch)
        {
            found.push_back (line.address);
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViewFind::GetVisible
//
////////////////////////////////////////////////////////////////////////////////

CodeViewFind::Visible CodeViewFind::GetVisible() const
{
    Visible  visible = {};



    for (size_t column = 0; column < visible.size(); column++)
    {
        visible[column] = m_list.IsColumnVisible (column);
    }

    return visible;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViewFind::Search
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Word> CodeViewFind::Search (const Lines & lines, const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex)
{
    return FindLines (lines, m_options, GetVisible(), m_source, needle, matchCase, wholeWord, isRegex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViewFind::GetFindControl
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * CodeViewFind::GetFindControl()
{
    return m_list.GetFindControl();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViewFind::GetFindBounds
//
////////////////////////////////////////////////////////////////////////////////

RECT CodeViewFind::GetFindBounds() const
{
    return m_list.GetFindBounds();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViewFind::IsFindVisible
//
////////////////////////////////////////////////////////////////////////////////

bool CodeViewFind::IsFindVisible() const
{
    return m_list.IsFindVisible();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViewFind::GetSelectionText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CodeViewFind::GetSelectionText() const
{
    return m_list.GetSelectionText();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViewFind::SelectMatch
//
//  The match after the selected line, or the one before it, wrapping at the
//  ends of memory.
//
////////////////////////////////////////////////////////////////////////////////

DxuiFindResult CodeViewFind::SelectMatch (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, bool forward, int & outIndex, int & outCount)
{
    std::shared_ptr<const Lines>  lines  = m_lines();
    std::vector<Word>             found;
    std::optional<Word>           from;
    int                           chosen = -1;
    DxuiFindResult                result = DxuiFindResult::NotFound;



    if (lines == nullptr)
    {
        return m_list.SelectMatch (needle, matchCase, wholeWord, isRegex, forward, outIndex, outCount);
    }

    found    = Search (*lines, needle, matchCase, wholeWord, isRegex);
    from     = m_where (false);
    m_list.HighlightFindMatches (needle, matchCase, wholeWord, isRegex);
    outIndex = 0;
    outCount = (int) found.size();

    if (found.empty())
    {
        return result;
    }

    for (int hit = 0; hit < outCount && forward && chosen < 0 && from.has_value(); hit++)
    {
        chosen = (found[(size_t) hit] > *from) ? hit : -1;
    }

    for (int hit = outCount - 1; hit >= 0 && !forward && chosen < 0 && from.has_value(); hit--)
    {
        chosen = (found[(size_t) hit] < *from) ? hit : -1;
    }

    result   = (chosen < 0 && from.has_value()) ? DxuiFindResult::Wrapped : DxuiFindResult::Found;
    chosen   = (chosen >= 0) ? chosen : (forward ? 0 : outCount - 1);
    outIndex = chosen + 1;
    m_select (found[(size_t) chosen]);
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViewFind::SelectMatchInView
//
//  As the needle is typed: the first match at or after the view's top
//  line, or the first in memory past the last.
//
////////////////////////////////////////////////////////////////////////////////

DxuiFindResult CodeViewFind::SelectMatchInView (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, int & outIndex, int & outCount)
{
    std::shared_ptr<const Lines>  lines  = m_lines();
    std::vector<Word>             found;
    Word                          top    = 0;
    int                           chosen = 0;



    if (lines == nullptr)
    {
        return m_list.SelectMatchInView (needle, matchCase, wholeWord, isRegex, outIndex, outCount);
    }

    found    = Search (*lines, needle, matchCase, wholeWord, isRegex);
    top      = m_where (true).value_or (0);
    m_list.HighlightFindMatches (needle, matchCase, wholeWord, isRegex);
    outIndex = 0;
    outCount = (int) found.size();

    if (found.empty())
    {
        return DxuiFindResult::NotFound;
    }

    while (chosen < outCount && found[(size_t) chosen] < top)
    {
        chosen++;
    }

    chosen   = (chosen < outCount) ? chosen : 0;
    outIndex = chosen + 1;
    m_select (found[(size_t) chosen]);
    return DxuiFindResult::Found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViewFind::SetFindScopeToSelection
//
////////////////////////////////////////////////////////////////////////////////

void CodeViewFind::SetFindScopeToSelection()
{
    m_list.SetFindScopeToSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViewFind::ClearFindScope
//
////////////////////////////////////////////////////////////////////////////////

void CodeViewFind::ClearFindScope()
{
    m_list.ClearFindScope();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViewFind::ClearFindHighlights
//
////////////////////////////////////////////////////////////////////////////////

void CodeViewFind::ClearFindHighlights()
{
    m_list.ClearFindHighlights();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeViewFind::SetFindColors
//
////////////////////////////////////////////////////////////////////////////////

void CodeViewFind::SetFindColors (uint32_t matchArgb, uint32_t currentArgb)
{
    m_list.SetFindColors (matchArgb, currentArgb);
}
