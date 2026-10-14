#include "Pch.h"

#include "CassoExplorer/Model/AppleFontFile.h"
#include "CassoExplorer/Model/AppleTextFont.h"


//  The parts of a TrueType file this font has, in the format's own terms.
static constexpr uint32_t  s_kSfntVersion        = 0x00010000;
static constexpr uint32_t  s_kHeadMagic          = 0x5F0F3CF5;
static constexpr uint32_t  s_kChecksumTarget     = 0xB1B0AFBA;
static constexpr size_t    s_kHeadAdjustOffset   = 8;
static constexpr uint32_t  s_kHeadFlags          = 0x000B;     // baseline at 0, left side bearing at 0, whole pixels per em
static constexpr Byte      s_kOnCurve            = 0x01;
static constexpr uint32_t  s_kWindowsPlatform    = 3;
static constexpr uint32_t  s_kUnicodeBmp         = 1;
static constexpr uint32_t  s_kEnglishUs          = 0x0409;
static constexpr uint32_t  s_kRegularSelection   = 0x0040;
static constexpr uint32_t  s_kBasicLatinRange    = 0x00000001;
static constexpr uint32_t  s_kLatin1CodePage     = 0x00000001;
static constexpr int       s_kHeaderBytes        = 12;
static constexpr int       s_kTableRecordBytes   = 16;
static constexpr int       s_kRowCount           = AppleTextFont::kDotsHigh;
static constexpr int       s_kDotCount           = AppleTextFont::kDotsWide;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFontFile::Build
//
//  Glyph 0 is the empty .notdef; glyph 1 on are the characters from space to
//  tilde, in order, so the character map is one range.
//
////////////////////////////////////////////////////////////////////////////////

