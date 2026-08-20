#include "Pch.h"

#include "CommandLineParser.h"





//
//  Every bare-word subcommand. Anything not in this table is a source
//  filename, which puts the parser in AS65 mode -- so a new subcommand is a
//  row here plus an arm in Parse, not a reshaped dispatcher.
//
static constexpr CommandLineParser::SubcommandName  s_kSubcommands[] =
{
    { "run",  CommandLineOptions::Subcommand::Run  },
    { "disk", CommandLineOptions::Subcommand::Disk },
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
//  because the condition that makes it a request cannot hold inside a
//  subcommand. as65 asks for a question mark that is the ONLY parameter; every
//  argument this function judges has a verb in front of it, so a `?` reaching
//  here is somebody's operand -- a file on a disk is allowed to be called that.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::IsHelpRequest (const std::string & arg)
{
    return arg == "--help" || arg == "-help" || arg == "-?" || arg == "-h" ||
           arg == "/help"  || arg == "/?"    || arg == "/h";
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
                          << " is not an address -- write it as $XXXX\n";

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
                      << " -- try: " << DescribeDiskOptions() << "\n";

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
//  Resolves a bare word to a subcommand, or None when the word is not one --
//  which is the signal that it is a source filename and AS65 mode applies.
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

void CommandLineParser::ParseAs65Flags (int argc, char * argv[], CommandLineOptions & options)
{
    int   argIndex = 1;
    // Set when an argument ends parsing outright -- a help request, or an
    // option this grammar does not have (or no longer has). Only the
    // unknown-single-letter case leaves showHelp set; the rest answer by name.
    bool  stop     = false;

    options.subcommand = CommandLineOptions::Subcommand::As65;



    while (argIndex < argc && !stop)
    {
        // A `/` form of a long option is canonicalized before anything
        // tests for one, so `/cpu 65c02` and `/flat` work the way `/l` and `/o`
        // already did. Without this the single-character normalization further
        // down reads `/flat` as the concatenated flags -f -l -a -t.
        std::string arg = CanonicalLongFlag (argv[argIndex],
                              std::span<const char * const> (s_kpszAs65LongOptions));

        // Check for help requests
        if (arg == "--help" || arg == "-help" || arg == "-?" || arg == "/?" || arg == "/help")
        {
            if (arg[0] == '/')
            {
                options.flagPrefix = '/';
            }

            options.showHelp = true;
            stop             = true;
            continue;
        }

        //  `--cpu` IS WITHDRAWN AND `-x` REPLACES IT. The two selected the same
        //  instruction set, and `-x` is as65's own name for that switch --
        //  "Use 65SC02 extensions" -- so the tool kept two forms of one
        //  capability, one of which no as65 user would reach for.
        //
        //  It is answered by NAME rather than falling into the generic `--`
        //  refusal below, because command lines and makefiles carrying it
        //  already exist and "unknown option: --cpu" tells their author nothing
        //  about what to type instead. Same reasoning as the bare `-o`.
        //  BOTH PREFIXES ARE MATCHED HERE EXPLICITLY, because `cpu` has left
        //  the long-option table and so `/cpu` is no longer canonicalized into
        //  `--cpu` on its way past. Without this the slash form would fall
        //  into the concatenation walk and be read as -c -p -u -- cycle counts,
        //  a pass 1 listing, and an unknown flag -- which is a true reading of
        //  as65's grammar and a useless answer to somebody migrating.
        if (arg == "--cpu" || arg.rfind ("--cpu=", 0) == 0 ||
            arg == "/cpu"  || arg.rfind ("/cpu=",  0) == 0)
        {
            const char *  longPrefix = (arg[0] == '/') ? "/" : "--";

            if (arg[0] == '/')
            {
                options.flagPrefix = '/';
            }

            std::cerr << "Error: " << longPrefix << "cpu is gone -- use "
                      << options.flagPrefix << "x for the 65C02.\n"
                      << "       " << options.flagPrefix
                      << "x is as65's own name for the switch, and selects the same\n"
                      << "       instruction set. The default is still a strict 6502.\n";

            options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            stop                 = true;
            continue;
        }

        // Long options selecting a binary output SHAPE.
        //
        // THE DEFAULT IS THE ASSEMBLED BYTES, and --flat is how the old
        // full-64-KB padded image is asked for. There is no flag for the
        // default: `--raw` used to name it and was retired with it, because an
        // option that selects what naming nothing already selects costs a line
        // of help and buys no capability.
        if (arg == "--flat")
        {
            options.outputFormat      = CommandLineOptions::OutputFormat::Binary;
            options.outputFormatNamed = true;
            argIndex++;
            continue;
        }

        if (arg == "--dos-bin")
        {
            options.outputFormat      = CommandLineOptions::OutputFormat::DosBinary;
            options.outputFormatNamed = true;
            argIndex++;
            continue;
        }

        //  A `--` OPTION THIS GRAMMAR DOES NOT HAVE IS REFUSED, not handed to
        //  the concatenation walk below as a run of single letters.
        //
        //  `--out` is what that cost. The walk read `-`, complained about a
        //  flag named `-`, read `o`, took the rest of the argument as its glued
        //  value and set the output file to `ut`, then took the NEXT argument
        //  as the input file. Three wrong decisions, one warning, and exit 1.
        //
        //  Nothing as65 accepts is refused here: as65 has no `--` form at all,
        //  its long options being this project's own addition. The `/`
        //  forms deliberately fall through instead -- `/oFILE` is the glued
        //  form as65 documents, so `/out` genuinely does mean `-o ut` and must
        //  keep meaning it.
        //
        //  showHelp IS DELIBERATELY NOT SET, which is where this parts company
        //  with the unknown-single-letter refusal further down. That one prints
        //  the whole usage page, and the usage page is long: the sentence
        //  explaining the mistake scrolls away above it, so the reader is
        //  answered by being buried. The lines here say what was wrong and what
        //  to type instead, which is the entire content that page would add.
        if (arg.rfind ("--", 0) == 0)
        {
            std::cerr << "Error: unknown option: " << arg << "\n"
                      << "       Assembling takes single-letter flags with their values\n"
                      << "       attached -- the output file is -oFILE, and --out belongs\n"
                      << "       to `disk`.\n";

            options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            stop                 = true;
            continue;
        }

        // Normalize / prefix to - for flag parsing
        if (arg[0] == '/')
        {
            arg[0] = '-';
        }

        //  Non-flag argument is the input file -- and there is exactly one of
        //  them.
        //
        //  A SECOND ONE IS REFUSED RATHER THAN DROPPED. as65's synopsis is
        //  `as65 [-cdghilnopqstvwxz] file`, one file, and it documents nothing
        //  about a surplus argument -- so what happened to one was this tool's
        //  own answer, and the answer was to take the first and throw the rest
        //  away in silence. `casso pg.a65 -opg.bin -h 60` assembled, wrote the
        //  binary, exited 0, and never said that `60` had gone nowhere.
        //
        //  THE MESSAGE NAMES THE LIKELY CAUSE WHEN IT CAN SEE ONE, because the
        //  likely cause is nearly always the same: a value typed with a space
        //  in front of it, for an option that glues its value. Two things say
        //  so -- a surplus argument that is all digits, and an option standing
        //  in front of it that takes a parameter -- and when either holds the
        //  glued form is offered by name.
        //
        //  THE SHELL IS NOT ASKED ABOUT HERE ANY MORE. A command line PowerShell
        //  cut in half never reaches this point: RejoinShellSplitArguments puts
        //  the halves back together before parsing begins, so `CassoCli prog.a65
        //  -oprog.bin` assembles instead of arriving as `-oprog` and `.bin` and
        //  being explained to the reader who typed it correctly.
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
                std::cerr << "       If " << arg << " was meant as a value, as65 glues it to its option:\n"
                          << "       " << previous << arg << ", not " << previous << " " << arg << ".\n";
            }
            else if (IsPlainDecimal (arg))
            {
                std::cerr << "       If " << arg << " was meant as a value, as65 glues every value to its\n"
                          << "       option, with no space between them.\n";
            }

            options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            stop                 = true;
            continue;
        }

        // Parse concatenated flags: -tlfile means -t -lfile
        size_t pos = 1;

        while (pos < arg.size())
        {
            char         flag = arg[pos];
            std::string  rest = arg.substr (pos + 1);

            switch (flag)
            {
            case 'c':
                options.cycleCounts = true;
                pos++;
                break;

            case 't':
                options.symbolTable = true;
                pos++;
                break;

            //  as65 documents BOTH forms of this one, which is why a bare -l
            //  stays legal where a bare -o does not: "-l  Generate pass 2
            //  listing" and "-l<filename>  Listing file name". The parameter is
            //  a STRING either way, so the flag takes the rest of its argument
            //  and nothing may follow it in the group -- `-lt` names a listing
            //  file called `t`, it is not `-l -t`.
            case 'l':
                options.generateListing = true;

                if (!rest.empty())
                {
                    options.listingFile = rest;
                }
                else
                {
                    options.listingToStdout = true;
                }

                pos = arg.size();
                break;

            //  as65: `-o<filename>`. A STRING parameter, glued, with nothing
            //  following it in the group -- and that form is untouched.
            //
            //  THE SEPARATED `-o <file>` IS ACCEPTED AS WELL, by owner
            //  decision. It takes MORE than as65 does and never less, so every
            //  as65 command line still reads exactly as as65 reads it, and
            //  nothing that used to assemble stops assembling.
            //
            //  WHAT EARNS IT IS A SHELL, NOT A PREFERENCE. PowerShell parses a
            //  token beginning with a single `-` as a parameter name, and a
            //  parameter name may not contain a `.`, so it cuts `-oprog.bin`
            //  into `-oprog` and `.bin` before this program is even started.
            //  Nearly every output name has an extension, so in that shell --
            //  the one this tool is typed into most -- the glued form does not
            //  survive being typed unquoted.
            //
            //  IT DOES NOT EVEN FAIL CONSISTENTLY, which is the other half of
            //  the case for accepting a separated value. A colon before the
            //  first dot suppresses the cut, so `-oC:\out\prog.bin` arrives
            //  whole and works while `-oprog.bin` beside it does not. See
            //  IsShellSplitFragment for the measurement behind both.
            //
            //  ONLY -o TAKES A SEPARATED VALUE, AND THE ASYMMETRY IS THE POINT
            //  RATHER THAN AN OVERSIGHT. -l, -d, -w and -g each have a bare
            //  form as65 documents -- a listing to standard output, a DEBUG
            //  definition, 133 columns, and no parameter at all -- so for any
            //  of them the word that follows is genuinely ambiguous with the
            //  bare reading, and telling the two apart takes a GUESS about what
            //  that word looks like. That guess was here for -d, where it
            //  defined a label called `demo.a65`, and it was deleted for being
            //  unprincipled. -o has NO bare form -- one is refused below -- so
            //  the argument after it can only be its filename, and no guess is
            //  involved. Ambiguity is what separates the two cases, and -o is
            //  the only flag that has none.
            //
            //  It is taken VERBATIM, whatever it looks like. Skipping a value
            //  that "looks like a flag" would be the same guess arriving by a
            //  different door, and a file may legitimately be named one.
            //
            //  AN -o WITH NOTHING AFTER IT AT ALL IS STILL REFUSED, which also
            //  retires a hang for good: it used to match no branch and advance
            //  nothing, so the walk reread the same character forever and the
            //  process had to be killed.
            //
            //  showHelp IS NOT SET, which parts this from the unknown-option
            //  refusal below on the same rule the `--out` refusal already uses:
            //  these two lines ARE the answer, and the usage page would bury
            //  them. An option that does not exist is the case with nothing
            //  specific to say, and that one gets the page.
            case 'o':
                if (!rest.empty())
                {
                    options.outputFile = rest;
                }
                else if (argIndex + 1 < argc)
                {
                    options.outputFile = argv[argIndex + 1];
                    argIndex++;
                }
                else
                {
                    std::cerr << "Error: " << options.flagPrefix
                              << "o needs a filename after it, attached or separated:\n"
                              << "       " << options.flagPrefix << "oprog.bin, or "
                              << options.flagPrefix << "o prog.bin\n";

                    options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
                    stop                 = true;
                }

                pos = arg.size();
                break;

            case 'm':
                options.macroExpansion = true;
                pos++;
                break;

            //  as65: "-h<lines> ... The special case -h0 indicates an infinite
            //  page length."
            //
            //  ITS PARAMETER IS NUMERIC, WHICH IS WHAT DECIDES HOW MUCH OF THE
            //  ARGUMENT IT MAY TAKE. as65's rule: "no other option can follow
            //  one that may have a string parameter. Other options can follow
            //  one that has a numeric parameter" -- and its own worked example
            //  is `-h80t`, "which specifies 80 lines per page and a symbol
            //  table". This ran `pos` to the end of the argument instead, so
            //  `-h80t` set the height and threw the `t` away without a word.
            //
            //  The separated `-h 60` this used to accept was never as65's. It
            //  was added on the strength of this tool's own help text, which
            //  documented a form the parser did not read, and it is gone: the
            //  number after a bare -h is the next argument, not the height.
            //  A BARE -h IS REFUSED, and the silence in the manual is the
            //  evidence rather than an omission. as65 documents the bare form
            //  of -w -- "If the -w option is given without a number following
            //  it, then the listing will be 133 columns wide" -- and documents
            //  no bare form of -h, on the same page, by the same author. It
            //  silently did nothing here: the height kept whatever it already
            //  had and the flag might as well not have been typed.
            //
            //  `-h0` IS NAMED IN THE MESSAGE because a reader who wants no page
            //  breaks has a real form for it and would otherwise reach for
            //  the bare flag to ask.
            //
            //  This is the flag walk, which the FIRST argument never reaches --
            //  a leading `-h` is the top-level help request and is answered
            //  before any grammar is chosen.
            case 'h':
            {
                int     height = options.pageHeight;
                size_t  digits = TakeGluedCount (rest, height);

                if (digits == 0)
                {
                    std::cerr << "Error: " << options.flagPrefix
                              << "h takes its line count ATTACHED: "
                              << options.flagPrefix << "h60\n"
                              << "       Use " << options.flagPrefix
                              << "h0 for no page breaks at all.\n";

                    options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
                    pos                  = arg.size();
                    stop                 = true;
                    break;
                }

                pos               += 1 + digits;
                options.pageHeight = height;
                break;
            }

            //  as65: "-w<width>  Specify column width... If the -w option is
            //  given without a number following it, then the listing will be
            //  133 columns wide, otherwise it will be the number of colulmns
            //  specified (between 60 and 200)."
            //
            //  A BARE -w MEANING 133 IS as65's OWN FORM, not this project's --
            //  which is why it survives where the separated `-w 100` does not.
            //  Numeric like -h, so `-w100t` is `-w100 -t`.
            case 'w':
            {
                int     width  = kWideListingColumns;
                size_t  digits = TakeGluedCount (rest, width);

                pos             += 1 + digits;
                options.pageWidth = width;
                break;
            }

            case 'v':
                options.verbose = true;
                pos++;
                break;

            case 'q':
                options.quiet = true;
                pos++;
                break;

            case 'n':
                options.disableOpt = true;
                pos++;
                break;

            //  as65: "Ignore case in opcodes. In this way, the assembler does
            //  not differentiate between `adc` and `ADC`, for example. Labels
            //  are still case sensitive."
            //
            //  ACCEPTED AND RECORDED NOWHERE, because there is nothing to
            //  record. This assembler folds opcode case unconditionally and
            //  keeps labels case-sensitive, which is precisely what the flag
            //  asks for -- so honoring it is a no-op in the strict sense: the
            //  behavior is already the one requested.
            //
            //  IT USED TO SET A FIELD, and the field was the problem. Nothing
            //  read it, nothing could read it usefully, and a stored `true`
            //  sitting in two structs invited somebody to implement a
            //  conditional case-folding that the assembler does not need and
            //  cannot want. `-n` is the flag with real work behind it.
            case 'i':
                pos++;
                break;

            //  as65: "Use 65SC02 extensions. This CPU has several additional
            //  instructions. When this option is not specified the assembler
            //  rejects the 65SC02 extensions." One instruction set, reachable
            //  by the name as65 gave it.
            //
            //  IT IS THE ONLY FORM NOW. `--cpu` selected the same
            //  instruction set under a name as65 never had, so the tool carried
            //  two ways to ask for one thing and the as65-shaped one was the
            //  one an as65 user would reach for. `--cpu` is answered by name
            //  above rather than left to the generic refusal.
            case 'x':
                options.cpuTarget = CommandLineOptions::CpuTarget::M65C02;
                pos++;
                break;

            case 'p':
                options.pass1Listing = true;
                pos++;
                break;

            case 'z':
                options.fillZero = true;
                options.fillByte = 0x00;
                pos++;
                break;

            //  as65: "-g   Generate source-level debug information file. This
            //  file can then be used in in-system debugging or a software
            //  simulator." THAT IS THE WHOLE ENTRY -- the flag takes NO
            //  parameter, and the filename, its extension and its format are
            //  all undocumented. So it names nothing, takes nothing, and other
            //  options may follow it in a group exactly as they follow -t.
            //
            //  BOTH `-g <file>` AND `-g<file>` ARE GONE. They were added here,
            //  and removing them takes away a capability as65 never had rather
            //  than matching one it did; naming the debug file will need a
            //  form of this project's own if it is wanted back. The derived
            //  name -- the source with a .dbg extension -- is what remains, and
            //  it is what a bare -g always produced.
            case 'g':
                options.debugInfo = true;
                pos++;
                break;

            //  as65: "Define a label before the first source line is read. If
            //  no name is specified, DEBUG is defined. The label is EQUated to
            //  be 1."
            //
            //  THE NAME IS GLUED, AND -d NEVER CONSUMES THE ARGUMENT AFTER IT.
            //  as65 notates it `-d<name>`, and its parameter is a STRING, which
            //  is the case its concatenation rule singles out: "no other option
            //  can follow one that may have a string parameter". So the flag
            //  takes the rest of its own argument and stops there -- a bare -d
            //  is the DEBUG default and nothing else.
            //
            //  A SEPARATED `-d NAME` WAS THIS TOOL'S INVENTION AND IS GONE. It
            //  took whatever followed, which meant `casso -d demo.a65` defined
            //  a label called `demo.a65` and left no input file; the repair
            //  was a heuristic -- take the next argument unless it looks like a
            //  flag or like a source file -- and as65 has no such rule because
            //  the glued form never needs one. `casso -d prog.a65` defines
            //  DEBUG and assembles prog.a65 on the plain reading.
            //  THE `=VALUE` HALF IS THIS TOOL'S OWN, and a value it cannot read
            //  is refused rather than replaced. It used to fall back to 1 in
            //  silence, which is the worst of the three possible answers: `-d
            //  VER=1.0` defined VER as 1 and `-dADDR=$6000` defined ADDR as 1,
            //  each of them assembling a source that then took a branch nobody
            //  chose. A NAME with no value is 1 because as65 says so; a value
            //  that was typed and not understood is a refusal.
            //
            //  The whole text after the `=` has to be consumed, so a trailing
            //  fragment cannot be dropped either -- 1.0 is not 1.
            case 'd':
            {
                std::string  def   = rest;
                size_t       eqPos = 0;
                std::string  name;
                int32_t      value = 1;
                bool         taken = true;



                if (def.empty())
                {
                    def = "DEBUG";
                }

                eqPos = def.find ('=');

                if (eqPos != std::string::npos)
                {
                    name = def.substr (0, eqPos);
                    std::string    valStr = def.substr (eqPos + 1);
                    char         * end    = nullptr;
                    long           v      = strtol (valStr.c_str(), &end, 0);

                    taken = !valStr.empty() && end != nullptr && *end == '\0';

                    if (taken)
                    {
                        value = (int32_t) v;
                    }
                    else
                    {
                        std::cerr << "Error: " << options.flagPrefix << "d cannot read `"
                                  << valStr << "` as a value.\n"
                                  << "       Write it as a decimal or 0x-prefixed number, or leave the\n"
                                  << "       `=` off entirely -- a name on its own is defined as 1.\n";
                    }
                }
                else
                {
                    name = def;
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

                pos = arg.size();
                break;
            }

            case 's':
                // -s = S-record output (.s19), -s2 = Intel HEX output (.hex)
                options.outputFormatNamed = true;

                if (!rest.empty() && rest[0] == '2')
                {
                    options.outputFormat = CommandLineOptions::OutputFormat::IntelHex;

                    if (rest.size() > 1)
                    {
                        options.outputFile = rest.substr (1);
                    }
                }
                else
                {
                    options.outputFormat = CommandLineOptions::OutputFormat::SRecord;

                    if (!rest.empty())
                    {
                        options.outputFile = rest;
                    }
                }

                pos = arg.size();
                break;

            //  AN ILLEGAL OPTION ENDS THE COMMAND LINE. as65's DIAGNOSTICS:
            //  "Help message if only parameter is a question mark, or if an
            //  illegal option has been specified." Usage is printed and
            //  nothing is assembled.
            //
            //  THIS REVERSES A SHIPPED DECISION, by owner ruling, choosing
            //  parity over the behavior that was here: the flag was dropped
            //  with a warning, the assembly ran, the output was written, and
            //  the status was 1. The cost of that was a build whose makefile
            //  passed a flag this assembler does not have and got a binary
            //  shaped by the flags that were left -- reported under the same
            //  status an ordinary assembler warning earns.
            //
            //  THE ASSEMBLER'S PAGE IS THE ONE PRINTED, because the assembler's
            //  grammar is the one that was violated. The complaint goes to
            //  stderr and the page to stdout, so a caller redirecting either
            //  one keeps the sentence rather than losing it above the page.
            default:
                std::cerr << "Error: unknown option: " << options.flagPrefix << flag << "\n";
                options.showHelp     = true;
                options.helpPage     = CommandLineOptions::HelpPage::Assemble;
                options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
                pos                  = arg.size();
                stop                 = true;
                break;
            }
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
        else if (arg == "--fill" && argIndex + 1 < argc)
        {
            hr = ParseFillByte (argv[++argIndex], options.fillByte);

            if (FAILED (hr))
            {
                std::cerr << "Error: Invalid fill byte value\n";
                options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            }
        }
        else if (arg == "--load" && argIndex + 1 < argc)
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
        else if (arg == "--entry" && argIndex + 1 < argc)
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
        else if (arg == "--stop" && argIndex + 1 < argc)
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
        else if (arg == "--max-cycles" && argIndex + 1 < argc)
        {
            hr = ParseDecimal (argv[++argIndex], options.maxCycles);

            if (FAILED (hr))
            {
                std::cerr << "Error: Invalid max-cycles value\n";
                options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            }
        }
        else if (arg == "--reset-vector")
        {
            options.useResetVector = true;
        }
        else if (arg == "--warn")
        {
            options.warningMode = WarningMode::Warn;
        }
        else if (arg == "--no-warn")
        {
            options.warningMode = WarningMode::NoWarn;
        }
        else if (arg == "--fatal-warnings")
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
        }

        argIndex++;
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
//
//  An unrecognized first argument is NOT an error -- it is a source filename,
//  which is exactly how as65 was invoked -- so AS65 is the fallback rather
//  than a named mode. Adding a subcommand means adding a row to the table and
//  an arm below; the fallback is unaffected.
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
        options.flagPrefix = '/';
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

        //  A LONE `?` OPENS THE ASSEMBLER'S PAGE, and is the only thing that
        //  does. It is as65's own usage request, and assembling IS as65 mode,
        //  so the request lands on the page describing the grammar it comes
        //  from. Every other form asks for the general page. The `argc == 2`
        //  condition is already spent above, so a `?` reaching here was the
        //  whole command line.
        if (first == "?")
        {
            options.helpPage = CommandLineOptions::HelpPage::Assemble;
        }
    }
    else if (isVer)
    {
        options.subcommand  = CommandLineOptions::Subcommand::Version;
        options.showVersion = true;
    }

    BAIL_OUT_IF (isHelp || isVer, S_OK);

    named  = LookUpSubcommand (first);
    isAs65 = named == CommandLineOptions::Subcommand::None;

    if (isAs65)
    {
        ParseAs65Flags    (argc, argv, options);
        ApplyAs65Defaults (options, fileExists);
    }

    // AS65 mode consumed the whole command line above; only a named
    // subcommand continues into its own option parser.
    BAIL_OUT_IF (isAs65, S_OK);

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
