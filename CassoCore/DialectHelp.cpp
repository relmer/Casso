#include "Pch.h"

#include "DialectHelp.h"

#include "CommandLineParser.h"
#include "DialectProfile.h"
#include "DialectRegistry.h"
#include "SubsetBoundary.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DialectHelp::GetAllDialects
//
//  Every dialect the command line can select, swept from the registry rather
//  than listed here.
//
//  A dialect added to the registry documents itself. Listing them by hand is
//  how a tool ends up supporting something its help has never heard of -- and
//  the whole point of naming the dialect on the command line is lost if the
//  reader cannot find out which names there are.
//
////////////////////////////////////////////////////////////////////////////////

std::string DialectHelp::GetAllDialects (char flagPrefix)
{
    std::string  text = "\nDialects -- the assembler the source is written for, named rather than guessed:\n";



    for (const DialectRegistry::Entry & entry : DialectRegistry::GetAllDialects())
    {
        text += GetDialect (*entry.profile, flagPrefix);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialectHelp::GetDialect
//
//  One dialect: how it is selected, its own flags, where its CPU comes from,
//  and where its supported subset ends.
//
//  Each part is absent when the profile has nothing to say, rather than printed
//  empty. A dialect with no flags of its own and no boundary reduces to its
//  selection line, which is the honest description of it.
//
////////////////////////////////////////////////////////////////////////////////

std::string DialectHelp::GetDialect (const DialectProfile & profile, char flagPrefix)
{
    std::span<const SubsetBoundaryRow>  boundary    = profile.GetSubsetBoundary();
    std::string                         text;
    bool                                hasBoundary = !boundary.empty();



    text  = std::string ("\n  ") + profile.GetName() + " <source> [flags]\n";
    text += ComposeFlagLines (profile.GetId(), flagPrefix);
    text += ComposeCpuLine   (profile, flagPrefix);
    text += ComposeOutputShapeLines (profile.GetId(), flagPrefix);

    if (hasBoundary)
    {
        // Indented to match the rows the composer writes rather than the flags
        // above, so the heading sits with what it introduces.
        text += std::string ("\n  Where ") + profile.GetName() + " support ends:\n";
        text += SubsetBoundary::ComposeHelpText (boundary);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialectHelp::ComposeFlagLines
//
//  A dialect's own flags, from the same table its parser walks.
//
//  A dialect whose grammar is not table-driven contributes nothing here, which
//  is why an empty span is a legitimate answer rather than a missing case: the
//  AS65 grammar is a hand-rolled walk over a historical command line, and its
//  flag reference is printed from where that walk lives.
//
////////////////////////////////////////////////////////////////////////////////

std::string DialectHelp::ComposeFlagLines (DialectId dialect, char flagPrefix)
{
    constexpr size_t                                 kDescriptionColumn = 27;
    std::span<const CommandLineParser::DialectFlag>  flags              = CommandLineParser::GetFlags (dialect);
    std::string                                      text;
    std::string                                      rendered;



    for (const CommandLineParser::DialectFlag & flag : flags)
    {
        rendered = std::string ("    ") + flagPrefix + flag.letter;

        if (flag.valueName[0] != '\0')
        {
            rendered += std::string (" ") + flag.valueName;
        }

        text += PadTo (rendered, kDescriptionColumn) + flag.description + "\n";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialectHelp::ComposeCpuLine
//
//  What the CPU flag does for this dialect, which is the profile's answer and
//  not this function's.
//
//  Both readings are stated in the same place, because they answer one question
//  and a reader comparing two dialects is asking exactly that question. The
//  refusal names the in-source directive, so the line says what to write instead
//  of the flag rather than only that the flag is unavailable.
//
////////////////////////////////////////////////////////////////////////////////

std::string DialectHelp::ComposeCpuLine (const DialectProfile & profile, char flagPrefix)
{
    constexpr size_t  kDescriptionColumn = 27;
    std::string       text;
    bool              isInSource         = profile.GetCpuSelectionSource() == CpuSelectionSource::InSource;



    if (isInSource)
    {
        text = PadTo ("    " + CommandLineParser::FormatLongOption ("--cpu", flagPrefix) + " <target>", kDescriptionColumn)
             + "Refused: the CPU target is selected in the source,\n"
             + PadTo ("", kDescriptionColumn)
             + "with the " + profile.GetCpuDirectiveName() + " directive\n";
    }
    else
    {
        text = PadTo ("    " + CommandLineParser::FormatLongOption ("--cpu", flagPrefix) + " <6502|65c02>", kDescriptionColumn)
             + "Target CPU (default: 6502). 65c02 enables the CMOS\n"
             + PadTo ("", kDescriptionColumn)
             + "opcodes (STZ, BRA, RMBn/SMBn, BBRn/BBSn, ...);\n"
             + PadTo ("", kDescriptionColumn)
             + "under 6502 those are rejected as invalid\n";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialectHelp::ComposeOutputShapeLines
//
//  The output shapes a dialect names, from the same table its parser matches
//  against.
//
//  A dialect offering no choice contributes nothing, which is why an empty span
//  is a legitimate answer here too: the shape a source assembles to is not a
//  question every grammar asks.
//
////////////////////////////////////////////////////////////////////////////////

std::string DialectHelp::ComposeOutputShapeLines (DialectId dialect, char flagPrefix)
{
    constexpr size_t                                 kDescriptionColumn = 27;
    std::span<const CommandLineParser::OutputShape>  shapes             = CommandLineParser::GetOutputShapes (dialect);
    std::string                                      text;



    for (const CommandLineParser::OutputShape & shape : shapes)
    {
        text += PadTo (std::string ("    ") + CommandLineParser::FormatLongOption (shape.option, flagPrefix), kDescriptionColumn) + shape.description + "\n";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialectHelp::PadTo
//
//  Right-pads to a column, leaving a single space where the text is already
//  that wide, so a long flag name pushes its description along instead of
//  running into it.
//
////////////////////////////////////////////////////////////////////////////////

std::string DialectHelp::PadTo (const std::string & text, size_t width)
{
    std::string  padded = text;
    bool         fits   = text.size() < width;



    padded += fits ? std::string (width - text.size(), ' ') : std::string (" ");

    return padded;
}
