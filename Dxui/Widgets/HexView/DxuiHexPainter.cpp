#include "Pch.h"

#include "Widgets/HexView/DxuiHexPainter.h"
#include "Widgets/DxuiHexView.h"
#include "Theme/DxuiColor.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexPainter::DxuiHexPainter
//
////////////////////////////////////////////////////////////////////////////////

DxuiHexPainter::DxuiHexPainter (DxuiHexView & view)
    : m_view (view)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexPainter::PaintLineRow
//
//  The line number on a line's first row, then the row's characters.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexPainter::PaintLineRow (IDxuiTextRenderer & text, const IDxuiTheme & theme, uint64_t row)
{
    DxuiFontHandle  font   = theme.MonospaceFont();
    uint64_t        start  = 0;
    uint64_t        end    = 0;
    uint64_t        line   = 0;
    uint64_t        length = 0;
    bool            starts = false;
    int             margin = m_view.m_scaler.ToPx (DxuiHexView::s_kSelectionMarginDip);
    int             column = m_view.m_layout.GetColumnStartCell (DxuiHexViewTypes::Column::Text);
    std::wstring    label;



    font.sizeDip = m_view.m_scaler.ToPxf (font.sizeDip * m_zoom);

    if (!m_view.m_layout.GetRowSpan (row, start, end, line, starts))
    {
        return;
    }

    if (starts)
    {
        label = std::to_wstring (line);
        label.insert (0, (size_t) (std::max) (0, m_view.m_layout.GetLineDigits() - (int) label.size()), L' ');

        DrawCell (text, m_view.m_layout.GetRowOffsetRect (row), label.c_str(),
                  DxuiColor::Mix (theme.ContentBackground(), theme.Foreground(), m_textStrength * s_kAddressStrength), font);
    }

    length = m_view.m_layout.GetShownLength (start, end);

    for (uint64_t i = 0; i < length; i++)
    {
        RECT      cell     = m_view.m_layout.GetCellRect (column + (int) i, row, 1);
        uint32_t  argb     = GetByteColor (theme, 0);
        wchar_t   chars[2] = { GetCharFor (m_view.m_layout.GetByteAt (start + i)), 0 };

        if (m_view.IsByteSelected (start + i))
        {
            RECT  fill = cell;

            fill.top    -= margin;
            fill.bottom += margin;

            FillCell (text, fill, theme.SelectionBackground());
            argb = theme.Foreground();
        }

        DrawCell (text, cell, chars, argb, font);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexPainter::FormatValue
//
//  A value's text, right-aligned to its cells. `present` is how many of its
//  bytes the source has, which is short only for the last value of the file:
//  in hex the missing bytes are blanks, and in decimal they count as zero.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiHexPainter::FormatValue (uint64_t value, int present) const
{
    std::wstring  out;
    int           cells    = m_view.m_layout.GetValueCells();
    int64_t       asSigned = (int64_t) value;



    if (m_view.m_format == DxuiHexViewTypes::ValueFormat::Hex)
    {
        for (int index = m_view.m_grouping - 1; index >= 0; index--)
        {
            if (index < present)
            {
                out.push_back (GetHexDigit ((int) ((value >> (index * 8 + 4)) & 0xF)));
                out.push_back (GetHexDigit ((int) ((value >> (index * 8)) & 0xF)));
            }
            else
            {
                out.append (L"  ");
            }
        }

        return out;
    }

    if (m_view.m_format == DxuiHexViewTypes::ValueFormat::Signed)
    {
        if ((present < 8) && (((value >> (present * 8 - 1)) & 1) != 0))
        {
            asSigned = (int64_t) (value | (~0ull << (present * 8)));
        }

        out = std::to_wstring (asSigned);
    }
    else
    {
        out = std::to_wstring (value);
    }

    if ((int) out.size() < cells)
    {
        out.insert (0, (size_t) (cells - (int) out.size()), L' ');
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexPainter::GetByteColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiHexPainter::GetByteColor (const IDxuiTheme & theme, uint8_t mark) const
{
    uint32_t  argb = DxuiColor::Mix (theme.ContentBackground(), theme.Foreground(), m_textStrength);



    if (m_markColor)
    {
        uint32_t  marked = argb;

        if (m_markColor (mark, marked))
        {
            argb = marked;
        }
    }

    return argb;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexPainter::EnsureCellSize
//
//  Measures the cell size once from the face used to draw the columns. A size
//  the host has already set is left unchanged, so the geometry can be tested
//  without a device.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexPainter::EnsureCellSize (IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiFontHandle  font   = theme.MonospaceFont();
    float           fontPx = m_view.m_scaler.ToPxf (font.sizeDip * m_zoom);
    std::wstring    face   = (font.face != nullptr) ? font.face : L"";
    float           width  = 0.0f;
    float           height = 0.0f;
    HRESULT         hr     = S_OK;



    //  Measured again at a new DPI, size or face, unless the caller set the
    //  size.
    if (m_view.m_cellWidthDip > 0 && (m_cellFontPx == 0.0f || (m_cellFontPx == fontPx && m_cellFontFace == face)))
    {
        return;
    }

    //  Eight digits at once, so a face whose advance is not a whole number
    //  of DIPs is rounded once rather than eight times.
    hr = text.MeasureString (L"00000000", fontPx, font.face, width, height);

    if (FAILED (hr) || (width <= 0.0f) || (height <= 0.0f))
    {
        return;
    }

    m_view.SetCellSizeDip ((int) ((width / 8.0f) + 0.5f), (int) (height + 0.5f));

    m_cellFontPx   = fontPx;
    m_cellFontFace = face;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexPainter::ReadRow
//
//  The only place bytes are read: one row at a time, into reused buffers, so
//  the cost of a frame depends on the visible rows, not the source size.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexPainter::ReadRow (uint64_t row)
{
    uint64_t  start = row * (uint64_t) m_view.m_bytesPerRow;
    uint64_t  bytes = m_view.m_source->GetByteCount();
    uint64_t  left  = (start < bytes) ? (bytes - start) : 0;
    int       count = (int) (std::min) (left, (uint64_t) m_view.m_bytesPerRow);



    if (count <= 0)
    {
        return 0;
    }

    m_rowBytes.resize ((size_t) m_view.m_bytesPerRow);
    m_rowMarks.resize ((size_t) m_view.m_bytesPerRow);

    m_view.m_source->ReadBytes (start, std::span<uint8_t> (m_rowBytes.data(), (size_t) count));
    m_view.m_source->ReadMarks (start, std::span<uint8_t> (m_rowMarks.data(), (size_t) count));

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexPainter::PaintRow
//
//  The offset, then each byte's digits and its character. Selected bytes are
//  lit in both columns at once, which is the whole point of selecting bytes
//  rather than characters.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexPainter::PaintRow (IDxuiTextRenderer & text, const IDxuiTheme & theme, uint64_t row, int count)
{
    DxuiFontHandle  font    = theme.MonospaceFont();
    RECT            gutter  = {};
    int             digits  = m_view.m_layout.GetOffsetDigits();
    uint64_t        address = m_view.m_originAddress + (row * (uint64_t) m_view.m_bytesPerRow);
    std::wstring    label;



    if (m_view.IsLineMode())
    {
        (void) count;

        PaintLineRow (text, theme, row);
        return;
    }

    gutter = m_view.m_layout.GetRowOffsetRect (row);

    //  The theme's size is in DIPs; the renderer draws in pixels, and the cell
    //  size was measured at this same pixel size.
    font.sizeDip = m_view.m_scaler.ToPxf (font.sizeDip * m_zoom);
    for (int digit = digits - 1; digit >= 0; digit--)
    {
        label.push_back (GetHexDigit ((int) ((address >> (digit * 4)) & 0xF)));
    }

    //  The addresses are a shade darker than the bytes, so the bytes stand out.
    DrawCell (text, gutter, label.c_str(), DxuiColor::Mix (theme.ContentBackground(), theme.Foreground(), m_textStrength * s_kAddressStrength), font);

    //  The value column, a value at a time.
    for (int first = 0; m_view.m_showValues && (first < count); first += m_view.m_grouping)
    {
        uint64_t  offset   = (row * (uint64_t) m_view.m_bytesPerRow) + (uint64_t) first;
        int       present  = (std::min) (m_view.m_grouping, count - first);
        uint64_t  value    = 0;
        bool      selected = false;
        uint32_t  argb     = GetByteColor (theme, m_rowMarks[(size_t) first]);
        RECT      cell     = m_view.m_layout.GetCellRect (m_view.m_layout.GetColumnStartCell (DxuiHexViewTypes::Column::Hex) + ((first / m_view.m_grouping) * (m_view.m_layout.GetValueCells() + 1)),
                                                          row, m_view.m_layout.GetValueCells());

        for (int index = 0; index < present; index++)
        {
            value   |= (uint64_t) m_rowBytes[(size_t) (first + index)] << (index * 8);
            selected = selected || m_view.IsByteSelected (offset + (uint64_t) index);
        }

        if (selected)
        {
            FillCell (text, m_view.m_layout.GetValueSelectionRect (offset, cell), theme.SelectionBackground());
        }

        DrawCell (text, cell, FormatValue (value, present).c_str(), selected ? theme.Foreground() : argb, font);
    }

    //  The text column, a byte at a time.
    for (int index = 0; index < count; index++)
    {
        uint64_t      offset  = (row * (uint64_t) m_view.m_bytesPerRow) + (uint64_t) index;
        RECT          txtRect = m_view.m_layout.GetByteRect (offset, DxuiHexViewTypes::Column::Text);
        uint32_t      argb    = GetByteColor (theme, m_rowMarks[(size_t) index]);
        std::wstring  charOf  = { GetCharFor (m_rowBytes[(size_t) index]) };

        //  Selected bytes are drawn in the full foreground, over the selection.
        if (m_view.IsByteSelected (offset))
        {
            FillCell (text, m_view.m_layout.GetSelectionCellRect (offset, index, txtRect, false), theme.SelectionBackground());
            argb = theme.Foreground();
        }

        DrawCell (text, txtRect, charOf.c_str(), argb, font);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexPainter::DrawCell
//
//  All text here starts at a cell boundary in a fixed-width face, so a cell
//  run is always left-aligned, unwrapped, and exactly as wide as its
//  characters.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexPainter::DrawCell (IDxuiTextRenderer & text, const RECT & rect, const wchar_t * chars, uint32_t argb, const DxuiFontHandle & font)
{
    HRESULT  hr = S_OK;



    //  The box is a cell wider than the characters: a glyph whose ink runs past
    //  its advance, or a width rounded down to whole pixels, is otherwise
    //  cut off at its right edge.
    hr = text.DrawString (chars,
                          (float) rect.left,
                          (float) rect.top,
                          (float) (rect.right - rect.left) + (float) (rect.bottom - rect.top),
                          (float) (rect.bottom - rect.top),
                          argb,
                          font.sizeDip,
                          font.face,
                          DxuiTextHAlign::Left,
                          DxuiTextVAlign::Top,
                          DxuiFontWeight::Normal,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexPainter::FillCell
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexPainter::FillCell (IDxuiTextRenderer & text, const RECT & rect, uint32_t argb)
{
    HRESULT  hr = S_OK;



    hr = text.FillRect ((float) rect.left,
                        (float) rect.top,
                        (float) (rect.right - rect.left),
                        (float) (rect.bottom - rect.top),
                        argb);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexPainter::GetHexDigit
//
////////////////////////////////////////////////////////////////////////////////

wchar_t DxuiHexPainter::GetHexDigit (int value)
{
    return (value < 10) ? (wchar_t) (L'0' + value) : (wchar_t) (L'A' + value - 10);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexPainter::GetCharFor
//
//  Anything the face cannot show as itself becomes a period, so the text
//  column stays one cell a byte and the two columns keep their rows.
//
////////////////////////////////////////////////////////////////////////////////

wchar_t DxuiHexPainter::GetCharFor (uint8_t byte) const
{
    uint8_t  value = (m_view.m_encoding == DxuiHexViewTypes::TextEncoding::AppleHighBit) ? (uint8_t) (byte & 0x7F) : byte;



    //  A page break in text is drawn blank, as the line break before it is.
    if (value == s_kFormFeed
        && (!m_view.m_showValues || m_view.m_encoding == DxuiHexViewTypes::TextEncoding::Dos || m_view.m_encoding == DxuiHexViewTypes::TextEncoding::Windows))
    {
        return L' ';
    }

    //  Control codes decode to code page 437's symbols, all but the line breaks and
    //  tabs that lay the text out.
    if (m_view.m_encoding == DxuiHexViewTypes::TextEncoding::Dos
        && (value >= 0x7F || (value < 0x20 && value != '\t' && value != '\r' && value != '\n')))
    {
        return GetCodePageChar (s_kDosCodePage, MB_USEGLYPHCHARS, value);
    }

    if (value >= 0x80 && m_view.m_encoding == DxuiHexViewTypes::TextEncoding::Windows)
    {
        return GetCodePageChar (s_kWindowsCodePage, MB_ERR_INVALID_CHARS, value);
    }

    return ((value >= 0x20) && (value < 0x7F)) ? (wchar_t) value : L'.';
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexPainter::GetCodePageChar
//
//  A byte in the code page, or a period for one that is undefined there
//  or maps to a control code.
//
////////////////////////////////////////////////////////////////////////////////

wchar_t DxuiHexPainter::GetCodePageChar (UINT codePage, DWORD flags, uint8_t byte)
{
    char     narrow = (char) byte;
    wchar_t  wide   = 0;
    int      count  = MultiByteToWideChar (codePage, flags, &narrow, 1, &wide, 1);



    return (count == 1 && wide >= 0xA0) ? wide : L'.';
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexPainter::GetTextFor
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiHexPainter::GetTextFor (uint64_t offset, uint64_t count) const
{
    std::wstring          out;
    std::vector<uint8_t>  bytes;



    if ((m_view.m_source == nullptr) || (count == 0))
    {
        return out;
    }

    bytes.resize ((size_t) count);
    m_view.m_source->ReadBytes (offset, std::span<uint8_t> (bytes.data(), bytes.size()));

    for (size_t index = 0; index < bytes.size(); index++)
    {
        //  In line mode a copy keeps the line breaks the view shows.
        if (m_view.IsLineMode() && m_view.m_layout.IsLineBreak (bytes[index]))
        {
            if (bytes[index] != '\n' || index == 0 || !m_view.m_layout.IsLineBreak (bytes[index - 1]) || bytes[index - 1] == '\n')
            {
                out.append (L"\r\n");
            }

            continue;
        }

        out.push_back (GetCharFor (bytes[index]));
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexPainter::GetHexFor
//
//  The digits spaced by the grouping in force, counted from the first byte
//  asked for, so a copy of a selection reads the way the column does.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiHexPainter::GetHexFor (uint64_t offset, uint64_t count) const
{
    std::wstring          out;
    std::vector<uint8_t>  bytes;



    if ((m_view.m_source == nullptr) || (count == 0))
    {
        return out;
    }

    bytes.resize ((size_t) count);
    m_view.m_source->ReadBytes (offset, std::span<uint8_t> (bytes.data(), bytes.size()));

    //  Decimal values copy as the numbers shown, a space between each.
    if (m_view.m_format != DxuiHexViewTypes::ValueFormat::Hex)
    {
        for (size_t first = 0; first < bytes.size(); first += (size_t) m_view.m_grouping)
        {
            uint64_t      value   = 0;
            int           present = (int) (std::min) ((size_t) m_view.m_grouping, bytes.size() - first);
            std::wstring  shown;

            for (int index = 0; index < present; index++)
            {
                value |= (uint64_t) bytes[first + (size_t) index] << (index * 8);
            }

            shown = FormatValue (value, present);
            shown.erase (0, shown.find_first_not_of (L' '));

            out += out.empty() ? shown : (L" " + shown);
        }

        return out;
    }

    for (size_t index = 0; index < bytes.size(); index++)
    {
        out.push_back (GetHexDigit ((bytes[index] >> 4) & 0xF));
        out.push_back (GetHexDigit (bytes[index] & 0xF));
    }

    return out;
}
