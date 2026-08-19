#include "Pch.h"

#include "CommandLineParser.h"

#include "DialectProfile.h"
#include "DialectRegistry.h"





//
//  Every bare-word subcommand. Anything not in this table is rejected -- so a
//  new subcommand is a row here plus an arm in Parse, not a reshaped
//  dispatcher.
//
static constexpr CommandLineParser::SubcommandName  s_kSubcommands[] =
{
    { "as65",   CommandLineOptions::Subcommand::As65   },
    { "merlin", CommandLineOptions::Subcommand::Merlin },
    { "run",    CommandLineOptions::Subcommand::Run    },
};


//
//  The Merlin grammar's flags. Short on purpose: Merlin's source answers in
//  itself most of what as65 answers with a flag -- the object's name, the CPU --
//  so what remains here is what only the invocation can say.
//
static constexpr CommandLineParser::DialectFlag  s_kMerlinFlags[] =
{
    { 'o', CommandLineParser::FlagArgument::Required, "<file>",
           "Output file, which beats any name the source gives itself" },
    { 'l', CommandLineParser::FlagArgument::Optional, "[<file>]",
           "Generate listing; with no filename attached it goes to stdout" },
    { 'v', CommandLineParser::FlagArgument::None,     "",
           "Verbose mode" },

    //  Merlin asks the operator for a keyboard-input symbol and waits. A batch
    //  assembly has nobody to ask, so the answer has to arrive with the
    //  invocation -- and without this row the three vendor sources that ask
    //  questions cannot be assembled from a command line at all.
    { 'd', CommandLineParser::FlagArgument::Required, "<symbol>[=<value>]",
           "Answer a symbol the source asks for; a bare symbol answers 1" },
};


//
//  Which dialect each flag table belongs to. as65 has no row: its grammar is a
//  hand-rolled walk over a historical command line rather than a table, and
//  claiming a table it does not walk would be the drift this exists to prevent.
//
static constexpr CommandLineParser::DialectFlagTable  s_kDialectFlags[] =
{
    { DialectId::Merlin, s_kMerlinFlags, std::size (s_kMerlinFlags) },
};


//
//  The output shapes the merlin grammar names.
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
//  as65 has no row. Its grammar is a hand-rolled walk that spells --raw and
//  --dos-bin inline, and its own usage block documents them, so a row here
//  would be a second description of a tool this table does not drive.
//
static constexpr CommandLineParser::OutputShape  s_kMerlinOutputShapes[] =
{
    { "--dos-bin", CommandLineOptions::OutputFormat::DosBinary,
                   "Write the bytes behind a 4-byte DOS 3.3 header carrying origin and length" },
    { "--flat",    CommandLineOptions::OutputFormat::Binary,
                   "Write a full 64 KB image with the bytes at their origin" },
};


