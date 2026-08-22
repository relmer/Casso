#include "Pch.h"

#include "CommandLineParser.h"

#include "As65ExitStatus.h"
#include "DialectProfile.h"
#include "DialectRegistry.h"





//
//  Every bare-word subcommand. Anything not in this table is rejected -- so a
//  new subcommand is a row here plus an arm in Parse, not a reshaped
//  dispatcher.
//
static constexpr CommandLineParser::SubcommandName  s_kSubcommands[] =
{
    { "run",  CommandLineOptions::Subcommand::Run  },
    { "disk", CommandLineOptions::Subcommand::Disk },
    { "as65",   CommandLineOptions::Subcommand::As65   },
    { "merlin", CommandLineOptions::Subcommand::Merlin },
    { "run",    CommandLineOptions::Subcommand::Run    },
};


//
//  Second-level verbs of the `disk` subcommand. Descriptive words are what help
//  displays; every other word on the same row is an alias accepted because
//  somebody will type it.
//
//  THE ALIASES COME FROM THREE DIFFERENT HABITS, and all three are real.
//  `catalog` and `cat` are the words DOS 3.3 and ProDOS themselves answer to,
//  so anyone who used these machines types one of them before anything else.
//  `dir` and `del` are what the host shell trained them to type. `ls` and `rm`
//  are what a Unix shell did.
//
//  `cat` was left out once on the grounds that it collides with the Unix
//  meaning of printing a file. That reasoning weighed a convention from another
//  platform above the literal command of the machine this tool exists to serve,
//  and the machine wins: on an Apple II, CAT lists the disk.
//
static constexpr CommandLineParser::DiskVerbName  s_kDiskVerbs[] =
{
    { "list",    CommandLineOptions::DiskOptions::Verb::List   },
    { "ls",      CommandLineOptions::DiskOptions::Verb::List   },
    { "dir",     CommandLineOptions::DiskOptions::Verb::List   },
    { "cat",     CommandLineOptions::DiskOptions::Verb::List   },
    { "catalog", CommandLineOptions::DiskOptions::Verb::List   },
    { "get",     CommandLineOptions::DiskOptions::Verb::Get    },
    { "read",    CommandLineOptions::DiskOptions::Verb::Get    },
    { "put",     CommandLineOptions::DiskOptions::Verb::Put    },
    { "write",   CommandLineOptions::DiskOptions::Verb::Put    },
    { "delete",  CommandLineOptions::DiskOptions::Verb::Delete },
    { "rm",      CommandLineOptions::DiskOptions::Verb::Delete },
    { "del",     CommandLineOptions::DiskOptions::Verb::Delete },
    { "boot",    CommandLineOptions::DiskOptions::Verb::Boot   },
};




//
//  The Merlin grammar's flags. Short on purpose: Merlin's source answers in
//  itself most of what as65 answers with a flag -- the object's name, the CPU --
//  so what remains here is what only the invocation can say.
//
static constexpr CommandLineParser::DialectFlag  s_kMerlinFlags[] =
{
    { "o", CommandLineParser::ValueKind::Filename, CommandLineParser::Attachment::AttachedOrSeparate,
           nullptr,
           CommandLineParser::FlagCategory::AssembledCode, "<file>",
           "Rename output file (default: <source>.bin)" },
    { "l", CommandLineParser::ValueKind::Filename, CommandLineParser::Attachment::AttachedOnly,
           "-",
           CommandLineParser::FlagCategory::Listing, "<file>",
           "Generate listing; alone, it goes to stdout" },
    { "v", CommandLineParser::ValueKind::None,     CommandLineParser::Attachment::AttachedOnly,
           nullptr,
           CommandLineParser::FlagCategory::General, "",
           "Verbose: an assembly summary on stderr" },

    //  Merlin asks the operator for a keyboard-input symbol and waits. A batch
    //  assembly has nobody to ask, so the answer has to arrive with the
    //  invocation -- and without this row the three vendor sources that ask
    //  questions cannot be assembled from a command line at all.
    { "d", CommandLineParser::ValueKind::SymbolDefinition, CommandLineParser::Attachment::AttachedOrSeparate,
           nullptr,
           CommandLineParser::FlagCategory::General, "<name>[=<value>]",
           "Define a symbol the source expects (defaults to 1)" },
};




//
//  The as65 grammar's flags, which is the whole of as65's switch set.
//
//  IT IS A TABLE FOR THE REASON MERLIN'S IS: the parser walks it and the help is
//  generated from it, so the tool cannot document a flag it does not take or
//  take one it does not document. as65's set resisted this while a row was a
//  single `char` -- `-s2` is not `-s` followed by `2` -- and stopped resisting
//  once the option became a string matched longest-first.
//
//  THE ORDER HERE IS THE ORDER THE HELP PRINTS, within each category. It is not
//  alphabetical on purpose: `-c`, `-l`, `-m` and `-p` all shape one listing and
//  alphabetical order put them four places apart.
//
//  `-i` AND `-n` ARE ROWS THAT DO NOTHING, and they are rows rather than
//  special cases in the walk because as65 accepts them: a command line carrying
//  either must not be refused. `-i` asks for case-insensitive opcodes, which
//  this assembler does unconditionally, so honoring it is already done. `-n` is
//  not implemented -- see the issue named in its description.
//
static constexpr CommandLineParser::DialectFlag  s_kAs65Flags[] =
{
    { "x",  CommandLineParser::ValueKind::None,     CommandLineParser::Attachment::AttachedOnly,
            nullptr,
            CommandLineParser::FlagCategory::AssembledCode, "",
            "Allow 65C02 instructions" },
    { "d",  CommandLineParser::ValueKind::SymbolDefinition, CommandLineParser::Attachment::AttachedOnly,
            "DEBUG=1",
            CommandLineParser::FlagCategory::AssembledCode, "<name>[=<value>]",
            "Define a symbol. If -d is specified alone, it defines DEBUG=1. If value is not given, it defaults to 1." },
    { "i",  CommandLineParser::ValueKind::None,     CommandLineParser::Attachment::AttachedOnly,
            nullptr,
            CommandLineParser::FlagCategory::AssembledCode, "",
            "Case-insensitive opcodes. Already the default; accepted as a no-op" },
    { "n",  CommandLineParser::ValueKind::None,     CommandLineParser::Attachment::AttachedOnly,
            nullptr,
            CommandLineParser::FlagCategory::AssembledCode, "",
            "Disable optimizations; NYI. Tracked at https://github.com/relmer/Casso/issues/118" },

    { "o",  CommandLineParser::ValueKind::Filename, CommandLineParser::Attachment::AttachedOrSeparate,
            nullptr,
            CommandLineParser::FlagCategory::AssembledCode, "<file>",
            "Where the assembled bytes go. Defaults to <source>.bin, or <source>.s19 under -s and <source>.hex under -s2" },
    { "s2", CommandLineParser::ValueKind::Filename, CommandLineParser::Attachment::AttachedOnly,
            "",
            CommandLineParser::FlagCategory::OutputFormat, "<file>",
            "Output Intel HEX format (.hex)" },
    { "s",  CommandLineParser::ValueKind::Filename, CommandLineParser::Attachment::AttachedOnly,
            "",
            CommandLineParser::FlagCategory::OutputFormat, "<file>",
            "Output S-record format (.s19)" },
    { "z",  CommandLineParser::ValueKind::None,     CommandLineParser::Attachment::AttachedOnly,
            nullptr,
            CommandLineParser::FlagCategory::OutputFormat, "",
            // No long option is named here. A description is printed verbatim
            // under whichever prefix the reader typed, so a `--flat` written
            // into one leaks a dashed option onto a slash-prefixed page.
            "Pad with $00 rather than $FF" },

    { "l",  CommandLineParser::ValueKind::Filename, CommandLineParser::Attachment::AttachedOnly,
            "-",
            CommandLineParser::FlagCategory::Listing, "<file>",
            "Generate listing (-l alone = stdout, -lprog.lst = to that file)" },
    { "p",  CommandLineParser::ValueKind::None,     CommandLineParser::Attachment::AttachedOnly,
            nullptr,
            CommandLineParser::FlagCategory::Listing, "",
            "Generate pass 1 listing" },
    { "c",  CommandLineParser::ValueKind::None,     CommandLineParser::Attachment::AttachedOnly,
            nullptr,
            CommandLineParser::FlagCategory::Listing, "",
            "Show cycle counts in listing" },
    { "m",  CommandLineParser::ValueKind::None,     CommandLineParser::Attachment::AttachedOnly,
            nullptr,
            CommandLineParser::FlagCategory::Listing, "",
            "Show macro expansions in listing" },
    { "h",  CommandLineParser::ValueKind::Number,   CommandLineParser::Attachment::AttachedOnly,
            nullptr,
            CommandLineParser::FlagCategory::Listing, "<lines>",
            "Page height for listing (-h0 = one continuous page, the default)" },
    { "w",  CommandLineParser::ValueKind::Number,   CommandLineParser::Attachment::AttachedOnly,
            "133",
            CommandLineParser::FlagCategory::Listing, "<width>",
            "Column width (default: 79, -w alone = 133)" },

    { "t",  CommandLineParser::ValueKind::None,     CommandLineParser::Attachment::AttachedOnly,
            nullptr,
            CommandLineParser::FlagCategory::Debug, "",
            "Generate symbol table" },
    { "g",  CommandLineParser::ValueKind::None,     CommandLineParser::Attachment::AttachedOnly,
            nullptr,
            CommandLineParser::FlagCategory::Debug, "",
            "Generate debug information file, <source>.dbg" },

    { "v",  CommandLineParser::ValueKind::None,     CommandLineParser::Attachment::AttachedOnly,
            nullptr,
            CommandLineParser::FlagCategory::General, "",
            "Verbose mode" },
    { "q",  CommandLineParser::ValueKind::None,     CommandLineParser::Attachment::AttachedOnly,
            nullptr,
            CommandLineParser::FlagCategory::General, "",
            "Quiet mode (suppress progress)" },
};


//
//  Every LONG option form, without a prefix, in the two grammars that have
//  any.
//
//  THESE TABLES EXIST TO MAKE `/` A REAL PREFIX RATHER THAN A PRINTED ONE. The
//  usage text writes every flag with whichever prefix the reader asked for, and
//  a help that offers `/out` while the parser takes only `--out` is worse than
//  one that never mentioned it. Single-letter flags already worked with either
//  prefix; only the long ones did not.
//
//  Matching against a table rather than rewriting any leading `/` is what keeps
//  a ProDOS path working: `/MOUSEPAINT/STARTUP` begins with a slash and is an
//  operand, not a flag, so only an argument that is exactly one of these words
//  is treated as one.
//
static constexpr const char *  s_kpszDiskOptions[] =
{
    "out",
    "as",
    "type",
    "addr",
    "text",
    "basic",
};


//  TWO ENTRIES, AND THE TABLE STILL EARNS ITS KEEP. `cpu` left it when `-x`
//  replaced `--cpu`, and what remains is not scaffolding around a pair: the
//  table is what stops the single-character normalization from reading `/flat`
//  as the concatenated flags -f -l -a -t. One entry would still need it.
static constexpr const char *  s_kpszAs65LongOptions[] =
{
    "flat",
    "dos-bin",
};


static constexpr const char *  s_kpszRunLongOptions[] =
{
    "load",
    "entry",
    "stop",
    "max-cycles",
    "reset-vector",
    "fill",
    "warn",
    "no-warn",
    "fatal-warnings",
};




//
//  Which dialect each flag table belongs to. Both have one now: as65's grammar
//  was a hand-rolled walk over a historical command line until the option
//  became a string matched longest-first, which is what `-s2` needed.
//
static constexpr CommandLineParser::DialectFlagTable  s_kDialectFlags[] =
{
    { DialectId::As65,   s_kAs65Flags,   std::size (s_kAs65Flags)   },
    { DialectId::Merlin, s_kMerlinFlags, std::size (s_kMerlinFlags) },
};


//
//  The output formats the merlin grammar names.
//
//  The DEFAULT stays the assembled bytes and nothing else. A Merlin source
//  names its own origin, so "the object" is what a developer asking for output
//  means, and that is what the subcommand has always written. These two rows
//  say what the bytes can be wrapped in instead.
//
//  --dos-bin is the one that closes a real gap rather than adding a
//  convenience. The 4-byte header carries the ORIGIN, and raw output throws it
//  away -- so a developer wrapping the bytes by hand has to already know an
//  address that usually comes from an ORG buried in the source. The assembler
//  knows it; nothing else reliably does.
//
//  as65 has no row. Its grammar is a hand-rolled walk that writes --raw and
//  --dos-bin inline, and its own usage block documents them, so a row here
//  would be a second description of a tool this table does not drive.
//
static constexpr CommandLineParser::OutputFormatFlag  s_kMerlinOutputFormats[] =
{
    { "--dos-bin", CommandLineOptions::OutputFormat::DosBinary,
                   "Write the bytes behind a 4-byte DOS 3.3 header" },
    { "--flat",    CommandLineOptions::OutputFormat::Binary,
                   "Write a full 64 KB image at the origin, padded with $FF" },
};


