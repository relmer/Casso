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
//  Every LONG option spelling, without a prefix, in the two grammars that have
//  any.
//
//  THESE TABLES EXIST TO MAKE `/` A REAL PREFIX RATHER THAN A PRINTED ONE. The
//  usage text spells every flag with whichever prefix the reader asked for, and
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


static constexpr const char *  s_kpszAs65LongOptions[] =
{
    "cpu",
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
//  Every spelling the top level accepts is accepted here, because a reader who
//  learned `--help` from one command line will type it on the next one and a
//  subcommand that answers only its own spelling is a trap. The `/` forms are
//  included for the same reason the option tables carry them: the help spells
//  itself with whichever prefix was typed, so both prefixes have to work.
//
//  MATCHED EXACTLY AND IN LOWER CASE, which is what keeps a ProDOS path out of
//  it. `/HELP` is a legal volume path and stays an operand; only the lowercase
//  flag spelling a person types at a shell is read as a request.
//
//  THE ONE TOP-LEVEL SPELLING MISSING HERE IS A BARE `?`, and it is missing
//  because the condition that makes it a request cannot hold inside a
//  subcommand. as65 asks for a question mark that is the ONLY parameter; every
//  argument this function judges has a verb in front of it, so a `?` reaching
//  here is somebody's operand -- a file on a disk is allowed to be called that.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::IsHelpRequest (const std::string & arg)
{
    return arg == "--help" || arg == "-help" || arg == "-?" ||
           arg == "/help"  || arg == "/?";
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
//  CanonicalLongFlag
//
//  An argument reduced to the one spelling the grammars below test for, so
//  `/out` and `--out` reach the same arm.
//
//  ONLY AN EXACT OPTION NAME IS REWRITTEN. A ProDOS path is `/VOLUME/FILE` and
//  is an operand; rewriting every leading slash would turn one into a flag and
//  lose it. Anything not in the table comes back untouched, which is what lets
//  a caller pass one string through for both flags and positionals.
//
//  An attached value is carried across, because `--cpu=65c02` is a spelling the
//  assembler grammar accepts and `/cpu=65c02` therefore has to be one too. The
//  name is matched against the part BEFORE the `=` for exactly that reason.
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
//  spells every flag with whichever prefix the reader asked for and offering a
//  spelling the parser rejects is worse than never offering it. See
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
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::ParseDiskOptions (
    int                   argc,
    char               *  argv[],
    int                   argIndex,
    CommandLineOptions &  options)
{
    int   i          = argIndex;
    int   positional = 0;
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

        if (arg == "--addr" && hasValue)
        {
            Word     address = 0;
            HRESULT  hr      = ParseAddress (argv[i + 1], address);

            if (SUCCEEDED (hr))
            {
                options.disk.loadAddress    = address;
                options.disk.hasLoadAddress = true;
            }

            i++;
            continue;
        }

        //  A DASH INTRODUCES A FLAG AND NOTHING ELSE, so one this grammar does
        //  not have is refused rather than counted as an operand.
        //
        //  Only a dash. A ProDOS path is spelled `/VOLUME/FILE` and is an
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
//  Spelled with `--` regardless of what the caller typed. Both prefixes are
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
//  suffix, so the function is correct regardless of how the call site spells
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
//                    back spelled the way they type
//    attached values a flag's argument may be glued to it or separated
//
//  Which is why this is a hand-rolled walk rather than a table: a table-driven
//  parser would have to encode all three exceptions anyway, and every one of
//  them is about matching a specific historical tool.
//
//  CONCATENATION IS CONFINED TO THIS FUNCTION and belongs to as65 alone. It is
//  the reason `-lsc` is three flags rather than one unknown one, and it is also
//  the reason `-o` swallows whatever follows it -- neither is a property anyone
//  would design in today. `run` and `disk` are modern grammars parsed
//  elsewhere; neither packs, and the help says so under Assembly options rather
//  than as a claim about the tool.
//
//  The stop flag ends parsing outright for a help request, a bad --cpu target,
//  or a `--` option this grammar does not have, so no later argument can
//  quietly undo the decision. The first two also set showHelp, which is what
//  puts the usage page on the screen; the third deliberately does not -- see
//  the refusal itself for why.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::ParseAs65Flags (int argc, char * argv[], CommandLineOptions & options)
{
    int   argIndex = 1;
    // Set when an argument ends parsing outright -- a help request, or a bad
    // --cpu target. Both leave showHelp set, so the caller prints usage.
    bool  stop     = false;

    options.subcommand = CommandLineOptions::Subcommand::As65;



    while (argIndex < argc && !stop)
    {
        // A `/` spelling of a long option is canonicalized before anything
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

        // Long option: --cpu <target> / --cpu=<target> selects the target
        // instruction set. Default stays 6502 so 65C02-only opcodes never assemble
        // by accident; only an explicit --cpu 65c02 unlocks the CMOS tier.
        if (arg == "--cpu" || arg.rfind ("--cpu=", 0) == 0)
        {
            std::string val;

            if (arg == "--cpu")
            {
                if (argIndex + 1 < argc)
                {
                    val = argv[++argIndex];
                }
            }
            else
            {
                val = arg.substr (6);   // after "--cpu="
            }

            for (char & c : val)
            {
                c = (char) tolower ((unsigned char) c);
            }

            if (val == "6502")
            {
                options.cpuTarget = CommandLineOptions::CpuTarget::M6502;
            }
            else if (val == "65c02")
            {
                options.cpuTarget = CommandLineOptions::CpuTarget::M65C02;
            }
            else
            {
                std::cerr << "Error: unknown --cpu target '" << val
                          << "' (expected 6502 or 65c02)\n";
                options.showHelp     = true;
                options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
                stop                 = true;
            }

            if (!stop)
            {
                argIndex++;
            }

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
        //  spellings deliberately fall through instead -- `/oFILE` is the glued
        //  form as65 documents, so `/out` genuinely does mean `-o ut` and must
        //  keep meaning it.
        //
        //  showHelp IS DELIBERATELY NOT SET, which is where this parts company
        //  with the bad-`--cpu` refusal below it. That one prints the whole
        //  usage page, and the usage page is 180 lines: the sentence explaining
        //  the mistake scrolls away above it, so the reader is answered by
        //  being buried. The two lines here say what was wrong and what to type
        //  instead, which is the entire content that page would have added.
        if (arg.rfind ("--", 0) == 0)
        {
            std::cerr << "Error: unknown option: " << arg << "\n"
                      << "       Assembling takes single-letter flags -- the output file\n"
                      << "       is -o <file>, and --out belongs to `disk`.\n";

            options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;
            stop                 = true;
            continue;
        }

        // Normalize / prefix to - for flag parsing
        if (arg[0] == '/')
        {
            arg[0] = '-';
        }

        // Non-flag argument is the input file
        if (arg[0] != '-' && arg[0] != '/')
        {
            if (options.inputFile.empty())
            {
                options.inputFile = arg;
            }

            argIndex++;
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

            case 'l':
                options.generateListing = true;

                if (!rest.empty())
                {
                    options.listingFile = rest;
                    pos = arg.size();
                }
                else if (argIndex + 1 < argc && argv[argIndex + 1][0] != '-' && argv[argIndex + 1][0] != '/')
                {
                    options.listingFile = argv[++argIndex];
                    pos = arg.size();
                }
                else
                {
                    options.listingToStdout = true;
                    pos++;
                }

                break;

            case 'o':
                //  The final `pos++` is what keeps a trailing `-o` from
                //  hanging the tool. With nothing glued to the flag and
                //  nothing after it, neither branch below used to run and
                //  neither advanced the walk, so the enclosing loop reread
                //  the same character forever: `casso demo.a65 -o` never
                //  returned, printed nothing and could only be killed.
                if (!rest.empty())
                {
                    options.outputFile = rest;
                    pos = arg.size();
                }
                else if (argIndex + 1 < argc)
                {
                    options.outputFile = argv[++argIndex];
                    pos = arg.size();
                }
                else
                {
                    pos++;
                }

                break;

            case 'm':
                options.macroExpansion = true;
                pos++;
                break;

            case 'h':
            {
                int  height = options.pageHeight;

                if (TakeCountValue (argc, argv, argIndex, rest, height))
                {
                    options.pageHeight = height;
                    pos = arg.size();
                }
                else
                {
                    pos++;
                }

                break;
            }

            case 'w':
            {
                //  A bare -w is not "no width given" the way a bare -h is "no
                //  pagination": it selects the wide listing, which is the only
                //  reason to type the flag without a number.
                int  width = kWideListingColumns;

                if (TakeCountValue (argc, argv, argIndex, rest, width))
                {
                    pos = arg.size();
                }
                else
                {
                    pos++;
                }

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

            case 'i':
                options.ignoreOpcodeCase = true;
                pos++;
                break;

            //  as65: "Use 65SC02 extensions. This CPU has several additional
            //  instructions. When this option is not specified the assembler
            //  rejects the 65SC02 extensions." That is the same tier `--cpu
            //  65c02` already selects, so this selects it and nothing else --
            //  one instruction set, reachable by the name as65 gave it and by
            //  the name this tool gave it.
            //
            //  BOTH SPELLINGS STAY. `--cpu` is the one another feature builds
            //  on, and withdrawing it to leave a single as65-shaped switch
            //  would break work in flight for the sake of a tidiness nobody
            //  asked for.
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

            case 'g':
                options.debugInfo = true;

                //  Named either way, on the same rule -l already uses: glued
                //  to the flag, or the next argument when that is not itself a
                //  flag. Accepting only the glued form meant `-g out.dbg`
                //  wrote the derived name and dropped `out.dbg` without a word.
                if (!rest.empty())
                {
                    options.debugFile = rest;
                    pos = arg.size();
                }
                else if (argIndex + 1 < argc && argv[argIndex + 1][0] != '-' && argv[argIndex + 1][0] != '/')
                {
                    options.debugFile = argv[++argIndex];
                    pos = arg.size();
                }
                else
                {
                    pos++;
                }

                break;

            //  as65: "Define a label before the first source line is read. If
            //  no name is specified, DEBUG is defined. The label is EQUated to
            //  be 1."
            //
            //  A BARE -d IS THAT DEFAULT, not an invitation to eat the next
            //  argument. It used to take whatever followed unconditionally, and
            //  the two things that usually follow are the source file and
            //  another flag: `casso -d demo.a65` defined a label called
            //  `demo.a65`, left no input file, and exited saying none was
            //  given, while `casso demo.a65 -d -o out.bin` defined a label
            //  called `-o`, dropped the output name and wrote the derived one.
            //  Neither defined DEBUG, which is the only thing a bare -d was
            //  ever asking for.
            //
            //  A separated name still works, because it always has here and a
            //  caller relying on it is not wrong. What it may not be is a
            //  source file: that test is the one the glued-only form of as65
            //  never needed, and it is the same test ApplyAs65Defaults uses to
            //  recognize an input.
            case 'd':
            {
                std::string  def   = rest;
                size_t       eqPos = 0;
                std::string  name;
                int32_t      value = 1;



                if (def.empty() && TakesSeparatedSymbolName (argc, argv, argIndex))
                {
                    def = argv[++argIndex];
                }

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

                    if (end != valStr.c_str())
                    {
                        value = (int32_t) v;
                    }
                }
                else
                {
                    name = def;
                }

                if (!name.empty())
                {
                    options.predefinedSymbols[name] = value;
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

            default:
                //  A complaint rather than a refusal: the flag is dropped and
                //  the assembly still runs and still writes its output, which
                //  is exactly what exit status 1 means here.
                std::cerr << "Warning: Unknown flag: -" << flag << "\n";
                options.parseVerdict = CommandLineOptions::ParseVerdict::Complaint;
                pos++;
                break;
            }
        }

        argIndex++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeCountValue
//
//  Reads the numeric argument of a concatenable AS65 flag, glued to the flag or
//  standing next to it, and says whether one was there.
//
//  BOTH SPELLINGS, because the usage text has always documented the separated
//  one -- `-h <lines>` -- and only the glued one was ever read. `-h 10` set
//  nothing and reported nothing, so the number went to the assembler's input
//  path resolution and the listing came out exactly as it would have with no
//  flag at all.
//
//  A separated value is taken only when it PARSES AS A NUMBER, which is what
//  distinguishes it from the next thing on the command line. `-h` before a
//  source file must not eat the source file, and `-w -v` must not eat the -v;
//  neither is a count, so neither is taken.
//
//  A glued value is consumed whether or not it parses, matching what the flag
//  did before: the characters after the flag are its argument by position, and
//  handing an unreadable one back to the concatenation walk would read it as
//  more flags.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::TakeCountValue (int                   argc,
                                        char               *  argv[],
                                        int                &  argIndex,
                                        const std::string  &  attached,
                                        int                &  value)
{
    HRESULT   hr        = S_OK;
    uint32_t  parsed    = 0;
    bool      hasNext   = (argIndex + 1) < argc;
    bool      isNumeric = false;
    bool      wasTaken  = false;



    if (!attached.empty())
    {
        hr = ParseDecimal (attached.c_str(), parsed);

        if (SUCCEEDED (hr))
        {
            value = (int) parsed;
        }

        wasTaken = true;
    }
    else if (hasNext)
    {
        hr        = ParseDecimal (argv[argIndex + 1], parsed);
        isNumeric = SUCCEEDED (hr);

        if (isNumeric)
        {
            value    = (int) parsed;
            wasTaken = true;
            argIndex++;
        }
    }

    return wasTaken;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakesSeparatedSymbolName
//
//  Whether the argument standing after a bare -d is a name for it to define.
//
//  TWO THINGS IT IS NOT, and a bare -d used to take both. A flag is not a
//  symbol name -- `demo.a65 -d -o out.bin` defined a label called `-o` and then
//  lost the output name, because the argument that should have been -o's had
//  already been spent. And a source file is not one either: `-d demo.a65`
//  defined `demo.a65` and left the run with no input at all.
//
//  The source-file test is by extension rather than by asking the filesystem,
//  which is the same test that recognizes an input everywhere else here, and it
//  is deliberately the weaker of the two. A file that exists is not the
//  question -- a build script naming a source that has not been generated yet
//  is still naming a source -- and a parser that consults the disk gives a
//  different reading of the same command line on two machines.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::TakesSeparatedSymbolName (int argc, char * argv[], int argIndex)
{
    const char *  next     = nullptr;
    bool          hasNext  = (argIndex + 1) < argc;
    bool          isTaken  = false;



    if (hasNext)
    {
        next    = argv[argIndex + 1];
        isTaken = next[0] != '-' && next[0] != '/' && !IsAssemblySource (next);
    }

    return isTaken;
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
//  writes an S-record without a second flag, and `-g` alone yields a .dbg
//  beside the source.
//
//  Each name is only inferred when the user did not supply it, so an explicit
//  -o or -g always wins.
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
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::ParseRunOptions (int argc, char * argv[], int argIndex, CommandLineOptions & options)
{
    HRESULT  hr = S_OK;



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
//  chosen prefix in flagPrefix so usage text is spelled back the way they type.
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



    if (argc < 2)
    {
        options.showHelp = true;
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
