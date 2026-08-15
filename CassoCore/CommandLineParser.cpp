#include "Pch.h"

#include "CommandLineParser.h"





//
//  Every bare-word subcommand. Anything not in this table is a source
//  filename, which puts the parser in AS65 mode -- so a new subcommand is a
//  row here plus an arm in Parse, not a reshaped dispatcher.
//
static constexpr CommandLineParser::SubcommandName  s_kSubcommands[] =
{
    { "run", CommandLineOptions::Subcommand::Run },
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
//  The stop flag ends parsing outright for a help request or a bad --cpu
//  target. Both leave showHelp set, so the caller prints usage and no later
//  argument can quietly undo that decision.
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

Error:
    return options;
}
