#pragma once

#include "Pch.h"

class HeatMapSymbols;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRange
//
//  One region of memory the heat map can be focused on. A user's range is a
//  start and an end typed as the console reads them, so a symbol works as
//  well as a number: the end is the last address, a length after a plus
//  ("+$20"), or nothing for a symbol whose file gave its size. A built-in
//  range is a preset's id instead, and has no text of its own. Either is in
//  the set's map only while it is included.
//
////////////////////////////////////////////////////////////////////////////////

struct HeatMapRange
{
    std::string  name;
    std::string  start;
    std::string  end;
    std::string  preset;
    bool         included = true;

    bool operator== (const HeatMapRange & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSet
//
//  A named list of ranges, in the order the map stacks them.
//
////////////////////////////////////////////////////////////////////////////////

struct HeatMapRangeSet
{
    std::string                name;
    std::vector<HeatMapRange>  ranges;

    bool operator== (const HeatMapRangeSet & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeSets
//
//  Every set the user has made, and which one the heat map shows, kept as
//  text in the user's settings. Every set holds every built-in range, so
//  one can be included or left out but never deleted or changed; a range a
//  later build adds joins each set left out.
//
//  The edits that can be refused say why, in the console's words where the
//  console's evaluator refused the text.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapRangeSets
{
public:
    //  A built-in range; "My program" has no fixed span, and follows the
    //  symbols loaded from files.
    struct Preset
    {
        const char  * id    = nullptr;
        const char  * name  = nullptr;
        Word          first = 0;
        Word          last  = 0;
    };

    static constexpr const char  * kProgramPreset = "program";

    //  The map's label for showing every address rather than a set.
    static constexpr const char  * kpszAllMemory  = "All memory";

    //  A range as the map lays it out.
    struct Span
    {
        std::string  name;
        Word         first = 0;
        Word         last  = 0;

        bool operator== (const Span & other) const = default;
    };

    enum class Field
    {
        Name,
        Start,
        End,
        Size,
    };

    std::vector<HeatMapRangeSet>  sets;
    std::string                   shown;

    bool operator== (const HeatMapRangeSets & other) const = default;

    static std::span<const Preset>  GetPresets   ();
    static bool                     TryGetPreset (const std::string & id, Preset & preset);
    static bool                     IsBuiltIn    (const HeatMapRange & range) { return !range.preset.empty(); }

    //  Every built-in range, left out: what a new set starts with.
    static HeatMapRangeSet          MakeSet      (const std::string & name);

    //  Never empty, so text written once reads as written, not as unset.
    std::string                     ToText       () const;
    static HeatMapRangeSets         FromText     (const std::string & text);

    HeatMapRangeSet               * FindSet      (const std::string & name);
    const HeatMapRangeSet         * FindSet      (const std::string & name) const;
    std::string                     GetNewSetName () const;

    bool  TryAddSet      (const std::string & name, std::string & error);
    bool  TryRenameSet   (const std::string & from, const std::string & to, std::string & error);
    bool  TryDeleteSet   (const std::string & name);

    //  A range's own edits, refused for a built-in one. An edit that would
    //  leave the range unreadable is refused too, and the range kept as it
    //  was. Start takes a whole span as well as a start: "$6000-$95FF",
    //  "ZP_VARS..+$20", "$300:$3FF" or "$300,$100".
    static bool  TryEditRange   (HeatMapRangeSet & set, size_t index, Field field, const std::string & text,
                                 const HeatMapSymbols & symbols, std::string & error);
    static bool  TryRemoveRange (HeatMapRangeSet & set, size_t index, std::string & error);
    static bool  TryMoveRange   (HeatMapRangeSet & set, size_t index, int delta);

    //  What a range covers, or why it covers nothing.
    static bool  TryResolve     (const HeatMapRange & range, const HeatMapSymbols & symbols, Word & first, Word & last, std::string & error);

    //  Whether a range has no place in a list or on the map now: "My program"
    //  while no symbols are loaded from a file.
    static bool  IsHidden       (const HeatMapRange & range, const HeatMapSymbols & symbols);

    //  The name a list and a header show: a preset's own, the user's, or the
    //  range's text where the user gave none.
    static std::string  GetName (const HeatMapRange & range);

    //  One to four hex digits, with or without a dollar sign: a range's text
    //  that says no more than the address it reads as.
    static bool  IsHexNumber    (const std::string & text);

    //  A whole span typed as one: true with its start and end apart.
    static bool  TrySplit       (const std::string & text, std::string & start, std::string & end);

    //  The shown set's included ranges that resolve, in its order; empty for
    //  all memory, and for a set with nothing in it.
    std::vector<Span>  GetShownSpans (const HeatMapSymbols & symbols) const;

private:
    static std::string               Trim         (const std::string & text);
    static std::string               Quote        (const std::string & text);
    static std::vector<std::string>  SplitWords   (const std::string & line);
    static void                      AddPresets   (HeatMapRangeSet & set);
};