//  Which dialect an output-shape table belongs to, on the same principle as the
//  flag tables above: a dialect offering no choice simply has no row.
static constexpr CommandLineParser::OutputShapeTable  s_kOutputShapeTables[] =
{
    { DialectId::Merlin, s_kMerlinOutputShapes, std::size (s_kMerlinOutputShapes) },
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
//  GetAllSubcommands
//
////////////////////////////////////////////////////////////////////////////////

std::span<const CommandLineParser::SubcommandName> CommandLineParser::GetAllSubcommands()
{
    return std::span<const SubcommandName> (s_kSubcommands);
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
//  CommandLineParser::GetOutputShapes
//
////////////////////////////////////////////////////////////////////////////////

std::span<const CommandLineParser::OutputShape> CommandLineParser::GetOutputShapes (DialectId dialect)
{
    std::span<const OutputShape>  shapes;



    for (const OutputShapeTable & table : s_kOutputShapeTables)
    {
        if (table.dialect == dialect)
        {
            shapes = std::span<const OutputShape> (table.shapes, table.count);
            break;
        }
    }

    return shapes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineParser::ApplyOutputShape
//
//  Selects the output shape an argument names, if it names one.
//
//  Returns whether the argument was consumed, so a grammar that offers no
//  shapes -- an empty table -- consumes nothing and its parse is unchanged.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandLineParser::ApplyOutputShape (const std::string & arg, DialectId dialect, CommandLineOptions & options)
{
    bool  matched = false;



    for (const OutputShape & shape : GetOutputShapes (dialect))
    {
        if (arg == shape.spelling)
        {
            options.outputFormat = shape.format;
            matched              = true;
            break;
        }
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
//                    back spelled the way they type
//    attached values a flag's argument may be glued to it or separated
//
//  Which is why this is a hand-rolled walk rather than a table: a table-driven
//  parser would have to encode all three exceptions anyway, and every one of
//  them is about matching a specific historical tool.
//
//  The stop flag ends parsing outright for a help request or a bad --cpu
//  target. Both leave showHelp set, so the caller prints usage and no later
//  argument can quietly undo that decision.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::ParseAs65Flags (int argc, char * argv[], int startIndex, CommandLineOptions & options)
{
    int   argIndex = startIndex;
    // Set when an argument ends parsing outright -- a help request, or a bad
    // --cpu target. Both leave showHelp set, so the caller prints usage.
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

            // Recognized by every grammar; honored only where the active
            // dialect's profile says the CPU comes from the command line.
            //
            // This call can never refuse anything TODAY -- the only dialect
            // reaching it says its CPU comes from the command line -- and
            // deleting it is a mutation no test catches. It stays because it is
            // what makes the refusal a property of the mechanism rather than of
            // one grammar: flip this dialect's profile and the refusal happens
            // here too, which is the registry sweep's whole claim. Without the
            // call, that sweep would be true of a parser with a per-dialect arm.
            refused = RefuseCpuFlagWhereSelectedInSource (options);
            stop    = refused;

            if (!refused && val == "6502")
            {
                options.cpuTarget    = CommandLineOptions::CpuTarget::M6502;
                options.hasCpuTarget = true;
            }
            else if (!refused && val == "65c02")
            {
                options.cpuTarget    = CommandLineOptions::CpuTarget::M65C02;
                options.hasCpuTarget = true;
            }
            else if (!refused)
            {
                std::cerr << "Error: unknown --cpu target '" << val
                          << "' (expected 6502 or 65c02)\n";
                options.showHelp = true;
                stop             = true;
            }

            if (!stop)
            {
                argIndex++;
            }

            continue;
        }

        // Long options selecting a binary output SHAPE. The default stays the
        // as65 full-64-KB image, so an invocation that names neither is
        // unaffected.
        if (arg == "--raw")
        {
            options.outputFormat = CommandLineOptions::OutputFormat::Raw;
            argIndex++;
            continue;
        }

        if (arg == "--dos-bin")
        {
            options.outputFormat = CommandLineOptions::OutputFormat::DosBinary;
            argIndex++;
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

                break;

            case 'm':
                options.macroExpansion = true;
                pos++;
                break;

            case 'h':
            {
                if (!rest.empty())
                {
                    uint32_t val = 0;
                    HRESULT  hr  = ParseDecimal (rest.c_str(), val);

                    if (SUCCEEDED (hr))
                    {
                        options.pageHeight = (int) val;
                    }

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
                if (!rest.empty())
                {
                    uint32_t val = 0;
                    HRESULT  hr  = ParseDecimal (rest.c_str(), val);

                    if (SUCCEEDED (hr))
                    {
                        options.pageWidth = (int) val;
                    }

                    pos = arg.size();
                }
                else
                {
                    pos++;
                }

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
                options.caseSensitive = true;
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

                if (!rest.empty())
                {
                    options.debugFile = rest;
                    pos = arg.size();
                }
                else
                {
                    pos++;
                }

                break;

            case 'd':
            {
                std::string def = rest;

                if (def.empty() && argIndex + 1 < argc)
                {
                    def = argv[++argIndex];
                }

                if (!def.empty())
                {
                    size_t       eqPos = def.find ('=');
                    std::string  name;
                    int32_t      value = 1;

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
                }

                pos = arg.size();
                break;
            }

            case 's':
                // -s = S-record output (.s19), -s2 = Intel HEX output (.hex)
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
                std::cerr << "Warning: Unknown flag: -" << flag << "\n";
                pos++;
                break;
            }
        }

        argIndex++;
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
        options.cpuFlagRefusal = std::string ("--cpu is not accepted for ") + profile.GetName()
                               + ": the CPU target is selected in the source, with the "
                               + profile.GetCpuDirectiveName() + " directive.";
    }

    return isInSource;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindMerlinFlag
//
////////////////////////////////////////////////////////////////////////////////

const CommandLineParser::DialectFlag * CommandLineParser::FindMerlinFlag (char letter)
{
    const DialectFlag *  found = nullptr;



    for (const DialectFlag & flag : s_kMerlinFlags)
    {
        if (flag.letter == letter)
        {
            found = &flag;
            break;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddSymbolDefinition
//
//  Records one `NAME=VALUE` definition, which is how an answer the source asks
//  for arrives when there is no operator to ask.
//
//  A bare name answers 1, so a source testing only whether a symbol was given
//  needs no value typed. A value that will not convert leaves that 1 in place
//  rather than becoming zero, because zero is a meaningful answer in these
//  sources and inventing it from a typo would assemble a different object
//  silently.
//
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::AddSymbolDefinition (const std::string & definition, CommandLineOptions & options)
{
    size_t       equals = definition.find ('=');
    std::string  name   = definition;
    int32_t      value  = 1;



    if (equals != std::string::npos)
    {
        std::string    text = definition.substr (equals + 1);
        char         * end  = nullptr;
        long           read = strtol (text.c_str(), &end, 0);

        name = definition.substr (0, equals);

        if (end != text.c_str())
        {
            value = (int32_t) read;
        }
    }

    if (!name.empty())
    {
        options.predefinedSymbols[name] = value;
    }
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
                                         CommandLineOptions & options)
{
    bool  applied = true;



    switch (letter)
    {
    case 'o':
        options.outputFile = value;
        break;

    case 'l':
        options.generateListing = true;
        options.listingFile     = value;
        options.listingToStdout = value.empty();
        break;

    case 'v':
        options.verbose = true;
        break;

    case 'd':
        AddSymbolDefinition (value, options);
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
//  A listing filename must be ATTACHED, unlike the as65 spelling that also
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
        size_t       pos = 1;

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

        // The CPU flag is recognized by every grammar and honored by the ones
        // whose dialect takes its CPU from the command line. Whether this is one
        // of them is the profile's answer, not this parser's.
        if (arg == "--cpu" || arg.rfind ("--cpu=", 0) == 0)
        {
            if (arg == "--cpu" && argIndex + 1 < argc)
            {
                argIndex++;
            }

            stop = RefuseCpuFlagWhereSelectedInSource (options);

            if (!stop)
            {
                argIndex++;
            }

            continue;
        }

        // An output shape, matched against the same table the help text is
        // composed from. Placed beside the CPU flag rather than in the letter
        // loop below, because these are whole words: a letter loop would read
        // --flat as -f -l -a -t and warn four times about flags nobody wrote.
        if (ApplyOutputShape (arg, DialectId::Merlin, options))
        {
            argIndex++;
            continue;
        }

        if (arg[0] == '/')
        {
            options.flagPrefix = '/';
            arg[0]             = '-';
        }

        if (arg[0] != '-')
        {
            if (options.inputFile.empty())
            {
                options.inputFile = arg;
            }

            argIndex++;
            continue;
        }

        while (pos < arg.size())
        {
            const DialectFlag  *  flag    = FindMerlinFlag (arg[pos]);
            std::string           rest    = arg.substr (pos + 1);
            std::string           value;
            bool                  known   = flag != nullptr;
            bool                  applied = false;

            if (!known)
            {
                std::cerr << "Warning: Unknown flag: -" << arg[pos] << "\n";
                pos++;
                continue;
            }

            if (flag->argument == FlagArgument::Required)
            {
                value = rest;

                if (value.empty() && argIndex + 1 < argc)
                {
                    value = argv[++argIndex];
                }

                pos = arg.size();
            }
            else if (flag->argument == FlagArgument::Optional)
            {
                value = rest;
                pos   = value.empty() ? pos + 1 : arg.size();
            }
            else
            {
                pos++;
            }

            applied = ApplyMerlinFlag (flag->letter, value, options);

            if (!applied)
            {
                std::cerr << "Warning: Unknown flag: -" << flag->letter << "\n";
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
////////////////////////////////////////////////////////////////////////////////

void CommandLineParser::ParseRunOptions (int argc, char * argv[], int argIndex, CommandLineOptions & options)
{
    HRESULT  hr = S_OK;



    while (argIndex < argc)
    {
        std::string arg (argv[argIndex]);

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
            }
        }
        else if (arg == "--max-cycles" && argIndex + 1 < argc)
        {
            hr = ParseDecimal (argv[++argIndex], options.maxCycles);

            if (FAILED (hr))
            {
                std::cerr << "Error: Invalid max-cycles value\n";
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
    bool                            isMerlin = false;



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

    isHelp = first == "--help" || first == "-help" || first == "-h" || first == "-?";
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

Error:
    return options;
}
