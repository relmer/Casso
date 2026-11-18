#include "Pch.h"

#include "Cli/CommandLineQuoting.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineQuoting::QuoteArgument
//
//  A run of backslashes is literal unless a quote follows it. Before an
//  escaped quote, or before the closing quote, each backslash in the run is
//  written twice, so the parser halves the run back to its length.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CommandLineQuoting::QuoteArgument (const std::wstring & argument)
{
    std::wstring  quoted      = L"\"";
    size_t        backslashes = 0;



    for (wchar_t ch : argument)
    {
        if (ch == L'\\')
        {
            backslashes++;
            continue;
        }

        if (ch == L'"')
        {
            quoted.append (backslashes * 2 + 1, L'\\');
        }
        else
        {
            quoted.append (backslashes, L'\\');
        }

        quoted      += ch;
        backslashes  = 0;
    }

    quoted.append (backslashes * 2, L'\\');
    quoted += L'"';

    return quoted;
}
