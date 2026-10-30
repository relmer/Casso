#include "Pch.h"

#include "CassoExplorer/Model/PreviewText.h"
#include "CassoExplorer/CassoExplorerBrowser.h"





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
            rows.push_back (std::move (row));
        }
    }
    else if (preview.kind == PreviewContent::Kind::Details)
    {
        for (const std::pair<std::wstring, std::wstring> & field : preview.details)
        {
            //  A section's heading is a row of one cell, under a blank row
            //  unless it starts the text.
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
    else
    {
        for (const std::wstring & line : preview.lines)
        {
            rows.push_back (DxuiTextView::Row { (preview.kind == PreviewContent::Kind::Listing) ? SplitLineNumber (line)
                                                                                               : std::vector<std::wstring> { line } });
        }
    }

    if (!preview.warning.empty())
    {
        warning.cells.push_back (preview.warning);
        warning.warning = true;

        rows.push_back (DxuiTextView::Row());
        rows.push_back (warning);
    }

    return rows;
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