//  Which dialect an output-format table belongs to, on the same principle as the
//  flag tables above: a dialect offering no choice simply has no row.
//
//  The output formats the as65 grammar names.
//
//  THE DEFAULT IS THE ASSEMBLED BYTES and has no row, because a flag whose only
//  effect is to select the default earns a line in the help and buys no
//  capability -- which is why the `--raw` that used to name it is gone. These
//  two say what the bytes can be wrapped in instead.
//
//  `-s` and `-s2` are NOT here. They are switches of the concatenating grammar,
//  carry an optional attached filename, and live in the flag table above; a
//  format row is a whole word typed on its own.
//
static constexpr CommandLineParser::OutputFormatFlag  s_kAs65OutputFormats[] =
{
    { "--flat",    CommandLineOptions::OutputFormat::Binary,
                   "Write a full 64 KB image at the origin, padded with the fill byte" },
    { "--dos-bin", CommandLineOptions::OutputFormat::DosBinary,
                   "Write the bytes behind a 4-byte DOS 3.3 header (load address + length), ready to BLOAD" },
};


static constexpr CommandLineParser::OutputFormatTable  s_kOutputFormatTables[] =
{
    { DialectId::As65,   s_kAs65OutputFormats,   std::size (s_kAs65OutputFormats)   },
    { DialectId::Merlin, s_kMerlinOutputFormats, std::size (s_kMerlinOutputFormats) },
};


//  Source extensions tried, in order, for an input path with no extension.
static constexpr const char *  s_kpszSourceExtensions[] =
{
    ".a65",
    ".asm",
    ".s",
};





