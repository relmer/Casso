#include "Pch.h"

#include "Ui/Debugger/HeatStrip.h"
#include "Core/TextEncoding.h"
#include "Debugger/HeatAccessJump.h"





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
    size_t  blocks  = std::min (count, kMaxBlocks);
    int     hottest = -1;



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
        int       level   = 0;

        for (size_t i = block * count / blocks; i < (block + 1) * count / blocks; i++)
        {
            Word  address = (Word) (first + i);

            execute = std::max (execute, levels.execute[address]);
            read    = std::max (read,    levels.read[address]);
            write   = std::max (write,   levels.write[address]);
        }

        level = isTokens ? std::max (read, write) : std::max ({ execute, read, write });

        if (isTokens && write > read)
        {
            color = GetScaleColor (Kind::Write, write, palette);
        }
        else if (isTokens)
        {
            color = GetScaleColor (Kind::Run, read, palette);
        }
        else if (execute >= std::max (read, write))
        {
            color = GetScaleColor (Kind::Run, execute, palette);
        }
        else if (write >= read)
        {
            color = GetScaleColor (Kind::Write, write, palette);
        }
        else
        {
            color = GetScaleColor (Kind::Read, read, palette);
        }

        if (level > hottest)
        {
            hottest            = level;
            strip.hottest      = color;
            strip.hottestLevel = (Byte) level;
            strip.hottestKind  = (isTokens && write <= read)           ? Kind::Run
                               : isTokens                              ? Kind::Write
                               : (execute >= std::max (read, write))   ? Kind::Run
                               : (write >= read)                       ? Kind::Write
                                                                       : Kind::Read;
        }

        strip.colors.push_back ({ (int) strip.text.size(), (int) strip.text.size() + 1, color });
        strip.text += L' ';
    }

    return strip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatStrip::GetScaleStops
//
////////////////////////////////////////////////////////////////////////////////

