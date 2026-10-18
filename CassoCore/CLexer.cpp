#include "Pch.h"

#include "CLexer.h"





static constexpr const wchar_t * s_kpszKeywords[] =
{
    L"auto", L"break", L"case", L"const", L"continue", L"default", L"do", L"else", L"enum", L"extern",
    L"for", L"goto", L"if", L"inline", L"register", L"restrict", L"return", L"sizeof", L"static",
    L"struct", L"switch", L"typedef", L"union", L"volatile", L"while", L"asm", L"__asm__", L"__fastcall__",
    L"fastcall", L"cdecl", L"__cdecl__", L"_Bool", L"_Static_assert",
};

static constexpr const wchar_t * s_kpszTypes[] =
{
    L"char", L"short", L"int", L"long", L"signed", L"unsigned", L"float", L"double", L"void",
    L"bool", L"size_t", L"int8_t", L"int16_t", L"int32_t", L"uint8_t", L"uint16_t", L"uint32_t",
};





////////////////////////////////////////////////////////////////////////////////
//
//  CLexer::GetRuns
//
//  A line whose first character past its indent is # is a preprocessor line
//  to its end, a comment on it included, unless a block comment is open.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<CLexer::Run> CLexer::GetRuns (const std::wstring & line, bool & ioIsInComment)
{
    std::vector<Run>  runs;
    size_t            at    = 0;
    size_t            end   = 0;
    size_t            first = line.find_first_not_of (L" \t");



    if (!ioIsInComment && first != std::wstring::npos && line[first] == L'#')
    {
        runs.push_back ({ (int) first, (int) (line.size() - first), Token::Preprocessor });
        return runs;
    }

    while (at < line.size())
    {
        wchar_t  ch   = line[at];
        wchar_t  next = (at + 1 < line.size()) ? line[at + 1] : L'\0';



        if (ioIsInComment || (ch == L'/' && next == L'*'))
        {
            size_t  from = at;
            size_t  stop = line.find (L"*/", ioIsInComment ? at : at + 2);



            ioIsInComment = stop == std::wstring::npos;
            end           = ioIsInComment ? line.size() : stop + 2;

            runs.push_back ({ (int) from, (int) (end - from), Token::Comment });
            at = end;
        }
        else if (ch == L'/' && next == L'/')
        {
            runs.push_back ({ (int) at, (int) (line.size() - at), Token::Comment });
            at = line.size();
        }
        else if (ch == L'"' || ch == L'\'')
        {
            end = ScanQuoted (line, at);
            runs.push_back ({ (int) at, (int) (end - at), Token::String });
            at = end;
        }
        else if (iswdigit (ch) || (ch == L'.' && iswdigit (next)))
        {
            end = ScanNumber (line, at);
            runs.push_back ({ (int) at, (int) (end - at), Token::Number });
            at = end;
        }
        else if (IsWordStart (ch))
        {
            std::wstring  word;



            for (end = at; end < line.size() && IsWordChar (line[end]); end++)
            {
            }

            word = line.substr (at, end - at);

            if (IsKeyword (word))
            {
                runs.push_back ({ (int) at, (int) (end - at), Token::Keyword });
            }
            else if (IsType (word))
            {
                runs.push_back ({ (int) at, (int) (end - at), Token::Type });
            }

            at = end;
        }
        else
        {
            at++;
        }
    }

    return runs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CLexer::IsCFile
//
////////////////////////////////////////////////////////////////////////////////

bool CLexer::IsCFile (const std::wstring & name)
{
    size_t        dot       = name.find_last_of (L"./\\");
    std::wstring  extension = (dot != std::wstring::npos && name[dot] == L'.') ? name.substr (dot) : std::wstring();



    std::ranges::transform (extension, extension.begin(), [] (wchar_t ch) { return (wchar_t) towlower (ch); });

    return extension == L".c" || extension == L".h";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CLexer::IsKeyword
//
////////////////////////////////////////////////////////////////////////////////

bool CLexer::IsKeyword (const std::wstring & word)
{
    return std::ranges::any_of (s_kpszKeywords, [&word] (const wchar_t * each) { return word == each; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CLexer::IsType
//
////////////////////////////////////////////////////////////////////////////////

bool CLexer::IsType (const std::wstring & word)
{
    return std::ranges::any_of (s_kpszTypes, [&word] (const wchar_t * each) { return word == each; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CLexer::ScanQuoted
//
//  Past the closing quote, a backslash escaping the character after it; the
//  line's end for one never closed.
//
////////////////////////////////////////////////////////////////////////////////

size_t CLexer::ScanQuoted (const std::wstring & line, size_t at)
{
    wchar_t  quote = line[at];
    size_t   i     = at + 1;



    while (i < line.size() && line[i] != quote)
    {
        i += (line[i] == L'\\') ? 2 : 1;
    }

    return std::min (i + 1, line.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CLexer::ScanNumber
//
//  Decimal, hex (0x), octal and floating point, with their suffixes: every
//  letter, digit and dot that follows, and a sign after an exponent (e, or p
//  in hex).
//
////////////////////////////////////////////////////////////////////////////////

size_t CLexer::ScanNumber (const std::wstring & line, size_t at)
{
    size_t   i        = at;
    bool     isHex    = line[at] == L'0' && at + 1 < line.size() && towlower (line[at + 1]) == L'x';
    wchar_t  exponent = isHex ? L'p' : L'e';



    while (i < line.size())
    {
        wchar_t  ch     = line[i];
        bool     isSign = (ch == L'+' || ch == L'-') && i > at && towlower (line[i - 1]) == exponent;

        if (!iswalnum (ch) && ch != L'.' && !isSign)
        {
            break;
        }

        i++;
    }

    return i;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CLexer::IsWordStart
//
////////////////////////////////////////////////////////////////////////////////

bool CLexer::IsWordStart (wchar_t ch)
{
    return iswalpha (ch) || ch == L'_';
}





////////////////////////////////////////////////////////////////////////////////
//
//  CLexer::IsWordChar
//
////////////////////////////////////////////////////////////////////////////////

bool CLexer::IsWordChar (wchar_t ch)
{
    return iswalnum (ch) || ch == L'_';
}
