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





////////////////////////////////////////////////////////////////////////////////
//
//  ArtifactSink
//
//  Where a successful assembly's two files go.
//
//  THE STATUSES ON THE FAR SIDE OF THE WRITE WERE UNREACHABLE WITHOUT IT. An
//  assembly that fails never gets this far, so 2 and 3 could be asserted with
//  no files at all; one that SUCCEEDS writes an object before it returns, so
//  0, 5, and the "wrote nothing" 2 a failed write earns could not be asserted
//  without putting a real file on a real disk. Unit tests here do not, so
//  those three went untested, which is the same hole that let two exit-code
//  mappers disagree with each other for a release.
//
//  ONLY THE TWO CALLS ON THE SUCCESS PATH ARE BEHIND THIS. The symbol table
//  goes to stdout, and the debug and symbol files are written only when a flag
//  asks for them, so a test that names neither reaches no disk through them.
//
////////////////////////////////////////////////////////////////////////////////

class ArtifactSink
{
public:
    virtual ~ArtifactSink () = default;

    virtual HRESULT  WriteBinary  (const AssemblyResult & result,
                                   const CommandLineOptions & options) = 0;

    virtual HRESULT  WriteListing (const AssemblyResult & result,
                                   const CommandLineOptions & options,
                                   const std::vector<DialectReportLine> & reports) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FileArtifactSink
//
//  The sink that writes files, which is what every production caller wants.
//
////////////////////////////////////////////////////////////////////////////////

class FileArtifactSink : public ArtifactSink
{
public:
    HRESULT  WriteBinary  (const AssemblyResult & result,
                           const CommandLineOptions & options) override;

    HRESULT  WriteListing (const AssemblyResult & result,
                           const CommandLineOptions & options,
                           const std::vector<DialectReportLine> & reports) override;
};




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
