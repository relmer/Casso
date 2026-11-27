#include "Pch.h"

#include "FileTypes/QuickDrawText.h"
#include "FileTypes/QuickDrawFont.h"





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawText::Draw
//
//  Each character drawn at the pen, which then moves by its advance: the
//  font's widest one when font flag bit 0 is set, plus the character extra,
//  plus the space extra for a space. The extras are fractions of a pixel,
//  so the pen position is a fraction and each character is drawn at the pixel
//  nearest it.
//
////////////////////////////////////////////////////////////////////////////////

void QuickDrawText::Draw (QuickDrawCanvas & canvas, const QuickDrawFont & font, const QuickDrawTextStyle & style, QuickDrawCanvas::Point at, std::span<const Byte> text)
{
    constexpr Byte  kSpace = 0x20;



    double                             pen = at.h;
    std::vector<std::pair<Cell, int>>  cells;



    for (Byte character : text)
    {
        Cell  cell;



        BuildCell (font, style, character, cell);
        cells.emplace_back (std::move (cell), (int) std::lround (pen));

        pen += cells.back().first.advance + style.chExtra + ((character == kSpace) ? style.spExtra : 0.0);
    }

    //  Every box's background, then every character's set pixels, so a
    //  slanted character's overhang isn't painted over by the next box.
    for (bool set : { false, true })
    {
        for (const auto & [cell, penH] : cells)
        {
            DrawCell (canvas, style, cell, penH, at.v, set);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawText::BuildCell
//
//  The character's set pixels, then the face. Bold sets each pixel's right
//  neighbor too and widens the advance by one. Italic slants the rows, one
//  pixel right for every two above the baseline. Outline replaces the
//  pixels with the ones around them and widens the advance by one; shadow
//  does the same with a second outline a pixel down and right, and widens
//  it by two. Underline is the row below the baseline, across the advance.
//
////////////////////////////////////////////////////////////////////////////////

void QuickDrawText::BuildCell (const QuickDrawFont & font, const QuickDrawTextStyle & style, Byte character, Cell & outCell)
{
    constexpr Word  kFixedFlag = 0x0001;



    int                            advance = 0;
    int                            width   = 0;
    int                            offset  = 0;
    int                            grow    = 0;
    std::set<std::pair<int, int>>  ink;
    std::set<std::pair<int, int>>  ring;



    outCell = Cell();

    if (!font.TryGetChar (character, advance, width, offset))
    {
        return;
    }

    for (int y = 0; y < font.GetAscent() + font.GetDescent(); y++)
    {
        for (int x = 0; x < width; x++)
        {
            if (font.IsSet (character, x, y))
            {
                ink.insert ({ offset + x, y - font.GetAscent() });
            }
        }
    }

    if (style.face & kBold)
    {
        for (const auto & [x, y] : std::set<std::pair<int, int>> (ink))
        {
            ink.insert ({ x + 1, y });
        }

        grow++;
    }

    if (style.face & kItalic)
    {
        std::set<std::pair<int, int>>  slanted;

        for (const auto & [x, y] : ink)
        {
            slanted.insert ({ x + ((-y) >> 1), y });
        }

        ink = std::move (slanted);
    }

    if (style.face & (kOutline | kShadow))
    {
        for (const auto & [x, y] : ink)
        {
            for (int dy = -1; dy <= ((style.face & kShadow) ? 2 : 1); dy++)
            {
                for (int dx = -1; dx <= ((style.face & kShadow) ? 2 : 1); dx++)
                {
                    if (!ink.contains ({ x + dx, y + dy }))
                    {
                        ring.insert ({ x + dx, y + dy });
                    }
                }
            }
        }

        ink   = std::move (ring);
        grow += (style.face & kShadow) ? 2 : 1;
    }

    outCell.advance = ((style.fontFlags & kFixedFlag) ? font.GetWidMax() : advance) + grow;

    if (style.face & kUnderline)
    {
        for (int x = 0; x < outCell.advance; x++)
        {
            ink.insert ({ x, 1 });
        }
    }

    outCell.left   = 0;
    outCell.right  = outCell.advance;
    outCell.top    = -font.GetAscent();
    outCell.bottom = font.GetDescent();

    for (const auto & [x, y] : ink)
    {
        outCell.left   = (std::min) (outCell.left,   x);
        outCell.right  = (std::max) (outCell.right,  x + 1);
        outCell.top    = (std::min) (outCell.top,    y);
        outCell.bottom = (std::max) (outCell.bottom, y + 1);
    }

    outCell.set = std::move (ink);
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawText::DrawCell
//
//  The set pixels, or the rest of the box. A foreground mode draws the set
//  pixels alone.
//  A color is a pixel value in 320 mode; in 640 mode it is two pixels'
//  values, as a solid pattern of it would draw them.
//
////////////////////////////////////////////////////////////////////////////////

void QuickDrawText::DrawCell (QuickDrawCanvas & canvas, const QuickDrawTextStyle & style, const Cell & cell, int penH, int baseV, bool set)
{
    constexpr int  kForeOnly = 0x0004;
    constexpr int  kCombine  = 0x0003;



    bool  fore  = false;
    int   color = 0;
    int   mode  = (style.mode & kCombine) | (style.mode & QuickDrawCanvas::kModeNot);



    for (int y = cell.top; y < cell.bottom; y++)
    {
        for (int x = cell.left; x < cell.right; x++)
        {
            fore = cell.set.contains ({ x, y });

            if (fore != set || (!fore && (style.mode & kForeOnly)))
            {
                continue;
            }

            color = fore ? style.foreColor : style.backColor;
            canvas.PlotPixel (penH + x, baseV + y, canvas.GetColorValue (color, penH + x), mode);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawText::GetFamilyName
//
//  A family's name from Apple's IIgs Technical Note 41, or nullptr.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * QuickDrawText::GetFamilyName (Word family)
{
    switch (family)
    {
        case 0xFFFD:  return L"Chicago";
        case 0xFFFE:  return L"Shaston";
        case 0:
        case 1:       return L"System Font";
        case 2:       return L"New York";
        case 3:       return L"Geneva";
        case 4:       return L"Monaco";
        case 5:       return L"Venice";
        case 6:       return L"London";
        case 7:       return L"Athens";
        case 8:       return L"San Francisco";
        case 9:       return L"Toronto";
        case 11:      return L"Cairo";
        case 12:      return L"Los Angeles";
        case 13:      return L"Zapf Dingbats";
        case 14:      return L"Bookman";
        case 15:      return L"Helvetica Narrow";
        case 16:      return L"Palatino";
        case 18:      return L"Zapf Chancery";
        case 20:      return L"Times";
        case 21:      return L"Helvetica";
        case 22:      return L"Courier";
        case 23:      return L"Symbol";
        case 24:      return L"Taliesin";
        case 33:      return L"Avant Garde";
        case 34:      return L"New Century Schoolbook";
        default:      return nullptr;
    }
}
