#pragma once

#include "AssemblerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandLineOptions
//
//  Everything the CLI's grammars can express, in one struct.
//
//  ONE struct for every subcommand, rather than a variant, because most fields
//  are shared and the subcommand already says which arm applies. A variant
//  would force every consumer to unpack before reading a field that means the
//  same thing either way.
//
//  That rationale holds for the assembler-shaped subcommands, which genuinely
//  do share an input file, an output file, a fill byte, and addresses. It does
//  NOT hold for `disk`: a command, an image path, a file name, and an encoding
//  selector mean nothing to any other subcommand, and nothing already here
//  means anything to `disk`. Flattening eight unrelated fields in would leave
//  the paragraph above technically present and quietly untrue -- "most fields
//  are shared" would stop being so, and the next subcommand would inherit a
//  struct that is half assembler options and half something else.
//
//  So `disk` nests. The grouping is visible at every use site, the shared-field
//  character of the top level survives, and the cost is one level of
//  indirection in code that is not hot.
//
//  Address fields are paired with has-flags because 0 is a legal address. A
//  load address of $0000 and no load address given are different requests, and
//  the value alone cannot tell them apart.
//
//  Defaults are the useful ones rather than zeroes -- $FF fill, $8000 load,
//  strict 6502 -- so an option omitted behaves the way as65 did.
//
//  flagPrefix records which prefix the USER typed, so usage text and
//  diagnostics come back written the way they invoked the tool.
//
//  parseVerdict records what the parser had to SAY about the command line, so
//  the exit code can reflect it. A parser that printed a complaint and left no
//  trace of it behind meant every mistyped flag reported success -- the
//  diagnostic reached the user's screen and never reached their build script.
//
//  Lives in the core library rather than beside the executable's main so the
//  UnitTest project can link the parser that fills it. The executable keeps
//  only the platform edge -- reading files, writing them, printing.
//
////////////////////////////////////////////////////////////////////////////////

struct CommandLineOptions
{
    enum class Subcommand    { None, Run, Help, Version, As65, Merlin, Disk };
    enum class CpuTarget     { M6502, M65C02 };

    //  What the parser made of the command line itself, apart from anything
    //  the assembler, the emulator or a disk found afterwards.
    //
    //  Clean means the command line was taken as written. Refused means nothing
    //  was attempted -- an option no grammar here has, a value that could not be
    //  read, or the EMPTY command line, where nothing was attempted because
    //  nothing was asked for. They map onto the two exit statuses a command line
    //  alone can decide: 0, and the 2 this tool documents as "produced no
    //  output".
    //
    //  THERE WAS A THIRD, `Complaint`, AND NOTHING PRODUCES IT NOW. It existed
    //  for one case -- an unrecognized assembler flag, dropped so the assembly
    //  could run on without it, reported as status 1. as65 prints usage and
    //  assembles nothing instead ("Help message if only parameter is a question
    //  mark, or if an illegal option has been specified"), parity won that by
    //  owner ruling, and the value was removed with the behavior rather than
    //  left as an arm nothing reaches. Status 1 is still spent, by
    //  As65ExitStatus, on an assembly that warned -- which is a fact about the
    //  SOURCE and was never a verdict on the command line.
    enum class ParseVerdict  { Clean, Refused };

    //  Which page a help request asks for.
    //
    //  THE HELP IS TIERED, so a request has to say which tier. One page
    //  describing three grammars ran to four screens, and a reader arrives
    //  already knowing which of the three things they came to do -- so the
    //  general page names the modes and the routes to them, and each mode's
    //  flags wait behind the route to that mode.
    //
    //  A LONE `?` ASKS FOR THE GENERAL PAGE, like every other form of the
    //  request at the top level. It opened the assembler's page while a bare
    //  source file still assembled, which made the top level as65 mode; the
    //  top level names a mode and runs nothing now, so it names no grammar.
    //
    //  `disk` IS ABSENT ON PURPOSE. Its help is a command of the disk grammar
    //  (DiskOptions::Command::Help) and is answered by the runner that answers
    //  every other disk command, which is what lets it be built and tested beside
    //  the code it describes.
    //
    //  ONE ARM PER GRAMMAR, Merlin included. Its flags, its examples and where
    //  its supported subset ends are all its own, and the dialect that answers
    //  `merlin --help` is not the one that answers `as65 --help`.
    enum class HelpPage      { General, Assemble, Merlin, Run };

    //
    //  Everything the `disk` subcommand expresses. Nested rather than
    //  flattened, per the note above.
    //
    struct DiskOptions
    {
        //  Help is not a command the grammar table holds -- it is what `disk
        //  --help` resolves to. Keeping it out of that table is deliberate:
        //  the table is swept to check every command is described in the help,
        //  and the help request is not one of the things being described.
        enum class Command      { None, List, Get, Put, Delete, Boot, Create, Init, Stamp, Help };

