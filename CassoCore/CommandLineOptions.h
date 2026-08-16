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
//  NOT hold for `disk`: a verb, an image path, a file name, and an encoding
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
//  diagnostics come back spelled the way they invoked the tool.
//
//  Lives in the core library rather than beside the executable's main so the
//  UnitTest project can link the parser that fills it. The executable keeps
//  only the platform edge -- reading files, writing them, printing.
//
////////////////////////////////////////////////////////////////////////////////

struct CommandLineOptions
{
    enum class Subcommand    { None, Run, Help, Version, As65, Disk };
    enum class CpuTarget     { M6502, M65C02 };

    //
    //  Everything the `disk` subcommand expresses. Nested rather than
    //  flattened, per the note above.
    //
    struct DiskOptions
    {
        enum class Verb      { None, List, Get, Put, Delete, Boot };

        //  How a payload's bytes relate to the file on the host. Verbatim means
        //  no CHARACTER conversion -- length and header semantics still apply,
        //  because those record where a file ends rather than transforming it.
        enum class Encoding  { Verbatim, Text, Basic };

        Verb         verb           = Verb::None;
        Encoding     encoding       = Encoding::Verbatim;
        std::string  imagePath;                        // the disk image
        std::string  path;                             // the file ON the disk
        std::string  hostFile;                         // source for put, --out for get
        std::string  typeName;                         // --type, as the user spelled it
        Word         loadAddress    = 0;               // --addr
        bool         hasLoadAddress = false;           // $0000 is a legal address
        bool         longListing    = false;           // --long
    };

    //  Binary is the as65 full-64-KB image and stays the default, so an
    //  existing invocation keeps producing exactly what it always did. Raw and
    //  DosBinary are the shapes a modern toolchain wants -- see OutputFormats.
    enum class OutputFormat  { Binary, SRecord, IntelHex, Raw, DosBinary };

    Subcommand    subcommand      = Subcommand::None;
    OutputFormat  outputFormat    = OutputFormat::Binary;
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
    bool          hasLoadAddress  = false;
    bool          hasStopAddress  = false;
    bool          hasEntryAddress = false;
    char          flagPrefix      = '-';                // '-' for Unix-style, '/' for Windows-style

    //  Live only when subcommand == Disk. Grouped so its boundary is visible
    //  at every use site rather than mixed into the assembler's fields.
    DiskOptions   disk;

    // AS65-compatible options
    bool                                      cycleCounts       = false;   // -c
    bool                                      macroExpansion    = false;   // -m
    int                                       pageHeight        = 0;   // -h<N>
    int                                       pageWidth         = 80;   // -w<N>
    bool                                      caseSensitive     = false;   // -i (no-op)
    bool                                      pass1Listing      = false;   // -p
    bool                                      symbolTable       = false;   // -t
    bool                                      debugInfo         = false;   // -g
    bool                                      quiet             = false;   // -q
    bool                                      disableOpt        = false;   // -n (no-op)
    bool                                      fillZero          = false;   // -z
    std::unordered_map<std::string, int32_t>  predefinedSymbols;   // -d
};
