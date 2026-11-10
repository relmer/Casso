#include "Pch.h"

#include "FileTypes/VisiCalcSheet.h"
#include "FileTypes/VisiCalcEvaluator.h"
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
//  VisiCalcSheet::FormatFixed
//
//  As VisiCalc draws it: without a zero ahead of the point or a sign on
//  a zero.
//
////////////////////////////////////////////////////////////////////////////////

std::string VisiCalcSheet::FormatFixed (double number, int decimals)
{
    std::string  text   = std::format ("{:.{}f}", number, decimals);
    bool         isZero = text.find_first_of ("123456789") == std::string::npos;
    size_t       zero   = 0;



    if (isZero && text.starts_with ("-"))
    {
        text.erase (0, 1);
    }

    zero = text.starts_with ("-") ? 1 : 0;

    if (text.size() > zero + 1 && text[zero] == '0' && text[zero + 1] == '.')
    {
        text.erase (zero, 1);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcSheet::FormatScientific
//
//  A mantissa of as many digits as fit, then E and the power of ten.
//  Returns an empty string where no mantissa fits.
//
////////////////////////////////////////////////////////////////////////////////

std::string VisiCalcSheet::FormatScientific (double number, size_t limit)
{
    int     power    = (int) std::floor (std::log10 (std::fabs (number)));
    double  mantissa = number / std::pow (10.0, power);



    for (int decimals = kDigits - 1; decimals >= 0; decimals--)
    {
        std::string  digits = std::format ("{:.{}f}", mantissa, decimals);
        bool         ten    = digits.starts_with ("10") || digits.starts_with ("-10");
        std::string  text;

        if (ten)
        {
            digits = std::format ("{:.{}f}", mantissa / 10, decimals);
        }

        if (digits.find ('.') != std::string::npos)
        {
            digits.erase (digits.find_last_not_of ('0') + 1);
            digits.erase (digits.find_last_not_of ('.') + 1);
        }

        text = digits + "E" + std::to_string (power + (ten ? 1 : 0));

        if (text.size() <= limit)
        {
            return text;
        }
    }

    return std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcSheet::FormatGeneral
//
//  As many decimal places as fit, to kDigits significant digits, without
//  trailing zeros. A number whose whole part doesn't fit, or whose digits
//  would all be dropped, is drawn with an exponent.
//
////////////////////////////////////////////////////////////////////////////////

std::string VisiCalcSheet::FormatGeneral (double number, size_t limit)
{
    double  size  = std::fabs (number);
    int     whole = (size < 1) ? 0 : (int) std::floor (std::log10 (size)) + 1;



    if (number == 0)
    {
        return "0";
    }

    for (int decimals = std::max (0, kDigits - whole); decimals >= 0; decimals--)
    {
        std::string  text = FormatFixed (number, decimals);

        if (text.find ('.') != std::string::npos)
        {
            text.erase (text.find_last_not_of ('0') + 1);
            text.erase (text.find_last_not_of ('.') + 1);
        }

        if (text.empty() || text == "-")
        {
            break;
        }

        if (text.size() <= limit)
        {
            return text;
        }
    }

    return FormatScientific (number, limit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcSheet::FormatNumber
//
//  I rounds to a whole number, $ to cents, and * draws that many
//  asterisks; any other format is the general one. A number too long for
//  the limit is a row of > signs.
//
////////////////////////////////////////////////////////////////////////////////

std::string VisiCalcSheet::FormatNumber (double number, char format, size_t limit)
{
    std::string  text;



    if (format == '*')
    {
        return std::string ((size_t) std::clamp (number, 0.0, (double) limit), '*');
    }

    text = (format == 'I') ? FormatFixed (std::round (number), 0)
         : (format == '$') ? FormatFixed (number, 2)
         :                   FormatGeneral (number, limit);

    return (text.empty() || text.size() > limit) ? std::string (limit, '>') : text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcSheet::Draw
//
//  A label starts at the left and a value ends at the right, short of the
//  column's first character, unless the format is L or R.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring VisiCalcSheet::Draw (const Sheet & sheet, const Cell & cell, const Value & value)
{
    size_t       width  = sheet.columnWidth;
    size_t       limit  = (width > 0) ? width - 1 : 0;
    char         format = (cell.format != 0 && cell.format != 'D') ? cell.format : sheet.format;
    bool         left   = cell.label ? (format != 'R') : (format == 'L' || format == '*');
    std::string  text;



    if (cell.repeat)
    {
        return GetEntered (sheet, cell);
    }

    text = (value.kind == Value::Kind::Label)        ? cell.contents.substr (0, width)
         : (value.kind == Value::Kind::Number)       ? FormatNumber (value.number, format, limit)
         : (value.kind == Value::Kind::Boolean)      ? std::string (value.number != 0 ? "TRUE" : "FALSE")
         : (value.kind == Value::Kind::NotAvailable) ? std::string ("NA")
         : (value.kind == Value::Kind::Error)        ? std::string ("ERROR")
         :                                             std::string();

    if (!cell.label && text.size() > limit)
    {
        text.resize (limit);
    }

    if (text.size() < width)
    {
        text.insert (left ? text.size() : 0, width - text.size(), ' ');
    }

    return TextEncoding::NarrowToWide (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcSheet::Decode
//
//  A row for each sheet row with an entry, its number first, and a column
//  for each sheet column up to the last with an entry. Each cell is drawn
//  as VisiCalc draws it after a recalculation, without the padding, or as
//  typed where the options specify formulas.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT VisiCalcSheet::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT                                         hr = S_OK;
    Sheet                                           sheet;
    std::vector<Value>                              values;
    std::map<size_t, std::vector<std::wstring>>     rows;



    UNREFERENCED_PARAMETER (facts);

    hr = Parse (bytes, sheet, outError);
    CHR (hr);

    if (!options.formulas)
    {
        values = VisiCalcEvaluator::Evaluate (sheet);
    }

    outFile.kind      = DecodedFile::Kind::Grid;
    outFile.sheetForm = options.formulas ? SheetForm::Formulas : SheetForm::Values;
    outFile.grid.columns.push_back (L"Row");

    for (size_t column = 0; column < sheet.columns; column++)
    {
        outFile.grid.columns.push_back (AppleWorksSs::GetColumnLetters (column));
    }

    for (size_t at = 0; at < sheet.cells.size(); at++)
    {
        const Cell &                 cell = sheet.cells[at];
        std::vector<std::wstring> &  row  = rows[cell.row];
        std::wstring                 text = options.formulas ? GetEntered (sheet, cell) : Draw (sheet, cell, values[at]);

        if (row.empty())
        {
            row.resize (sheet.columns + 1);
            row[0] = std::to_wstring (cell.row);
        }

        if (!options.formulas)
        {
            text.erase (text.find_last_not_of (L' ') + 1);
            text.erase (0, text.find_first_not_of (L' '));
        }

        row[cell.column + 1] = text;
    }

    for (auto & [number, row] : rows)
    {
        outFile.grid.rows.push_back (std::move (row));
    }

Error:
    return hr;
}