        //  How a payload's bytes relate to the file on the host. Verbatim means
        //  no CHARACTER conversion -- length and header semantics still apply,
        //  because those record where a file ends rather than transforming it.
        //
        //  IT IS THE DEFAULT AND NOTHING WRITES IT. There was a --verbatim
        //  flag, and once verbatim became the default the only thing it could
        //  still do was cancel a --text or --basic earlier on the same line --
        //  a combination nothing needs and no caller wrote. An option whose
        //  whole remaining purpose is to undo another option is one more thing
        //  to read in the help for no capability at all.
        enum class Encoding  { Verbatim, Text, Basic };

        Command   command  = Command::None;
        Encoding  encoding = Encoding::Verbatim;
        //  The command as it was typed, kept so a refusal can quote it. The
        //  enum above cannot: every word this grammar does not have maps
        //  to the same None, so "unknown disk command" could not say which.
        std::string  commandWord;

        std::string  imagePath;                        // the disk image
        std::string  path;                             // the file ON the disk
        std::string  hostFile;                         // source for put, --out for get
        std::string  typeName;                         // --type, as the user wrote it
        Word         loadAddress    = 0;               // --load
        bool         hasLoadAddress = false;           // $0000 is a legal address

        //
        //  What `create` and `init` are being asked to make.
        //
        //  Kept as the words the reader typed rather than as enums, because
        //  the parser has no business knowing which container types exist:
        //  the runner owns that table, refuses what is not in it, and can
        //  name the ones that are. `--type` is read only under `create`;
        //  `init` takes the container as it finds it.
        //
        std::string  containerType;                    // --type: dsk, do, po, woz
        std::string  formatName;                       // --format: dos33, prodos, none
        std::string  volumeName;                       // --volume: a DOS number or a ProDOS name

        //  TWO WAYS TO BOOT, AND THEY ARE NOT THE SAME MECHANISM, which is
        //  why they are two fields rather than one flag with a mode.
        //
        //  `--bootable <image>` copies an operating system onto the disk, so
        //  it needs a DOS 3.3 master or a ProDOS system disk to copy FROM.
        //  `--boot <binary>` writes no operating system at all: the boot
        //  sector loads the binary and jumps to it. Naming both is asking
        //  for a disk that boots two ways, and is refused.
        bool         bootable       = false;          // --bootable, with or without a path
        std::string  bootableFrom;                     // --bootable <os image>, when named
        std::string  directBootFile;                   // --boot <binary>

        //  --exec, for a direct-boot payload whose first byte is not its
        //  first instruction. A header, a jump table or a length word at the
        //  front is ordinary, and making the entry follow the load address
        //  would force such a payload to be rebuilt just to boot.
        Word         entryAddress    = 0;
        bool         hasEntryAddress = false;

        //
        //  Where `stamp` lays its bytes down: a track and a DOS LOGICAL
        //  sector, which is the numbering a source listing and a boot loader
        //  both speak. The physical position on the disk differs from it by
        //  the interleave, and translating between the two is the engine's
        //  job rather than the caller's.
        int          track           = 0;
        int          sector          = 0;
    };

    //  Raw -- the assembled bytes and nothing else -- is the default, because
    //  it is what somebody assembling a routine actually wants and what every
    //  other step in the build loop takes. NOTHING WRITES IT: a flag whose only
    //  effect is to select the default earns a line in the help and buys no
    //  capability, so the `--raw` that used to name it is gone. Binary is the
    //  full 64 KB padded image, which a ROM burner or a reference comparison
    //  wants and which needs --flat to ask for. THAT IS NOT WHAT AS65 WRITES:
    //  its binary runs from the lowest address the source used to the highest,
    //  the same span Raw holds, which is why Raw is the default here. DosBinary carries
    //  the Apple DOS 3.3 header. See OutputFormats for what each one writes.
    enum class OutputFormat  { Binary, SRecord, IntelHex, Raw, DosBinary };

    Subcommand    subcommand      = Subcommand::None;
    ParseVerdict  parseVerdict    = ParseVerdict::Clean;
    HelpPage      helpPage        = HelpPage::General;
    OutputFormat  outputFormat    = OutputFormat::Raw;

    //  Whether a shape flag was TYPED, as opposed to the default standing.
    //
    //  Separate from outputFormat because the value alone cannot answer it. The
    //  default used to be Binary and nothing written Binary, so "equals Binary"
    //  meant "nobody said" -- which is what let a `.s19` output file select an
    //  S-record when no flag was given. Raw is the default now, and a test
    //  against the value would call every remaining shape flag silence the
    //  moment one of them selected the default too.
    bool          outputFormatNamed = false;
    CpuTarget     cpuTarget       = CpuTarget::M6502;   // --cpu (default: strict 6502)
    std::string   inputFile;
    std::string   outputFile;
    std::string   listingFile;                          // -l<file> listing output file
    std::string   symbolFile;
    std::string   debugFile;                            // -g debug info output file
    Byte          fillByte        = 0xFF;
    Word          loadAddress     = 0x8000;
    Word          stopAddress     = 0;
    Word          entryAddress    = 0;
    uint32_t      maxCycles       = 0;
    bool          useResetVector  = false;
    bool          verbose         = false;
    bool          generateListing = false;
    bool          listingToStdout = false;
    WarningMode   warningMode     = WarningMode::Warn;
    bool          showVersion     = false;
    bool          showHelp        = false;

