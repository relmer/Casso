#include "Pch.h"

#include "CommandLineParser.h"



struct BasicCommandWord
{
    const char                                *  word;
    CommandLineOptions::BasicOptions::Command    command;
    int                                          operandCount;
    const char                                *  operands[2];
};



static constexpr BasicCommandWord  s_kBasicCommands[] =
{
    { "tokenize", CommandLineOptions::BasicOptions::Command::Tokenize, 2, { "<listing>", "<program>" } },
    { "list",     CommandLineOptions::BasicOptions::Command::List,     2, { "<program>", "<listing>" } },
    { "renumber", CommandLineOptions::BasicOptions::Command::Renumber, 2, { "<program>", "<output>"  } },
    { "xref",     CommandLineOptions::BasicOptions::Command::Xref,     1, { "<program>", ""          } },
};

static constexpr const char *  s_kpszBasicOptions[] =
{
    "applesoft",
    "integer",
    "start",
    "step",
};





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineParser::ParseBasicOptions
//
//      basic tokenize <listing> <program> --applesoft | --integer
//      basic list     <program> <listing>
//      basic renumber <program> <output> [--start <n>] [--step <n>]
//      basic xref     <program>
//
//  Every option is accepted with a `/` prefix too. A `/` argument that is not
//  an option of this grammar is an operand, because a path can begin with one.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::ParseBasicOptions (
    int                   argc,
    char               *  argv[],
    int                   argIndex,
    CommandLineOptions &  options)
{
    using Command = CommandLineOptions::BasicOptions::Command;
    using Dialect = CommandLineOptions::BasicOptions::Dialect;

    const BasicCommandWord  * command    = nullptr;
    std::string               operands[2];
    std::string               problem;
    int                       i          = argIndex;
    int                       positional = 0;
    bool                      isHelp     = false;
    bool                      hasNumbers = false;



    isHelp    = IsLoneQuestionMark (argc, argv, argIndex);

    for (int probe = argIndex; probe < argc; probe++)
    {
        if (IsHelpRequest (argv[probe]))
        {
            isHelp             = true;
            options.flagPrefix = (argv[probe][0] == '/') ? '/' : options.flagPrefix;
        }
    }

    if (isHelp)
    {
        options.basic.command = Command::Help;
        return;
    }

    for (int probe = argIndex; probe < argc; probe++)
    {
        if (argv[probe][0] == '/' && GetCanonicalLongFlag (argv[probe], std::span<const char * const> (s_kpszBasicOptions)).rfind ("--", 0) == 0)
        {
            options.flagPrefix = '/';
            break;
        }
    }

    if (i < argc)
    {
        for (const BasicCommandWord & candidate : s_kBasicCommands)
        {
            if (_stricmp (candidate.word, argv[i]) == 0)
            {
                command = &candidate;
            }
        }

        if (command == nullptr)
        {
            problem = std::string ("Error: unknown basic command '") + argv[i] + "'\n";
        }

        i++;
    }
    else
    {
        problem = "Error: required parameter <command> missing\n";
    }

    for ( ; i < argc && problem.empty(); i++)
    {
        std::string  arg      = GetCanonicalLongFlag (argv[i], std::span<const char * const> (s_kpszBasicOptions));
        bool         hasValue = (i + 1) < argc;

        if (arg == "--applesoft" || arg == "--integer")
        {
            Dialect  dialect = (arg == "--integer") ? Dialect::Integer : Dialect::Applesoft;

            if (command->command != Command::Tokenize)
            {
                problem = "Error: " + FormatLongOption (arg, options.flagPrefix) + " is an option of the tokenize command\n";
            }
            else if (options.basic.dialect != Dialect::Unstated && options.basic.dialect != dialect)
            {
                problem = "Error: " + FormatLongOption ("--applesoft", options.flagPrefix) + " and "
                        + FormatLongOption ("--integer", options.flagPrefix) + " cannot be combined\n";
            }

            options.basic.dialect = dialect;
            continue;
        }

        if ((arg == "--start" || arg == "--step") && hasValue)
        {
            std::string  text = argv[++i];

            hasNumbers = true;

            if (!IsPlainDecimal (text) || text.size() > 6)
            {
                problem = "Error: illegal " + FormatLongOption (arg, options.flagPrefix) + " value\n"
                          "       A line number is written in decimal digits.\n";
            }
            else if (arg == "--start")
            {
                options.basic.start = (uint32_t) std::stoul (text);
            }
            else
            {
                options.basic.step = (uint32_t) std::stoul (text);
            }

            continue;
        }

        if (arg[0] == '-' && arg.size() > 1)
        {
            problem = std::string ("Error: unknown option '") + argv[i] + "'\n";
            continue;
        }

        if (positional >= command->operandCount)
        {
            problem = std::string ("Error: unexpected argument '") + argv[i] + "'\n";
            continue;
        }

        operands[positional++] = argv[i];
    }

    if (problem.empty() && hasNumbers && command->command != Command::Renumber)
    {
        problem = "Error: " + FormatLongOption ("--start", options.flagPrefix) + " and "
                + FormatLongOption ("--step", options.flagPrefix) + " are options of the renumber command\n";
    }

    if (problem.empty() && positional < command->operandCount)
    {
        problem = std::string ("Error: required parameter ") + command->operands[positional] + " missing\n";
    }

    if (problem.empty() && command->command == Command::Tokenize && options.basic.dialect == Dialect::Unstated)
    {
        problem = "Error: state the dialect with " + FormatLongOption ("--applesoft", options.flagPrefix) + " or "
                + FormatLongOption ("--integer", options.flagPrefix) + "\n";
    }

    if (!problem.empty())
    {
        Refusal (options) << problem;

        options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
        return;
    }

    options.basic.command    = command->command;
    options.basic.inputFile  = operands[0];
    options.basic.outputFile = operands[1];
}
