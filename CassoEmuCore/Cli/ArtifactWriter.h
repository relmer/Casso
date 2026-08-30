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

    //  What went wrong, in the words a reader sees. Empty when nothing did, and
    //  empty by default for a sink that says its own piece as it goes.
    //
    //  ON THE INTERFACE BECAUSE THE CALLER IS THE ONE WITH A CONSOLE. A sink
    //  that writes onto a volume carries its refusals rather than printing them,
    //  which is what lets a test read them -- and for one release that meant
    //  nobody printed them at all: every refusal on the disk path exited
    //  non-zero and said nothing. The tests could not see it, because what they
    //  assert is that the sink PRODUCES the text.
    virtual const std::string &  GetDiagnostics() const;
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
    //  How many artifact sets an assembly's outputs call for, and what each one
    //  is called.
    //
    //  ONE SOURCE PRODUCING SEVERAL PROGRAMS PRODUCES SEVERAL OF EVERYTHING.
    //  A single listing spanning all of them makes a reader hunting for one walk
    //  past the others, and a single debug file is worse than inconvenient: its
    //  index runs from address to name, and two outputs may both begin at $0300,
    //  so the entries collide and one name silently wins. Splitting is what makes
    //  the by-address half answerable at all.
    //
    //  The name comes from the output's own name with the extension replaced, so
    //  the artifacts sit beside the file they describe.
    static std::string     ResolveArtifactName (const std::string & outputName,
                                                const std::string & extension);

    //  One output's share of an assembly: its bytes, its listing lines, and the
    //  symbols defined within it.
    //
    //  What sits above the first output -- the equates and macro definitions --
    //  goes into EVERY one of these rather than into the first, because a file
    //  missing the definitions its code refers to does not stand alone, and
    //  standing alone is the whole point of splitting.
    static AssemblyResult  ForOutput           (const AssemblyResult & result, size_t index);

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