void AppleFontFile::Build (const AppleTextFont & font, std::vector<Byte> & outFont)
{
    std::vector<Table>  tables (9);
    std::vector<Byte>   glyph;
    int                 glyphCount  = 1 + (kLastChar - kFirstChar + 1);
    int                 points      = 0;
    int                 contours    = 0;
    int                 maxPoints   = 0;
    int                 maxContours = 0;
    int                 left        = 0;
    Table               loca;
    Table               hmtx;



    loca.tag = MakeTag ("loca");
    hmtx.tag = MakeTag ("hmtx");

    tables[0].tag = MakeTag ("glyf");

    //  The .notdef glyph has no outline, so it starts and ends where the
    //  first character's outline starts.
    PutU32 (loca.bytes, 0);
    PutU32 (loca.bytes, 0);
    PutU16 (hmtx.bytes, (uint32_t) kAdvance);
    PutU16 (hmtx.bytes, 0);

    for (wchar_t ch = kFirstChar; ch <= kLastChar; ch++)
    {
        BuildGlyph (font, ch, glyph, points, contours, left);

        tables[0].bytes.insert (tables[0].bytes.end(), glyph.begin(), glyph.end());
        PutU32 (loca.bytes, (uint32_t) tables[0].bytes.size());
        PutU16 (hmtx.bytes, (uint32_t) kAdvance);
        PutU16 (hmtx.bytes, (uint32_t) left);

        maxPoints   = (std::max) (maxPoints, points);
        maxContours = (std::max) (maxContours, contours);
    }

    tables[1].tag = MakeTag ("head");
    tables[2].tag = MakeTag ("hhea");
    tables[3].tag = MakeTag ("maxp");
    tables[4].tag = MakeTag ("cmap");
    tables[5].tag = MakeTag ("name");
    tables[6].tag = MakeTag ("OS/2");
    tables[7].tag = MakeTag ("post");

    BuildHead (tables[1].bytes);
    BuildHhea (tables[2].bytes, glyphCount);
    BuildMaxp (tables[3].bytes, glyphCount, maxPoints, maxContours);
    BuildCmap (tables[4].bytes);
    BuildName (tables[5].bytes);
    BuildOs2  (tables[6].bytes);
    BuildPost (tables[7].bytes);

    tables[8] = std::move (loca);
    tables.push_back (std::move (hmtx));

    Assemble (tables, outFont);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFontFile::PutU16
//
//  TrueType is big-endian throughout; a signed value is written as its two's
//  complement in the same bytes.
//
////////////////////////////////////////////////////////////////////////////////

void AppleFontFile::PutU16 (std::vector<Byte> & out, uint32_t value)
{
    out.push_back ((Byte) ((value >> 8) & 0xFF));
    out.push_back ((Byte) (value & 0xFF));
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFontFile::PutU32
//
////////////////////////////////////////////////////////////////////////////////

void AppleFontFile::PutU32 (std::vector<Byte> & out, uint32_t value)
{
    PutU16 (out, value >> 16);
    PutU16 (out, value & 0xFFFF);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFontFile::MakeTag
//
////////////////////////////////////////////////////////////////////////////////

uint32_t AppleFontFile::MakeTag (const char (& name)[5])
{
    return ((uint32_t) (Byte) name[0] << 24) | ((uint32_t) (Byte) name[1] << 16) | ((uint32_t) (Byte) name[2] << 8) | (uint32_t) (Byte) name[3];
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFontFile::Checksum
//
//  The sum of the big-endian words over `length` bytes from `offset`, the
//  last word padded with zeros.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t AppleFontFile::Checksum (const std::vector<Byte> & bytes, size_t offset, size_t length)
{
    uint32_t  sum = 0;



    for (size_t at = 0; at < length; at += 4)
    {
        uint32_t  word = 0;

        for (size_t i = 0; i < 4; i++)
        {
            word = (word << 8) | ((at + i < length) ? bytes[offset + at + i] : 0u);
        }

        sum += word;
    }

    return sum;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFontFile::BuildGlyph
//
//  Every run of lit dots in a row is a rectangle, wound clockwise as an outer
//  contour is. Row 0 is the top of the cell, and y counts up from the
//  baseline. A character with no lit dots, such as space, has no outline.
//
////////////////////////////////////////////////////////////////////////////////

void AppleFontFile::BuildGlyph (const AppleTextFont & font, wchar_t ch, std::vector<Byte> & outGlyph, int & outPoints, int & outContours, int & outLeft)
{
    struct Box { int left; int bottom; int right; int top; };

    std::vector<Box>  boxes;
    int               xMin  = INT_MAX;
    int               yMin  = INT_MAX;
    int               xMax  = INT_MIN;
    int               yMax  = INT_MIN;
    int               lastX = 0;
    int               lastY = 0;



    for (int row = 0; row < s_kRowCount; row++)
    {
        Byte  dots = font.GetGlyphRow (ch, row);
        int   top  = kAscent - row * kUnitsPerDot;
        int   dot  = 0;

        while (dot < s_kDotCount)
        {
            int  first = dot;

            while (dot < s_kDotCount && (dots & (1 << dot)) != 0)
            {
                dot++;
            }

            if (dot > first)
            {
                boxes.push_back ({ first * kUnitsPerDot, top - kUnitsPerDot, dot * kUnitsPerDot, top });
            }
            else
            {
                dot++;
            }
        }
    }

    outGlyph.clear();
    outPoints   = (int) boxes.size() * 4;
    outContours = (int) boxes.size();
    outLeft     = 0;

    if (boxes.empty())
    {
        return;
    }

    for (const Box & box : boxes)
    {
        xMin = (std::min) (xMin, box.left);
        yMin = (std::min) (yMin, box.bottom);
        xMax = (std::max) (xMax, box.right);
        yMax = (std::max) (yMax, box.top);
    }

    outLeft = xMin;

    PutU16 (outGlyph, (uint32_t) boxes.size());
    PutU16 (outGlyph, (uint32_t) xMin);
    PutU16 (outGlyph, (uint32_t) yMin);
    PutU16 (outGlyph, (uint32_t) xMax);
    PutU16 (outGlyph, (uint32_t) yMax);

    for (size_t i = 0; i < boxes.size(); i++)
    {
        PutU16 (outGlyph, (uint32_t) (i * 4 + 3));
    }

    //  No instructions, and every point on the curve with both coordinates as
    //  16-bit deltas from the point before.
    PutU16 (outGlyph, 0);
    outGlyph.insert (outGlyph.end(), (size_t) outPoints, s_kOnCurve);

    for (const Box & box : boxes)
    {
        for (int x : { box.left, box.left, box.right, box.right })
        {
            PutU16 (outGlyph, (uint32_t) (x - lastX));
            lastX = x;
        }
    }

    for (const Box & box : boxes)
    {
        for (int y : { box.bottom, box.top, box.top, box.bottom })
        {
            PutU16 (outGlyph, (uint32_t) (y - lastY));
            lastY = y;
        }
    }

    while (outGlyph.size() % 4 != 0)
    {
        outGlyph.push_back (0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFontFile::BuildHead
//
//  The checksum adjustment is left zero for Assemble, which sums the whole
//  file.
//
////////////////////////////////////////////////////////////////////////////////

void AppleFontFile::BuildHead (std::vector<Byte> & out)
{
    PutU32 (out, s_kSfntVersion);
    PutU32 (out, s_kSfntVersion);
    PutU32 (out, 0);
    PutU32 (out, s_kHeadMagic);
    PutU16 (out, s_kHeadFlags);
    PutU16 (out, (uint32_t) kUnitsPerEm);
    PutU32 (out, 0);
    PutU32 (out, 0);
    PutU32 (out, 0);
    PutU32 (out, 0);
    PutU16 (out, 0);
    PutU16 (out, (uint32_t) -kDescent);
    PutU16 (out, (uint32_t) kAdvance);
    PutU16 (out, (uint32_t) kAscent);
    PutU16 (out, 0);                        // macStyle
    PutU16 (out, (uint32_t) s_kRowCount);   // smallest readable size, one pixel a dot
    PutU16 (out, 2);                        // left to right
    PutU16 (out, 1);                        // loca has 32-bit offsets
    PutU16 (out, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFontFile::BuildHhea
//
//  No line gap: a listing's rows are adjacent, as on the screen.
//
////////////////////////////////////////////////////////////////////////////////

void AppleFontFile::BuildHhea (std::vector<Byte> & out, int glyphCount)
{
    PutU32 (out, s_kSfntVersion);
    PutU16 (out, (uint32_t) kAscent);
    PutU16 (out, (uint32_t) -kDescent);
    PutU16 (out, 0);
    PutU16 (out, (uint32_t) kAdvance);
    PutU16 (out, 0);
    PutU16 (out, 0);
    PutU16 (out, (uint32_t) kAdvance);
    PutU16 (out, 1);
    PutU16 (out, 0);
    PutU16 (out, 0);

    for (int i = 0; i < 4; i++)
    {
        PutU16 (out, 0);
    }

    PutU16 (out, 0);
    PutU16 (out, (uint32_t) glyphCount);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFontFile::BuildMaxp
//
////////////////////////////////////////////////////////////////////////////////

void AppleFontFile::BuildMaxp (std::vector<Byte> & out, int glyphCount, int maxPoints, int maxContours)
{
    PutU32 (out, s_kSfntVersion);
    PutU16 (out, (uint32_t) glyphCount);
    PutU16 (out, (uint32_t) maxPoints);
    PutU16 (out, (uint32_t) maxContours);
    PutU16 (out, 0);
    PutU16 (out, 0);
    PutU16 (out, 2);

    for (int i = 0; i < 8; i++)
    {
        PutU16 (out, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFontFile::BuildCmap
//
//  One Unicode subtable in format 4 with two segments: space to tilde,
//  offset to glyph 1 on, and the 0xFFFF segment the format ends with.
//
////////////////////////////////////////////////////////////////////////////////

void AppleFontFile::BuildCmap (std::vector<Byte> & out)
{
    static constexpr uint32_t  kSegmentCount = 2;
    static constexpr uint32_t  kSubtableSize = 16 + 8 * kSegmentCount;
    static constexpr uint32_t  kLastCode     = 0xFFFF;



    PutU16 (out, 0);
    PutU16 (out, 1);
    PutU16 (out, s_kWindowsPlatform);
    PutU16 (out, s_kUnicodeBmp);
    PutU32 (out, 12);

    PutU16 (out, 4);
    PutU16 (out, kSubtableSize);
    PutU16 (out, 0);
    PutU16 (out, kSegmentCount * 2);
    PutU16 (out, 4);                        // searchRange: two segments, times two
    PutU16 (out, 1);                        // entrySelector
    PutU16 (out, 0);                        // rangeShift

    PutU16 (out, kLastChar);
    PutU16 (out, kLastCode);
    PutU16 (out, 0);
    PutU16 (out, kFirstChar);
    PutU16 (out, kLastCode);
    PutU16 (out, (uint32_t) (1 - (int) kFirstChar));
    PutU16 (out, 1);
    PutU16 (out, 0);
    PutU16 (out, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFontFile::BuildName
//
//  Family, style, unique name, full name and PostScript name, in UTF-16 for
//  the Windows platform.
//
////////////////////////////////////////////////////////////////////////////////

void AppleFontFile::BuildName (std::vector<Byte> & out)
{
    std::wstring                                    postScript = kFamily;
    std::vector<std::pair<uint32_t, std::wstring>>  entries;
    std::vector<Byte>                               strings;



    std::erase (postScript, L' ');

    entries = { { 1, kFamily }, { 2, L"Regular" }, { 3, kFamily }, { 4, kFamily }, { 6, postScript } };

    PutU16 (out, 0);
    PutU16 (out, (uint32_t) entries.size());
    PutU16 (out, (uint32_t) (6 + 12 * entries.size()));

    for (const auto & [id, text] : entries)
    {
        PutU16 (out, s_kWindowsPlatform);
        PutU16 (out, s_kUnicodeBmp);
        PutU16 (out, s_kEnglishUs);
        PutU16 (out, id);
        PutU16 (out, (uint32_t) (text.size() * 2));
        PutU16 (out, (uint32_t) strings.size());

        for (wchar_t ch : text)
        {
            PutU16 (strings, (uint32_t) ch);
        }
    }

    out.insert (out.end(), strings.begin(), strings.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFontFile::BuildOs2
//
//  Version 4. The panose proportion of 9 marks the face as fixed-width.
//
////////////////////////////////////////////////////////////////////////////////

void AppleFontFile::BuildOs2 (std::vector<Byte> & out)
{
    static constexpr Byte  kPanose[10] = { 2, 0, 0, 9, 0, 0, 0, 0, 0, 0 };



    PutU16 (out, 4);
    PutU16 (out, (uint32_t) kAdvance);
    PutU16 (out, 400);
    PutU16 (out, 5);
    PutU16 (out, 0);                        // installable

    for (int size : { 650, 600, 0, 75, 650, 600, 0, 350 })
    {
        PutU16 (out, (uint32_t) size);
    }

    PutU16 (out, (uint32_t) (kUnitsPerDot / 2));
    PutU16 (out, (uint32_t) (3 * kUnitsPerDot));
    PutU16 (out, 0);
    out.insert (out.end(), std::begin (kPanose), std::end (kPanose));
    PutU32 (out, s_kBasicLatinRange);
    PutU32 (out, 0);
    PutU32 (out, 0);
    PutU32 (out, 0);
    PutU32 (out, MakeTag ("CASO"));
    PutU16 (out, s_kRegularSelection);
    PutU16 (out, kFirstChar);
    PutU16 (out, kLastChar);
    PutU16 (out, (uint32_t) kAscent);
    PutU16 (out, (uint32_t) -kDescent);
    PutU16 (out, 0);
    PutU16 (out, (uint32_t) kAscent);
    PutU16 (out, (uint32_t) kDescent);
    PutU32 (out, s_kLatin1CodePage);
    PutU32 (out, 0);
    PutU16 (out, (uint32_t) (4 * kUnitsPerDot));
    PutU16 (out, (uint32_t) (6 * kUnitsPerDot));
    PutU16 (out, 0);
    PutU16 (out, kFirstChar);
    PutU16 (out, 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFontFile::BuildPost
//
//  Version 3, which has no glyph name list.
//
////////////////////////////////////////////////////////////////////////////////

void AppleFontFile::BuildPost (std::vector<Byte> & out)
{
    PutU32 (out, 0x00030000);
    PutU32 (out, 0);
    PutU16 (out, (uint32_t) -kDescent);
    PutU16 (out, (uint32_t) (kUnitsPerDot / 2));
    PutU32 (out, 1);                        // fixed pitch

    for (int i = 0; i < 4; i++)
    {
        PutU32 (out, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFontFile::Assemble
//
//  The table directory sorted by tag, each table at a four-byte boundary
//  with its checksum, and then the head table's adjustment, which makes the
//  whole file sum to the format's constant.
//
////////////////////////////////////////////////////////////////////////////////

void AppleFontFile::Assemble (std::vector<Table> & tables, std::vector<Byte> & outFont)
{
    uint32_t  count       = (uint32_t) tables.size();
    uint32_t  power       = 1;
    uint32_t  selector    = 0;
    size_t    offset      = 0;
    size_t    headOffset  = 0;
    uint32_t  adjustment  = 0;



    std::sort (tables.begin(), tables.end(), [] (const Table & a, const Table & b) { return a.tag < b.tag; });

    while (power * 2 <= count)
    {
        power *= 2;
        selector++;
    }

    outFont.clear();
    PutU32 (outFont, s_kSfntVersion);
    PutU16 (outFont, count);
    PutU16 (outFont, power * s_kTableRecordBytes);
    PutU16 (outFont, selector);
    PutU16 (outFont, count * s_kTableRecordBytes - power * s_kTableRecordBytes);

    offset = (size_t) s_kHeaderBytes + (size_t) count * s_kTableRecordBytes;

    for (const Table & table : tables)
    {
        PutU32 (outFont, table.tag);
        PutU32 (outFont, Checksum (table.bytes, 0, table.bytes.size()));
        PutU32 (outFont, (uint32_t) offset);
        PutU32 (outFont, (uint32_t) table.bytes.size());

        offset += (table.bytes.size() + 3) & ~(size_t) 3;
    }

    for (const Table & table : tables)
    {
        if (table.tag == MakeTag ("head"))
        {
            headOffset = outFont.size();
        }

        outFont.insert (outFont.end(), table.bytes.begin(), table.bytes.end());

        while (outFont.size() % 4 != 0)
        {
            outFont.push_back (0);
        }
    }

    adjustment = s_kChecksumTarget - Checksum (outFont, 0, outFont.size());

    for (size_t i = 0; i < 4; i++)
    {
        outFont[headOffset + s_kHeadAdjustOffset + i] = (Byte) (adjustment >> (24 - 8 * i));
    }
}
