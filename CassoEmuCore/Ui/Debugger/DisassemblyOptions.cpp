#include "Pch.h"

#include "DisassemblyOptions.h"
#include "SourceSyntax.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptions::ToText
//
////////////////////////////////////////////////////////////////////////////////

std::string DisassemblyOptions::ToText() const
{
    std::string  text;



    for (int i = 0; i < kOptionCount; i++)
    {
        if (!m_on[(size_t) i])
        {
            continue;
        }

        text += text.empty() ? "" : " ";
        text += GetKey ((Option) i);
    }

    //  Every option off still differs from the defaults, which empty text
    //  gives.
    return text.empty() ? std::string ("none") : text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptions::FromText
//
////////////////////////////////////////////////////////////////////////////////

DisassemblyOptions DisassemblyOptions::FromText (const std::string & text)
{
    DisassemblyOptions  options;
    std::istringstream  words (text);
    std::string         word;



    if (text.empty())
    {
        return options;
    }

    options.m_on.fill (false);

    while (words >> word)
    {
        for (int i = 0; i < kOptionCount; i++)
        {
            if (word == GetKey ((Option) i))
            {
                options.m_on[(size_t) i] = true;
            }
        }
    }

    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptions::GetLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DisassemblyOptions::GetLabel (Option option)
{
    switch (option)
    {
    case Option::Addresses:   return L"Show address";
    case Option::CodeBytes:   return L"Show code bytes";
    case Option::Source:      return L"Show source code";
    case Option::Symbols:     return L"Show symbol names";
    case Option::LineNumbers: return L"Show line numbers";
    }

    return L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptions::GetKey
//
////////////////////////////////////////////////////////////////////////////////

const char * DisassemblyOptions::GetKey (Option option)
{
    switch (option)
    {
    case Option::Addresses:   return "address";
    case Option::CodeBytes:   return "bytes";
    case Option::Source:      return "source";
    case Option::Symbols:     return "symbols";
    case Option::LineNumbers: return "lines";
    }

    return "";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptions::BuildRows
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DisassemblyOptions::Row> DisassemblyOptions::BuildRows (
    const std::vector<DebuggerViewSnapshot::CodeLine>  & lines,
    bool                                                 showSource,
    const HasLineFn                                    & hasLine)
{
    std::vector<Row>     rows;
    std::pair<int, int>  previous = { -1, 0 };



    for (size_t i = 0; i < lines.size(); i++)
    {
        const DebuggerViewSnapshot::CodeLine  & line  = lines[i];
        std::pair<int, int>                     place = { line.sourceFileId, line.sourceLine };

        if (showSource && line.sourceFileId >= 0 && place != previous && hasLine && hasLine (line.sourceFileId, line.sourceLine))
        {
            rows.push_back ({ -1, line.sourceFileId, line.sourceLine });
        }

        previous = place;
        rows.push_back ({ (int) i, line.sourceFileId, line.sourceLine });
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptions::GetLineOfRow
//
//  The instruction a row shows, or -1 for a source row or no row.
//
////////////////////////////////////////////////////////////////////////////////

int DisassemblyOptions::GetLineOfRow (const std::vector<Row> & rows, int row)
{
    if (row < 0 || row >= (int) rows.size())
    {
        return -1;
    }

    return rows[(size_t) row].codeLine;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptions::GetRowOfLine
//
////////////////////////////////////////////////////////////////////////////////

int DisassemblyOptions::GetRowOfLine (const std::vector<Row> & rows, int codeLine)
{
    for (size_t i = 0; i < rows.size(); i++)
    {
        if (codeLine >= 0 && rows[i].codeLine == codeLine)
        {
            return (int) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptions::GetInstructionText
//
////////////////////////////////////////////////////////////////////////////////

std::string DisassemblyOptions::GetInstructionText (const DebuggerViewSnapshot::CodeLine & line, bool showSymbols)
{
    std::string  text = line.instruction;
    size_t       at   = std::string::npos;



    if (showSymbols || line.shownOperand.empty() || line.shownOperand == line.memoryOperand)
    {
        return text;
    }

    at = text.rfind (line.shownOperand);

    if (at != std::string::npos)
    {
        text.replace (at, line.shownOperand.size(), line.memoryOperand);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptions::CanTakeBreakpoint
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyOptions::CanTakeBreakpoint (const DebuggerViewSnapshot::CodeLine & line)
{
    std::string  mnemonic = line.instruction.substr (0, line.instruction.find (' '));



    return !mnemonic.empty() && SourceSyntax::IsMnemonic (std::wstring (mnemonic.begin(), mnemonic.end()));
}
