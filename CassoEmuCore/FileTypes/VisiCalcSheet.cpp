#include "Pch.h"

#include "FileTypes/VisiCalcSheet.h"
#include "FileTypes/AppleWorks.h"
#include "FileTypes/TextCharset.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcSheet::Matches
//
//  A text file whose name ends .VC and whose first character starts an
//  entry or a setting.
//
////////////////////////////////////////////////////////////////////////////////

bool VisiCalcSheet::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    bool  typed = facts.HasProDosType (kProDosText) || (facts.IsDos33() && facts.type == kDosText);
    Byte  first = bytes.empty() ? 0 : (Byte) (bytes[0] & 0x7F);



    return typed && facts.IsNameEnding (".VC") && (first == '>' || first == '/');
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcSheet::ParseCoordinate
//
//  One or two column letters, A to BK, then the row, 1 to 254. The column
//  is from 0 and the row from 1.
//
////////////////////////////////////////////////////////////////////////////////

bool VisiCalcSheet::ParseCoordinate (std::string_view text, size_t & outColumn, size_t & outRow, size_t & outLength)
{
    size_t  at      = 0;
    size_t  column  = 0;
    size_t  letters = 0;
    size_t  row     = 0;



    while (at < text.size() && letters < 2 && text[at] >= 'A' && text[at] <= 'Z')
    {
        column = column * 26 + (size_t) (text[at] - 'A' + 1);
        at++;
        letters++;
    }

    while (at < text.size() && text[at] >= '0' && text[at] <= '9' && row <= kMaxRows)
    {
        row = row * 10 + (size_t) (text[at] - '0');
        at++;
    }

    if (letters == 0 || column > kMaxColumns || row == 0 || row > kMaxRows)
    {
        return false;
    }

    outColumn = column - 1;
    outRow    = row;
    outLength = at;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcSheet::ApplyGlobal
//
//  /GC is the column width, /GF the format letter, and /GO the order, C for
//  columns or R for rows; other settings don't affect the cells' text.
//
////////////////////////////////////////////////////////////////////////////////

void VisiCalcSheet::ApplyGlobal (std::string_view line, Sheet & ioSheet)
{
    if (line.starts_with ("/GC") && line.size() > 3)
    {
        size_t  width = 0;

        for (char c : line.substr (3))
        {
            if (c < '0' || c > '9')
            {
                break;
            }

            width = width * 10 + (size_t) (c - '0');
        }

        if (width > 0)
        {
            ioSheet.columnWidth = width;
        }
    }
    else if (line.starts_with ("/GF") && line.size() > 3)
    {
        ioSheet.format = line[3];
    }
    else if (line.starts_with ("/GO") && line.size() > 3)
    {
        ioSheet.byColumns = line[3] != 'R';
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcSheet::Parse
//
//  The lines are Apple text, the high bit set or clear. A line of anything
//  else, or an entry whose coordinate doesn't read, fails the file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT VisiCalcSheet::Parse (std::span<const Byte> bytes, Sheet & outSheet, std::wstring & outError)
{
    HRESULT  hr    = S_OK;
    size_t   lines = 0;
    bool     ok    = true;



    for (const std::wstring & wide : TextCharset::DecodeLines (bytes, Charset::AppleText))
    {
        std::string  line   = TextEncoding::WideToNarrow (wide);
        Cell         cell;
        size_t       length = 0;

        while (!line.empty() && (line.back() == ' ' || line.back() == '\0'))
        {
            line.pop_back();
        }

        if (line.empty())
        {
            continue;
        }

        lines++;

        if (line[0] == '/')
        {
            ApplyGlobal (line, outSheet);
            continue;
        }

        ok = line[0] == '>' && ParseCoordinate (std::string_view (line).substr (1), cell.column, cell.row, length) && line.size() > length + 1 && line[length + 1] == ':';
        CBRFEx (ok, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = std::format (L"Line {} isn't a VisiCalc entry.", lines));

        line.erase (0, length + 2);

        if (line.starts_with ("/F") && line.size() >= 3)
        {
            cell.format = line[2];
            line.erase (0, 3);
        }

        if (line.starts_with ("/-"))
        {
            cell.repeat   = true;
            cell.contents = line.substr (2);
        }
        else if (line.starts_with ("\""))
        {
            cell.label    = true;
            cell.contents = line.substr (1);
        }
        else
        {
            cell.contents = line;
        }

        outSheet.columns = std::max (outSheet.columns, cell.column + 1);
        outSheet.rows    = std::max (outSheet.rows, cell.row);
        outSheet.cells.push_back (std::move (cell));
    }

    ok = lines > 0;
    CBRFEx (ok, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The file doesn't have any VisiCalc entries.");

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcSheet::GetEntered
//
////////////////////////////////////////////////////////////////////////////////

std::wstring VisiCalcSheet::GetEntered (const Sheet & sheet, const Cell & cell)
{
    std::string  text;



    if (cell.repeat && !cell.contents.empty())
    {
        while (text.size() < sheet.columnWidth)
        {
            text += cell.contents;
        }

        text.resize (sheet.columnWidth);
    }
    else
    {
        text = cell.contents;
    }

    return TextEncoding::NarrowToWide (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcSheet::Decode
//
//  A row for each sheet row with an entry, its number first, and a column
//  for each sheet column up to the last with an entry.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT VisiCalcSheet::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT                                         hr = S_OK;
    Sheet                                           sheet;
    std::map<size_t, std::vector<std::wstring>>     rows;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);

    hr = Parse (bytes, sheet, outError);
    CHR (hr);

    outFile.kind = DecodedFile::Kind::Grid;
    outFile.grid.columns.push_back (L"Row");

    for (size_t column = 0; column < sheet.columns; column++)
    {
        outFile.grid.columns.push_back (AppleWorksSs::GetColumnLetters (column));
    }

    for (const Cell & cell : sheet.cells)
    {
        std::vector<std::wstring> &  row = rows[cell.row];

        if (row.empty())
        {
            row.resize (sheet.columns + 1);
            row[0] = std::to_wstring (cell.row);
        }

        row[cell.column + 1] = GetEntered (sheet, cell);
    }

    for (auto & [number, row] : rows)
    {
        outFile.grid.rows.push_back (std::move (row));
    }

Error:
    return hr;
}
