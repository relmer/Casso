#include "Pch.h"

#include "Ui/Debugger/SourceLineText.h"
#include "Ui/Debugger/SourceSyntax.h"

#include "Core/TextEncoding.h"
#include "Debugger/SymbolDescriptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourceLineText::GetWordAt
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SourceLineText::GetWordAt (const std::wstring & line, int ch)
{
    int  start = 0;
    int  end   = 0;



    //  With no symbols to match, the call only sets where the word starts.
    (void) FindSymbolAt (line, ch, {}, start);

    if (start < 0 || start >= (int) line.size())
    {
        return {};
    }

    for (end = start; end < (int) line.size() && (iswalnum (line[(size_t) end]) || line[(size_t) end] == L'_' || line[(size_t) end] == L'.' || line[(size_t) end] == L'@'); end++)
    {
    }

    return (end > start && line.substr (0, (size_t) start).find (L';') == std::wstring::npos) ? line.substr ((size_t) start, (size_t) (end - start)) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceLineText::IsMacroDefinition
//
//  The first two words of the line, before a comment, compared with the
//  name in its case and the keyword in any case. A label may end in a colon.
//
////////////////////////////////////////////////////////////////////////////////

bool SourceLineText::IsMacroDefinition (const std::wstring & line, const std::wstring & name)
{
    std::wistringstream        words (line.substr (0, line.find (L';')));
    std::wstring               first;
    std::wstring               second;
    auto                       isKeyword = [] (std::wstring word)
    {
        std::ranges::transform (word, word.begin(), [] (wchar_t each) { return (wchar_t) towlower (each); });
        return word == L"macro" || word == L".macro" || word == L"mac" || word == L".mac";
    };



    words >> first >> second;

    if (!first.empty() && first.back() == L':')
    {
        first.pop_back();
    }

    return (first == name && isKeyword (second)) || (isKeyword (first) && second == name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceLineText::FindSymbolAt
//
//  A word is letters, digits, _, . and @, as a cheap local symbol is written
//  in ca65. A symbol is matched by its case first, then in any case.
//
////////////////////////////////////////////////////////////////////////////////

const SymbolInfo * SourceLineText::FindSymbolAt (const std::wstring & line, int ch, const std::vector<SymbolInfo> & symbols, int & start)
{
    auto                isWordChar = [] (wchar_t each) { return iswalnum (each) || each == L'_' || each == L'.' || each == L'@'; };
    int                 size        = (int) line.size();
    int                 end         = 0;
    std::string         name;
    const SymbolInfo  * found       = nullptr;



    start = ch;

    if (ch < 0 || ch > size)
    {
        return nullptr;
    }

    //  A caret just past a word is on that word.
    if ((ch == size || !isWordChar (line[(size_t) ch])) && ch > 0 && isWordChar (line[(size_t) ch - 1]))
    {
        start = ch - 1;
    }

    if (start >= size || !isWordChar (line[(size_t) start]) || line.substr (0, (size_t) start).find (L';') != std::wstring::npos)
    {
        return nullptr;
    }

    while (start > 0 && isWordChar (line[(size_t) start - 1]))
    {
        start--;
    }

    for (end = start; end < size && isWordChar (line[(size_t) end]); end++)
    {
    }

    name = TextEncoding::WideToNarrow (line.substr ((size_t) start, (size_t) (end - start)));

    for (const SymbolInfo & symbol : symbols)
    {
        if (symbol.name == name)
        {
            found = &symbol;
            break;
        }

        found = (found == nullptr && _stricmp (symbol.name.c_str(), name.c_str()) == 0) ? &symbol : found;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceLineText::FindCodeStart
//
//  A line that starts with a space has no label; a comment line has no code.
//
////////////////////////////////////////////////////////////////////////////////

int SourceLineText::FindCodeStart (const std::wstring & line)
{
    int   size    = (int) line.size();
    int   at      = 0;
    bool  isAll   = size > 0 && (line[0] == L';' || line[0] == L'*');



    while (!isAll && at < size && !iswspace (line[(size_t) at]))
    {
        at++;
    }

    while (!isAll && at < size && iswspace (line[(size_t) at]))
    {
        at++;
    }

    return (isAll || at >= size || line[(size_t) at] == L';') ? 0 : at;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceLineText::TryGetOperandSymbolText
//
//  Past the mnemonic, each word in turn, up to the comment; only the
//  symbol's first line, not the byte a < or > takes.
//
////////////////////////////////////////////////////////////////////////////////

bool SourceLineText::TryGetOperandSymbolText (const std::wstring & line, int codeAt, const std::vector<SymbolInfo> & symbols, std::wstring & text)
{
    int           at      = FindOperandSymbol (line, codeAt, symbols);
    bool          isFound = at >= 0 && TryGetSymbolText (line, at, symbols, text);



    if (isFound)
    {
        text = text.substr (0, text.find (L'\n'));
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceLineText::FindOperandSymbol
//
//  Past the mnemonic, each word in turn, up to the comment.
//
////////////////////////////////////////////////////////////////////////////////

int SourceLineText::FindOperandSymbol (const std::wstring & line, int codeAt, const std::vector<SymbolInfo> & symbols)
{
    auto    isWordChar = [] (wchar_t each) { return iswalnum (each) || each == L'_' || each == L'.' || each == L'@'; };
    int     size       = (int) line.size();
    int     at         = std::clamp (codeAt, 0, size);
    int     found      = -1;
    int     start      = 0;



    while (at < size && !iswspace (line[(size_t) at]))
    {
        at++;
    }

    for (; at < size && line[(size_t) at] != L';' && found < 0; at++)
    {
        if (!isWordChar (line[(size_t) at]) || (at > 0 && isWordChar (line[(size_t) at - 1])))
        {
            continue;
        }

        found = (FindSymbolAt (line, at, symbols, start) != nullptr) ? at : -1;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceLineText::IsOperandSymbolAt
//
//  The same symbol written as the line's label, or in its comment, is not
//  the operand's.
//
////////////////////////////////////////////////////////////////////////////////

bool SourceLineText::IsOperandSymbolAt (const std::wstring & line, int codeAt, int ch, const std::vector<SymbolInfo> & symbols)
{
    int   start   = 0;
    int   operand = FindOperandSymbol (line, codeAt, symbols);



    return operand >= 0 && FindSymbolAt (line, ch, symbols, start) != nullptr && start == operand;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceLineText::TryGetSymbolText
//
////////////////////////////////////////////////////////////////////////////////

bool SourceLineText::TryGetSymbolText (const std::wstring & line, int ch, const std::vector<SymbolInfo> & symbols, std::wstring & text,
                                   const SymbolInfo ** symbol)
{
    int                 start       = 0;
    const SymbolInfo  * found       = FindSymbolAt (line, ch, symbols, start);
    wchar_t             op          = 0;
    bool                isImmediate = false;



    if (found == nullptr)
    {
        return false;
    }

    if (symbol != nullptr)
    {
        *symbol = found;
    }

    text = std::format (L"{} = ${:04X}{}", TextEncoding::NarrowToWide (found->name), found->address, found->isConstant ? L", a constant" : L"");

    op          = (start > 0) ? line[(size_t) start - 1] : 0;
    isImmediate = start > 1 && line[(size_t) start - 2] == L'#';

    if (op == L'<' || op == L'>')
    {
        text += std::format (L"\n{}{}{} = {}${:02X}, its {} byte", isImmediate ? L"#" : L"", op, TextEncoding::NarrowToWide (found->name),
                             isImmediate ? L"#" : L"", (op == L'<') ? (found->address & 0xFF) : (found->address >> 8), (op == L'<') ? L"low" : L"high");
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceLineText::AppendSymbolDetails
//
////////////////////////////////////////////////////////////////////////////////

void SourceLineText::AppendSymbolDetails (std::wstring & text, const std::string & name, Word address, bool isConstant,
                                      const DebuggerViewSnapshot & snapshot, const std::string & comment)
{
    const char  * about = SymbolDescriptions::Find (name);
    auto          read  = std::ranges::lower_bound (snapshot.symbolBytes, address, {}, &DebuggerViewSnapshot::SymbolBytes::address);



    if (!isConstant && read != snapshot.symbolBytes.end() && read->address == address)
    {
        text += std::format (L"\n${:04X}:", address);

        for (size_t i = 0; i < read->count; i++)
        {
            text += std::format (L" {:02X}", read->bytes[i]);
        }
    }

    if (about != nullptr)
    {
        text += L"\n" + TextEncoding::NarrowToWide (about);
    }

    if (!comment.empty())
    {
        text += L"\n" + TextEncoding::NarrowToWide (comment);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceLineText::IsInstructionLine
//
//  Whether a line's opcode is a 65C02 mnemonic.
//
////////////////////////////////////////////////////////////////////////////////

bool SourceLineText::IsInstructionLine (const std::wstring & line, SourceSyntax::Assembler assembler)
{
    for (const SourceSyntax::Run & run : SourceSyntax::GetSourceRuns (line, assembler))
    {
        if (run.token == SourceSyntax::Token::Mnemonic)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceLineText::IsLabelLine
//
//  A symbol in the first column, then nothing but a comment.
//
////////////////////////////////////////////////////////////////////////////////

bool SourceLineText::IsLabelLine (const std::wstring & line, SourceSyntax::Assembler assembler)
{
    std::vector<SourceSyntax::Run>  runs = SourceSyntax::GetSourceRuns (line, assembler);



    if (runs.empty() || runs[0].start != 0 || runs[0].token != SourceSyntax::Token::Symbol)
    {
        return false;
    }

    for (size_t i = 1; i < runs.size(); i++)
    {
        if (runs[i].token != SourceSyntax::Token::Comment)
        {
            return false;
        }
    }

    return true;
}
