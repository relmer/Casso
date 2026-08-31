#include "Pch.h"

#include "DialectHelp.h"

#include "CommandLineParser.h"
#include "DialectProfile.h"
#include "DialectRegistry.h"



//  The order categories print in, which is the order someone reads them in:
//  what comes out first, then what to look at, then what to debug with, then
//  the rest. Fixed here rather than taken from the flag table, so a row added
//  in the middle of a table does not reshuffle the help.
static constexpr CommandLineParser::FlagCategory  s_kCategoryOrder[] =
{
    CommandLineParser::FlagCategory::AssembledCode,
    CommandLineParser::FlagCategory::OutputFormat,
    CommandLineParser::FlagCategory::Listing,
    CommandLineParser::FlagCategory::Debug,
    CommandLineParser::FlagCategory::General,
};





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
    std::string  text = "\nDialects (the assembler the source is written for, named rather than guessed):\n";



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
//  One dialect: how it is selected, and its own flags.
//
//  Each part is absent when the profile has nothing to say, rather than printed
//  empty. A dialect with no flags of its own reduces to its selection line,
//  which is the honest description of it.
//
//  WHERE A SUBSET ENDS IS NOT HERE. The Merlin boundary is six paragraphs of
//  why, and a reader who typed --help wanted the flags. It lives in
//  docs/merlin-subset.md, where there is room to explain what widens each one.
//
////////////////////////////////////////////////////////////////////////////////

std::string DialectHelp::GetDialect (const DialectProfile & profile, char flagPrefix)
{
    std::string  text;



    text  = std::string ("\n  ") + profile.GetName() + " <source> [flags]\n";
    text += GetDialectFlags (profile, flagPrefix);

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialectHelp::GetDialectFlags
//
//  The flag lines alone, for a caller that prints its own heading.
//
////////////////////////////////////////////////////////////////////////////////

std::string DialectHelp::GetDialectFlags (const DialectProfile & profile, char flagPrefix)
{
    return ComposeFlagLines (profile.GetId(), flagPrefix);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialectHelp::ComposeFlagLines
//
//  A dialect's own flags, from the same table its parser walks.
//
//  BOTH DIALECTS FEED THIS NOW. AS65's grammar resisted a table only while a
//  row was a single character -- `-s2` is not `-s` followed by `2` -- and an
//  option matched as a STRING, longest first, settled that. An empty span is
//  still a legitimate answer, for a dialect that adds no flags of its own.
//
////////////////////////////////////////////////////////////////////////////////

std::string DialectHelp::ComposeFlagLines (DialectId dialect, char flagPrefix)
{
    //  Wide enough for the longest rendered flag plus a real gutter. as65's
    //  `-d[<name>[=<value>]]` is 20 characters at an indent of 4, and a row
    //  that reaches the description column with one space left has no gutter
    //  for the wrapper to find -- so its continuation fell back to the left
    //  margin instead of lining up under the text it continues.
    constexpr size_t                                      kDescriptionColumn = 27;
    std::span<const CommandLineParser::DialectFlag>       flags              = CommandLineParser::GetFlags (dialect);
    std::span<const CommandLineParser::OutputFormatFlag>  formats            = CommandLineParser::GetOutputFormats (dialect);
    std::string                                           text;
    std::string                                           rendered;



    // Grouped by what the reader is trying to do, in a fixed order rather than
    // the table's. A category with no rows prints no heading, so a dialect that
    // writes no debug file is not offered an empty section.
    for (CommandLineParser::FlagCategory category : s_kCategoryOrder)
    {
        std::string  group;

        for (const CommandLineParser::DialectFlag & flag : flags)
        {
            if (flag.category != category)
            {
                continue;
            }

            rendered = std::string ("    ") + flagPrefix + flag.option;

            // Three shapes, and each one is read off the row rather than chosen
            // here. A value that may be SEPARATED is written with the space,
            // because that form parses; writing `-l <file>` would document a
            // form the parser reads as a filename called `<file>` and a source
            // that has gone missing. An attached value is joined, and it is
            // bracketed only when the flag HAS a bare form -- brackets mean
            // optional, and `-h[<lines>]` promised a bare `-h` that is refused.
            if (flag.valueName[0] != '\0')
            {
                bool  separable = flag.attachment == CommandLineParser::Attachment::AttachedOrSeparate;
                bool  optional  = flag.bareDefault != nullptr;

                if (separable)
                {
                    rendered += std::string (" ") + flag.valueName;
                }
                else
                {
                    rendered += optional ? std::string ("[") + flag.valueName + "]"
                                         : std::string (flag.valueName);
                }
            }

            group += PadTo (rendered, kDescriptionColumn) + flag.description + "\n";
        }

        // The long-option formats print under the same heading as `-s` and
        // `-s2`, because they answer the same question and only one of the
        // four can be asked. They used to sit with the assembled-code flags,
        // which left a reader picking the formats out of a run of eight.
        if (category == CommandLineParser::FlagCategory::OutputFormat)
        {
            for (const CommandLineParser::OutputFormatFlag & format : formats)
            {
                group += PadTo (std::string ("    ") + CommandLineParser::FormatLongOption (format.option, flagPrefix),
                                kDescriptionColumn)
                       + format.description + "\n";
            }
        }

        if (!group.empty())
        {
            text += std::string ("\n  ") + CommandLineParser::DescribeCategory (category) + "\n" + group;
        }
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