////////////////////////////////////////////////////////////////////////////////
//
//  IsHelpRequest
//
//  Whether one argument is the user asking for the usage text.
//
//  Every form the top level accepts is accepted here, because a reader who
//  learned `--help` from one command line will type it on the next one and a
//  subcommand that answers only its own form is a trap. The `/` forms are
//  included for the same reason the option tables carry them: the help writes
//  itself with whichever prefix was typed, so both prefixes have to work.
//
//  MATCHED EXACTLY AND IN LOWER CASE, which is what keeps a ProDOS path out of
//  it. `/HELP` is a legal volume path and stays an operand; only the lowercase
//  flag form a person types at a shell is read as a request.
//
//  `-h` IS SAFE HERE AND NOWHERE ELSE. It is the listing page height in the
//  assembler's flag walk, which is the only grammar that has such a thing; the
//  `run` and `disk` grammars this function judges have no page and no height,
//  so nothing else is competing for the two characters. A reader who learned
//  `-h` from any other command line types it here first.
//
//  THE ONE TOP-LEVEL FORM MISSING HERE IS A BARE `?`, and it is missing
//  because this function cannot judge it. as65 asks for a question mark that is
//  the ONLY parameter, and one argument on its own does not know whether it was
//  alone. IsLoneQuestionMark below is handed the whole tail and answers that.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::IsHelpRequest (const std::string & arg)
{
    return arg == "--help" || arg == "-help" || arg == "-?" || arg == "-h" ||
           arg == "/help"  || arg == "/?"    || arg == "/h";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineParser::IsLoneQuestionMark
//
//  Whether everything a subcommand was given is a single bare `?`.
//
//  as65 documents "Help message if only parameter is a question mark", and
//  keeps it unadorned: no dash, no slash. `CassoCli as65 ?` is that command
//  line now that assembling names its dialect, so the condition is measured
//  from the word after `as65` rather than from the program name.
//
//  THE "ONLY PARAMETER" HALF IS THE WHOLE SAFETY OF IT. A `?` with anything
//  beside it is an operand and stays one -- a DOS 3.3 catalog will hold a file
//  called `?` quite happily, so `disk get img.dsk ?` has to keep meaning what
//  it says. Alone, there is nothing for it to be an operand OF: no subcommand
//  here does anything useful with one argument that is not a filename, and a
//  filename is what `?` cannot be on this host.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::IsLoneQuestionMark (int argc, char * argv[], int startIndex)
{
    return (argc - startIndex) == 1 && std::string (argv[startIndex]) == "?";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineParser::ExitCodeForRefusal
//
//  What a command line this parser turned down reports to the shell.
//
//  IT IS NOT ONE NUMBER, and that is the whole reason this exists. Assembling
//  answers with as65's 1, "Incorrect parameter specified on the commandline",
//  because a script ported from as65 branches on it. `run` and `disk` have no
//  such status and fold a refusal into their 2 -- "nothing could be started"
//  and "nothing was done" -- which is the same claim in each grammar's words.
//
//  The executable used to return a flat 2 for every mode, so an as65 build
//  script testing for 1 never saw it and read "could not open a file" instead.
//
////////////////////////////////////////////////////////////////////////////////

int CommandLineParser::ExitCodeForRefusal (CommandLineOptions::Subcommand mode)
{
    bool  startedNothing = mode == CommandLineOptions::Subcommand::Run ||
                           mode == CommandLineOptions::Subcommand::Disk;



    return startedNothing ? kNothingStarted : As65ExitStatus::kBadCommandLine;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineParser::RoundToInstalledSize
//
//  Installed memory in gigabytes, as a person would say it.
//
//  WHAT THE OS REPORTS IS NOT WHAT IS FITTED. A machine with 32 GB in it
//  answers something under 32: the firmware reserves a slice for itself before
//  Windows ever counts, and the count is in bytes of 2^30 rather than the
//  round number on the box. Printing that verbatim gives "on your 31 GB
//  machine", which reads like a bug in the tool rather than a fact about the
//  machine.
//
//  So the raw figure is rounded UP to the next size somebody actually buys.
//  The ladder is doubling with the halfway steps that real modules produce
//  (12, 24, 48, 96), and past the end of it the number is used as it comes --
//  a host with more memory than this list anticipates is better served by an
//  honest odd number than by a wrong round one.
//
////////////////////////////////////////////////////////////////////////////////

unsigned CommandLineParser::RoundToInstalledSize (uint64_t bytes)
{
    constexpr uint64_t  kGigabyte = 1024ull * 1024ull * 1024ull;
    constexpr unsigned  kSizes[]  = { 1, 2, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 512, 1024 };
    //  Rounded UP, so anything past a boundary lands on the next size rather
    //  than the one below it: 32 GB fitted reports ~31.8 and must not become
    //  24. Integer division truncates, so the remainder is carried by hand.
    unsigned            whole     = (unsigned) (bytes / kGigabyte);
    unsigned            rounded   = whole + ((bytes % kGigabyte) != 0 ? 1 : 0);



    if (bytes == 0)
    {
        return 0;
    }

    for (unsigned size : kSizes)
    {
        if (rounded <= size)
        {
            return size;
        }
    }

    return rounded;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineParser::BuildAssembleExitCodes
//
//  The assembler's exit codes, with status 4 told properly.
//
//  4 IS as65'S OUT-OF-MEMORY AND CANNOT HAPPEN HERE. It belongs to a 16-bit
//  tool assembling out of a 640 KB real-mode heap; the line names the machine's
//  own memory because the size of the gap is the whole joke, and because a
//  reader who DOES somehow see a 4 should be told plainly that it is a bug.
//
//  Composed here rather than in the executable for the reason every other
//  claim in this header is: the test assembly links this library and not that
//  executable, so a sentence written beside the printing code is a sentence
//  nothing can check. The one thing this cannot do for itself is ask the OS how
//  much memory is fitted, so that number arrives as an argument.
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandLineParser::BuildAssembleExitCodes (unsigned installedGigabytes)
{
    std::string  machine = (installedGigabytes == 0)
                               ? std::string ("your machine")
                               : std::to_string (installedGigabytes) + " GB machine";
    std::string  text;



    text  = "    0  Assembled successfully\n";
    text += "    1  Bad command line\n";
    text += "    2  Error opening source or output file\n";
    text += "    3  Error assembling source file\n";
    text += "    4  Out of memory, says AS65, assembling your 64K binary. On your "
          + machine + ". Sure. If you run out of memory doing 6502 assembly, "
            "definitely open an issue, because something has gone *very* wrong :)\n";
    text += "    5  Assembled with warnings";

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LookUpDiskVerb
//
////////////////////////////////////////////////////////////////////////////////

CommandLineOptions::DiskOptions::Verb CommandLineParser::LookUpDiskVerb (const std::string & word)
{
    CommandLineOptions::DiskOptions::Verb  verb = CommandLineOptions::DiskOptions::Verb::None;
    size_t                                 i    = 0;



    for (i = 0; i < std::size (s_kDiskVerbs); i++)
    {
        if (word == s_kDiskVerbs[i].name)
        {
            verb = s_kDiskVerbs[i].verb;
            break;
        }
    }

    return verb;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPlainDecimal
//
//  Whether a word is nothing but decimal digits.
//
//  It is asked of an argument that has NO slot left to go in, and it is what
//  separates the two ways that happens. A bare word is usually a second
//  filename; a bare number is almost always a value somebody typed a space in
//  front of, because every value this assembler takes is glued.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::IsPlainDecimal (const std::string & text)
{
    bool  isDecimal = !text.empty();



    for (char c : text)
    {
        if (isdigit ((unsigned char) c) == 0)
        {
            isDecimal = false;
        }
    }

    return isDecimal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrailingParameterFlag
//
//  The letter a group of assembler flags ends on, when that letter is one which
//  takes a parameter -- and 0 otherwise.
//
//  This is what lets a surplus argument be diagnosed with the option it was
//  meant for rather than in the abstract. `casso prog.a65 -l listing.txt` leaves
//  `listing.txt` with nowhere to go, and the useful thing to say is not that it
//  is surplus but that `-llisting.txt` is the form that would have worked.
//
//  ONLY THE LETTERS WHOSE BARE FORM IS LEGAL ARE HERE. `-h` and `-o` also take
//  parameters and neither can ever be the argument standing in front of a
//  surplus one: a bare `-h` is refused outright, and a bare `-o` CONSUMES the
//  argument after it, so nothing is left over to be surplus. Listing them would
//  be an arm nothing reaches.
//
//  A `--` long option is excluded because none of this grammar's long options
//  take a value at all, so its last letter says nothing about what follows it.
//
////////////////////////////////////////////////////////////////////////////////

char CommandLineParser::TrailingParameterFlag (const std::string & previous)
{
    //  as65's own notations: -d<name>, -l<filename>, -s<n>, -w<width>.
    std::string_view  kTakesParameter = "dlsw";
    char              flag            = 0;
    bool              isFlag          = previous.size() >= 2 &&
                                        (previous[0] == '-' || previous[0] == '/');
    bool              isLong          = previous.rfind ("--", 0) == 0;



    if (isFlag && !isLong && kTakesParameter.find (previous.back()) != std::string_view::npos)
    {
        flag = previous.back();
    }

    return flag;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsShellSplitFragment
//
//  Whether `arg` is the back half of an argument a SHELL cut in two, with
//  `previous` as the front half.
//
//  PowerShell parses a token beginning with a single `-` as a parameter name,
//  and a parameter name may not contain a `.`. The token therefore ENDS at the
//  first dot and the remainder arrives as an argument of its own. MEASURED by
//  passing each of these to a native executable that prints its argv, under
//  PowerShell 7.6.5 and Windows PowerShell 5.1 alike:
//
//      -oprog.bin        ->  -oprog     .bin
//      -oprog.bin.x      ->  -oprog     .bin.x     (the FIRST dot, not the last)
//      -osub\x.bin       ->  -osub\x    .bin       (a separator does not cut)
//      -dVER=1.0         ->  -dVER=1    .0
//      -o.bin            ->  -o         .bin
//      -oC:\out\prog.bin ->  arrives whole
//      /oprog.bin        ->  arrives whole
//      --flat.x          ->  arrives whole
//      -h60  -oa_b  -oa-b  -oa1        ->  each arrives whole
//
//  THREE OF THOSE ARE IMMUNE AND IT IS WORTH KNOWING WHY. A `:` anywhere before
//  the first dot suppresses the split outright -- that is the `-name:value`
//  syntax -- so an absolute path glues fine while the relative name beside it
//  does not, which is what makes the failure look random to whoever meets it. A
//  `/` prefix and a `--` prefix are not parameter names at all and are handed
//  over untouched.
//
//  No other shell does any of this: cmd.exe, bash, make and any argument array
//  pass the whole token, which is why as65 parity is untouched by all of it.
//
//  THE SHELL IS NOT DETECTED, THE SHAPE IS. Reading the parent process would be
//  fragile, untestable, and wrong the moment the command line arrives from a
//  script. Every condition below is visible in argv itself:
//
//    the back half   begins with the `.` the cut was made at
//    the front half  is a single-dash flag group ENDING IN AN ATTACHED NAME.
//                    Only -o, -l, -d and -s take a string parameter; -h and -w
//                    take digits, and digits hold no dot
//    uncut           the front half itself contains neither a `.` nor a `:`.
//                    The cut is made at the FIRST dot and a colon prevents it
//                    entirely, so a front half carrying either was never cut
//
//  THAT LAST CONDITION IS WHAT KEEPS AN ORDINARY COMMAND LINE OUT. A relative
//  path written `./prog.a65` standing behind `-oout.bin` satisfies the first
//  two and is not a fragment at all -- and the front half says so, because
//  `-oout.bin` still carries the dot the shell would have cut at.
//
//  A FRONT HALF OF EXACTLY `-o` IS NOT ONE EITHER, and needs no diagnostic: the
//  flag now takes a separated value, so `-o.bin` and `-o..\out\p.bin` arrive as
//  two arguments and mean precisely what was typed.
//
//  NEITHER `run` NOR `disk` CAN PRODUCE THIS SHAPE, which is why neither of
//  them asks. Every value in those grammars is SEPARATED -- `--out prog.bin`,
//  `-o prog.bin` -- so a value is its own token, does not begin with `-`, and
//  is never a parameter name to be cut. The one way to reach the shape there is
//  to type a glued form neither grammar accepts, where the answer is not
//  "quote it" but "that flag takes its value separately" -- which is what those
//  grammars already say.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::IsShellSplitFragment (const std::string & previous,
                                              const std::string & arg)
{
    //  The as65 flags whose parameter is a STRING, which is to say a name or a
    //  path -- the only values that can carry a dot at all.
    std::string_view  kAttachesAName = "odls";
    bool              isBackHalf     = !arg.empty() && arg[0] == '.';
    bool              isFlagGroup    = previous.size() >= 3 &&
                                       previous[0] == '-' &&
                                       previous[1] != '-';
    size_t            attached       = std::string::npos;
    bool              attachesAName  = false;
    bool              isUncut        = false;



    if (isBackHalf && isFlagGroup)
    {
        attached      = previous.find_first_of (kAttachesAName, 1);
        attachesAName = attached != std::string::npos && attached + 1 < previous.size();
        isUncut       = previous.find_first_of (".:") == std::string::npos;
    }

    return isBackHalf && isFlagGroup && attachesAName && isUncut;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RejoinShellSplitArguments
//
//  The command line as it was typed, with the halves PowerShell cut apart put
//  back together.
//
//  THE WHOLE LIST IS WALKED RATHER THAN THE PAIR AT HAND, because which argument
//  ends up homeless depends on the order the user typed. `CassoCli prog.a65
//  -oprog.bin` leaves the back half `.bin` with nowhere to go; `CassoCli
//  -oprog.bin prog.a65` lets `.bin` fill the source-file slot and strands
//  `prog.a65` instead. One cut, surfacing at two different arguments, so the
//  repair looks for the cut itself and not for whichever argument failed.
//
//  ONE PASS, AND A JOINED ARGUMENT IS NOT RE-EXAMINED. A shell makes the cut at
//  the FIRST dot, so a joined argument carries one and can never be the front
//  half of another split -- IsShellSplitFragment requires a front half with no
//  dot in it. Feeding the result back through would therefore find nothing, and
//  looking anyway would mean a repair could depend on a previous repair, which
//  is a rule far harder to state than the one it would replace.
//
//  See CommandLineParser.h for why rejoining cannot change what an already-valid
//  command line means.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> CommandLineParser::RejoinShellSplitArguments (int argc, char * argv[])
{
    std::vector<std::string>  rejoined;



    for (int i = 0; i < argc; i++)
    {
        std::string  arg = argv[i];

        if (!rejoined.empty() && IsShellSplitFragment (rejoined.back(), arg))
        {
            rejoined.back() += arg;
        }
        else
        {
            rejoined.push_back (arg);
        }
    }

    return rejoined;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskOperandCount
//
//  How many positional operands a disk verb HAS A USE FOR.
//
//  It differs by verb and always has: `list` names a disk and nothing else,
//  while every other verb names a disk and a file. The count was never written
//  down, so the parser filled two slots for every verb and the verbs that read
//  only one discarded the other in silence -- `disk list img.dsk PROG` catalogs
//  the disk and never says that PROG went nowhere.
//
//  Zero means "do not enforce a count", which is the honest answer for the verb
//  that was not recognized and for a help request. An unknown verb is reported
//  by the runner in its own words, and preempting that with a complaint about
//  operand three would answer the wrong question.
//
////////////////////////////////////////////////////////////////////////////////

int CommandLineParser::DiskOperandCount (CommandLineOptions::DiskOptions::Verb verb)
{
    int  count = 0;



    switch (verb)
    {
    case CommandLineOptions::DiskOptions::Verb::List:
        count = 1;
        break;

    case CommandLineOptions::DiskOptions::Verb::Get:
    case CommandLineOptions::DiskOptions::Verb::Put:
    case CommandLineOptions::DiskOptions::Verb::Delete:
    case CommandLineOptions::DiskOptions::Verb::Boot:
        count = 2;
        break;

    default:
        count = 0;
        break;
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskVerbWord
//
//  The descriptive word a verb is written with, read from the table rather than
//  retyped so a diagnostic cannot name a verb the grammar no longer has. The
//  first row carrying a verb is its descriptive form; the rest are aliases.
//
////////////////////////////////////////////////////////////////////////////////

const char * CommandLineParser::DiskVerbWord (CommandLineOptions::DiskOptions::Verb verb)
{
    const char *  word = "disk";



    for (const DiskVerbName & entry : s_kDiskVerbs)
    {
        if (entry.verb == verb)
        {
            word = entry.name;
            break;
        }
    }

    return word;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDiskOptionNeedingValue
//
//  Whether an argument is one of the disk options that takes a value.
//
//  Asked only when such an option ended the command line with nothing after it.
//  Without this the argument fell into the unknown-option refusal, which told
//  the reader "unknown disk option: --addr" and then listed `--addr` among the
//  options to try instead -- a message that contradicts itself in two lines.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::IsDiskOptionNeedingValue (const std::string & arg)
{
    return arg == "--out" || arg == "--as" || arg == "--type" || arg == "--addr";
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsRunOptionNeedingValue
//
//  The same question for the `run` grammar, asked for the same reason: every
//  value-taking arm there declines an option that has nothing after it, and
//  what caught the leftovers was a complaint that the option does not exist.
//
//  The single-letter forms are written with a dash because the caller has
//  already normalized a leading slash to one.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::IsRunOptionNeedingValue (const std::string & arg)
{
    return arg == "-o"     || arg == "-l"     || arg == "--fill" ||
           arg == "--load" || arg == "--entry" || arg == "--stop" ||
           arg == "--max-cycles";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CanonicalLongFlag
//
//  An argument reduced to the one form the grammars below test for, so
//  `/out` and `--out` reach the same arm.
//
//  ONLY AN EXACT OPTION NAME IS REWRITTEN. A ProDOS path is `/VOLUME/FILE` and
//  is an operand; rewriting every leading slash would turn one into a flag and
//  lose it. Anything not in the table comes back untouched, which is what lets
//  a caller pass one string through for both flags and positionals.
//
//  An attached value is carried across, because a long option may be written
//  `--name=value` and `/name=value` therefore has to be one too. The name is
//  matched against the part BEFORE the `=` for exactly that reason.
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandLineParser::CanonicalLongFlag (const std::string             & arg,
                                                  std::span<const char * const>   names)
{
    std::string  canonical = arg;
    size_t       equals    = 0;
    std::string  word;



    if (arg.size() < 2 || arg[0] != '/')
    {
        return canonical;
    }

    equals = arg.find ('=');
    word   = (equals == std::string::npos) ? arg.substr (1)
                                           : arg.substr (1, equals - 1);

    for (const char * name : names)
    {
        if (word == name)
        {
            canonical = "--" + arg.substr (1);
            break;
        }
    }

    return canonical;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CanonicalDiskFlag
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandLineParser::CanonicalDiskFlag (const std::string & arg)
{
    return CanonicalLongFlag (arg, std::span<const char * const> (s_kpszDiskOptions));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseDiskOptions
//
//  Grammar, positional then flagged:
//
//      disk list   <image>
//      disk get    <image> <path> [--out <file>] [--text | --basic]
//      disk put    <image> <file> [--as <path>] [--type <t>] [--addr $XXXX]
//                                 [--text | --basic]
//      disk delete <image> <path>
//      disk boot   <image> <path>
//
//  Positional meaning depends on the verb: `put` names a HOST file to place and
//  optionally renames it with --as, while every other verb names a file already
//  on the disk. That asymmetry is inherent -- put is the only verb whose second
//  operand lives on the host -- so it is spelled out rather than smoothed over.
//
//  Each option is also accepted with a `/` prefix, because the usage text
//  writes every flag with whichever prefix the reader asked for and offering a
//  form the parser rejects is worse than never offering it. See
//  CanonicalDiskFlag for why that is a table lookup and not a rewrite of any
//  leading slash.
//
//  Parsing does not validate that required operands are present. That is the
//  runner's job, because a missing operand needs a message naming what was
//  expected, and the parser has no way to report one.
//
//  AN ARGUMENT THAT LOOKS LIKE A FLAG AND IS NOT ONE IS REFUSED HERE, though,
//  because the alternative is not a missing operand -- it is an extra one. This
//  grammar has no positional past the second, so `disk get img F -o host.bin`
//  used to put `-o` and `host.bin` in slots nothing reads: the file went to
//  standard output, the name the caller gave was dropped, and the exit status
//  said it had all worked. A flag it does not have is now a refusal, and the
//  suggestion names the flags it does have.
//
//  AN EXTRA OPERAND IS REFUSED ON THE SAME GROUND, and the count comes from the
//  VERB -- see DiskOperandCount. Two slots were filled whatever the verb, so
//  `disk list img.dsk PROG` filled a slot `list` does not read and `disk get
//  img.dsk PROG extra` filled none at all; both exited 0 having said nothing.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::ParseDiskOptions (
    int                   argc,
    char               *  argv[],
    int                   argIndex,
    CommandLineOptions &  options)
{
    int   i          = argIndex;
    int   positional = 0;
    int   limit      = 0;
    bool  wantsHelp  = false;



    //  A help request anywhere in the disk arguments wins outright, and is
    //  looked for BEFORE the verb: `disk --help` would otherwise offer
    //  `--help` to the verb table, be told it is not a verb, and answer a
    //  request for the grammar with a complaint about the grammar.
    //  The lone `?` is looked for the other way round -- across the whole tail
    //  at once rather than one argument at a time -- because it is a request
    //  only when nothing else was typed. A catalog will hold a file called `?`,
    //  so `disk get img.dsk ?` has to keep meaning what it says.
    wantsHelp = IsLoneQuestionMark (argc, argv, argIndex);

    for (int probe = argIndex; probe < argc; probe++)
    {
        if (IsHelpRequest (argv[probe]))
        {
            wantsHelp = true;

            if (argv[probe][0] == '/')
            {
                options.flagPrefix = '/';
            }
        }
    }

    if (wantsHelp)
    {
        options.disk.verb = CommandLineOptions::DiskOptions::Verb::Help;
        return;
    }

    if (i < argc)
    {
        options.disk.verb = LookUpDiskVerb (argv[i]);
        i++;
    }

    limit = DiskOperandCount (options.disk.verb);

    for ( ; i < argc; i++)
    {
        std::string  arg      = CanonicalDiskFlag (argv[i]);
        bool         hasValue = (i + 1) < argc;

        if (arg == "--text")
        {
            options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;
            continue;
        }

        if (arg == "--basic")
        {
            options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;
            continue;
        }

        if (arg == "--out" && hasValue)
        {
            options.disk.hostFile = argv[i + 1];
            i++;
            continue;
        }

        if (arg == "--as" && hasValue)
        {
            options.disk.path = argv[i + 1];
            i++;
            continue;
        }

        if (arg == "--type" && hasValue)
        {
            options.disk.typeName = argv[i + 1];
            i++;
            continue;
        }

        //  AN ADDRESS THAT COULD NOT BE READ IS REFUSED, NOT DROPPED. It was
        //  dropped, and the result was a message that contradicted the command
        //  line it was answering: `disk put img prog.bin --addr zzz` said "is a
        //  binary, which has to be told where it loads -- give --addr $XXXX" to
        //  somebody who had just given --addr. The value they typed was gone,
        //  so the runner saw a command line with no address on it at all.
        //
        //  This is the rule the rest of the tool already states -- Refused
        //  covers "a value that could not be read" -- and `run` applies it to
        //  every address it takes. This one option was the exception.
        if (arg == "--addr" && hasValue)
        {
            Word     address = 0;
            HRESULT  hr      = ParseAddress (argv[i + 1], address);

            if (SUCCEEDED (hr))
            {
                options.disk.loadAddress    = address;
                options.disk.hasLoadAddress = true;
            }
            else
            {
                std::cerr << "Error: " << argv[i + 1]
                          << " is not an address. Write it as $XXXX\n";

                options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            }

            i++;
            continue;
        }

        //  AN OPTION THAT RAN OUT OF COMMAND LINE IS NOT AN UNKNOWN ONE. Every
        //  arm above needs a value and so declines the argument when there is
        //  none left, which used to drop it into the refusal below -- reporting
        //  "unknown disk option: --addr" and then listing `--addr` among the
        //  options to try instead.
        if (IsDiskOptionNeedingValue (arg))
        {
            std::cerr << "Error: " << argv[i] << " needs a value after it\n";

            options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            continue;
        }

        //  A DASH INTRODUCES A FLAG AND NOTHING ELSE, so one this grammar does
        //  not have is refused rather than counted as an operand.
        //
        //  Only a dash. A ProDOS path is written `/VOLUME/FILE` and is an
        //  operand, which is the same reason CanonicalDiskFlag matches a table
        //  instead of rewriting every leading slash -- so a slash that reached
        //  here is a path, and a path is exactly what the positional block
        //  below is for.
        if (arg.size() > 1 && arg[0] == '-')
        {
            std::cerr << "Error: unknown disk option: " << argv[i]
                      << ". Try: " << DescribeDiskOptions() << "\n";

            options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            continue;
        }

        //  AN OPERAND THE VERB HAS NO SLOT FOR IS REFUSED, and the count is the
        //  verb's own -- `list` names a disk, everything else names a disk and
        //  a file. Two slots were filled for every verb regardless, so the
        //  verbs that read one discarded the other without a word: `disk list
        //  img.dsk PROG` catalogs the whole disk and never mentions PROG, and
        //  `disk get img.dsk PROG extra` extracts PROG and never mentions
        //  extra. Both exited 0.
        //
        //  A verb the table did not recognize is left alone, count zero. The
        //  runner reports that in its own words, and a complaint about operand
        //  three would answer a question nobody asked.
        if (limit > 0 && positional >= limit)
        {
            std::cerr << "Error: surplus argument: " << arg << "\n"
                      << "       `disk " << DiskVerbWord (options.disk.verb) << "` takes "
                      << (limit == 1 ? "the image and nothing else.\n"
                                     : "the image and one file.\n");

            options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            positional++;
            continue;
        }

        // Anything else is positional. The first is always the image.
        if (positional == 0)
        {
            options.disk.imagePath = arg;
        }
        else if (positional == 1)
        {
            // `put` takes a host file here; every other verb takes a path on
            // the disk. --as may override the on-disk name afterwards.
            if (options.disk.verb == CommandLineOptions::DiskOptions::Verb::Put)
            {
                options.disk.hostFile = arg;
            }
            else
            {
                options.disk.path = arg;
            }
        }

        positional++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAllSubcommands
//
////////////////////////////////////////////////////////////////////////////////

std::span<const CommandLineParser::SubcommandName> CommandLineParser::GetAllSubcommands()
{
    return std::span<const SubcommandName> (s_kSubcommands);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAllDiskVerbs
//
//  Every verb the disk grammar accepts, aliases included, so a test can sweep
//  the whole table rather than a hand-picked sample. What it is for is the help
//  output: a verb added here and not described there is a capability the user
//  cannot find, and only a sweep of this table can notice.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const CommandLineParser::DiskVerbName> CommandLineParser::GetAllDiskVerbs()
{
    return std::span<const DiskVerbName> (s_kDiskVerbs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DescribeDiskOptions
//
//  The disk options read out of the table that defines them, so the suggestion
//  a refused argument earns cannot fall behind the grammar.
//
//  Written with `--` regardless of what the caller typed. Both prefixes are
//  accepted, and the refusal is already telling the reader they got the option
//  wrong -- offering it back in the prefix they just mistyped would suggest the
//  prefix was the mistake.
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandLineParser::DescribeDiskOptions()
{
    std::string  text;



    for (const char * option : s_kpszDiskOptions)
    {
        if (!text.empty())
        {
            text += ", ";
        }

        text += "--";
        text += option;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DescribeCategory
//
//  The heading one category of flags prints under.
//
//  Here rather than at the printing edge so every caller heads a group the same
//  way, and so adding a category is a case here rather than a search for
//  whoever wrote the headings.
//
////////////////////////////////////////////////////////////////////////////////

const char * CommandLineParser::DescribeCategory (FlagCategory category)
{
    const char *  heading = "General:";



    switch (category)
    {
        case FlagCategory::AssembledCode:  heading = "Assembled code:";  break;
        case FlagCategory::OutputFormat:   heading = "Output formats (mutually exclusive):";  break;
        case FlagCategory::Listing:        heading = "Listing:";         break;
        case FlagCategory::Debug:          heading = "Debug:";           break;
        case FlagCategory::General:        heading = "General:";         break;
    }

    return heading;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetFlags
//
//  A dialect's own flags, or none.
//
//  A lookup rather than a test on the dialect, so nothing here has to be edited
//  when a dialect is added -- and so a dialect whose grammar is not a table is
//  described by the absence of a row rather than by a special case.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const CommandLineParser::DialectFlag> CommandLineParser::GetFlags (DialectId dialect)
{
    std::span<const DialectFlag>  flags;



    for (const DialectFlagTable & table : s_kDialectFlags)
    {
        if (table.dialect == dialect)
        {
            flags = std::span<const DialectFlag> (table.flags, table.count);
            break;
        }
    }

    return flags;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineParser::GetOutputFormats
//
////////////////////////////////////////////////////////////////////////////////

std::span<const CommandLineParser::OutputFormatFlag> CommandLineParser::GetOutputFormats (DialectId dialect)
{
    std::span<const OutputFormatFlag>  formats;



    for (const OutputFormatTable & table : s_kOutputFormatTables)
    {
        if (table.dialect == dialect)
        {
            formats = std::span<const OutputFormatFlag> (table.formats, table.count);
            break;
        }
    }

    return formats;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineParser::ApplyOutputFormat
//
//  Selects the output format an argument names, if it names one.
//
//  Returns whether the argument was consumed, so a grammar that offers no
//  formats -- an empty table -- consumes nothing and its parse is unchanged.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::SelectOutputFormat (const std::string & flag,
                                            CommandLineOptions::OutputFormat format,
                                            CommandLineOptions & options)
{
    bool  alreadyChosen = !options.outputFormatFlag.empty();
    bool  disagrees     = alreadyChosen && options.outputFormat != format;



    // The same flag twice is not a conflict -- it asks for one thing, twice.
    if (disagrees && options.outputFormatConflict.empty())
    {
        options.outputFormatConflict = "Only one output format is allowed; "
                                     + options.outputFormatFlag + " and " + flag + " were both given.";
    }

    if (!alreadyChosen)
    {
        options.outputFormat     = format;
        options.outputFormatFlag = flag;
    }

    //  RECORDED IN BOTH FORMS BECAUSE TWO QUESTIONS ARE ASKED OF IT. The flag's
    //  own text is what a conflict message names; whether ANY format was named
    //  is what decides the extension fallback, and it is asked where the flag
    //  that chose it is of no interest. Setting one and not the other left the
    //  fallback claiming an output name a format flag had already claimed.
    options.outputFormatNamed = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineParser::ApplyOutputFormat
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::ApplyOutputFormat (const std::string & arg, DialectId dialect, CommandLineOptions & options)
{
    bool  matched = false;



    for (const OutputFormatFlag & format : GetOutputFormats (dialect))
    {
        if (IsLongOption (arg, format.option, options))
        {
            SelectOutputFormat (FormatLongOption (format.option, options.flagPrefix), format.format, options);
            matched = true;
            break;
        }
    }

    return matched;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineParser::NoteFlagPrefix
//
//  The FIRST prefix wins, which is the whole point of recording it separately
//  from the default.
//
//  A command line mixing the two prefixes is a typo, not a request, and the
//  only wrong answer is to echo back a prefix the user never typed. Taking
//  the first means the answer depends on how the invocation opens rather than
//  on which flag happens to sit last -- an order nobody thinks of as ordered.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::NoteFlagPrefix (char prefix, CommandLineOptions & options)
{
    if (!options.flagPrefixSeen)
    {
        options.flagPrefix     = prefix;
        options.flagPrefixSeen = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineParser::FormatLongOption
//
//  `--name` becomes `/name` on a slash command line.
//
//  One slash, not two: `//name` is nobody's convention, and the Windows form of
//  a long option has always been a single slash ahead of the whole word.
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandLineParser::FormatLongOption (const std::string & canonical, char flagPrefix)
{
    std::string  bare    = canonical;
    std::string  written = canonical;



    while (!bare.empty() && bare[0] == '-')
    {
        bare.erase (0, 1);
    }

    if (flagPrefix == '/')
    {
        written = std::string ("/") + bare;
    }

    return written;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineParser::IsLongOption
//
//  Matches a long option in either form, so the parser accepts what the
//  help text advertises.
//
//  This exists because the slash form used to fall through to the LETTER loop,
//  where it did not fail -- it did something else. `/raw` became `-raw`, warned
//  about an unknown `-r` and `-a`, and wrote the 64 KB image the flag was asked
//  to suppress; `/dos-bin` became `-dos-bin`, where `-d` swallowed `os-bin` as
//  a symbol definition and no warning appeared at all. Both produced the wrong
//  file and reported success.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::IsLongOption (const std::string & arg, const std::string & canonical,
                                      CommandLineOptions & options)
{
    bool  matched = (arg == canonical);



    if (!matched && !arg.empty() && arg[0] == '/')
    {
        matched = (arg == FormatLongOption (canonical, '/'));
    }

    if (matched && !arg.empty())
    {
        NoteFlagPrefix (arg[0] == '/' ? '/' : '-', options);
    }

    return matched;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineParser::IsLongOptionWithValue
//
//  The `=value` form of the above, for options that accept one attached.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::IsLongOptionWithValue (const std::string & arg, const std::string & canonical,
                                               std::string & value, CommandLineOptions & options)
{
    std::string  dashed  = canonical + "=";
    std::string  slashed = FormatLongOption (canonical, '/') + "=";
    bool         matched = false;



    if (arg.rfind (dashed, 0) == 0)
    {
        value   = arg.substr (dashed.size());
        matched = true;
    }
    else if (arg.rfind (slashed, 0) == 0)
    {
        value   = arg.substr (slashed.size());
        matched = true;
    }

    if (matched)
    {
        NoteFlagPrefix (arg[0] == '/' ? '/' : '-', options);
    }

    return matched;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseBoundedHex
//
//  Shared by ParseAddress and ParseFillByte, which differ only in their upper
//  bound. Accepts an optional `$` prefix and requires the whole string to be
//  consumed, so "12zz" is rejected rather than silently read as $12.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CommandLineParser::ParseBoundedHex (const char * text, long maxValue, long & outValue)
{
    HRESULT       hr      = S_OK;
    const char *  hex     = text;
    char       *  end     = nullptr;
    long          value   = 0;
    bool          isValid = false;



    CBREx (text != nullptr, E_INVALIDARG);
    CBREx (text[0] != '\0', E_INVALIDARG);

    if (hex[0] == '$')
    {
        hex++;
    }

    value   = strtol (hex, &end, 16);
    isValid = end != hex && *end == '\0' && value >= 0 && value <= maxValue;
    CBREx (isValid, E_INVALIDARG);

    outValue = value;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseAddress
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CommandLineParser::ParseAddress (const char * text, Word & address)
{
    HRESULT  hr    = S_OK;
    long     value = 0;



    hr = ParseBoundedHex (text, 0xFFFF, value);
    CHR (hr);

    address = (Word) value;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseDecimal
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CommandLineParser::ParseDecimal (const char * text, uint32_t & value)
{
    HRESULT  hr      = S_OK;
    char *   end     = nullptr;
    long     val     = 0;
    bool     isValid = false;



    CBREx (text != nullptr, E_INVALIDARG);
    CBREx (text[0] != '\0', E_INVALIDARG);

    val     = strtol (text, &end, 10);
    isValid = end != text && *end == '\0' && val >= 0;
    CBREx (isValid, E_INVALIDARG);

    value = (uint32_t) val;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseFillByte
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CommandLineParser::ParseFillByte (const char * text, Byte & fillByte)
{
    HRESULT  hr    = S_OK;
    long     value = 0;



    hr = ParseBoundedHex (text, 0xFF, value);
    CHR (hr);

    fillByte = (Byte) value;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndsWith
//
//  Case-insensitive suffix test, used for file-extension matching.
//
//  Case-insensitive because these are Windows paths: a user typing FOO.ASM and
//  a makefile emitting foo.asm name the same file, and a case-sensitive test
//  would silently classify one of them as having no recognized extension.
//
//  Both sides are lowered rather than assuming the caller passes a lowercase
//  suffix, so the function is correct regardless of how the call site writes
//  its literal.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::EndsWith (const std::string & str, const std::string & suffix)
{
    std::string  strEnd;
    std::string  suffLower = suffix;
    bool         fits      = suffix.size() <= str.size();
    bool         matches   = false;



    if (fits)
    {
        strEnd = str.substr (str.size() - suffix.size());

        for (auto & c : strEnd)
        {
            c = (char) tolower ((unsigned char) c);
        }

        for (auto & c : suffLower)
        {
            c = (char) tolower ((unsigned char) c);
        }

        matches = strEnd == suffLower;
    }

    return matches;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryAutoExtend
//
//  Resolves an extensionless source path by trying the common assembler
//  extensions in order, so `casso build` finds build.a65 the way as65 does.
//
//  "Has an extension" is decided against the last path SEPARATOR, not just the
//  last dot. A dot before the final separator belongs to a directory name --
//  `src/v1.2/build` has no extension despite containing a dot -- and testing
//  for a bare dot would leave that path unresolved.
//
//  Existence decides the match, so a path that already resolves is returned
//  untouched and a name with no candidate on disk is returned unchanged for
//  the caller to report against the name the user actually typed.
//
//  The existence check is injected. The parser is otherwise pure, and keeping
//  the one filesystem question behind a caller-supplied predicate is what
//  lets the whole grammar be unit-tested; an absent predicate simply means
//  nothing can be found, so the path comes back as typed.
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandLineParser::TryAutoExtend (const std::string & path, const FileExistsFn & fileExists)
{
    std::string  result    = path;
    std::string  candidate;
    size_t       dot       = path.rfind ('.');
    size_t       sep       = path.find_last_of ("/\\");
    bool         hasProbe  = (bool) fileExists;
    bool         hasExt    = false;
    bool         found     = false;



    // A dot after the last separator is an extension; a dot before it belongs
    // to a directory name and does not count.
    hasExt = dot != std::string::npos && (sep == std::string::npos || dot > sep);

    for (const char * extension : s_kpszSourceExtensions)
    {
        if (hasExt || found || !hasProbe)
        {
            break;
        }

        candidate = path + extension;

        if (fileExists (candidate))
        {
            result = candidate;
            found  = true;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripExtension
//
////////////////////////////////////////////////////////////////////////////////

std::string CommandLineParser::StripExtension (const std::string & path)
{
    std::string  result = path;
    size_t       dot    = path.rfind ('.');
    size_t       sep    = path.find_last_of ("/\\");
    bool         hasExt = false;



    // See TryAutoExtend: only a dot after the last separator is an extension.
    hasExt = dot != std::string::npos && (sep == std::string::npos || dot > sep);

    if (hasExt)
    {
        result = path.substr (0, dot);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsAssemblySource
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::IsAssemblySource (const std::string & path)
{
    return EndsWith (path, ".asm") || EndsWith (path, ".s") ||
           EndsWith (path, ".a65") || EndsWith (path, ".a65c");
}





////////////////////////////////////////////////////////////////////////////////
//
//  LookUpSubcommand
//
//  Resolves a bare word to a subcommand, or None when the word is not one.
//
//  None used to mean "a source filename, so assemble it" -- the fallback that
//  let `CassoCli input.a65` work. It now means exactly what it says, and the
//  caller reports it, because inferring a dialect from the absence of a word
//  is the guess this feature exists to remove.
//
////////////////////////////////////////////////////////////////////////////////

CommandLineOptions::Subcommand CommandLineParser::LookUpSubcommand (const std::string & word)
{
    CommandLineOptions::Subcommand  token = CommandLineOptions::Subcommand::None;



    for (const SubcommandName & entry : s_kSubcommands)
    {
        if (word == entry.name)
        {
            token = entry.token;
            break;
        }
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseAs65Flags
//
//  Parses the AS65-compatible command line, which is not a modern one and
//  cannot be handled by a modern parser.
//
//  Three period conventions have to be honored together:
//
//    concatenation   flags pack into one argument, so `-lsc` is three flags,
//                    not an unknown flag named "lsc"
//    prefix parity   `/` and `-` both introduce a flag. The prefix the user
//                    chose is REMEMBERED in flagPrefix so the usage text comes
//                    back written the way they type
//    attached values a flag's argument is GLUED to it, and how much of the
//                    argument it takes depends on the KIND of parameter. as65:
//                    "no other option can follow one that may have a string
//                    parameter. Other options can follow one that has a numeric
//                    parameter." So -d and -o take the rest of the argument,
//                    while -h and -w take only their digits and hand back what
//                    follows -- `-h80t` is `-h80 -t`
//                    back using the prefix they type
//    attached values a flag's argument may be glued to it or separated
//
//  Which is why this is a hand-rolled walk rather than a table: a table-driven
//  parser would have to encode all three exceptions anyway, and every one of
//  them is about matching a specific historical tool.
//
//  CONCATENATION IS CONFINED TO THIS FUNCTION and belongs to as65 alone. It is
//  the reason `-lsc` is three flags rather than one unknown one, and it is also
//  the reason `-o` swallows the rest of its own argument -- neither is a
//  property anyone would design in today. `run` and `disk` are modern grammars
//  parsed elsewhere; neither packs, and the help says so under Assembly options
//  rather than as a claim about the tool.
//
//  ONE FLAG ALSO READS THE ARGUMENT AFTER ITS OWN, and exactly one: `-o` takes
//  a separated filename as well as a glued one. That is more than as65 accepts
//  and never less, and it is confined to the flag with no bare form to be
//  ambiguous with -- the arm says why at length.
//
//  The stop flag ends parsing outright for a help request, a withdrawn or
//  unknown `--` option, an -o with nothing at all after it, or a single letter
//  this grammar does not have, so no later argument can quietly undo the
//  decision. Only the last of those sets showHelp, which is what puts the usage
//  page on the screen; the others answer by name instead -- see each refusal
//  for why.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::ParseAs65Flags (int argc, char * argv[], int startIndex, CommandLineOptions & options)
{
    int   argIndex = startIndex;
    // Set when an argument ends parsing outright -- a help request. It leaves
    // showHelp set, so the caller prints usage.
    bool  stop     = false;
    // A CPU flag the active dialect's profile does not accept. It ends parsing
    // too, but leaves showHelp clear: the refusal explains itself.
    bool  refused  = false;



    options.subcommand       = CommandLineOptions::Subcommand::As65;
    options.dialect          = DialectId::As65;
    options.dialectSelection = DialectSelection::Stated;



    while (argIndex < argc && !stop)
    {
        std::string arg (argv[argIndex]);
        std::string attachedValue;

        //  A help request under `as65` asks for THE ASSEMBLER'S page, not the
        //  general one. The reader has already said which grammar they are in
        //  by typing the subcommand, and answering with a table of contents
        //  they have finished reading is a page they have to navigate twice.
        //
        //  `-h` IS NOT ON THIS LIST, and IsHelpRequest is therefore the wrong
        //  question to ask here. Every other grammar may treat a bare `-h` as
        //  help; this one cannot, because `-h<lines>` is as65's page height and
        //  the flag walk below owns it.
        //
        //  The unadorned `?` is on it, and is as65's own: this is the grammar
        //  that documents it, and `CassoCli as65 ?` is where an as65 user types
        //  it now that assembling names its dialect. Measured from startIndex
        //  rather than from the walk's own position, because "only parameter"
        //  is the whole condition: `as65 prog.a65 ?` has a surplus argument to
        //  complain about and is not a request for anything.
        if (arg == "--help" || arg == "-help" || arg == "-?" || arg == "/?" || arg == "/help" ||
            IsLoneQuestionMark (argc, argv, startIndex))
        {
            if (arg[0] == '/')
            {
                NoteFlagPrefix ('/', options);
            }

            options.showHelp = true;
            options.helpPage = CommandLineOptions::HelpPage::Assemble;
            stop             = true;
            continue;
        }

        // Long options selecting a binary output FORMAT.
        //
        // THE DEFAULT IS THE ASSEMBLED BYTES, which is what `--raw` used to
        // name. A flag for the default is a flag that does nothing, so it was
        // withdrawn and `--flat` asks for the full 64 KB image instead -- the
        // shape a ROM burner or a byte-for-byte comparison wants, and the one
        // thing the default cannot give you. Merlin's table already reads this
        // way; this is as65 agreeing with it.
        if (IsLongOption (arg, "--flat", options))
        {
            SelectOutputFormat (FormatLongOption ("--flat", options.flagPrefix), CommandLineOptions::OutputFormat::Binary, options);
            argIndex++;
            continue;
        }

        if (IsLongOption (arg, "--dos-bin", options))
        {
            SelectOutputFormat (FormatLongOption ("--dos-bin", options.flagPrefix), CommandLineOptions::OutputFormat::DosBinary, options);
            argIndex++;
            continue;
        }

        //  A `--` OPTION THIS GRAMMAR DOES NOT HAVE IS REFUSED, NOT WALKED.
        //
        //  Everything below reads a single dash as a group of letters, so
        //  `--out` reaching it becomes `-o` with the filename `ut` -- the tool
        //  writing a file called `ut` and reporting success, for a flag it does
        //  not have. `disk`'s options are the ones a user is most likely to try
        //  here, which is exactly how that was found.
        //
        //  The two it DOES have were matched above, so anything still carrying
        //  `--` at this point is unknown by definition. A single dash is left
        //  alone: that is as65's own concatenated form and the walk owns it.
        //
        //  `--cpu` IS ANSWERED BY NAME FIRST, in both prefixes. Command lines
        //  carrying it already exist, and `/cpu` left to the walk below is read
        //  as `-c -p -u` -- a true reading of as65's grammar and a useless
        //  answer to somebody migrating off the flag. Both branches withdrew it
        //  in favor of as65's own `-x`, so this points there rather than
        //  reporting an unknown option.
        if (arg == "--cpu" || arg.rfind ("--cpu=", 0) == 0 ||
            arg == "/cpu"  || arg.rfind ("/cpu=",  0) == 0)
        {
            const char *  longPrefix = (arg[0] == '/') ? "/" : "--";

            if (arg[0] == '/')
            {
                NoteFlagPrefix ('/', options);
            }

            std::cerr << "Error: " << longPrefix << "cpu is gone. Use "
                      << options.flagPrefix << "x for the 65C02.\n"
                      << "       " << options.flagPrefix
                      << "x is AS65's own name for the switch, and selects the same\n"
                      << "       instruction set. The default is still a strict 6502.\n";

            options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            stop                 = true;
            continue;
        }

        if (arg.rfind ("--", 0) == 0)
        {
            NoteFlagPrefix ('-', options);
            RecordUnrecognizedFlag (arg, options);
            argIndex++;
            continue;
        }

        // Normalize / prefix to - for flag parsing, recording which prefix the
        // invocation opened with on the way past.
        if (arg[0] == '/')
        {
            NoteFlagPrefix ('/', options);
            arg[0] = '-';
        }
        else if (arg[0] == '-')
        {
            NoteFlagPrefix ('-', options);
        }

        //  A NON-FLAG ARGUMENT IS THE SOURCE FILE, AND A SECOND ONE IS AN
        //  ERROR RATHER THAN LITTER. It used to be dropped in silence, so
        //  `CassoCli as65 a.a65 b.a65` assembled a.a65, never mentioned
        //  b.a65, and exited 0 -- and the likeliest way to type it is a
        //  value with a space in front of it, which this grammar attaches.
        //  So the message names the glued form when it can see one.
        if (arg[0] != '-' && arg[0] != '/')
        {
            std::string  previous   = (argIndex > 1) ? argv[argIndex - 1] : "";
            char         wantsValue = TrailingParameterFlag (previous);

            if (options.inputFile.empty())
            {
                options.inputFile = arg;
                argIndex++;
                continue;
            }

            std::cerr << "Error: surplus argument: " << arg << "\n";
            std::cerr << "       Assembling takes one source file, and "
                      << options.inputFile << " is already it.\n";

            if (wantsValue != 0)
            {
                std::cerr << "       If " << arg << " was meant as a value, AS65 glues it to its option:\n"
                          << "       " << previous << arg << ", not " << previous << " " << arg << ".\n";
            }
            else if (IsPlainDecimal (arg))
            {
                std::cerr << "       If " << arg << " was meant as a value, AS65 glues every value to its\n"
                          << "       option, with no space between them.\n";
            }

            options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            stop                 = true;
            continue;
        }

        //  THE FLAG WALK READS THE TABLE. Every rule it applies is a column of
        //  s_kAs65Flags rather than a case of its own: which option matched,
        //  what kind of value it takes, whether that value may be separated,
        //  and what a bare one means. A flag is added by adding a row.
        //
        //  MATCHING IS LONGEST-FIRST, which is what makes `-s2` an option
        //  rather than `-s` carrying the filename `2`. See MatchFlag.
        //
        //  A NUMBER LEAVES THE GROUP OPEN AND A NAME CLOSES IT, which is as65's
        //  own rule -- "no other option can follow one that may have a string
        //  parameter" -- so `-h80t` is `-h80 -t` while `-lfile` is one flag and
        //  its filename. Deriving it from ValueKind is what stopped every arm
        //  from having to remember it, which is how `-g` came to eat a filename
        //  it does not take.
        size_t  pos = 1;

        while (pos < arg.size() && !stop)
        {
            size_t                matched = 0;
            const DialectFlag  *  flag    = MatchFlag (DialectId::As65, arg, pos, matched);
            std::string           rest;
            std::string           value;
            bool                  refused = false;

            if (flag == nullptr)
            {
                //  as65's DIAGNOSTICS: usage is printed "if an illegal option
                //  has been specified", and nothing after it is acted on.
                RecordUnrecognizedFlag (std::string (1, options.flagPrefix) + arg[pos], options);
                stop = true;
                break;
            }

            rest = arg.substr (pos + matched);

            if (flag->value == ValueKind::None)
            {
                pos += matched;
            }
            else if (flag->value == ValueKind::Number)
            {
                size_t  digits = 0;

                while (digits < rest.size() && isdigit ((unsigned char) rest[digits]) != 0)
                {
                    digits++;
                }

                if (digits > 0)
                {
                    value = rest.substr (0, digits);
                    pos  += matched + digits;
                }
                else if (flag->bareDefault != nullptr)
                {
                    value = flag->bareDefault;
                    pos  += matched;
                }
                else
                {
                    //  A numeric option given no number. as65 documents no bare
                    //  form for -h, and the neighboring argument is not one: a
                    //  separated value here would be somebody's source file.
                    std::cerr << "Error: " << options.flagPrefix << flag->option
                              << " takes its number ATTACHED: " << options.flagPrefix
                              << flag->option << "60\n";
                    refused = true;
                }
            }
            else
            {
                value = rest;

                if (value.empty() && flag->attachment == Attachment::AttachedOrSeparate
                                  && argIndex + 1 < argc)
                {
                    value = argv[++argIndex];
                }

                if (value.empty() && flag->bareDefault != nullptr)
                {
                    value = flag->bareDefault;
                }

                //  A NAME-TAKING OPTION WITH NO BARE FORM AND NO NAME IS
                //  REFUSED. `-o` at the end of a command line named no file,
                //  and taking it as the empty name wrote the assembled bytes
                //  over whatever the empty path resolves to -- or, before the
                //  refusal existed, span forever looking for the value.
                if (value.empty() && flag->bareDefault == nullptr)
                {
                    std::cerr << "Error: " << options.flagPrefix << flag->option
                              << " needs a filename after it, attached or separated:\n"
                              << "       " << options.flagPrefix << flag->option << "prog.bin, or "
                              << options.flagPrefix << flag->option << " prog.bin\n";
                    refused = true;
                }

                //  A name consumes the rest of the argument whether it found one
                //  or not: what follows a string option is its value, and a flag
                //  letter sitting there would be part of the name.
                pos = arg.size();
            }

            if (refused)
            {
                options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
                stop                 = true;
                break;
            }

            ApplyAs65Flag (flag, value, options, stop);
        }

        argIndex++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeGluedCount
//
//  Reads the digits glued to a numeric AS65 flag and says how many characters
//  they occupied, so the walk resumes at the first character that is not one.
//
//  THE DIGIT COUNT IS THE WHOLE POINT. as65 lets other options follow a flag
//  whose parameter is numeric -- `-h80t` is 80 lines per page AND a symbol
//  table -- and the only thing that can say where the number stops and the next
//  flag starts is where the digits stop. The flag used to take the rest of its
//  argument whatever it held, which read `-h80t` as a height of 80 and threw
//  the `t` away.
//
//  Nothing is written to `value` when there are no digits, so a bare flag keeps
//  whatever default its own case chose -- which differs: no pagination for -h,
//  the wide listing for -w.
//
////////////////////////////////////////////////////////////////////////////////

size_t CommandLineParser::TakeGluedCount (const std::string & rest, int & value)
{
    HRESULT   hr     = S_OK;
    size_t    digits = 0;
    uint32_t  parsed = 0;



    while (digits < rest.size() && isdigit ((unsigned char) rest[digits]) != 0)
    {
        digits++;
    }

    if (digits > 0)
    {
        hr = ParseDecimal (rest.substr (0, digits).c_str(), parsed);

        if (SUCCEEDED (hr))
        {
            value = (int) parsed;
        }
    }

    return digits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddSymbolDefinition
//
//  as65's `-d`: a name, optionally equated to a value, defined as 1 without one.
//
//  A VALUE IT CANNOT READ IS REFUSED RATHER THAN QUIETLY MADE 1. `-dADDR=$6000`
//  and `-dVER=1.0` each defined the symbol as 1 in silence, and the source then
//  took a branch nobody chose. `$6000` is the assembler's own hex syntax and
//  reads perfectly well INSIDE a source file; on the command line it is not a
//  number this flag knows, and the difference is worth a sentence rather than a
//  wrong answer.
//
//  A BARE `-d` ARRIVES HERE AS `DEBUG=1`, because that is what its row says a
//  bare one means. Nothing here has to know that.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::AddSymbolDefinition (const std::string & definition,
                                             CommandLineOptions & options,
                                             bool & stop)
{
    size_t       eqPos = definition.find ('=');
    std::string  name  = definition;
    int32_t      value = 1;
    bool         taken = true;



    if (eqPos != std::string::npos)
    {
        std::string    text = definition.substr (eqPos + 1);
        char         * end  = nullptr;
        long           read = strtol (text.c_str(), &end, 0);

        name  = definition.substr (0, eqPos);
        taken = !text.empty() && end != nullptr && *end == 0;

        if (taken)
        {
            value = (int32_t) read;
        }
        else
        {
            std::cerr << "Error: " << options.flagPrefix << "d cannot read `"
                      << text << "` as a value.\n"
                      << "       Write it as a decimal or 0x-prefixed number, or leave the\n"
                      << "       `=` off entirely: a name on its own is defined as 1.\n";
        }
    }

    if (taken && name.empty())
    {
        std::cerr << "Error: " << options.flagPrefix
                  << "d needs a name in front of the `=`.\n";
        taken = false;
    }

    if (taken)
    {
        options.predefinedSymbols[name] = value;
    }
    else
    {
        options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
        stop                 = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyAs65Flag
//
//  What one matched as65 flag does to the options, given the value the walk
//  already read for it.
//
//  THE WALK DECIDED THE GRAMMAR; THIS DECIDES THE MEANING. Nothing here reads
//  the argument, counts characters, or looks at the next argv entry -- all of
//  that came out of the flag's own row before this is called. What is left is
//  one option, one value, one field, which is what makes a new flag a row plus
//  a case rather than a new piece of parsing.
//
//  `stop` IS AN OUT-PARAMETER BECAUSE TWO FLAGS END THE COMMAND LINE. A `-d`
//  whose value cannot be read is refused, and so is one with nothing in front
//  of its `=`; both leave the rest of the line unparsed, because a run that is
//  not going to happen should not go on being configured.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::ApplyAs65Flag (const DialectFlag * flag,
                                       const std::string & value,
                                       CommandLineOptions & options,
                                       bool & stop)
{
    std::string  option = flag->option;



    if (option == "x")
    {
        options.cpuTarget    = CommandLineOptions::CpuTarget::M65C02;
        options.hasCpuTarget = true;
    }
    else if (option == "d")
    {
        AddSymbolDefinition (value, options, stop);
    }
    else if (option == "i" || option == "n")
    {
        //  Accepted and recorded nowhere. `-i` asks for case-insensitive
        //  opcodes, which this assembler does unconditionally; `-n` is not
        //  implemented, and its help says where that is tracked. Both must
        //  parse, because an as65 command line carrying either is a command
        //  line this mode exists to accept.
        if (option == "n")
        {
            options.disableOpt = true;
        }
    }
    else if (option == "o")
    {
        options.outputFile = value;
    }
    else if (option == "s" || option == "s2")
    {
        SelectOutputFormat (std::string (1, options.flagPrefix) + option,
                            option == "s2" ? CommandLineOptions::OutputFormat::IntelHex
                                           : CommandLineOptions::OutputFormat::SRecord,
                            options);

        //  The format flags carry an optional output name of their own, which
        //  is as65's own shape: `-s2out.hex` names the file in one word.
        if (!value.empty())
        {
            options.outputFile = value;
        }
    }
    else if (option == "z")
    {
        //  Both, because two readers ask differently: the assembler takes the
        //  byte, and the fill decision is asked as a yes/no where the byte is
        //  of no interest. Setting one and not the other left `-z` documented
        //  and inert.
        options.fillZero = true;
        options.fillByte = 0x00;
    }
    else if (option == "l")
    {
        //  The bare form's default is "-", which is how the table says stdout
        //  without this having to know what an empty filename would mean.
        bool  toStdout = value == "-";

        options.generateListing  = true;
        options.listingToStdout  = toStdout;
        options.listingFile      = toStdout ? std::string() : value;
    }
    else if (option == "p")
    {
        options.pass1Listing = true;
    }
    else if (option == "c")
    {
        options.cycleCounts = true;
    }
    else if (option == "m")
    {
        options.macroExpansion = true;
    }
    else if (option == "h")
    {
        options.pageHeight = atoi (value.c_str());
    }
    else if (option == "w")
    {
        options.pageWidth = atoi (value.c_str());
    }
    else if (option == "t")
    {
        options.symbolTable = true;
    }
    else if (option == "g")
    {
        options.debugInfo = true;
    }
    else if (option == "v")
    {
        options.verbose = true;
    }
    else if (option == "q")
    {
        options.quiet = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyAs65Defaults
//
//  Fills in the names AS65 mode infers rather than requires.
//
//  The input name is auto-extended FIRST, so both derived names come from the
//  resolved source path rather than the possibly extensionless one the user
//  typed. The output extension follows the selected FORMAT, so `-s file.a65`
//  writes an S-record without a second flag, and `-g` yields a .dbg beside the
//  source.
//
//  THE DEBUG NAME IS ALWAYS THE DERIVED ONE, because as65's -g takes no
//  parameter -- its entry is one sentence and names no file, extension or
//  format. The output name is inferred only when -o did not supply it, so an
//  explicit -oFILE always wins.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::ApplyAs65Defaults (CommandLineOptions & options, const FileExistsFn & fileExists)
{
    std::string  ext        = ".bin";
    bool         hasInput   = false;
    bool         needsOut   = false;
    bool         needsDebug = false;



    if (!options.inputFile.empty())
    {
        options.inputFile = TryAutoExtend (options.inputFile, fileExists);
    }

    hasInput   = !options.inputFile.empty();
    needsOut   = options.outputFile.empty() && hasInput;
    needsDebug = options.debugInfo && options.debugFile.empty() && hasInput;

    if (needsOut)
    {
        if (options.outputFormat == CommandLineOptions::OutputFormat::SRecord)
        {
            ext = ".s19";
        }
        else if (options.outputFormat == CommandLineOptions::OutputFormat::IntelHex)
        {
            ext = ".hex";
        }

        options.outputFile = StripExtension (options.inputFile) + ext;
    }

    if (needsDebug)
    {
        options.debugFile = StripExtension (options.inputFile) + ".dbg";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineParser::RecordUnrecognizedFlag
//
//  An argument the active grammar does not know, carried out by name rather
//  than printed here.
//
//  It used to be a warning written to stderr from inside the parser, after
//  which parsing -- and the assembly -- carried on. That made a typo silent in
//  every way that mattered: the warning scrolled past, the exit code was 0, and
//  the output file was written as though the flag had been honored. Recording
//  it lets the edge refuse the invocation and print the help for the mode the
//  flag was meant for.
//
//  The FIRST one is the one reported. A command line with two typos gets one
//  message and the help, and the second typo is obvious against the help.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::RecordUnrecognizedFlag (const std::string & flag, CommandLineOptions & options)
{
    if (options.unrecognizedFlag.empty())
    {
        options.unrecognizedFlag = flag;
    }

    //  THE VERDICT MOVES WITH THE FLAG, because two different callers ask two
    //  different ways. The executable reads the flag's own text, to name it in
    //  the message; anything deciding whether the command line was ACTED ON
    //  reads the verdict. Recording one without the other is how a refused flag
    //  came to print a complaint and still report success -- the diagnostic
    //  reached the user's screen and never reached their build script.
    options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefuseCpuFlagWhereSelectedInSource
//
//  Refuses a command-line CPU flag for a dialect that takes its CPU from the
//  source, and words the refusal.
//
//  Driven entirely by the PROFILE. Nothing here names a dialect, and nothing
//  here knows which dialects have an in-source CPU directive: the profile says
//  where its CPU comes from and what the directive is called, and both halves of
//  the sentence are its own. A test that flips a profile's answer therefore
//  flips the behavior, which is what makes the claim checkable rather than a
//  matter of reading the code.
//
//  Refused rather than ignored, because a flag that is accepted and does nothing
//  is worse than one that errors -- and because accepting the wider instruction
//  set without the directive that selects it would assemble source the real
//  assembler rejects.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::RefuseCpuFlagWhereSelectedInSource (CommandLineOptions & options)
{
    const DialectProfile  & profile    = DialectRegistry::Get (options.dialect);
    bool                    isInSource = profile.GetCpuSelectionSource() == CpuSelectionSource::InSource;



    if (isInSource)
    {
        options.cpuFlagRefusal = std::string (1, options.flagPrefix) + "x is not accepted for " + profile.GetName()
                               + ": the CPU target is selected in the source, with the "
                               + profile.GetCpuDirectiveName() + " directive.";
    }

    return isInSource;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MatchFlag
//
//  The flag a dialect's table matches at this position, taking the LONGEST row
//  that fits.
//
//  LONGEST MATCH IS WHAT MAKES `-s2` ORDINARY. `s` and `s2` are both rows, and
//  `-s2out.hex` has two readings that both parse: the s2 flag with the filename
//  out.hex, or the s flag with the filename 2out.hex. as65 resolves it the first
//  way, this always resolved it the first way with a hand-written peek inside
//  the `s` arm, and stating the rule once for every dialect is the only change.
//  The documented cost is that `-s` cannot name a file beginning with `2`.
//
//  It takes the whole remaining argument rather than one character because a
//  row is a string now, and returns how much it consumed so the walk knows
//  where the value starts.
//
////////////////////////////////////////////////////////////////////////////////

const CommandLineParser::DialectFlag * CommandLineParser::MatchFlag (DialectId dialect,
                                                                     const std::string & text,
                                                                     size_t at,
                                                                     size_t & outLength)
{
    const DialectFlag *  found = nullptr;



    outLength = 0;

    for (const DialectFlag & flag : GetFlags (dialect))
    {
        size_t  length = strlen (flag.option);

        if (text.compare (at, length, flag.option) == 0 && length > outLength)
        {
            found     = &flag;
            outLength = length;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyMerlinFlag
//
//  What one flag DOES, once the walk above has taken whatever value it carries.
//
//  Reports whether it recognized the letter rather than ignoring one it does
//  not, so a row added to the table without an arm here is a flag the help
//  advertises and the parser drops -- which is exactly the drift the table
//  exists to prevent, and which the table sweep in the tests would otherwise
//  pass over in silence.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::ApplyMerlinFlag (char                 letter,
                                         const std::string  & value,
                                         CommandLineOptions & options,
                                         bool               & stop)
{
    bool  applied = true;



    switch (letter)
    {
    case 'o':
        options.outputFile = value;
        break;

    case 'l':
        //  "-" is what the table says a bare -l means, so the sentinel is
        //  read here rather than every caller having to know that an empty
        //  filename and "no filename" are the same request.
        options.generateListing = true;
        options.listingToStdout = value == "-";
        options.listingFile     = options.listingToStdout ? std::string() : value;
        break;

    case 'v':
        options.verbose = true;
        break;

    case 'd':
        AddSymbolDefinition (value, options, stop);
        break;

    default:
        applied = false;
        break;
    }

    return applied;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseMerlinFlags
//
//  Parses the Merlin grammar, which is the flag table above and nothing else.
//
//  The table decides which letters exist and how each takes its value; this walk
//  decides nothing of its own. That is what makes the help text generated from
//  the same rows a description of this parser rather than a second account of it.
//
//  A listing filename must be ATTACHED, unlike the as65 form that also
//  accepts a separated one. Merlin's own source names the object file, so the
//  bare word after a flag is far more likely to be the source than a listing
//  path, and swallowing it would leave an assembly with no input and a listing
//  nobody asked for.
//
//  The output format is Raw for this dialect and is not a flag. A Merlin object
//  IS the assembled stream: the origin relocates rather than seeks, so padding
//  it out to an address-indexed image would scatter one contiguous object across
//  the address space.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::ParseMerlinFlags (int argc, char * argv[], int startIndex, CommandLineOptions & options)
{
    int   argIndex = startIndex;
    bool  stop     = false;



    options.subcommand       = CommandLineOptions::Subcommand::Merlin;
    options.dialect          = DialectId::Merlin;
    options.dialectSelection = DialectSelection::Stated;
    options.outputFormat     = CommandLineOptions::OutputFormat::Raw;

    while (argIndex < argc && !stop)
    {
        std::string  arg (argv[argIndex]);
        std::string  attachedValue;
        size_t       pos = 1;

        //  Merlin's own page, for the reason as65's arm gives: the subcommand
        //  already said which grammar the reader is in.
        //
        //  The lone `?` comes along even though it is as65's convention rather
        //  than Merlin's, because a reader who learned it one line ago types it
        //  here next and Merlin has nothing else to spend it on.
        if (IsHelpRequest (arg) || IsLoneQuestionMark (argc, argv, startIndex))
        {
            if (arg[0] == '/')
            {
                NoteFlagPrefix ('/', options);
            }

            options.showHelp = true;
            options.helpPage = CommandLineOptions::HelpPage::Merlin;
            stop             = true;
            continue;
        }

        // The CPU flag is recognized by every grammar and honored by the ones
        // whose dialect takes its CPU from the command line. Whether this is one
        // of them is the profile's answer, not this parser's.
        if (arg == "-x" || arg == "/x")
        {
            NoteFlagPrefix (arg[0] == '/' ? '/' : '-', options);

            stop = RefuseCpuFlagWhereSelectedInSource (options);

            if (!stop)
            {
                argIndex++;
            }

            continue;
        }

        // An output format, matched against the same table the help text is
        // composed from. Placed beside the CPU flag rather than in the letter
        // loop below, because these are whole words: a letter loop would read
        // --flat as -f -l -a -t and warn four times about flags nobody wrote.
        if (ApplyOutputFormat (arg, DialectId::Merlin, options))
        {
            argIndex++;
            continue;
        }

        if (arg[0] == '/')
        {
            NoteFlagPrefix ('/', options);
            arg[0]             = '-';
        }
        else if (arg[0] == '-')
        {
            // Noted here as well, or "first prefix wins" would silently mean
            // "first SLASH wins": a dash flag ahead of a slash one would leave
            // nothing recorded, and the slash would take an invocation it did
            // not open.
            NoteFlagPrefix ('-', options);
        }

        if (arg[0] != '-')
        {
            if (options.inputFile.empty())
            {
                options.inputFile = arg;
                argIndex++;
                continue;
            }

            //  A SECOND SOURCE FILE IS REFUSED RATHER THAN DROPPED, which is
            //  what the as65 arm does and for the same reason: a caller who
            //  named two files got one assembled, exit 0, and no word about
            //  the other. Merlin was still silently discarding it after as65
            //  stopped, so the two grammars disagreed about the same mistake.
            std::cerr << "Error: surplus argument: " << arg << "\n";
            std::cerr << "       Assembling takes one source file, and "
                      << options.inputFile << " is already it.\n";

            options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            stop                 = true;
            continue;
        }

        while (pos < arg.size())
        {
            size_t                matched = 0;
            const DialectFlag  *  flag    = MatchFlag (DialectId::Merlin, arg, pos, matched);
            std::string           rest;
            std::string           value;
            bool                  applied = false;

            if (flag == nullptr)
            {
                RecordUnrecognizedFlag (std::string (1, options.flagPrefix) + arg[pos], options);
                pos++;
                continue;
            }

            rest = arg.substr (pos + matched);

            //  A value that takes a name consumes the rest of the argument,
            //  which is what stops the name from swallowing a flag; one that
            //  takes nothing leaves the walk where it is. See ValueKind.
            if (flag->value == ValueKind::None)
            {
                pos += matched;
            }
            else
            {
                value = rest;

                if (value.empty() && flag->attachment == Attachment::AttachedOrSeparate
                                  && argIndex + 1 < argc)
                {
                    value = argv[++argIndex];
                }

                if (value.empty() && flag->bareDefault != nullptr)
                {
                    value = flag->bareDefault;
                }

                pos = value.empty() ? pos + matched : arg.size();
            }

            applied = ApplyMerlinFlag (flag->option[0], value, options, stop);

            if (!applied)
            {
                RecordUnrecognizedFlag (std::string (1, options.flagPrefix) + flag->option, options);
            }
        }

        argIndex++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyMerlinDefaults
//
//  Resolves the source path the same way AS65 mode does, so `merlin build`
//  finds build.s.
//
//  The OUTPUT name is deliberately not defaulted here. Merlin source can name
//  its own object, and that answer arrives only once the file has been read --
//  so the precedence between the flag and the directive is settled by the
//  assembler, which sees both, rather than guessed by a parser that sees one.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::ApplyMerlinDefaults (CommandLineOptions & options, const FileExistsFn & fileExists)
{
    bool  hasInput = !options.inputFile.empty();



    if (hasInput)
    {
        options.inputFile = TryAutoExtend (options.inputFile, fileExists);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseRunOptions
//
//  Parses the modern, separated-option grammar the `run` subcommand uses.
//
//  Separate from ParseAs65Flags because the two grammars are not compatible:
//  this one takes long options with separated values and no concatenation,
//  which is exactly what AS65 mode cannot accept.
//
//  EVERY DIAGNOSTIC HERE IS ALSO A REFUSAL. This grammar has no ignorable
//  mistakes: an option it does not know might have changed where the image
//  loads or when it stops, and a value it could not read certainly would have.
//  Running anyway and reporting success told a build script that a command line
//  it got wrong had worked.
//
//  A HELP REQUEST IS THE ONE ARGUMENT THAT IS NEITHER, and it is looked for
//  before any of them. `run --help` would otherwise be an option this grammar
//  does not have -- refused, with a diagnostic, exiting 2 -- which answers a
//  question the tool knows the answer to by complaining about it. It is looked
//  for ANYWHERE in the arguments for the reason the disk grammar gives: a
//  reader asks for help after typing the thing they wanted help with at least
//  as often as before it.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::ParseRunOptions (int argc, char * argv[], int argIndex, CommandLineOptions & options)
{
    HRESULT  hr        = S_OK;
    bool     wantsHelp = false;
    bool     refused   = false;



    //  And the lone `?`, for the reason the disk arm gives: it is a request
    //  only when there is nothing beside it.
    wantsHelp = IsLoneQuestionMark (argc, argv, argIndex);

    for (int probe = argIndex; probe < argc; probe++)
    {
        if (IsHelpRequest (argv[probe]))
        {
            wantsHelp = true;

            if (argv[probe][0] == '/')
            {
                options.flagPrefix = '/';
            }
        }
    }

    if (wantsHelp)
    {
        options.showHelp = true;
        options.helpPage = CommandLineOptions::HelpPage::Run;
        return;
    }

    while (argIndex < argc)
    {
        // The long options first, for the reason ParseAs65Flags gives: the
        // single-character normalization below turns `/load` into `-load`,
        // which matches nothing and is reported as an unknown option.
        std::string arg = CanonicalLongFlag (argv[argIndex],
                              std::span<const char * const> (s_kpszRunLongOptions));

        // Which assembler reads a SOURCE handed to `run`. Named the same way the
        // subcommands name it, because the question is the same question: a
        // dialect the tool inferred is a dialect nobody stated. Ignored when the
        // input is a binary, which needs no assembler at all.
        //
        // The default stays as65, so every `run` invocation written before this
        // existed keeps assembling what it always did.
        if (IsLongOption (arg, "--as65", options))
        {
            options.dialect          = DialectId::As65;
            options.dialectSelection = DialectSelection::Stated;
            argIndex++;
            continue;
        }

        if (IsLongOption (arg, "--merlin", options))
        {
            options.dialect          = DialectId::Merlin;
            options.dialectSelection = DialectSelection::Stated;
            argIndex++;
            continue;
        }

        // The CPU a SOURCE assembles for, in both spellings the assembler
        // subcommands take. Without these, `run` could assemble nothing that
        // used a 65C02 instruction -- it refused the flag and then reported
        // every such instruction as invalid, which is a source that can be
        // assembled and cannot be run.
        if (arg == "-x" || arg == "/x")
        {
            NoteFlagPrefix (arg[0] == '/' ? '/' : '-', options);
            options.cpuTarget    = CommandLineOptions::CpuTarget::M65C02;
            options.hasCpuTarget = true;
            argIndex++;
            continue;
        }

        // A symbol the source expects, the other assembler flag that changes
        // what gets assembled. The rest describe a file `run` never writes.
        if ((arg.rfind ("-d", 0) == 0 || arg.rfind ("/d", 0) == 0) && arg.size() > 2)
        {
            NoteFlagPrefix (arg[0] == '/' ? '/' : '-', options);
            AddSymbolDefinition (arg.substr (2), options, refused);
            argIndex++;
            continue;
        }

        if ((arg == "-d" || arg == "/d") && argIndex + 1 < argc)
        {
            NoteFlagPrefix (arg[0] == '/' ? '/' : '-', options);
            AddSymbolDefinition (argv[++argIndex], options, refused);
            argIndex++;
            continue;
        }


        // Normalize / prefix to - on Windows
        if (arg.size() > 1 && arg[0] == '/')
        {
            arg[0] = '-';
        }

        if (arg == "-o" && argIndex + 1 < argc)
        {
            options.outputFile = argv[++argIndex];
        }
        else if (arg == "-l" && argIndex + 1 < argc)
        {
            options.symbolFile = argv[++argIndex];
        }
        else if (arg == "-a")
        {
            options.generateListing = true;
        }
        else if (arg == "-v")
        {
            options.verbose = true;
        }
        else if (IsLongOption (arg, "--fill", options) && argIndex + 1 < argc)
        {
            hr = ParseFillByte (argv[++argIndex], options.fillByte);

            if (FAILED (hr))
            {
                std::cerr << "Error: Invalid fill byte value\n";
                options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            }
        }
        else if (IsLongOption (arg, "--load", options) && argIndex + 1 < argc)
        {
            hr = ParseAddress (argv[++argIndex], options.loadAddress);

            if (SUCCEEDED (hr))
            {
                options.hasLoadAddress = true;
            }
            else
            {
                std::cerr << "Error: Invalid load address\n";
                options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            }
        }
        else if (IsLongOption (arg, "--entry", options) && argIndex + 1 < argc)
        {
            hr = ParseAddress (argv[++argIndex], options.entryAddress);

            if (SUCCEEDED (hr))
            {
                options.hasEntryAddress = true;
            }
            else
            {
                std::cerr << "Error: Invalid entry address\n";
                options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            }
        }
        else if (IsLongOption (arg, "--stop", options) && argIndex + 1 < argc)
        {
            hr = ParseAddress (argv[++argIndex], options.stopAddress);

            if (SUCCEEDED (hr))
            {
                options.hasStopAddress = true;
            }
            else
            {
                std::cerr << "Error: Invalid stop address\n";
                options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            }
        }
        else if (IsLongOption (arg, "--max-cycles", options) && argIndex + 1 < argc)
        {
            hr = ParseDecimal (argv[++argIndex], options.maxCycles);

            if (FAILED (hr))
            {
                std::cerr << "Error: Invalid max-cycles value\n";
                options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            }
        }
        else if (IsLongOption (arg, "--reset-vector", options))
        {
            options.useResetVector = true;
        }
        else if (IsLongOption (arg, "--warn", options))
        {
            options.warningMode = WarningMode::Warn;
        }
        else if (IsLongOption (arg, "--no-warn", options))
        {
            options.warningMode = WarningMode::NoWarn;
        }
        else if (IsLongOption (arg, "--fatal-warnings", options))
        {
            options.warningMode = WarningMode::FatalWarnings;
        }
        else if (arg[0] != '-' && options.inputFile.empty())
        {
            options.inputFile = arg;
        }
        else if (arg[0] != '-')
        {
            //  A SECOND INPUT FILE IS NAMED AS ONE. It was already refused,
            //  which is the right verdict, but under the words "Unknown
            //  option" -- and a filename is not an option, so the reader was
            //  sent looking for a flag they had not typed.
            std::cerr << "Error: surplus argument: " << arg << "\n"
                      << "       `run` takes one input file, and " << options.inputFile
                      << " is already it.\n";

            options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
        }
        else if (IsRunOptionNeedingValue (arg))
        {
            //  An option that ran out of command line is not an unknown one.
            //  See IsRunOptionNeedingValue.
            std::cerr << "Error: " << argv[argIndex] << " needs a value after it\n";
            options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
        }
        else
        {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            RecordUnrecognizedFlag (arg, options);
        }

        argIndex++;
    }

    // The CPU flag is refused for a dialect whose source selects its own, here
    // as well as in that dialect's own subcommand. Checked after the loop
    // rather than inside it because the two facts arrive in either order:
    // `run src.s --merlin -x` and `run src.s -x --merlin` are the same request,
    // and a check inside the loop would refuse only one of them.
    if (options.hasCpuTarget)
    {
        RefuseCpuFlagWhereSelectedInSource (options);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Parse
//
//  The top-level dispatcher: decide which command line this IS, then parse it
//  accordingly.
//
//  Help and version are matched before either grammar, so they work regardless
//  of which one would have applied.
//
//  A leading `/` is normalized to `-` throughout, after recording the user's
//  chosen prefix in flagPrefix so usage text is written back the way they type.
//  chosen prefix in flagPrefix so usage text uses the prefix they type.
//
//  An unrecognized first argument IS an error, and is carried back out by name.
//  It used to be taken for a source filename, which is how as65 was invoked; the
//  guess is gone, and each assembler dialect is named by a subcommand of its
//  own. Adding one means adding a row to the table and an arm below.
//
////////////////////////////////////////////////////////////////////////////////

CommandLineOptions CommandLineParser::Parse (int argc, char * argv[], const FileExistsFn & fileExists)
{
    HRESULT                         hr       = S_OK;
    CommandLineOptions              options  = {};
    CommandLineOptions::Subcommand  named    = CommandLineOptions::Subcommand::None;
    std::string                     first;
    bool                            isHelp   = false;
    bool                            isVer    = false;
    bool                            isAs65   = false;
    //  THE SHELL'S DAMAGE IS UNDONE BEFORE ANY OF THE GRAMMAR SEES THE COMMAND
    //  LINE, which is what lets every reader below assume the arguments are the
    //  ones the user typed. Doing it per-grammar would mean three places that
    //  each have to remember, and the one that forgot would be the one refusing
    //  a command line as65 has accepted for thirty years.
    //
    //  The joined strings must outlive the parse, so they are held here and
    //  pointed at, rather than rebuilt inside the helper and returned by value
    //  into a dangling argv.
    std::vector<std::string>        typed    = RejoinShellSplitArguments (argc, argv);
    std::vector<char *>             args;
    bool                            isMerlin = false;



    for (std::string & one : typed)
    {
        args.push_back (one.data());
    }

    argc = static_cast<int> (args.size());
    argv = args.data();



    //  NO COMMAND LINE AT ALL PRINTS THE GENERAL PAGE AND PRODUCES NOTHING,
    //  and the status has to say the second half. It exited 0, so a script that
    //  invoked the tool with an argument variable that happened to be empty was
    //  told the run had worked.
    //
    //  THIS IS NOT AN as65 PARITY QUESTION. Nothing here has entered the
    //  assembler's grammar -- there is no source file, no `run`, no `disk` --
    //  so as65's statuses do not govern it. The value comes from this tool's
    //  own table instead, where 1 is a command line that could not be acted on.
    //
    //  The verdict carries it because the verdict is what the exit code is
    //  read from, and because a bare invocation is not the user ASKING for the
    //  page: `--help` still exits 0 and still prints the same text.
    if (argc < 2)
    {
        options.showHelp     = true;
        options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
    }

    BAIL_OUT_IF (argc < 2, S_OK);

    // Check for --help / --version first (accept / prefix on Windows)
    first = argv[1];

    if (first[0] == '/')
    {
        NoteFlagPrefix ('/', options);
        first[0] = '-';
    }

    //  A BARE `?`, AND ONLY WHEN IT IS THE WHOLE COMMAND LINE, is the usage
    //  request as65 documents: "Help message if only parameter is a question
    //  mark". The prefixed `-?` and `/?` were already accepted; the unadorned
    //  one was read as a source filename, so `casso ?` tried to open a file
    //  called `?` and exited 2 complaining it could not.
    //
    //  The "only parameter" condition is as65's own and is kept literally. A
    //  `?` further along a command line is an argument to whatever precedes it,
    //  and a filename is a perfectly legal thing to have called `?` on a host
    //  that allows it -- so nothing but the one-argument case changes meaning.
    isHelp = first == "--help" || first == "-help" || first == "-h" || first == "-?" ||
             (argc == 2 && first == "?");
    isVer  = first == "--version" || first == "-version";

    if (isHelp)
    {
        options.subcommand = CommandLineOptions::Subcommand::Help;
        options.showHelp   = true;

        //  A LONE `?` ASKS FOR THE GENERAL PAGE, like every other form of the
        //  request at this level, because at this level no grammar has been
        //  named yet.
        //
        //  It used to open the assembler's page, and that was right while it
        //  was written: a bare source file assembled, so the top level WAS
        //  as65 mode and a `?` typed there came from an as65 command line.
        //  Assembling names its dialect now. The top level selects a mode and
        //  runs nothing, so `CassoCli ?` and `CassoCli --help` are one
        //  question -- somebody who named no subcommand -- and answering them
        //  with different pages sent one of the two to a grammar they had not
        //  entered, which never mentions that merlin, run and disk exist.
        //
        //  as65 compatibility is unaffected: `CassoCli as65 ?` is where that
        //  command line lives, and the general page's second line says so.
    }
    else if (isVer)
    {
        options.subcommand  = CommandLineOptions::Subcommand::Version;
        options.showVersion = true;
    }

    BAIL_OUT_IF (isHelp || isVer, S_OK);

    named    = LookUpSubcommand (first);
    isAs65   = named == CommandLineOptions::Subcommand::As65;
    isMerlin = named == CommandLineOptions::Subcommand::Merlin;

    // An unrecognized first word is now an error rather than an assumed source
    // filename. The word is carried out so the caller can name the replacement
    // instead of printing usage: the population this breaks is build scripts,
    // which nobody re-reads until they fail, and "unknown argument" turns a
    // one-line fix into a bisect.
    if (named == CommandLineOptions::Subcommand::None)
    {
        options.unrecognizedArgument = first;
    }

    if (isAs65)
    {
        // From argv[2] -- argv[1] is the `as65` word itself. Every other
        // subcommand's parser already starts there.
        ParseAs65Flags    (argc, argv, 2, options);
        ApplyAs65Defaults (options, fileExists);
    }

    if (isMerlin)
    {
        ParseMerlinFlags    (argc, argv, 2, options);
        ApplyMerlinDefaults (options, fileExists);
    }

    // An assembler grammar consumed the rest of the command line above; only
    // another named subcommand continues into its own option parser.
    BAIL_OUT_IF (isAs65 || isMerlin, S_OK);

    options.subcommand = named;

    if (named == CommandLineOptions::Subcommand::Run)
    {
        ParseRunOptions (argc, argv, 2, options);
    }
    else if (named == CommandLineOptions::Subcommand::Disk)
    {
        ParseDiskOptions (argc, argv, 2, options);
    }

Error:
    return options;
}
