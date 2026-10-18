#include "Pch.h"

#include "Ui/Debugger/HeatStrip.h"

#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatStrip::Get
//
//  Block b covers bytes b * count / blocks up to (b + 1) * count / blocks,
//  so the bytes are shared out evenly, the address wrapping at $FFFF.
//
////////////////////////////////////////////////////////////////////////////////

HeatStrip::Strip HeatStrip::Get (const DebuggerViewSnapshot::HeatColumnLevels & levels, Word first, size_t count,
                                 const HeatMapView::Palette & palette, bool isTokens)
{
    Strip   strip;
    size_t  blocks = std::min (count, kMaxBlocks);



    if (levels.execute.size() != 0x10000 || levels.read.size() != 0x10000 || levels.write.size() != 0x10000)
    {
        return strip;
    }

    for (size_t block = 0; block < blocks; block++)
    {
        Byte      execute = 0;
        Byte      read    = 0;
        Byte      write   = 0;
        uint32_t  color   = 0;
        wchar_t   glyph   = s_kpszHeatBlock[0];

        for (size_t i = block * count / blocks; i < (block + 1) * count / blocks; i++)
        {
            Word  address = (Word) (first + i);

            execute = std::max (execute, levels.execute[address]);
            read    = std::max (read,    levels.read[address]);
            write   = std::max (write,   levels.write[address]);
        }

        if (isTokens && write > read)
        {
            glyph = L'w';
            color = HeatMapView::GetColor (HeatMapView::Mode::Data, 0, 0, write, palette);
        }
        else if (isTokens)
        {
            color = HeatMapView::GetColor (HeatMapView::Mode::Code, read, 0, 0, palette);
        }
        else if (execute >= std::max (read, write))
        {
            color = HeatMapView::GetColor (HeatMapView::Mode::Code, execute, 0, 0, palette);
        }
        else if (write >= read)
        {
            glyph = L'w';
            color = HeatMapView::GetColor (HeatMapView::Mode::Data, 0, 0, write, palette);
        }
        else
        {
            glyph = L'r';
            color = HeatMapView::GetColor (HeatMapView::Mode::Data, 0, read, 0, palette);
        }

        strip.colors.push_back ({ (int) strip.text.size(), (int) strip.text.size() + 1, color });
        strip.text += glyph;
    }

    return strip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatStrip::GetCell
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::Cell HeatStrip::GetCell (const DebuggerViewSnapshot::HeatColumnLevels & levels, Word first, size_t count,
                                       const HeatMapView::Palette & palette, bool isTokens)
{
    Strip               strip = Get (levels, first, count, palette, isTokens);
    DxuiListView::Cell  cell;



    cell.text        = std::move (strip.text);
    cell.colorRanges = std::move (strip.colors);

    return cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatStrip::CountBytes
//
////////////////////////////////////////////////////////////////////////////////

size_t HeatStrip::CountBytes (const std::string & bytes)
{
    std::istringstream  words (bytes);
    std::string         word;
    size_t              count = 0;



    while (words >> word)
    {
        count++;
    }

    return count;
}