    //  The first word, when it named no subcommand. Empty otherwise. Carried
    //  rather than turned into a message here so the wording lives with the
    //  other user-facing text, and so a test can assert WHICH word was rejected.
    std::string   unrecognizedArgument;

    //  An argument AFTER a recognized subcommand that its grammar does not
    //  know, as typed. Empty otherwise. The subcommand is valid, so the help
    //  the edge prints is that mode's rather than the general one.
    std::string   unrecognizedFlag;

    //
    //  Why the command line was refused, in the words a user reads. Empty when
    //  it was not.
    //
    //  CARRIED RATHER THAN PRINTED, which it was not until this existed: the
    //  parser wrote each refusal straight to stderr, from a library that has no
    //  business owning a console. Two things fell out of that. Nothing could
    //  test a single one of them, because the test assembly reads what Parse
    //  RETURNS; and the executable could not put the message after the help it
    //  now prints, so a 98-line page would have scrolled the explanation off
    //  the top of the screen.
    //
    std::string   refusalMessage;

    bool          hasLoadAddress  = false;
    bool          hasStopAddress  = false;
    bool          hasEntryAddress = false;
    char          flagPrefix      = '-';                // '-' for Unix-style, '/' for Windows-style

    //  Live only when subcommand == Disk. Grouped so its boundary is visible
    //  at every use site rather than mixed into the assembler's fields.
    DiskOptions   disk;

    //  Whether a prefixed argument has been seen yet, which is what makes the
    //  FIRST one the one that counts. Without it a mixed command line would be
    //  answered with whichever prefix happened to come last, so the same
    //  invocation could be echoed back two different ways depending on the
    //  order of flags nobody thinks of as ordered.
    bool          flagPrefixSeen  = false;

    //  Which dialect the invocation named, and whether it named one at all.
    //  Two fields rather than one, because the default dialect is also a dialect
    //  a caller can ask for by name: the value alone cannot say which happened,
    //  and what gets reported back to the developer turns on exactly that.
    DialectId         dialect          = DialectId::As65;
    DialectSelection  dialectSelection = DialectSelection::Defaulted;

    //  Whether a CPU flag was given at all, since an explicitly requested target
    //  and the one nothing asked for are the same value.
    bool              hasCpuTarget     = false;

    //  Why a CPU flag could not be honored, already worded. Empty when none was
    //  given, or when the active dialect takes its CPU from the command line.
    //  Composed where the dialect's own data is rather than at the printing
    //  edge, so the sentence naming the in-source directive is reachable from a
    //  test -- and so no caller has to know which dialects have one.
    std::string       cpuFlagRefusal;

    //  Why two output-format flags could not both be honored, already worded,
    //  and which flag chose the format that stands. Empty when at most one was
    //  given.
    //
    //  Two flags naming different formats is a request for two files where the
    //  tool writes one. Taking the last and discarding the first would be
    //  answering a question the user did not ask -- and silently, since both
    //  spellings are perfectly valid on their own.
    std::string       outputFormatConflict;
    std::string       outputFormatFlag;

    // AS65-compatible options
    bool                                      cycleCounts       = false;   // -c
    bool                                      macroExpansion    = false;   // -m
    int                                       pageHeight        = 0;   // -h<N>
    //  79, which is as65's: "Normally, the listing is printed using 79 columns
    //  for output to a 80-column screen or printer." It was 80 -- the width of
    //  the SCREEN rather than of the listing, which is the one column that does
    //  not fit on it. This project's own 002 contract said 79 as well, so the
    //  80 was drift from both authorities at once.
    //
    //  `-i` SETS NOTHING. It asks for case-insensitive opcodes, which this
    //  assembler does unconditionally, so the field it used to set was one
    //  nothing read and nothing could usefully read. See As65ExitStatus's
    //  neighbours in CommandLineParser for the flag itself.
    int                                       pageWidth         = 79;   // -w<N> (as65's default)
    bool                                      pass1Listing      = false;   // -p
    bool                                      symbolTable       = false;   // -t
    bool                                      debugInfo         = false;   // -g
    bool                                      quiet             = false;   // -q
    bool                                      disableOpt        = false;   // -n (no-op)
    bool                                      fillZero          = false;   // -z
    std::unordered_map<std::string, int32_t>  predefinedSymbols;   // -d
};
