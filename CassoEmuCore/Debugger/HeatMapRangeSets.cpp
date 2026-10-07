#include "Pch.h"

#include "Debugger/AppleWinParser.h"
#include "Debugger/HeatMapRangeSets.h"
#include "Debugger/HeatMapSymbols.h"





//  The Apple II's own regions, in address order, then the program's.
static constexpr HeatMapRangeSets::Preset  s_kPresets[] =
{
    { "zeropage",     "Zero page",     0x0000, 0x00FF },
    { "stack",        "Stack",         0x0100, 0x01FF },
    { "text1",        "Text page 1",   0x0400, 0x07FF },
    { "text2",        "Text page 2",   0x0800, 0x0BFF },
    { "hires1",       "Hi-res page 1", 0x2000, 0x3FFF },
    { "hires2",       "Hi-res page 2", 0x4000, 0x5FFF },
    { "io",           "I/O",           0xC000, 0xC0FF },
    { "languagecard", "Language Card", 0xD000, 0xFFFF },
    { "program",      "My program",    0x0000, 0x0000 },
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::GetPresets
//
////////////////////////////////////////////////////////////////////////////////

std::span<const HeatMapRangeSets::Preset> HeatMapRangeSets::GetPresets()
{
    return s_kPresets;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::TryGetPreset
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapRangeSets::TryGetPreset (const std::string & id, Preset & preset)
{
    for (const Preset & each : s_kPresets)
    {
        if (id == each.id)
        {
            preset = each;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::MakeSet
//
////////////////////////////////////////////////////////////////////////////////

HeatMapRangeSet HeatMapRangeSets::MakeSet (const std::string & name)
{
    HeatMapRangeSet  set;



    set.name = name;
    AddPresets (set);

    return set;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::AddPresets
//
//  Each built-in range the set lacks, at its end and left out.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapRangeSets::AddPresets (HeatMapRangeSet & set)
{
    for (const Preset & preset : s_kPresets)
    {
        bool  isHeld = std::ranges::any_of (set.ranges, [&preset] (const HeatMapRange & range) { return range.preset == preset.id; });



        if (!isHeld)
        {
            set.ranges.push_back ({ {}, {}, {}, preset.id, false });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::Quote
//
//  In double quotes, with a quote or a backslash inside escaped.
//
////////////////////////////////////////////////////////////////////////////////

std::string HeatMapRangeSets::Quote (const std::string & text)
{
    std::string  quoted = "\"";



    for (char ch : text)
    {
        if (ch == '"' || ch == '\\')
        {
            quoted += '\\';
        }

        quoted += ch;
    }

    return quoted + "\"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::ToText
//
//  A line for the set shown, then each set's line and a line for each of
//  its ranges in order:
//
//      show "Game"
//      set "Game"
//      preset zeropage on
//      range on "Sprites" "$6000" "$95FF"
//
////////////////////////////////////////////////////////////////////////////////

std::string HeatMapRangeSets::ToText() const
{
    std::string  text = "show " + Quote (shown) + "\n";



    for (const HeatMapRangeSet & set : sets)
    {
        text += "set " + Quote (set.name) + "\n";

        for (const HeatMapRange & range : set.ranges)
        {
            const char  * state = range.included ? "on" : "off";



            if (IsBuiltIn (range))
            {
                text += std::format ("preset {} {}\n", range.preset, state);
                continue;
            }

            text += std::format ("range {} {} {} {}\n", state, Quote (range.name), Quote (range.start), Quote (range.end));
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::SplitWords
//
//  Words apart at spaces, a quoted word whole with its escapes undone.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> HeatMapRangeSets::SplitWords (const std::string & line)
{
    std::vector<std::string>  words;
    std::string               word;
    bool                      isQuoted = false;
    bool                      hasWord  = false;



    for (size_t i = 0; i < line.size(); i++)
    {
        char  ch = line[i];



        if (isQuoted && ch == '\\' && i + 1 < line.size())
        {
            word += line[++i];
        }
        else if (ch == '"')
        {
            isQuoted = !isQuoted;
            hasWord  = true;
        }
        else if (!isQuoted && (ch == ' ' || ch == '\t' || ch == '\r'))
        {
            if (hasWord)
            {
                words.push_back (word);
            }

            word.clear();
            hasWord = false;
        }
        else
        {
            word    += ch;
            hasWord  = true;
        }
    }

    if (hasWord)
    {
        words.push_back (word);
    }

    return words;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::FromText
//
//  Lines it does not know are passed over, as are a set whose name another
//  set already has and a preset no build knows, so a hand-edited setting
//  still gives usable sets. A set shown that no longer exists shows all
//  memory.
//
////////////////////////////////////////////////////////////////////////////////

HeatMapRangeSets HeatMapRangeSets::FromText (const std::string & text)
{
    constexpr size_t     kRangeWords = 5;
    HeatMapRangeSets     result;
    HeatMapRangeSet    * current     = nullptr;
    std::istringstream   lines (text);
    std::string          line;
    Preset               preset;



    while (std::getline (lines, line))
    {
        std::vector<std::string>  words = SplitWords (line);



        if (words.size() >= 2 && words[0] == "show")
        {
            result.shown = words[1];
        }
        else if (words.size() >= 2 && words[0] == "set")
        {
            current = (words[1].empty() || result.FindSet (words[1]) != nullptr) ? nullptr : &result.sets.emplace_back (HeatMapRangeSet { words[1], {} });
        }
        else if (current != nullptr && words.size() >= 3 && words[0] == "preset" && TryGetPreset (words[1], preset))
        {
            current->ranges.push_back ({ {}, {}, {}, preset.id, words[2] == "on" });
        }
        else if (current != nullptr && words.size() >= kRangeWords && words[0] == "range")
        {
            current->ranges.push_back ({ words[2], words[3], words[4], {}, words[1] == "on" });
        }
    }

    for (HeatMapRangeSet & set : result.sets)
    {
        std::vector<HeatMapRange>  ranges;



        //  A preset listed twice is kept once, where it first appears.
        for (const HeatMapRange & range : set.ranges)
        {
            if (!IsBuiltIn (range) || std::ranges::none_of (ranges, [&range] (const HeatMapRange & kept) { return kept.preset == range.preset; }))
            {
                ranges.push_back (range);
            }
        }

        set.ranges = std::move (ranges);
        AddPresets (set);
    }

    if (result.FindSet (result.shown) == nullptr)
    {
        result.shown.clear();
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::FindSet
//
////////////////////////////////////////////////////////////////////////////////

HeatMapRangeSet * HeatMapRangeSets::FindSet (const std::string & name)
{
    for (HeatMapRangeSet & set : sets)
    {
        if (set.name == name)
        {
            return &set;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::FindSet
//
////////////////////////////////////////////////////////////////////////////////

const HeatMapRangeSet * HeatMapRangeSets::FindSet (const std::string & name) const
{
    for (const HeatMapRangeSet & set : sets)
    {
        if (set.name == name)
        {
            return &set;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::GetNewSetName
//
//  "Set 1", or the first number after it no set has.
//
////////////////////////////////////////////////////////////////////////////////

std::string HeatMapRangeSets::GetNewSetName() const
{
    int          number = 1;
    std::string  name   = "Set 1";



    while (FindSet (name) != nullptr)
    {
        name = std::format ("Set {}", ++number);
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::TryAddSet
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapRangeSets::TryAddSet (const std::string & name, std::string & error)
{
    std::string  trimmed = Trim (name);



    if (!TryRenameSet ({}, trimmed, error))
    {
        return false;
    }

    sets.push_back (MakeSet (trimmed));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::TryRenameSet
//
//  A set takes a name no other set has, and not the map's own All memory.
//  From an empty name, as a new set has, it only checks the name.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapRangeSets::TryRenameSet (const std::string & from, const std::string & to, std::string & error)
{
    std::string        trimmed = Trim (to);
    HeatMapRangeSet  * set     = from.empty() ? nullptr : FindSet (from);
    HeatMapRangeSet  * other   = FindSet (trimmed);



    if (trimmed.empty())
    {
        error = "A set needs a name.";
        return false;
    }

    if (trimmed == kpszAllMemory)
    {
        error = "All memory is the map's choice for every address, so a set cannot take that name.";
        return false;
    }

    if (other != nullptr && other != set)
    {
        error = std::format ("There is already a set called {}.", trimmed);
        return false;
    }

    if (set == nullptr && !from.empty())
    {
        error = std::format ("There is no set called {}.", from);
        return false;
    }

    if (set == nullptr)
    {
        return true;
    }

    if (shown == set->name)
    {
        shown = trimmed;
    }

    set->name = trimmed;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::TryDeleteSet
//
//  The map shows all memory once the set it showed is gone.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapRangeSets::TryDeleteSet (const std::string & name)
{
    size_t  removed = std::erase_if (sets, [&name] (const HeatMapRangeSet & set) { return set.name == name; });



    if (removed > 0 && shown == name)
    {
        shown.clear();
    }

    return removed > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::TryEditRange
//
//  The range is changed, read, and put back as it was if it does not read.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapRangeSets::TryEditRange (
    HeatMapRangeSet       & set,
    size_t                  index,
    Field                   field,
    const std::string     & text,
    const HeatMapSymbols  & symbols,
    std::string           & error)
{
    HeatMapRange  * range   = nullptr;
    HeatMapRange    saved;
    std::string     trimmed = Trim (text);
    std::string     start;
    std::string     end;
    Word            first   = 0;
    Word            last    = 0;



    if (index >= set.ranges.size())
    {
        error = "There is no such range.";
        return false;
    }

    range = &set.ranges[index];

    if (IsBuiltIn (*range))
    {
        error = "A built-in range cannot be changed. Include it or leave it out with its check box.";
        return false;
    }

    saved = *range;

    switch (field)
    {
    case Field::Name:
        range->name = trimmed;
        return true;

    case Field::Start:
        if (TrySplit (trimmed, start, end))
        {
            range->start = start;
            range->end   = end;
        }
        else
        {
            range->start = trimmed;
        }

        break;

    case Field::End:
        range->end = trimmed;
        break;

    case Field::Size:
        range->end = (trimmed.empty() || trimmed.starts_with ('+')) ? trimmed : "+" + trimmed;
        break;
    }

    if (!TryResolve (*range, symbols, first, last, error))
    {
        *range = saved;
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::TryRemoveRange
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapRangeSets::TryRemoveRange (HeatMapRangeSet & set, size_t index, std::string & error)
{
    if (index >= set.ranges.size())
    {
        error = "There is no such range.";
        return false;
    }

    if (IsBuiltIn (set.ranges[index]))
    {
        error = "A built-in range cannot be deleted. Leave it out with its check box instead.";
        return false;
    }

    set.ranges.erase (set.ranges.begin() + (ptrdiff_t) index);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::TryMoveRange
//
//  A built-in range moves as any other does: it stays in the set either way.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapRangeSets::TryMoveRange (HeatMapRangeSet & set, size_t index, int delta)
{
    ptrdiff_t  to = (ptrdiff_t) index + delta;



    if (index >= set.ranges.size() || to < 0 || to >= (ptrdiff_t) set.ranges.size())
    {
        return false;
    }

    std::swap (set.ranges[index], set.ranges[(size_t) to]);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::TryResolve
//
//  A user's range is read as the console reads a range: start:last for an
//  end, start,length for a size. A start with no end takes the size its
//  symbol's file gave.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapRangeSets::TryResolve (const HeatMapRange & range, const HeatMapSymbols & symbols, Word & first, Word & last, std::string & error)
{
    std::string   start = Trim (range.start);
    std::string   end   = Trim (range.end);
    std::string   text;
    Word          size  = 0;
    Preset        preset;
    DebugCommand  command;



    if (range.preset == kProgramPreset && !symbols.TryGetProgramSpan (first, last))
    {
        error = "No symbols are loaded from a file, so there is no program to cover.";
        return false;
    }

    if (range.preset == kProgramPreset)
    {
        return true;
    }

    if (TryGetPreset (range.preset, preset))
    {
        first = preset.first;
        last  = preset.last;
        return true;
    }

    if (start.empty())
    {
        error = "A range needs a start: an address or a symbol.";
        return false;
    }

    if (end.empty() && !symbols.TryGetSize (start, size))
    {
        error = std::format ("{} has no size of its own. Give the range an end or a size, as $6000-$95FF or {}..+$20.", start, start);
        return false;
    }

    if (end.empty())
    {
        text = std::format ("{},${:X}", start, size);
    }
    else if (end.starts_with ('+'))
    {
        text = start + "," + end.substr (1);
    }
    else
    {
        text = start + ":" + end;
    }

    if (!AppleWinParser::TryParseRange (text, symbols, command, error))
    {
        return false;
    }

    first = command.a1;
    last  = command.hasA2 ? command.a2 : command.a1;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::IsHidden
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapRangeSets::IsHidden (const HeatMapRange & range, const HeatMapSymbols & symbols)
{
    Word  first = 0;
    Word  last  = 0;



    return range.preset == kProgramPreset && !symbols.TryGetProgramSpan (first, last);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::GetName
//
////////////////////////////////////////////////////////////////////////////////

std::string HeatMapRangeSets::GetName (const HeatMapRange & range)
{
    Preset  preset;



    if (TryGetPreset (range.preset, preset))
    {
        return preset.name;
    }

    if (!range.name.empty())
    {
        return range.name;
    }

    return range.end.empty() ? range.start : range.start + ".." + range.end;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::TrySplit
//
//  ".." between a start and an end or a +length; the console's own
//  start:last and start,length; or a dash between two hex numbers, as
//  $6000-$95FF, which elsewhere would be a subtraction.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapRangeSets::TrySplit (const std::string & text, std::string & start, std::string & end)
{
    std::string  trimmed = Trim (text);
    size_t       at      = trimmed.find ("..");



    if (at != std::string::npos)
    {
        start = Trim (trimmed.substr (0, at));
        end   = Trim (trimmed.substr (at + 2));
        return true;
    }

    at = trimmed.find_first_of (":,");

    if (at != std::string::npos)
    {
        start = Trim (trimmed.substr (0, at));
        end   = Trim (trimmed.substr (at + 1));
        end   = (trimmed[at] == ',') ? "+" + end : end;
        return true;
    }

    at = trimmed.find ('-', 1);

    if (at == std::string::npos || !IsHexNumber (Trim (trimmed.substr (0, at))) || !IsHexNumber (Trim (trimmed.substr (at + 1))))
    {
        return false;
    }

    start = Trim (trimmed.substr (0, at));
    end   = Trim (trimmed.substr (at + 1));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::IsHexNumber
//
//  One to four hex digits, with or without a dollar sign.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapRangeSets::IsHexNumber (const std::string & text)
{
    constexpr size_t  kMaxDigits = 4;
    std::string       digits     = text.starts_with ('$') ? text.substr (1) : text;



    return !digits.empty() && digits.size() <= kMaxDigits && digits.find_first_not_of ("0123456789ABCDEFabcdef") == std::string::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::Trim
//
////////////////////////////////////////////////////////////////////////////////

std::string HeatMapRangeSets::Trim (const std::string & text)
{
    size_t  first = text.find_first_not_of (" \t\r\n");
    size_t  last  = text.find_last_not_of  (" \t\r\n");



    return (first == std::string::npos) ? std::string() : text.substr (first, last - first + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::GetShownSpans
//
////////////////////////////////////////////////////////////////////////////////

std::vector<HeatMapRangeSets::Span> HeatMapRangeSets::GetShownSpans (const HeatMapSymbols & symbols) const
{
    return GetSpans (shown, symbols);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::GetSpans
//
////////////////////////////////////////////////////////////////////////////////

std::vector<HeatMapRangeSets::Span> HeatMapRangeSets::GetSpans (
    const std::string     & name,
    const HeatMapSymbols  & symbols) const
{
    std::vector<Span>        spans;
    const HeatMapRangeSet  * set   = FindSet (name);
    std::string              error;



    if (set == nullptr)
    {
        return spans;
    }

    for (const HeatMapRange & range : set->ranges)
    {
        Span  span;



        if (!range.included || IsHidden (range, symbols) || !TryResolve (range, symbols, span.first, span.last, error))
        {
            continue;
        }

        span.name = GetName (range);
        spans.push_back (std::move (span));
    }

    return spans;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::FormatSpanWords
//
////////////////////////////////////////////////////////////////////////////////

std::string HeatMapRangeSets::FormatSpanWords (const std::vector<std::pair<Word, Word>> & spans)
{
    std::string  words;



    if (spans.empty())
    {
        return "none";
    }

    for (const auto & [first, last] : spans)
    {
        words += std::format ("{}{:04X}-{:04X}", words.empty() ? "" : " ", first, last);
    }

    return words;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets::TryParseSpanWords
//
//  Each span as two hex addresses with a dash between, the first no later
//  than the last; "none" for no span.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapRangeSets::TryParseSpanWords (
    const std::string                    & words,
    std::vector<std::pair<Word, Word>>   & spans)
{
    std::istringstream  stream (words);
    std::string         word;
    std::string         first;
    std::string         last;
    size_t              dash   = 0;
    Word                from   = 0;
    Word                to     = 0;



    spans.clear();

    while (stream >> word)
    {
        if (word == "none")
        {
            continue;
        }

        dash  = word.find ('-');
        first = word.substr (0, dash);
        last  = (dash == std::string::npos) ? std::string() : word.substr (dash + 1);

        if (!IsHexNumber (first) || !IsHexNumber (last) || first.starts_with ('$') || last.starts_with ('$'))
        {
            spans.clear();
            return false;
        }

        from = (Word) std::stoul (first, nullptr, 16);
        to   = (Word) std::stoul (last,  nullptr, 16);

        if (from > to)
        {
            spans.clear();
            return false;
        }

        spans.emplace_back (from, to);
    }

    return true;
}





