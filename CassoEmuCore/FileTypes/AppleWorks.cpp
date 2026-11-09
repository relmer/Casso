#include "Pch.h"

#include "FileTypes/AppleWorks.h"
#include "FileTypes/TextCharset.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorks::ReadWord
//
////////////////////////////////////////////////////////////////////////////////

Word AppleWorks::ReadWord (std::span<const Byte> bytes, size_t at)
{
    return (Word) (bytes[at] | (bytes[at + 1] << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorks::DecodeText
//
//  A byte past $7F is the character in its low seven bits, MouseText for a
//  control character's code.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AppleWorks::DecodeText (std::span<const Byte> bytes)
{
    std::wstring  text;



    for (Byte b : bytes)
    {
        Byte  low = (Byte) (b & 0x7F);

        if (b >= 0x80 && low < 0x20)
        {
            text += TextCharset::FromMouseText (low);
        }
        else if (low < 0x20)
        {
            text += L' ';
        }
        else
        {
            text += (wchar_t) low;
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorksWp::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWorksWp::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    return facts.HasProDosType (kFileType) && bytes.size() >= kHeaderBytes + 2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorksWp::AddText
//
//  Text in the style of the paragraph's last run joins it; text in another
//  style starts a run.
//
////////////////////////////////////////////////////////////////////////////////

void AppleWorksWp::AddText (Paragraph & paragraph, const std::wstring & text, bool bold, bool underline)
{
    bool  same = !paragraph.runs.empty() && paragraph.runs.back().bold == bold && paragraph.runs.back().underline == underline;



    if (same)
    {
        paragraph.runs.back().text += text;
    }
    else
    {
        paragraph.runs.push_back (Run { text, bold, false, underline });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorksWp::Decode
//
//  A file marked for 3.0 has two bytes before its first line record.
//  Superscript and subscript are dropped, the page number is drawn as #,
//  and the date and time as placeholders.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleWorksWp::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT    hr        = S_OK;
    Byte       version   = bytes[kMinVersionAt];
    size_t     at        = kHeaderBytes + ((version != 0) ? 2 : 0);
    Alignment  alignment = Alignment::Left;
    Paragraph  paragraph;
    bool       bold      = false;
    bool       underline = false;
    bool       open      = false;
    bool       ended     = false;
    bool       scripts   = false;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);
    UNREFERENCED_PARAMETER (outError);

    outFile.kind = DecodedFile::Kind::Document;

    while (at + 2 <= bytes.size())
    {
        Byte  low  = bytes[at];
        Byte  high = bytes[at + 1];

        if (high == kEndOfFile)
        {
            ended = true;
            break;
        }

        if (high == kCarriageLine)
        {
            outFile.paragraphs.push_back (Paragraph { alignment, {} });
            at += 2;
            continue;
        }

        if (high > kCarriageLine)
        {
            switch (high)
            {
                case kRightAligned: alignment = Alignment::Right;     break;
                case kJustified:    alignment = Alignment::Justified; break;
                case kUnjustified:  alignment = Alignment::Left;      break;
                case kCentered:     alignment = Alignment::Center;    break;
                default:                                              break;
            }

            at += 2;
            continue;
        }

        {
            std::span<const Byte>  record    = bytes.subspan (at + 2, std::min ((size_t) low, bytes.size() - at - 2));
            size_t                 count     = 0;
            bool                   carriage  = false;

            at += 2 + (size_t) low;

            if (record.size() < 2 || record[0] == kRuler)
            {
                continue;
            }

            count    = std::min ((size_t) (record[1] & 0x7F), record.size() - 2);
            carriage = (record[1] & 0x80) != 0;

            if (!open)
            {
                paragraph = Paragraph { alignment, {} };
                open      = true;
            }

            for (Byte b : record.subspan (2, count))
            {
                std::wstring  insert;

                switch (b)
                {
                    case kBoldOn:       bold      = true;      break;
                    case kBoldOff:      bold      = false;     break;
                    case kUnderlineOn:  underline = true;      break;
                    case kUnderlineOff: underline = false;     break;
                    case kPageNumber:   insert    = L"#";      break;
                    case kStickySpace:  insert    = L" ";      break;
                    case kDate:         insert    = L"[date]"; break;
                    case kTime:         insert    = L"[time]"; break;
                    case kTab:          insert    = L"\t";     break;
                    case kTabFill:                             break;

                    default:
                        if (b >= kSuperOn && b <= kSubOff)
                        {
                            scripts = true;
                        }
                        else if (b >= 0x20)
                        {
                            insert = AppleWorks::DecodeText (std::span<const Byte> (&b, 1));
                        }

                        break;
                }

                if (!insert.empty())
                {
                    AddText (paragraph, insert, bold, underline);
                }
            }

            if (carriage)
            {
                outFile.paragraphs.push_back (std::move (paragraph));
                open = false;
            }
        }
    }

    if (open)
    {
        outFile.paragraphs.push_back (std::move (paragraph));
    }

    if (scripts)
    {
        outFile.notes.push_back (L"Superscript and subscript are dropped.");
    }

    if (version > kVersion30)
    {
        outFile.notes.push_back (std::format (L"The file is marked for AppleWorks {}.{}; features past 3.0 are dropped.", version / 10, version % 10));
    }

    if (!ended)
    {
        outFile.notes.push_back (L"The file ends before its end marker.");
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorksDb::Matches
//
//  The header, with at least one category, is in the file.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWorksDb::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    return facts.HasProDosType (kFileType) && bytes.size() > kNamesAt + kNameBytes && bytes[kCategoriesAt] >= 1 && bytes[kCategoriesAt] <= 30;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorksDb::DecodeEntry
//
//  A date is $C0, the year's two digits, the month as A to L and the day's
//  two digits; a time is $D4, the hour as A to X and the minute's two
//  digits.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AppleWorksDb::DecodeEntry (std::span<const Byte> bytes)
{
    static constexpr const wchar_t *  kMonths[] = { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec" };



    if (bytes.size() == 6 && bytes[0] == kDate && bytes[3] >= 'A' && bytes[3] <= 'L')
    {
        return std::format (L"{} {}{} {}{}", kMonths[bytes[3] - 'A'], (wchar_t) bytes[4], (wchar_t) bytes[5], (wchar_t) bytes[1], (wchar_t) bytes[2]);
    }

    if (bytes.size() == 4 && bytes[0] == kTime && bytes[1] >= 'A' && bytes[1] <= 'X')
    {
        int  hour = bytes[1] - 'A';

        return std::format (L"{}:{}{} {}", (hour % 12 == 0) ? 12 : hour % 12, (wchar_t) bytes[2], (wchar_t) bytes[3], (hour < 12) ? L"AM" : L"PM");
    }

    return AppleWorks::DecodeText (bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorksDb::Decode
//
//  The header's length word doesn't count itself. The first record after
//  the reports is the standard values, which isn't an entry.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleWorksDb::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT  hr         = S_OK;
    size_t   categories = bytes[kCategoriesAt];
    size_t   at         = 2 + (size_t) AppleWorks::ReadWord (bytes, 0) + kReportBytes * bytes[kReportsAt];
    bool     standard   = true;
    bool     inside     = false;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);

    inside = kNamesAt + kNameBytes * categories <= bytes.size();
    CBRFEx (inside, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The database's category names aren't in the file.");

    outFile.kind = DecodedFile::Kind::Grid;

    for (size_t category = 0; category < categories; category++)
    {
        size_t  start  = kNamesAt + kNameBytes * category;
        size_t  length = std::min<size_t> (bytes[start], kNameBytes - 1);

        outFile.grid.columns.push_back (AppleWorks::DecodeText (bytes.subspan (start + 1, length)));
    }

    while (at + 2 <= bytes.size())
    {
        Word                       length = AppleWorks::ReadWord (bytes, at);
        size_t                     next   = at + 2 + length;
        size_t                     column = 0;
        std::vector<std::wstring>  row (categories);

        if (length == kEndOfFile)
        {
            break;
        }

        at += 2;

        while (at < bytes.size() && at < next)
        {
            Byte  control = bytes[at++];

            if (control == kEndOfRecord)
            {
                break;
            }

            if (control > 0x80)
            {
                column += control - 0x80u;
                continue;
            }

            {
                size_t  count = std::min<size_t> (control, bytes.size() - at);

                if (column < categories)
                {
                    row[column] = DecodeEntry (bytes.subspan (at, count));
                }

                at += count;
                column++;
            }
        }

        at = next;

        if (standard)
        {
            standard = false;
            continue;
        }

        outFile.grid.rows.push_back (std::move (row));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorksSs::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWorksSs::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    return facts.HasProDosType (kFileType) && bytes.size() >= kHeaderBytes + 2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorksSs::GetColumnLetters
//
//  A, B, ... Z, then AA, AB and on, from column 0.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AppleWorksSs::GetColumnLetters (size_t column)
{
    std::wstring  letters;



    do
    {
        letters.insert (letters.begin(), (wchar_t) (L'A' + column % 26));
        column = column / 26;
    }
    while (column-- > 0);

    return letters;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorksSs::AddThousands
//
//  Commas between each three digits of the whole part, which may follow a
//  minus sign and come before a decimal point.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AppleWorksSs::AddThousands (const std::wstring & digits)
{
    size_t        first = (!digits.empty() && digits[0] == L'-') ? 1 : 0;
    size_t        point = digits.find (L'.');
    size_t        whole = ((point == std::wstring::npos) ? digits.size() : point) - first;
    std::wstring  text  = digits.substr (0, first);



    for (size_t i = 0; i < whole; i++)
    {
        if (i > 0 && (whole - i) % 3 == 0)
        {
            text += L',';
        }

        text += digits[first + i];
    }

    return text + digits.substr (first + whole);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorksSs::FormatNumber
//
//  Fixed, dollars, commas and percent with the cell's decimal places; any
//  other format, appropriate among them, as the shortest exact form.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AppleWorksSs::FormatNumber (double value, int format, int decimals)
{
    switch (format)
    {
        case kFormatFixed:
            return std::format (L"{:.{}f}", value, decimals);

        case kFormatDollars:
            return (value < 0 ? L"-$" : L"$") + AddThousands (std::format (L"{:.{}f}", std::fabs (value), decimals));

        case kFormatCommas:
            return AddThousands (std::format (L"{:.{}f}", value, decimals));

        case kFormatPercent:
            return std::format (L"{:.{}f}%", value * 100.0, decimals);

        default:
            return std::format (L"{}", value);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorksSs::DecodeCell
//
//  A label's first flag byte has the high bit clear; a propagated label
//  repeats its character across the column. A value's second flag byte
//  sets bit 3 for a label result, a Pascal string, and bits 6 and 5 for a
//  result of @NA and @Error. A value of the standard format takes the
//  sheet's format and decimal places.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AppleWorksSs::DecodeCell (std::span<const Byte> cell, Byte width, int sheetFormat, int sheetDecimals)
{
    Byte    flags   = cell.empty() ? 0 : cell[0];
    Byte    more    = (cell.size() > 1) ? cell[1] : 0;
    int     format  = flags & 0x07;
    int     decimal = more & 0x07;
    double  value   = 0.0;



    if (cell.empty())
    {
        return std::wstring();
    }

    if ((flags & 0x80) == 0)
    {
        if ((flags & 0x20) != 0 && cell.size() > 1)
        {
            return std::wstring (std::max<Byte> (width, 1), (wchar_t) (cell[1] & 0x7F));
        }

        return AppleWorks::DecodeText (cell.subspan (1));
    }

    if ((flags & 0x20) == 0 && (more & 0x08) != 0)
    {
        size_t  length = (cell.size() > 2) ? std::min<size_t> (cell[2], cell.size() - 3) : 0;

        return AppleWorks::DecodeText (cell.subspan (std::min<size_t> (3, cell.size()), length));
    }

    if ((flags & 0x20) == 0 && (more & 0x40) != 0)
    {
        return L"NA";
    }

    if ((flags & 0x20) == 0 && (more & 0x20) != 0)
    {
        return L"ERROR";
    }

    if (cell.size() < 10)
    {
        return std::wstring();
    }

    memcpy (&value, cell.data() + 2, sizeof (value));

    if (format == kFormatStandard)
    {
        format  = sheetFormat;
        decimal = sheetDecimals;
    }

    return FormatNumber (value, format, decimal);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWorksSs::Decode
//
//  A file marked for 3.0 has two bytes before its first row. A row's
//  control bytes are an entry's length, a count of columns to skip past
//  $80, or $FF to end it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleWorksSs::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT  hr            = S_OK;
    size_t   at            = kHeaderBytes + ((bytes[kMinVersionAt] != 0) ? 2 : 0);
    int      sheetFormat   = bytes[kFormatAt];
    int      sheetDecimals = bytes[kDecimalsAt] & 0x07;
    size_t   columns       = 0;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);
    UNREFERENCED_PARAMETER (outError);

    outFile.kind = DecodedFile::Kind::Grid;

    while (at + 4 <= bytes.size())
    {
        Word                       length = AppleWorks::ReadWord (bytes, at);
        size_t                     next   = at + 2 + length;
        size_t                     column = 0;
        std::vector<std::wstring>  row    = { std::to_wstring (AppleWorks::ReadWord (bytes, at + 2)) };

        if (length == kEndOfFile)
        {
            break;
        }

        at += 4;

        while (at < bytes.size() && at < next)
        {
            Byte  control = bytes[at++];

            if (control == kEndOfRow)
            {
                break;
            }

            if (control > 0x80)
            {
                column += control - 0x80u;
                continue;
            }

            {
                size_t  count = std::min<size_t> (control, bytes.size() - at);
                Byte    width = (column < 127) ? bytes[kWidthsAt + column] : 0;

                if (row.size() < column + 2)
                {
                    row.resize (column + 2);
                }

                row[column + 1] = DecodeCell (bytes.subspan (at, count), width, sheetFormat, sheetDecimals);

                at += count;
                column++;
            }
        }

        at      = next;
        columns = std::max (columns, row.size() - 1);
        outFile.grid.rows.push_back (std::move (row));
    }

    outFile.grid.columns.push_back (L"Row");

    for (size_t column = 0; column < columns; column++)
    {
        outFile.grid.columns.push_back (GetColumnLetters (column));
    }

    for (std::vector<std::wstring> & row : outFile.grid.rows)
    {
        row.resize (columns + 1);
    }

    return hr;
}
