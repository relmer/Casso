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
//  An unrecognized first argument is NOT an error: it is a source filename,
//  which is exactly how as65 was invoked. AS65 is therefore the fallback
//  rather than a named mode.
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

    // One row of the disk-verb table, nested for the same reason
    // SubcommandName is: a bare struct in the .cpp would have external linkage
    // that no keyword can take away.
    struct DiskVerbName
    {
        const char                            *  name;
        CommandLineOptions::DiskOptions::Verb    verb;
    };

    //  The column width a bare -w selects. Named rather than written twice,
    //  because the usage text quotes the number and a help that quotes a
    //  different one from the parser is worse than one that quotes none.
    static constexpr int  kWideListingColumns = 133;

    static CommandLineOptions  Parse (int argc, char * argv[], const FileExistsFn & fileExists);

    // Whether one argument is the user asking for usage text, in any spelling
    // and either prefix. Public because a subcommand's own grammar has to ask
    // the same question the top level does.
    static bool  IsHelpRequest (const std::string & arg);

    // Shared with the executable, which needs the same tests when it decides
    // how to treat an input path and which output writer to use.
    static bool  IsAssemblySource (const std::string & path);
    static bool  EndsWith         (const std::string & str, const std::string & suffix);

    // Every accepted subcommand spelling, so tests can sweep the whole table
    // instead of a hand-picked sample.
    static std::span<const SubcommandName>  GetAllSubcommands();

    // Every accepted disk-verb spelling, aliases included, for the same reason
    // -- and because the help output has to describe all of them.
    static std::span<const DiskVerbName>    GetAllDiskVerbs();

private:
    static HRESULT  ParseBoundedHex (const char * text, long maxValue, long & outValue);
    static HRESULT  ParseAddress    (const char * text, Word & address);
    static HRESULT  ParseDecimal    (const char * text, uint32_t & value);
    static HRESULT  ParseFillByte   (const char * text, Byte & fillByte);

    //  The numeric argument of a concatenable AS65 flag, glued to it or
    //  standing next to it, and whether one was there at all.
    static bool  TakeCountValue (int                   argc,
                                 char               *  argv[],
                                 int                &  argIndex,
                                 const std::string  &  attached,
                                 int                &  value);

    static std::string  TryAutoExtend  (const std::string & path, const FileExistsFn & fileExists);
    static std::string  StripExtension (const std::string & path);

    static CommandLineOptions::Subcommand  LookUpSubcommand (const std::string & word);

    static void  ParseAs65Flags    (int argc, char * argv[], CommandLineOptions & options);
    static void  ApplyAs65Defaults (CommandLineOptions & options, const FileExistsFn & fileExists);
    static void  ParseRunOptions   (int argc, char * argv[], int argIndex, CommandLineOptions & options);
    static void  ParseDiskOptions  (int argc, char * argv[], int argIndex, CommandLineOptions & options);

    static CommandLineOptions::DiskOptions::Verb  LookUpDiskVerb (const std::string & word);

    //  An argument reduced to the `--` spelling the grammars test for, so
    //  `/long` and `--long` reach the same arm. Only an exact option name from
    //  the supplied table is rewritten -- a ProDOS path starts with a slash and
    //  must stay an operand.
    static std::string  CanonicalLongFlag (const std::string             & arg,
                                           std::span<const char * const>   names);

    static std::string  CanonicalDiskFlag (const std::string & arg);
};