std::array<uint32_t, 3> HeatStrip::GetScaleStops (Kind kind)
{
    switch (kind)
    {
    case Kind::Read:   return { 0xFF2462B4, 0xFF33AAE6, 0xFF9AEEFF };
    case Kind::Write:  return { 0xFF8A35B0, 0xFFD24CC2, 0xFFFFA6E8 };
    default:           return { 0xFFD8C060, 0xFFEC7A22, 0xFFE0201C };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatStrip::GetScaleColor
//
//  Levels 1 to 128 run from the first stop to the middle one, and 128 to
//  255 from the middle to the last.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t HeatStrip::GetScaleColor (Kind kind, Byte level, const HeatMapView::Palette & palette)
{
    constexpr float                kMiddle = 128.0f;
    constexpr float                kTop    = 255.0f;
    const std::array<uint32_t, 3>  stops   = GetScaleStops (kind);



    if (level == 0)
    {
        return palette.cold | 0xFF000000u;
    }

    if (level <= kMiddle)
    {
        return DxuiColor::Mix (stops[0], stops[1], ((float) level - 1.0f) / (kMiddle - 1.0f)) | 0xFF000000u;
    }

    return DxuiColor::Mix (stops[1], stops[2], ((float) level - kMiddle) / (kTop - kMiddle)) | 0xFF000000u;
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



    cell.text       = std::move (strip.text);
    cell.background = strip.hottest;

    if (!levels.executeCount.empty())
    {
        Tip  tip = GetTip (levels, first, count, isTokens);

        cell.tip             = std::move (tip.text);
        cell.tipGradient     = std::move (tip.gradient);
        cell.tipGradientMark = tip.mark;
    }

    return cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatStrip::GetLineCell
//
//  The count at the line's first token is how often BASIC began the line, so
//  the interpreter's other reads of the tokens, such as LIST's or a skipped
//  IF's, don't color it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::Cell HeatStrip::GetLineCell (const DebuggerViewSnapshot::HeatColumnLevels & levels, Word firstToken, const HeatMapView::Palette & palette)
{
    constexpr float     kTop  = 255.0f;
    DxuiListView::Cell  cell;
    Byte                level = 0;
    uint32_t            count = 0;



    if (levels.statement.size() != 0x10000 || levels.statementCount.size() != 0x10000)
    {
        return cell;
    }

    level = levels.statement[firstToken];
    count = levels.statementCount[firstToken];

    if (level == 0)
    {
        cell.tip = (count == 0) ? L"This line hasn't run since counting began."
                                : std::format (L"This line ran {} times since counting began, none of them lately.", TextEncoding::NarrowToWide (HeatAccessJump::GroupDigits (count)));
        return cell;
    }

    cell.background      = GetScaleColor (Kind::Run, level, palette);
    cell.tip             = std::format (L"This line ran {} {} since counting began.\n\n{}, from least to most. This one is {}% of the way up.",
                                        TextEncoding::NarrowToWide (HeatAccessJump::GroupDigits (count)), (count == 1) ? L"time" : L"times",
                                        GetScaleName (Kind::Run), std::lround ((float) level * 100.0f / kTop));
    cell.tipGradientMark = (float) level / kTop;

    for (uint32_t stop : GetScaleStops (Kind::Run))
    {
        cell.tipGradient.push_back (stop);
    }

    return cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatStrip::GetTip
//
//  A table of the bytes' counts, a row a byte, with a blank line and the
//  scale's meaning under it. A BASIC line's token reads are its runs, so
//  they are listed as runs.
//
////////////////////////////////////////////////////////////////////////////////

HeatStrip::Tip HeatStrip::GetTip (const DebuggerViewSnapshot::HeatColumnLevels & levels, Word first, size_t count, bool isTokens)
{
    constexpr float  kTop  = 255.0f;
    Tip              tip;
    Strip            strip;
    auto             group = [] (uint64_t value) { return TextEncoding::NarrowToWide (HeatAccessJump::GroupDigits (value)); };
    uint64_t         runs  = 0;
    uint64_t         reads = 0;
    uint64_t         puts  = 0;



    if (levels.executeCount.size() != 0x10000 || levels.readCount.size() != 0x10000 || levels.writeCount.size() != 0x10000 || count == 0)
    {
        return tip;
    }

    strip    = Get (levels, first, count, HeatMapView::Palette(), isTokens);
    tip.text = isTokens ? L"Byte\tRun\tWritten" : L"Byte\tRun\tRead\tWritten";

    for (size_t i = 0; i < count; i++)
    {
        Word  address = (Word) (first + i);

        runs  += isTokens ? levels.readCount[address] : levels.executeCount[address];
        reads += isTokens ? 0 : levels.readCount[address];
        puts  += levels.writeCount[address];

        if (count <= kMaxTipBytes)
        {
            tip.text += isTokens ? std::format (L"\n${:04X}\t{}\t{}",     address, group (levels.readCount[address]), group (levels.writeCount[address]))
                                 : std::format (L"\n${:04X}\t{}\t{}\t{}", address, group (levels.executeCount[address]), group (levels.readCount[address]), group (levels.writeCount[address]));
        }
    }

    if (count > kMaxTipBytes)
    {
        tip.text += isTokens ? std::format (L"\n${:04X}-${:04X}\t{}\t{}",     first, (Word) (first + count - 1), group (runs), group (puts))
                             : std::format (L"\n${:04X}-${:04X}\t{}\t{}\t{}", first, (Word) (first + count - 1), group (runs), group (reads), group (puts));
    }

    if (strip.hottestLevel == 0)
    {
        tip.text += L"\n\nNothing has touched these bytes since counting began.";
        return tip;
    }

    tip.text    += std::format (L"\n\n{}, from least to most. This one is {}% of the way up.", GetScaleName (strip.hottestKind), std::lround ((float) strip.hottestLevel * 100.0f / kTop));
    tip.mark     = (float) strip.hottestLevel / kTop;

    for (uint32_t stop : GetScaleStops (strip.hottestKind))
    {
        tip.gradient.push_back (stop);
    }

    return tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatStrip::GetScaleName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatStrip::GetScaleName (Kind kind)
{
    switch (kind)
    {
    case Kind::Read:   return L"Read: blue to pale cyan";
    case Kind::Write:  return L"Written: purple to pink";
    default:           return L"Run: yellow to red";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatStrip::GetHeadingTip
//
////////////////////////////////////////////////////////////////////////////////

HeatStrip::HeadingTip HeatStrip::GetHeadingTip (bool isTokens)
{
    HeadingTip         tip;
    std::vector<Kind>  kinds = isTokens ? std::vector<Kind> { Kind::Run } : std::vector<Kind> { Kind::Run, Kind::Read, Kind::Write };



    tip.text = isTokens ? L"How often BASIC ran the line: the hotter the color, the\n"
                          L"more often. A line that hasn't run has no color.\n"
                        : L"How often the line's bytes were run, read or written.\n"
                          L"Each block is a byte, or a share of a long line's bytes,\n"
                          L"in the color of what was done to it most.\n"
                          L"A byte that nothing touched has no color.\n";

    tip.lineScales.resize ((size_t) std::ranges::count (tip.text, L'\n'));

    for (size_t i = 0; i < kinds.size(); i++)
    {
        const std::array<uint32_t, 3>  stops = GetScaleStops (kinds[i]);



        tip.text += ((i > 0) ? L"\n" : L"") + GetScaleName (kinds[i]) + L", least to most";
        tip.lineScales.push_back (std::vector<uint32_t> (stops.begin(), stops.end()));
    }

    return tip;
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
