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
    //  THE ASSEMBLER'S LIST IS as65'S LIST. 0, 1, 2 and 3 carry the meanings the
    //  as65 manual assigns them, and warnings report 5 because as65 has no
    //  status for them. See As65ExitStatus for why 1 moved.
    //
    //  4 IS LISTED AND IS NEVER RETURNED, which is the one entry here that
    //  documents somebody else's tool. as65 spends it on a failed allocation,
    //  and a script ported from as65 may well still test for it; finding it
    //  absent from this list would leave the reader deciding whether it was
    //  renumbered, quietly merged into another status, or simply forgotten.
    //  Naming it settles that in one line, and the line can afford to be
    //  cheerful about it, because a 6502 assembler exhausting a modern host's
    //  memory to fill 64 KB is not a failure mode anybody needs to plan for.
    //
    //  IT IS A TABLE RATHER THAN PROSE. Four lines of explanation per status
    //  said what each one covered and what a script should conclude, which is
    //  reading for somebody porting a build and noise for everybody looking up a
    //  number they just got back. What the statuses MEAN belongs in the header
    //  that assigns them, and that is where it now lives.
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
        "    0  Assembled successfully\n"
        "    1  Bad command line\n"
        "    2  Error opening source or output file\n"
        "    3  Error assembling source file\n"
        "    4  Out of memory, says as65. 64K. On a machine with gigabytes. Sure.\n"
        "    5  Assembled with warnings";

    static constexpr const char *  kRunExitStatusHelpText =
        "    0  Ran to a stop: the stop address or the cycle limit\n"
        "    1  The input was source and did not assemble; nothing ran\n"
        "    2  Nothing could be started: no input file, an input that could not\n"
        "       be read, or a command line that was refused. An option this grammar\n"
        "       cannot read is refused rather than dropped, because one it misread\n"
        "       may have moved the load address.\n"
        "    3  The program reached an illegal opcode";

    //
    //  What KIND of value a flag takes, which decides three things at once: how
    //  the value is read, whether a concatenated group may continue past it, and
    //  how the value appears in help.
    //
    //  THE GROUP RULE IS as65's OWN, and it is derived here rather than written
    //  into each flag: "no other option can follow one that may have a string
    //  parameter. Other options can follow one that has a numeric parameter."
    //  So None and Number let the walk continue at the next character; Filename
    //  and SymbolDefinition consume the rest of the argument, because a name
    //  would otherwise swallow whatever came after it.
    //
    enum class ValueKind
    {
        None,              // the flag is the whole of it
        Number,            // digits, and the group may continue after them
        Filename,          // a path, which takes the rest of the argument
        SymbolDefinition,  // as65's -d: NAME, or NAME=VALUE
    };

    //  Whether a flag's value may be a separate argument, or must be attached.
    //
    //  as65 attaches every value. `-o` is the one exception, and it can be,
    //  because it has no bare form: whatever follows it can only be its
    //  filename. Every other flag here has a bare form, which is what would
    //  leave a separated value ambiguous.
    enum class Attachment
    {
        AttachedOnly,
        AttachedOrSeparate,
    };

    // Which part of the job a flag belongs to, so help can group them. Someone
    // looking for "how do I get a listing" should find the listing flags
    // together rather than scanning one alphabetical run for the four that
    // apply.
    enum class FlagCategory
    {
        AssembledCode,   // what is assembled, and what is written out
        Listing,         // the human-readable listing
        Debug,           // symbol and debug files
        General,         // everything else about the run itself
    };

    // One flag of a dialect's own grammar, as data. The table is what the
    // parser walks AND what the help text is generated from, so the two cannot
    // describe different tools.
    //
    //  THE OPTION IS A STRING, NOT A LETTER, and that is what stops `-s2` from
    //  being a special case. `s` and `s2` are simply two rows, and the walk
    //  takes the LONGEST row that matches at each position -- so `-s2out.hex`
    //  is Intel HEX to out.hex rather than S-records to a file called 2out.hex.
    //
    //  THAT AMBIGUITY IS REAL AND as65 HAS IT TOO: `-s` takes an optional
    //  attached filename, so both readings parse. Longest match settles it, at
    //  the documented cost that `-s` cannot name a file beginning with `2`.
    //  It was settled before by a hand-written peek inside the `s` arm; stating
    //  it once for the whole grammar is the only change.
    //
    //  bareDefault is what the flag means with no value attached, and it is
    //  data because it differs per flag: a bare `-w` is 133 columns, a bare `-d`
    //  defines DEBUG as 1, a bare `-l` lists to stdout. nullptr means a bare
    //  form is refused.
    //
    struct DialectFlag
    {
        const char   *  option;        // "s", "s2", "d" -- longest match wins
        ValueKind       value;
        Attachment      attachment;
        const char   *  bareDefault;   // what a bare flag means, or nullptr
        FlagCategory    category;
        const char   *  valueName;     // how the value appears in help
        const char   *  description;
    };

    // One output FORMAT a dialect's grammar accepts, as data, for the same
    // reason DialectFlag is: the parser walks this table and the help text is
    // generated from it, so a format the tool accepts and one the tool
    // documents cannot come apart.
    //
    // Named as a whole word rather than a letter, because these name a file format
    // rather than an assembler option, and the as65 grammar already writes
    // them that way.
    struct OutputFormatFlag
    {
        const char                    *  option;
        CommandLineOptions::OutputFormat format;
        const char                    *  description;
    };

    // The heading a category prints under, so the wording lives with the enum
    // rather than at whichever call site printed it first.
    static const char *  DescribeCategory (FlagCategory category);

    struct OutputFormatTable
    {
        DialectId                 dialect;
        const OutputFormatFlag  * formats;
        size_t                    count;
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

    //
    //  The flag a dialect's table matches at `at`, longest row first, with how
    //  many characters it consumed. Null when nothing matches.
    //
    //  PUBLIC BECAUSE LONGEST MATCH IS A RULE, not an implementation detail.
    //  It is what makes `-s2` an option rather than `-s` carrying the filename
    //  `2`, and a rule that decides between two readings of the same argument
    //  is one a test has to be able to state directly.
    //
    static const DialectFlag *  MatchFlag (DialectId dialect,
                                           const std::string & text,
                                           size_t at,
                                           size_t & outLength);

    static CommandLineOptions  Parse (int argc, char * argv[], const FileExistsFn & fileExists);

    // Whether one argument is the user asking for usage text, in any form
    // and either prefix. Public because a subcommand's own grammar has to ask
    // the same question the top level does.
    static bool  IsHelpRequest (const std::string & arg);

    // Shared with the executable, which needs the same tests when it decides
    // how to treat an input path and which output writer to use, and the same
    // answer about where a path's extension begins when it names an output
    // after an input.
    static bool         IsAssemblySource (const std::string & path);
    static bool         EndsWith         (const std::string & str, const std::string & suffix);
    static std::string  StripExtension   (const std::string & path);

    // Every accepted disk verb, aliases included, for the same reason -- and
    // because the help output has to describe all of them.
    static std::span<const DiskVerbName>    GetAllDiskVerbs();

    //  Every option the `disk` grammar takes, comma-separated and in their `--` form,
    //  for the refusal an argument that is none of them earns.
    //
    //  Read from the parser's own table rather than retyped, for the reason
    //  DiskCommandRunner::DescribeAcceptedVerbs already gives: a retyped list
    //  is a list that goes stale, and the one place it shows is a suggestion
    //  that omits the option the user was reaching for.
    static std::string  DescribeDiskOptions();

    //  The two questions that decide what a surplus argument is TOLD, and the
    //  two that decide whether an option was unknown or merely starved of its
    //  value. Public for the reason GetAllSubcommands is: the answers reach the
    //  user only as text on the error stream, which the parser writes and does
    //  not keep, so a test can pin the rule here or nowhere.
    //
    //  IsPlainDecimal    -- a word that is only digits: a separated value.
    //  TrailingParameterFlag -- the letter a flag group ends on, when that
    //                       letter takes a parameter, and 0 otherwise.
    static bool  IsPlainDecimal        (const std::string & text);
    static char  TrailingParameterFlag (const std::string & previous);

    //  Whether an argument is the BACK HALF of one a shell cut in two, with the
    //  argument in front of it as the front half. Public because it is the
    //  predicate the repair below turns on, and a rule that silently rejoins two
    //  arguments is one a test has to be able to state exactly.
    static bool  IsShellSplitFragment (const std::string & previous,
                                       const std::string & arg);

    //
    //  The command line as it was TYPED, with any halves PowerShell cut put back
    //  together.
    //
    //  THE TOOL USED TO REFUSE THIS COMMAND LINE AND EXPLAIN THE SHELL TO THE
    //  USER. The explanation was accurate and the refusal was still the wrong
    //  answer: `CassoCli prog.a65 -oprog.bin` is a correct as65 command line,
    //  the user typed it correctly, and it failed for a reason that had nothing
    //  to do with them. Being told to add quotes every time is a tax on using
    //  this tool from the shell most Windows users have.
    //
    //  REJOINING IS SAFE BECAUSE THE SHAPE IS UNREACHABLE OTHERWISE. Every
    //  command line IsShellSplitFragment accepts is one that could not parse:
    //  the front half is a flag group ending in a flag whose value is a name,
    //  the back half begins with the dot the cut was made at, and the front half
    //  carries neither dot nor colon, which is what proves the cut happened. A
    //  command line of that shape has no reading in which the two halves are
    //  separate arguments -- the assembler takes ONE source file, and the back
    //  half could only ever be a surplus one. So nothing that used to work is
    //  read differently now; only what used to be refused now runs.
    //
    //  IT REPAIRS SILENTLY. A warning would fire on a command line the user
    //  typed correctly, about a thing they cannot prevent from inside the shell,
    //  and there is nothing for them to do about it once it is already fixed.
    //
    static std::vector<std::string>  RejoinShellSplitArguments (int argc, char * argv[]);

    //  Whether an argument is an option that grammar HAS and which takes a
    //  value, so an option that merely ran out of command line is not reported
    //  as one that does not exist.
    static bool  IsDiskOptionNeedingValue (const std::string & arg);
    static bool  IsRunOptionNeedingValue  (const std::string & arg);
    // Every accepted subcommand name, so tests can sweep the whole table
    // instead of a hand-picked sample.
    static std::span<const SubcommandName>  GetAllSubcommands();

    // A dialect's own flags, empty for one whose grammar is not table-driven.
    // Public because the help text is generated from this exact table, which is
    // what keeps help and parser from drifting.
    static std::span<const DialectFlag>     GetFlags (DialectId dialect);

    // The output formats a dialect names on its command line, empty for one
    // that offers no choice. Public for the same reason GetFlags is.
    static std::span<const OutputFormatFlag>      GetOutputFormats (DialectId dialect);

    // A long option written with the prefix this invocation used: `--name` for a
    // dash command line, `/name` for a slash one. Public because the help text
    // and the diagnostics have to agree with the parser about how an option is
    // written -- printing `--dos-bin` at someone who typed `/o` tells them to
    // use a form this parser would then have to accept anyway.
    static std::string  FormatLongOption (const std::string & canonical, char flagPrefix);

    // Records the prefix the user typed, the FIRST time one appears. A command
    // line that mixes the two is answered with the prefix it opened with.
    static void         NoteFlagPrefix  (char prefix, CommandLineOptions & options);

    // Whether `arg` names the long option `canonical` (given as `--name`) in
    // either form, recording the prefix as a side effect.
    static bool         IsLongOption    (const std::string & arg, const std::string & canonical,
                                         CommandLineOptions & options);

    // Records the output format one flag asked for, refusing a second flag that
    // asks for a different one. Public because both grammars select formats and
    // both have to refuse the same way.
    static void         SelectOutputFormat (const std::string & flag,
                                            CommandLineOptions::OutputFormat format,
                                            CommandLineOptions & options);

    // The same, for the `--name=value` / `/name=value` form. `value` is filled
    // only when the option matched with a value attached.
    static bool         IsLongOptionWithValue (const std::string & arg, const std::string & canonical,
                                               std::string & value, CommandLineOptions & options);

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

    static CommandLineOptions::Subcommand  LookUpSubcommand (const std::string & word);

    //  What one matched as65 flag means, given the value its row told the walk
    //  to read. `stop` says the command line ended here, which two refusals do.
    static void  ApplyAs65Flag (const DialectFlag * flag,
                                const std::string & value,
                                CommandLineOptions & options,
                                bool & stop);

    //  as65's `-d`: a name, optionally equated to a value. Refuses a value it
    //  cannot read rather than quietly defining the symbol as 1.
    static void  AddSymbolDefinition (const std::string & definition,
                                      CommandLineOptions & options,
                                      bool & stop);

    static void  ParseDiskOptions  (int argc, char * argv[], int argIndex, CommandLineOptions & options);

    static CommandLineOptions::DiskOptions::Verb  LookUpDiskVerb (const std::string & word);

    //  How many operands a disk verb has a use for, and the descriptive word
    //  the help writes it with. An operand past the count is one the verb would
    //  otherwise read and discard.
    static int           DiskOperandCount (CommandLineOptions::DiskOptions::Verb verb);
    static const char *  DiskVerbWord     (CommandLineOptions::DiskOptions::Verb verb);

    //  An argument reduced to the `--` form the grammars test for, so
    //  `/out` and `--out` reach the same arm. Only an exact option name from
    //  the supplied table is rewritten -- a ProDOS path starts with a slash and
    //  must stay an operand.
    static std::string  CanonicalLongFlag (const std::string             & arg,
                                           std::span<const char * const>   names);

    static std::string  CanonicalDiskFlag (const std::string & arg);
    static void  ParseAs65Flags      (int argc, char * argv[], int startIndex, CommandLineOptions & options);
    static void  ApplyAs65Defaults   (CommandLineOptions & options, const FileExistsFn & fileExists);
    static void  ParseMerlinFlags    (int argc, char * argv[], int startIndex, CommandLineOptions & options);
    static void  ApplyMerlinDefaults (CommandLineOptions & options, const FileExistsFn & fileExists);
    static void  ParseRunOptions     (int argc, char * argv[], int argIndex, CommandLineOptions & options);

    static bool  RefuseCpuFlagWhereSelectedInSource (CommandLineOptions & options);
    static void  RecordUnrecognizedFlag (const std::string & flag, CommandLineOptions & options);

    static bool  ApplyOutputFormat (const std::string & arg, DialectId dialect, CommandLineOptions & options);


    static bool                 ApplyMerlinFlag (char                 letter,
                                                 const std::string  & value,
                                                 CommandLineOptions & options,
                                                 bool               & stop);
};
