#pragma once

#include "CommandLineOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineParser
//
//  Turns an argv into a CommandLineOptions. Pure data-in / data-out: it reads
//  no files, writes nothing, and prints nothing, which is what lets the
//  UnitTest project exercise the whole grammar.
//
//  The one thing the grammar genuinely needs from the filesystem -- does
//  `build` name a real `build.a65`? -- arrives as an injected predicate rather
//  than a direct probe, so a test supplies a synthetic answer and the
//  executable supplies the real check.
//
//  The subcommand table is data, so adding a subcommand is one row plus its
//  own flag parser. It used to be a single `first != "run"` test, which meant
//  every new subcommand reshaped the dispatcher.
//
//  An unrecognized first argument is an ERROR. It used to be taken for a source
//  filename, which is how as65 was invoked, and that guess is what made the
//  dialect something the tool decided rather than something the invocation
//  said. Every dialect is now named by a subcommand of its own.
//
////////////////////////////////////////////////////////////////////////////////

class CommandLineParser
{
public:
    // Answers "does this path exist?" without the parser touching the disk.
    using FileExistsFn = std::function<bool (const std::string &)>;

    // One row of the subcommand table. Nested rather than declared in the
    // .cpp: a bare struct there has external linkage and no keyword can
    // change that.
    struct SubcommandName
    {
        const char                      *  name;
        CommandLineOptions::Subcommand     token;
    };

    // Whether a flag carries a value, and whether it must.
    enum class FlagArgument
    {
        None,       // the flag is the whole of it
        Required,   // a value follows, attached to the flag or the next argument
        Optional,   // a value may follow, attached only -- absence means something
    };

    // One flag of a dialect's own grammar, as data. The table is what the
    // parser walks AND what the help text is generated from, so the two cannot
    // describe different tools.
    struct DialectFlag
    {
        char            letter;
        FlagArgument    argument;
        const char   *  valueName;     // how the value is spelled in help
        const char   *  description;
    };

    // Which dialect a flag table belongs to. A row rather than a test on the
    // dialect, so a dialect with a grammar of its own is added by stating it
    // and a dialect without one simply has no row.
    struct DialectFlagTable
    {
        DialectId             dialect;
        const DialectFlag  *  flags;
        size_t                count;
    };

    static CommandLineOptions  Parse (int argc, char * argv[], const FileExistsFn & fileExists);

    // Shared with the executable, which needs the same tests when it decides
    // how to treat an input path and which output writer to use, and the same
    // answer about where a path's extension begins when it names an output
    // after an input.
    static bool         IsAssemblySource (const std::string & path);
    static bool         EndsWith         (const std::string & str, const std::string & suffix);
    static std::string  StripExtension   (const std::string & path);

    // Every accepted subcommand spelling, so tests can sweep the whole table
    // instead of a hand-picked sample.
    static std::span<const SubcommandName>  GetAllSubcommands();

    // A dialect's own flags, empty for one whose grammar is not table-driven.
    // Public because the help text is generated from this exact table, which is
    // what keeps help and parser from drifting.
    static std::span<const DialectFlag>     GetFlags (DialectId dialect);

private:
    static HRESULT  ParseBoundedHex (const char * text, long maxValue, long & outValue);
    static HRESULT  ParseAddress    (const char * text, Word & address);
    static HRESULT  ParseDecimal    (const char * text, uint32_t & value);
    static HRESULT  ParseFillByte   (const char * text, Byte & fillByte);

    static std::string  TryAutoExtend  (const std::string & path, const FileExistsFn & fileExists);

    static CommandLineOptions::Subcommand  LookUpSubcommand (const std::string & word);

    static void  ParseAs65Flags      (int argc, char * argv[], int startIndex, CommandLineOptions & options);
    static void  ApplyAs65Defaults   (CommandLineOptions & options, const FileExistsFn & fileExists);
    static void  ParseMerlinFlags    (int argc, char * argv[], int startIndex, CommandLineOptions & options);
    static void  ApplyMerlinDefaults (CommandLineOptions & options, const FileExistsFn & fileExists);
    static void  ParseRunOptions     (int argc, char * argv[], int argIndex, CommandLineOptions & options);

    static bool  RefuseCpuFlagWhereSelectedInSource (CommandLineOptions & options);

    static void  AddSymbolDefinition (const std::string & definition, CommandLineOptions & options);

    static const DialectFlag *  FindMerlinFlag  (char letter);
    static bool                 ApplyMerlinFlag (char                 letter,
                                                 const std::string  & value,
                                                 CommandLineOptions & options);
};
