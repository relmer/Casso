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

    //
    //  What each mode's exit statuses mean, STATED UNDER THAT MODE BECAUSE THEY
    //  DIFFER.
    //
    //  One combined block used to stand near the top of the help and claim to
    //  hold for every mode. It did not. An assembly error exits 3 when a source
    //  file is assembled and 1 when `run` assembles the same file, and status 1
    //  means "the output was written anyway" in one mode and "nothing ran" in
    //  the other -- so a script that read the shared block and branched on 1
    //  learned the opposite of the truth in whichever mode it was not thinking
    //  of. Three statements that are each true beat one statement that is true
    //  of nothing.
    //
    //  THE ASSEMBLER'S BLOCK NAMES A STATUS IT DOES NOT PRODUCE, once, on
    //  purpose. as65 documents 4 for a failed allocation, and this assembler
    //  cannot reach that condition -- a 64 KB image on a virtual-memory host.
    //  Saying so costs two lines and answers the question a script ported from
    //  as65 would otherwise have to answer by experiment.
    //
    //  Every line below was measured by running the built binary, not carried
    //  over from the text it replaces.
    //
    //  These live beside the grammar so the test assembly can read them. The
    //  console executable is not linked there, so a claim written next to the
    //  printing code is a claim nothing can check. `disk` keeps its own, beside
    //  the runner that assigns it -- see DiskCommandRunner.
    //
    static constexpr const char *  kAssembleExitStatusHelpText =
        "    0  assembled cleanly, and every artifact asked for was written.\n"
        "    1  assembled, and the assembler had something to say. The output was still\n"
        "       written.\n"
        "    2  no file was opened -- no input file, an input that could not be\n"
        "       read, a command line that was refused, or an output file that\n"
        "       could not be written.\n"
        "    3  the source was read and did not assemble.\n"
        "       as65 also defines 4, no memory could be allocated. This assembler\n"
        "       does not produce it.\n"
        "       TWO AUTHORITIES MEET IN THIS LIST. 0, 2 and 3 carry as65's own\n"
        "       meanings. 1 does not: as65 spends it on a bad command line, and\n"
        "       this assembler spends it on an assembly that warned, refusing the\n"
        "       bad command line under 2 with the other cases that opened no file.";

    static constexpr const char *  kRunExitStatusHelpText =
        "    0  the program ran to a stop -- the stop address, or the cycle limit.\n"
        "    1  the input was source, and it did not assemble. Nothing ran.\n"
        "    2  nothing could be started -- no input file, an input that could not\n"
        "       be read, or a command line that was refused. An option this grammar\n"
        "       cannot read is refused rather than dropped, because one it misread\n"
        "       may have moved the load address.\n"
        "    3  the program reached an illegal opcode.";

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

    // Every accepted disk verb, aliases included, for the same reason -- and
    // because the help output has to describe all of them.
    static std::span<const DiskVerbName>    GetAllDiskVerbs();

    //  Every option the `disk` grammar takes, comma-separated and `--`-spelled,
    //  for the refusal an argument that is none of them earns.
    //
    //  Read from the parser's own table rather than retyped, for the reason
    //  DiskCommandRunner::DescribeAcceptedVerbs already gives: a retyped list
    //  is a list that goes stale, and the one place it shows is a suggestion
    //  that omits the option the user was reaching for.
    static std::string  DescribeDiskOptions();

private:
    static HRESULT  ParseBoundedHex (const char * text, long maxValue, long & outValue);
    static HRESULT  ParseAddress    (const char * text, Word & address);
    static HRESULT  ParseDecimal    (const char * text, uint32_t & value);
    static HRESULT  ParseFillByte   (const char * text, Byte & fillByte);

    //  The digits glued to a numeric AS65 flag, and how many characters they
    //  occupied -- which is where the flag ends and the next one in the group
    //  begins.
    static size_t  TakeGluedCount (const std::string & rest, int & value);

    static std::string  TryAutoExtend  (const std::string & path, const FileExistsFn & fileExists);
    static std::string  StripExtension (const std::string & path);

    static CommandLineOptions::Subcommand  LookUpSubcommand (const std::string & word);

    static void  ParseAs65Flags    (int argc, char * argv[], CommandLineOptions & options);
    static void  ApplyAs65Defaults (CommandLineOptions & options, const FileExistsFn & fileExists);
    static void  ParseRunOptions   (int argc, char * argv[], int argIndex, CommandLineOptions & options);
    static void  ParseDiskOptions  (int argc, char * argv[], int argIndex, CommandLineOptions & options);

    static CommandLineOptions::DiskOptions::Verb  LookUpDiskVerb (const std::string & word);

    //  An argument reduced to the `--` spelling the grammars test for, so
    //  `/out` and `--out` reach the same arm. Only an exact option name from
    //  the supplied table is rewritten -- a ProDOS path starts with a slash and
    //  must stay an operand.
    static std::string  CanonicalLongFlag (const std::string             & arg,
                                           std::span<const char * const>   names);

    static std::string  CanonicalDiskFlag (const std::string & arg);
};
