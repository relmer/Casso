#include "Pch.h"

#include "DisassemblyOptions.h"
#include "SourceSyntax.h"
#include "Core/TextEncoding.h"





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
        text += text.empty() ? "" : " ";
        text += m_on[(size_t) i] ? "" : "-";
        text += GetKey ((Option) i);
    }

    text += std::string (" ") + kDimPrefix + GetDimKey (m_dim);

    return text;
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
    bool                isOn        = false;
    bool                isFirstForm = text.find ('-') == std::string::npos;
    int                 i           = 0;



    if (isFirstForm)
    {
        for (i = 0; i < kFirstTextOptionCount && !text.empty(); i++)
        {
            options.m_on[(size_t) i] = false;
        }
    }

    while (words >> word)
    {
        for (i = 0; i < kDimCount && word.starts_with (kDimPrefix); i++)
        {
            if (word.substr (std::strlen (kDimPrefix)) == GetDimKey ((Dim) i))
            {
                options.m_dim = (Dim) i;
            }
        }

        isOn = !word.starts_with ('-');
        word = isOn ? word : word.substr (1);

        for (i = 0; i < kOptionCount; i++)
        {
            if (word == GetKey ((Option) i))
            {
                options.m_on[(size_t) i] = isOn;
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
    case Option::Addresses:     return L"Show address";
    case Option::CodeBytes:     return L"Show code bytes";
    case Option::Source:        return L"Show source code";
    case Option::Symbols:       return L"Show symbol names";
    case Option::LineNumbers:   return L"Show line numbers";
    case Option::OperandValues: return L"Show operand values";
    case Option::Pointers:      return L"Show pointers";
    case Option::BranchArrow:   return L"Show branch arrow";
    case Option::AddressColon:  return L"Show colon after address";
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
    case Option::Addresses:     return "address";
    case Option::CodeBytes:     return "bytes";
    case Option::Source:        return "source";
    case Option::Symbols:       return "symbols";
    case Option::LineNumbers:   return "lines";
    case Option::OperandValues: return "values";
    case Option::Pointers:      return "pointers";
    case Option::BranchArrow:   return "arrow";
    case Option::AddressColon:  return "colon";
    }

    return "";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptions::GetDimLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DisassemblyOptions::GetDimLabel (Dim dim)
{
    switch (dim)
    {
    case Dim::Source: return L"Source lines";
    case Dim::Code:   return L"Disassembly lines";
    case Dim::None:   return L"Neither";
    }

    return L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptions::GetDimKey
//
////////////////////////////////////////////////////////////////////////////////

const char * DisassemblyOptions::GetDimKey (Dim dim)
{
    switch (dim)
    {
    case Dim::Source: return "source";
    case Dim::Code:   return "code";
    case Dim::None:   return "none";
    }

    return "";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptions::GetSourceTip
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DisassemblyOptions::GetSourceTip (bool hasDebugFile)
{
    if (hasDebugFile)
    {
        return L"Show source code\nShow each source line above the code it built";
    }

    return L"Show source code\nNo debug file maps this code. To get source, load its debug file\n"
           L"(File > Open symbol or debug file), or open its source file (File > Open source file)";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptions::GetLineNumbersTip
//
//  Line numbers number the source lines, so while they are unavailable the
//  tip says what turns them on.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DisassemblyOptions::GetLineNumbersTip (bool hasDebugFile, bool sourceShown)
{
    if (!hasDebugFile)
    {
        return L"Show line numbers\nLine numbers number the source lines. To get them, load the code's debug file\n"
               L"(File > Open symbol or debug file), then turn on Show source code";
    }

    if (!sourceShown)
    {
        return L"Show line numbers\nLine numbers number the source lines. To get them, turn on Show source code";
    }

    return L"Show line numbers\nShow each source line's number in its file";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptions::GetSymbolsTip
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DisassemblyOptions::GetSymbolsTip (const std::vector<std::string> & sources)
{
    std::wstring  tip = L"Show symbol names\n";



    if (sources.empty())
    {
        return tip + L"No symbols are loaded";
    }

    tip += L"Symbols loaded from:";

    for (const std::string & source : sources)
    {
        tip += L"\n" + TextEncoding::NarrowToWide (source);
    }

    return tip;
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



    return !mnemonic.empty() && SourceSyntax::IsMnemonic (TextEncoding::NarrowToWide (mnemonic));
}
