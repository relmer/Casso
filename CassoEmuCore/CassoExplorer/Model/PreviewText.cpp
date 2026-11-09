#include "Pch.h"

#include "CassoExplorer/Model/PreviewText.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Core/TextEncoding.h"
#include "ApplesoftTokenizer.h"
#include "IntegerBasicDetokenizer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewText::GetSourceColors
//
//  The background is dark when its luminance is under half. An address and
//  bytes, a listing's columns, are drawn in the muted text color.
//
////////////////////////////////////////////////////////////////////////////////

SourceSyntax::Colors PreviewText::GetSourceColors (const IDxuiTheme * theme)
{
    SourceSyntax::Colors  colors;
    uint32_t              page   = (theme != nullptr) ? theme->ContentBackground() : 0u;
    uint32_t              luma   = ((page >> 16) & 0xFF) * 299 + ((page >> 8) & 0xFF) * 587 + (page & 0xFF) * 114;
    bool                  dark   = luma < s_kDarkLumaLimit;



    if (theme == nullptr)
    {
        return colors;
    }

    colors.mnemonic  = dark ? 0xFF8CBCF0 : 0xFF0000FF;
    colors.directive = dark ? 0xFFC586C0 : 0xFFAF00DB;
    colors.symbol    = dark ? 0xFF4EC9B0 : 0xFF2B91AF;
    colors.number    = dark ? 0xFFB5CEA8 : 0xFF098658;
    colors.string    = dark ? 0xFFD69D85 : 0xFFA31515;
    colors.comment   = dark ? 0xFF57A64A : 0xFF008000;
    colors.address   = theme->ForegroundMuted();
    colors.bytes     = theme->ForegroundMuted();

    return colors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewText::BuildDetailRows
//
//  A label and its value a row. A section's heading is a row of one cell,
//  under a blank row unless it starts the text.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiTextView::Row> PreviewText::BuildDetailRows (const std::vector<std::pair<std::wstring, std::wstring>> & details)
{
    std::vector<DxuiTextView::Row>  rows;



    for (const std::pair<std::wstring, std::wstring> & field : details)
    {
        if (field.second.empty())
        {
            if (!rows.empty())
            {
                rows.push_back (DxuiTextView::Row { { std::wstring() } });
            }

            rows.push_back (DxuiTextView::Row { { field.first } });
            rows.back().heading = true;
            continue;
        }

        rows.push_back (DxuiTextView::Row { { field.first, field.second } });
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewText::BuildDocumentRows
//
//  A row for each paragraph, aligned as it is, a span for each styled run.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiTextView::Row> PreviewText::BuildDocumentRows (const std::vector<Paragraph> & paragraphs)
{
    std::vector<DxuiTextView::Row>  rows;



    for (const Paragraph & paragraph : paragraphs)
    {
        DxuiTextView::Row  row { { std::wstring() } };

        row.alignment = (paragraph.alignment == Alignment::Center) ? DxuiTextHAlign::Center
                      : (paragraph.alignment == Alignment::Right)  ? DxuiTextHAlign::Right
                                                                   : DxuiTextHAlign::Left;

        for (const Run & run : paragraph.runs)
        {
            if (run.bold || run.italic || run.underline)
            {
                DxuiTextView::Span  span;

                span.start     = (int) row.cells[0].size();
                span.length    = (int) run.text.size();
                span.bold      = run.bold;
                span.italic    = run.italic;
                span.underline = run.underline;
                row.spans.push_back (span);
            }

            row.cells[0] += run.text;
        }

        rows.push_back (std::move (row));
    }

    return rows;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PreviewText::BuildTextRows
//
//  A listing that is not a valid program ends with a blank row and the
//  warning that says why.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiTextView::Row> PreviewText::BuildTextRows (const PreviewContent & preview, bool lineAddresses, const SourceSyntax::Colors & colors)
{
    std::vector<DxuiTextView::Row>  rows;
    DxuiTextView::Row               warning;
    std::vector<Word>               addresses;



    //  A catalog's heading is apart from its contents, in the muted text color.
    if (!preview.heading.empty())
    {
        DxuiTextView::Row  heading { { preview.heading } };

        if (colors.address != 0)
        {
            heading.spans.push_back ({ 0, 0, (int) preview.heading.size(), colors.address });
        }

        rows.push_back (std::move (heading));
        rows.push_back (DxuiTextView::Row { { std::wstring() } });
    }

    if (lineAddresses && preview.kind == PreviewContent::Kind::Listing)
    {
        addresses = GetLineAddresses (preview.bytes, preview.integerBasic);
    }

    if (!addresses.empty())
    {
        for (size_t i = 0; i < preview.lines.size(); i++)
        {
            DxuiTextView::Row  row { SplitLineNumber (preview.lines[i]) };

            row.cells.insert (row.cells.begin(), (i < addresses.size()) ? std::format (L"${:04X}", addresses[i]) : std::wstring());
            AddBasicSpans (row, colors);
            rows.push_back (std::move (row));
        }
    }
    else if (preview.kind == PreviewContent::Kind::Document)
    {
        rows = BuildDocumentRows (preview.paragraphs);
    }
    else if (preview.kind == PreviewContent::Kind::Details)
    {
        rows = BuildDetailRows (preview.details);
    }
    else if (preview.source)
    {
        //  An assembler's source has its line numbers, right aligned in a
        //  column of their own, as an editor's gutter has them, and its syntax
        //  colored by the grammar of the assembler it was written for, as the
        //  debugger colors it.
        size_t                     digits    = std::to_wstring ((std::max) (preview.lines.size(), (size_t) 1)).size();
        SourceSyntax::Assembler    assembler = SourceSyntax::DetectAssembler (preview.lines);
        SourceSyntax::Listing      listing   = SourceSyntax::DetectListing (preview.lines);

        for (size_t i = 0; i < preview.lines.size(); i++)
        {
            DxuiTextView::Row  row { { std::format (L"{:>{}}", i + 1, digits), preview.lines[i] } };

            if (colors.mnemonic != 0)
            {
                for (const SourceSyntax::Run & run : SourceSyntax::GetLineRuns (preview.lines[i], assembler, listing))
                {
                    row.spans.push_back ({ 1, run.start, run.length, colors.Get (run.token) });
                }
            }

            rows.push_back (std::move (row));
        }
    }
    else if (preview.disassembly)
    {
        //  A disassembled line is its address, its bytes and its instruction
        //  at fixed columns, colored as the debugger colors them.
        for (const std::wstring & line : preview.lines)
        {
            DxuiTextView::Row  row { { line } };

            if (colors.mnemonic != 0 && line.size() > s_kInstructionColumn)
            {
                row.spans.push_back ({ 0, 0, (int) s_kAddressDigits, colors.address });
                row.spans.push_back ({ 0, (int) s_kBytesColumn, (int) (s_kInstructionColumn - s_kBytesColumn), colors.bytes });

                for (SourceSyntax::Run run : SourceSyntax::GetInstructionRuns (line.substr (s_kInstructionColumn)))
                {
                    //  A data line's DFB or DW, and ??? over bytes that aren't
                    //  an instruction, are not mnemonics.
                    if (run.token == SourceSyntax::Token::Mnemonic && !SourceSyntax::IsMnemonic (line.substr (s_kInstructionColumn + run.start, run.length)) &&
                        !IsUndocumentedMnemonic (line.substr (s_kInstructionColumn + run.start, run.length)))
                    {
                        run.token = SourceSyntax::Token::Directive;
                    }

                    row.spans.push_back ({ 0, (int) s_kInstructionColumn + run.start, run.length, colors.Get (run.token) });
                }
            }

            rows.push_back (std::move (row));
        }
    }
    else
    {
        for (const std::wstring & line : preview.lines)
        {
            rows.push_back (DxuiTextView::Row { (preview.kind == PreviewContent::Kind::Listing) ? SplitLineNumber (line)
                                                                                               : std::vector<std::wstring> { line } });

            if (preview.kind == PreviewContent::Kind::Listing)
            {
                AddBasicSpans (rows.back(), colors);
            }
        }
    }

    //  A catalog's rows don't wrap, so a warning of several sentences, as a
    //  damaged directory's is, goes in a row a sentence.
    if (!preview.warning.empty())
    {
        size_t  start = 0;

        rows.push_back (DxuiTextView::Row());

        while (start < preview.warning.size())
        {
            size_t  end = preview.warning.find (L". ", start);

            end = (end == std::wstring::npos) ? preview.warning.size() : end + 1;

            warning.cells   = { preview.warning.substr (start, end - start) };
            warning.warning = true;
            rows.push_back (warning);

            start = preview.warning.find_first_not_of (L' ', end);
            start = (start == std::wstring::npos) ? preview.warning.size() : start;
        }
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewText::AddBasicSpans
//
//  A listing row's cells are its load address, when the address column is
//  on, its line number and its statements. The address and the number are in the muted color, and the
//  statements are colored as GetBasicRuns reads them.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewText::AddBasicSpans (DxuiTextView::Row & ioRow, const SourceSyntax::Colors & colors)
{
    int  last = (int) ioRow.cells.size() - 1;



    if (colors.mnemonic == 0 || last < 0)
    {
        return;
    }

    for (int cell = 0; cell < last; cell++)
    {
        ioRow.spans.push_back ({ cell, 0, (int) ioRow.cells[(size_t) cell].size(), colors.address });
    }

    for (const SourceSyntax::Run & run : GetBasicRuns (ioRow.cells[(size_t) last]))
    {
        ioRow.spans.push_back ({ last, run.start, run.length, colors.Get (run.token) });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewText::GetBasicRuns
//
//  Applesoft and Integer BASIC statements: a keyword of either dialect where
//  a word starts, a variable for any other word, strings, numbers, and a REM
//  to the end of the line as a comment.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SourceSyntax::Run> PreviewText::GetBasicRuns (const std::wstring & statements)
{
    std::vector<SourceSyntax::Run>  runs;
    size_t                          i = 0;
    size_t                          n = statements.size();



    while (i < n)
    {
        wchar_t  ch     = statements[i];
        size_t   end    = i + 1;
        bool     isWord = iswalpha (ch) && (i == 0 || !iswalnum (statements[i - 1]));
        size_t   length = isWord ? MatchBasicKeyword (statements, i) : 0;

        if (ch == L'"')
        {
            end = statements.find (L'"', i + 1);
            end = (end == std::wstring::npos) ? n : end + 1;
            runs.push_back ({ (int) i, (int) (end - i), SourceSyntax::Token::String });
        }
        else if (length > 0 && statements.compare (i, length, L"REM") == 0)
        {
            runs.push_back ({ (int) i, (int) (n - i), SourceSyntax::Token::Comment });
            end = n;
        }
        else if (length > 0)
        {
            end = i + length;
            runs.push_back ({ (int) i, (int) length, SourceSyntax::Token::Mnemonic });
        }
        else if (isWord)
        {
            while (end < n && (iswalnum (statements[end]) || statements[end] == L'$' || statements[end] == L'%'))
            {
                end++;
            }

            runs.push_back ({ (int) i, (int) (end - i), SourceSyntax::Token::Symbol });
        }
        else if (iswdigit (ch) || (ch == L'.' && end < n && iswdigit (statements[end])))
        {
            while (end < n && (iswdigit (statements[end]) || statements[end] == L'.'))
            {
                end++;
            }

            runs.push_back ({ (int) i, (int) (end - i), SourceSyntax::Token::Number });
        }

        i = end;
    }

    return runs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewText::MatchBasicKeyword
//
//  The length of the longest keyword of either dialect at `at`, or zero. The
//  keywords are the detokenizers' keywords that start with a letter.
//
////////////////////////////////////////////////////////////////////////////////

size_t PreviewText::MatchBasicKeyword (const std::wstring & text, size_t at)
{
    static const std::vector<std::wstring>  s_keywords = []()
    {
        std::set<std::wstring>     found;
        std::vector<std::wstring>  sorted;

        for (int token = ApplesoftTokenizer::kFirstToken; token <= ApplesoftTokenizer::kLastToken; token++)
        {
            const char *  keyword = ApplesoftTokenizer::GetKeyword ((Byte) token);

            found.insert ((keyword != nullptr) ? TextEncoding::NarrowToWide (keyword) : std::wstring());
        }

        for (int token = 0; token <= IntegerBasicDetokenizer::kLastToken; token++)
        {
            const char *  keyword = IntegerBasicDetokenizer::GetKeyword ((Byte) token);

            found.insert ((keyword != nullptr) ? TextEncoding::NarrowToWide (keyword) : std::wstring());
        }

        for (const std::wstring & keyword : found)
        {
            if (!keyword.empty() && iswalpha (keyword[0]))
            {
                sorted.push_back (keyword);
            }
        }

        std::ranges::sort (sorted, [] (const std::wstring & a, const std::wstring & b) { return a.size() > b.size(); });
        return sorted;
    }();



    for (const std::wstring & keyword : s_keywords)
    {
        if (text.compare (at, keyword.size(), keyword) == 0)
        {
            return keyword.size();
        }
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewText::IsUndocumentedMnemonic
//
//  The NMOS 6502's undocumented instructions, as a disassembly writes them.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewText::IsUndocumentedMnemonic (const std::wstring & word)
{
    static const std::set<std::wstring>  s_kMnemonics =
    {
        L"ALR", L"ANC", L"ANE", L"ARR", L"DCP", L"ISC", L"JAM", L"LAS", L"LAX", L"LXA", L"RLA", L"RRA",
        L"SAX", L"SBX", L"SHA", L"SHX", L"SHY", L"SLO", L"SRE", L"TAS",
    };



    return s_kMnemonics.contains (word);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewText::GetLineAddresses
//
//  Where each line of a BASIC program starts in memory.
//
//  An Applesoft line begins with a pointer to the next one, and a pointer is
//  an absolute address, so the first line's pointer less the second line's
//  offset in the file is the address the program loads at.
//
//  An Integer BASIC line begins with its length, and the program sits against
//  HIMEM, which DOS 3.3 on a 48K machine leaves at $9600. The addresses assume
//  that HIMEM, since the file does not record one.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Word> PreviewText::GetLineAddresses (const std::vector<Byte> & program, bool integerBasic)
{
    std::vector<size_t>  starts;
    std::vector<Word>    addresses;
    size_t               offset    = 0;
    Word                 firstLink = 0;
    Word                 base      = 0;



    if (integerBasic)
    {
        while (offset < program.size() && program[offset] != 0 && offset + program[offset] <= program.size())
        {
            starts.push_back (offset);
            offset += program[offset];
        }

        base = (Word) (kIntegerBasicHimem - program.size());
    }

    while (!integerBasic && offset + 4 <= program.size())
    {
        Word    link = (Word) (program[offset] | (program[offset + 1] << 8));
        size_t  end  = offset + 4;

        if (link == 0)
        {
            break;
        }

        while (end < program.size() && program[end] != 0)
        {
            end++;
        }

        if (end >= program.size())
        {
            break;
        }

        if (starts.empty())
        {
            firstLink = link;
        }

        starts.push_back (offset);
        offset = end + 1;

        if (starts.size() == 1)
        {
            base = (Word) (firstLink - offset);
        }
    }

    for (size_t start : starts)
    {
        addresses.push_back ((Word) (base + start));
    }

    return addresses;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewText::SplitLineNumber
//
//  A listing line's number and the statement after the spaces that follow it,
//  or the whole line as one cell when it does not start with a number.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> PreviewText::SplitLineNumber (const std::wstring & line)
{
    size_t  first = line.find_first_not_of (L' ');
    size_t  end   = std::wstring::npos;
    size_t  text  = std::wstring::npos;



    if (first != std::wstring::npos && iswdigit (line[first]))
    {
        end  = line.find_first_not_of (L"0123456789", first);
        text = (end == std::wstring::npos) ? std::wstring::npos : line.find_first_not_of (L' ', end);
    }

    if (text == std::wstring::npos || text == end)
    {
        return { line };
    }

    return { line.substr (0, end), line.substr (text) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewText::FormatPreviewError
//
//  "No preview", and three lines below it, why. An image refused for its size
//  has a whole sentence about the file for the command line; the preview
//  has only the reason. A read under way is no failure, and its message
//  has no heading.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PreviewText::FormatPreviewError (const std::wstring & message)
{
    std::wstring  reason = message;



    if (message.find (L"800K images") != std::wstring::npos)
    {
        reason = L"800K images are not supported yet";
    }

    if (message == CassoExplorerBrowser::s_kWorkingText)
    {
        return message;
    }

    if (reason.starts_with (L"No preview"))
    {
        return reason;
    }

    return L"No preview\n\n\n" + reason;
}
