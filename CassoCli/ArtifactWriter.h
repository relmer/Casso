#pragma once

#include "AssemblerTypes.h"
#include "CommandLineOptions.h"
#include "DialectReporting.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ArtifactWriter
//
//  An assembly's files: the object, the listing, the symbol table, the debug
//  info.
//
//  Every one of these is optional, and every one of them fails the same way --
//  no file. Collected here so the subcommands ask for artifacts rather than
//  opening streams, and so `run`, which writes none of them, never sees this
//  header.
//
//  WHAT GOES INTO A STREAM IS NOT DECIDED HERE. The three binary formats live
//  in CassoCore/OutputFormats where tests can reach them without a file at all;
//  what is here is the file handling around them, which is the part that needs
//  a disk to exercise.
//
////////////////////////////////////////////////////////////////////////////////

class ArtifactWriter
{
public:
    //  The object file, in whichever format the flags selected.
    static HRESULT  WriteBinary      (const AssemblyResult & result,
                                      const CommandLineOptions & options);

    //  The listing, to a named file or to stdout.
    static HRESULT  WriteListing     (const AssemblyResult & result,
                                      const CommandLineOptions & options,
                                      const std::vector<DialectReportLine> & reports);

    //  The symbol table, to stdout.
    static void     WriteSymbolTable (const AssemblyResult & result);

    //  The debug file: addresses by name and again by address.
    static HRESULT  WriteDebugInfo   (const AssemblyResult & result,
                                      const std::string & debugFile);

    //  The `-g` symbol file: NAME=$ADDR, one per line.
    static HRESULT  WriteSymbolFile  (const std::string & path, const std::unordered_map<std::string, Word> & symbols);

private:
    //  Which of the four formats this invocation asked for, extension included.
    static CommandLineOptions::OutputFormat  ResolveOutputFormat   (const CommandLineOptions & options);

    //  Open the file, hand the stream to the format's own writer, verify the
    //  write landed. Written once, because that part is the same for all three.
    static HRESULT                           WriteBinaryFormatFile (const std::string & path,
                                                                    const AssemblyResult & result,
                                                                    CommandLineOptions::OutputFormat format,
                                                                    Byte fillByte);
};
