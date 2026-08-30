#include "Pch.h"

#include "ArtifactWriter.h"
#include "Assembler.h"
#include "CommandLineParser.h"
#include "OutputFormats.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ArtifactWriter::ResolveOutputFormat
//
//  Decides which format to write.
//
//  An explicit format flag WINS. Extension matching remains, but only as the
//  fallback when no flag was given, which is what keeps as65-era build scripts
//  -- which name a .s19 or .hex output and pass no flag -- working unchanged.
//
//  Deriving purely from the extension, as this used to, meant `-s -o out.dat`
//  silently wrote a flat binary: the flag said S-record and the extension won
//  anyway. It also leaves the two new formats unreachable, since neither raw
//  nor DOS-binary output has an extension of its own to be recognized by.
//
////////////////////////////////////////////////////////////////////////////////

CommandLineOptions::OutputFormat ArtifactWriter::ResolveOutputFormat (const CommandLineOptions & options)
{
    CommandLineOptions::OutputFormat  format     = options.outputFormat;
    bool                              isDefault  = format == CommandLineOptions::OutputFormat::Binary;
    bool                              isSRec     = CommandLineParser::EndsWith (options.outputFile, ".s19");
    bool                              isHex      = CommandLineParser::EndsWith (options.outputFile, ".hex");



    if (isDefault && isSRec)
    {
        format = CommandLineOptions::OutputFormat::SRecord;
    }
    else if (isDefault && isHex)
    {
        format = CommandLineOptions::OutputFormat::IntelHex;
    }

    return format;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArtifactWriter::WriteBinaryFormatFile
//
//  Opens the output file in binary mode and hands the stream to the writer for
//  the chosen format.
//
//  The three binary formats differ only in what goes INTO the stream, so the
//  file handling -- open it, check it, verify the write landed -- is written
//  once here, and each format lives in OutputFormats where tests can reach it
//  without a file at all.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArtifactWriter::WriteBinaryFormatFile (const std::string & path,
                                           const AssemblyResult & result,
                                           CommandLineOptions::OutputFormat format,
                                           Byte fillByte)
{
    HRESULT  hr         = S_OK;
    bool     isOpen     = false;
    bool     wasWritten = false;
    std::ofstream  file (path, std::ios::binary);



    isOpen = file.is_open();
    CBR (isOpen);

    if (format == CommandLineOptions::OutputFormat::Raw)
    {
        OutputFormats::WriteRaw (result.bytes, file);
    }
    else if (format == CommandLineOptions::OutputFormat::DosBinary)
    {
        OutputFormats::WriteDosBinary (result.bytes, result.startAddress, file);
    }
    else
    {
        OutputFormats::WriteFlatImage (result.bytes, result.startAddress, fillByte, file);
    }

    wasWritten = file.good();
    CBR (wasWritten);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileArtifactSink::WriteBinary
//
//  Straight through to the writer, which is the whole of what this class is
//  for: somewhere for a test to stand instead.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileArtifactSink::WriteBinary (const AssemblyResult & result,
                                       const CommandLineOptions & options)
{
    HRESULT  hr       = S_OK;
    bool     isSingle = result.savePoints.size() <= 1;



    //  ONE OUTPUT TAKES THE PATH IT ALWAYS TOOK, unchanged, which is nearly
    //  every assembly. The name, the format and the padding are all the
    //  caller's, and routing it through the loop below would only re-derive
    //  what the options already say.
    BAIL_OUT_IF (isSingle, ArtifactWriter::WriteBinary (result, options));

    //  Several outputs, each under the name its own directive gave it. The
    //  source asked for these files individually, so the caller's single output
    //  name cannot serve them and each span carries its own.
    for (size_t i = 0; i < result.savePoints.size(); i++)
    {
        AssemblyResult      one         = ArtifactWriter::ForOutput (result, i);
        CommandLineOptions  spanOptions = options;
        const std::string & given       = result.savePoints[i].name;

        spanOptions.outputFile = given.empty() ? options.outputFile : given;

        hr = ArtifactWriter::WriteBinary (one, spanOptions);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileArtifactSink::WriteListing
//
//  One listing per output, and the caller's own listing for the ordinary
//  assembly that produces one.
//
//  ONE OUTPUT TAKES THE PATH IT ALWAYS TOOK, which includes going to standard
//  output when no file was named. That is what keeps `-l` pipeable for the
//  assembly that has one program in it, which is nearly every assembly.
//
//  SEVERAL OUTPUTS CANNOT GO TO ONE PLACE. Neither a single named file nor
//  standard output can hold them apart, so each is written beside its own
//  object with the extension replaced. A caller who named a listing file gets
//  that name's directory and stem for none of them, which is the same answer
//  the object side gives: a single name cannot serve several files.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileArtifactSink::WriteListing (const AssemblyResult & result,
                                        const CommandLineOptions & options,
                                        const std::vector<DialectReportLine> & reports)
{
    HRESULT  hr       = S_OK;
    bool     isSingle = result.savePoints.size() <= 1;



    BAIL_OUT_IF (isSingle, ArtifactWriter::WriteListing (result, options, reports));

    for (size_t i = 0; i < result.savePoints.size(); i++)
    {
        AssemblyResult      one         = ArtifactWriter::ForOutput (result, i);
        CommandLineOptions  spanOptions = options;
        const std::string & given       = result.savePoints[i].name;
        std::string         object      = given.empty() ? options.outputFile : given;

        spanOptions.listingFile = ArtifactWriter::ResolveArtifactName (object, ".lst");

        hr = ArtifactWriter::WriteListing (one, spanOptions, reports);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArtifactWriter::ResolveArtifactName
//
//  An output's name with its extension replaced, so the listing and the debug
//  file sit beside the object they describe.
//
//  Only a trailing extension on the last path component is replaced. A name
//  with a dot in a directory above it -- `..uild\prog` -- keeps the dot and
//  gains the extension, which a search for the last dot alone would get wrong.
//
////////////////////////////////////////////////////////////////////////////////

std::string ArtifactWriter::ResolveArtifactName (const std::string & outputName,
                                                 const std::string & extension)
{
    std::string  name    = outputName;
    size_t       lastSep = name.find_last_of ("/\\");
    size_t       dot     = name.find_last_of ('.');
    bool         hasExt  = (dot != std::string::npos) &&
                           (lastSep == std::string::npos || dot > lastSep);



    if (hasExt)
    {
        name.erase (dot);
    }

    return name + extension;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArtifactWriter::ForOutput
//
//  One output's share of an assembly.
//
//  THE LINES ABOVE THE FIRST OUTPUT GO INTO EVERY SHARE. They are the equates
//  and macro definitions a source states once and every program in it refers
//  to, and a listing missing them cannot be read on its own.
//
//  Lines filed under an output that does not exist -- which is what bytes
//  assembled after the last save are -- are attached to the last output rather
//  than dropped. Those bytes reach no file and the assembly says so, but the
//  listing is a record of what was assembled and must still show them.
//
//  Symbols are attributed by the line they were defined on, matched against the
//  lines this output covers. A symbol carries no output of its own: most are
//  bound in the pass that runs before any span has been cut.
//
////////////////////////////////////////////////////////////////////////////////

AssemblyResult ArtifactWriter::ForOutput (const AssemblyResult & result, size_t index)
{
    AssemblyResult                   one;
    const SavePoint                  empty   = {};
    bool                             inRange = index < result.savePoints.size();
    const SavePoint                & span    = inRange ? result.savePoints[index] : empty;
    bool                             isLast  = (index + 1) >= result.savePoints.size();
    std::unordered_map<int, bool>    covered;



    one.success      = result.success;
    one.listingTitle = result.listingTitle;
    one.bytes        = span.bytes;
    one.startAddress = span.loadAddress;
    one.endAddress   = (Word) (span.loadAddress + span.bytes.size());

    for (const AssemblyLine & line : result.listing)
    {
        bool shared = line.outputIndex == AssemblyLine::kSharedByEveryOutput;
        bool beyond = !shared && (line.outputIndex >= result.savePoints.size()) && isLast;
        bool mine   = !shared && (line.outputIndex == index);

        if (shared || beyond || mine)
        {
            one.listing.push_back (line);
        }

        if (shared || beyond || mine)
        {
            covered[line.lineNumber] = true;
        }
    }

    //  A symbol whose defining line is one this output shows. Predefined names
    //  record no line at all and are shared, which is what a name the assembler
    //  supplied should be.
    for (const auto & symbol : result.symbols)
    {
        auto  lineIt = result.symbolLines.find (symbol.first);
        int   line   = lineIt == result.symbolLines.end() ? 0 : lineIt->second;
        auto  kindIt = result.symbolKinds.find (symbol.first);

        if (line == 0 || covered.count (line) != 0)
        {
            one.symbols[symbol.first]     = symbol.second;
            one.symbolLines[symbol.first] = line;

            if (kindIt != result.symbolKinds.end())
            {
                one.symbolKinds[symbol.first] = kindIt->second;
            }
        }
    }

    return one;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArtifactWriter::WriteBinary
//
//  Writes the assembled image in the resolved format.
//
//  "nul" is the explicit bit bucket and is matched case-insensitively, since
//  it is a Windows device name that scripts write every way. Writing nothing
//  is SUCCESS on that path: it is how a caller asks for diagnostics only, and
//  reporting failure would break a build that deliberately discards output.
//
//  The text formats open the stream in text mode and the binary formats in
//  binary mode, which is the only reason this splits in two rather than
//  handing every format to one writer.
//
//  A DOS binary carries its length in 16 bits, so a span of exactly 64 KB is
//  refused here rather than written as a file claiming to be empty.
//
//  The failure diagnostic is emitted once for every format. It used to be
//  written out at four separate sites, which is three opportunities for the
//  wording to drift apart.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArtifactWriter::WriteBinary (const AssemblyResult & result,
                                        const CommandLineOptions & options)
{
    HRESULT                           hr        = S_OK;
    CommandLineOptions::OutputFormat  format    = ResolveOutputFormat (options);
    std::string                       outLower  = options.outputFile;
    size_t                            spanBytes = result.bytes.size();
    bool                              isNul     = false;
    bool                              isText    = false;
    bool                              isOpen    = false;
    bool                              fitsDos   = true;
    bool                              reported  = false;



    for (auto & c : outLower)
    {
        c = (char) tolower ((unsigned char) c);
    }

    // "nul" is the explicit bit bucket: nothing written, and that is success.
    isNul  = outLower == "nul";
    isText = format == CommandLineOptions::OutputFormat::SRecord ||
             format == CommandLineOptions::OutputFormat::IntelHex;

    if (format == CommandLineOptions::OutputFormat::DosBinary)
    {
        fitsDos = spanBytes <= OutputFormats::kMaxDosBinaryLength;

        if (!fitsDos)
        {
            std::println (stderr,
                          "Error: {} bytes is too large for a DOS 3.3 binary (limit {})",
                          spanBytes,
                          OutputFormats::kMaxDosBinaryLength);
            reported = true;
        }
    }

    CBR (fitsDos);

    if (!isNul && isText)
    {
        std::ofstream  outFile (options.outputFile);

        isOpen = outFile.is_open();
        CBR (isOpen);

        if (format == CommandLineOptions::OutputFormat::SRecord)
        {
            OutputFormats::WriteSRecord (result.bytes, result.startAddress, result.endAddress, result.startAddress, outFile);
        }
        else
        {
            OutputFormats::WriteIntelHex (result.bytes, result.startAddress, result.endAddress, result.startAddress, outFile);
        }
    }
    else if (!isNul)
    {
        hr = WriteBinaryFormatFile (options.outputFile, result, format, options.fillByte);
        CHR (hr);
    }

Error:
    // One diagnostic for every path -- it was written out identically at four
    // sites before, which is three chances for the wording to drift. Suppressed
    // when the failure already explained itself, so one problem never reports
    // twice.
    if (FAILED (hr) && !reported)
    {
        std::cerr << "Error: Cannot write output file: " << options.outputFile << "\n";
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArtifactWriter::WriteListing
//
//  Emits the assembly listing, to a file when one was named and to stdout
//  otherwise.
//
//  Defaulting to stdout is what makes `casso -l` pipeable, and it cannot fail
//  to open -- hence failure is only possible in the named-file case.
//
//  Page breaks are driven from the SOURCE TEXT rather than from a parsed
//  directive, because the listing is a faithful rendering of the input: a
//  `.page` line is reproduced where it appeared and emits a form feed plus a
//  repeated title, matching what a period assembler sent to a line printer.
//  The three forms tested are the ones the assembler itself accepts.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArtifactWriter::WriteListing (const AssemblyResult & result,
                                         const CommandLineOptions & options,
                                         const std::vector<DialectReportLine> & reports)
{
    HRESULT         hr      = S_OK;
    std::ostream *  listOut = &std::cout;
    std::ofstream   listFile;
    std::string     path    = options.listingFile;
    bool            isOpen  = false;



    // A dialect that names no file and does not ask for standard output gets a
    // listing beside its object. That is Merlin: its `-l` takes no filename,
    // because a source of its that saves twice produces two listings and one
    // name could serve at most one of them. AS65's bare `-l` asks for standard
    // output outright and reaches here saying so, so it is untouched by this.
    if (path.empty() && !options.listingToStdout)
    {
        path = ResolveArtifactName (options.outputFile, ".lst");
    }

    // Nothing named and standard output asked for: the listing goes there,
    // which cannot fail to open.
    if (!path.empty())
    {
        listFile.open (path);
        isOpen = listFile.is_open();

        if (!isOpen)
        {
            std::cerr << "Error: Cannot write listing file: " << path << "\n";
        }

        CBR (isOpen);

        listOut = &listFile;
    }

    // The dialect and CPU in effect belong INSIDE the listing rather than on a
    // line beside it, so a reader of the listing finds them where a header
    // belongs -- which is also what keeps them off stdout when the listing is
    // being piped from there.
    for (const DialectReportLine & report : reports)
    {
        if (report.sink == ReportSink::ListingHeader)
        {
            *listOut << report.text << "\n\n";
        }
    }

    if (!result.listingTitle.empty())
    {
        *listOut << result.listingTitle << "\n\n";
    }

    for (const auto & line : result.listing)
    {
        if (line.sourceText.find (".page") != std::string::npos ||
            line.sourceText.find (".PAGE") != std::string::npos ||
            line.sourceText.find ("page") == 0)
        {
            *listOut << "\f";

            if (!result.listingTitle.empty())
            {
                *listOut << result.listingTitle << "\n\n";
            }
        }

        for (const std::string & row : Assembler::FormatListingRows (line, options.cycleCounts, options.pageWidth))
        {
            *listOut << row << "\n";
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArtifactWriter::WriteSymbolTable
//
////////////////////////////////////////////////////////////////////////////////

void ArtifactWriter::WriteSymbolTable (const AssemblyResult & result)
{
    std::cout << "\nSymbol Table:\n";
    std::cout << Assembler::FormatSymbolTable (result.symbols, result.symbolKinds);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArtifactWriter::WriteDebugInfo
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArtifactWriter::WriteDebugInfo (const AssemblyResult & result,
                                           const std::string & debugFile)
{
    HRESULT  hr     = S_OK;
    bool     isOpen = false;
    std::ofstream  dbgFile (debugFile);



    isOpen = dbgFile.is_open();
    if (!isOpen)
    {
        std::cerr << "Error: Cannot write debug file: " << debugFile << "\n";
    }

    CBR (isOpen);

    dbgFile << Assembler::FormatDebugInfo (result.symbols);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArtifactWriter::WriteSymbolFile
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ArtifactWriter::ArtifactWriter::WriteSymbolFile (const std::string & path, const std::unordered_map<std::string, Word> & symbols)
{
    HRESULT                                    hr         = S_OK;
    std::vector<std::pair<std::string, Word>>  sorted;
    bool                                       isOpen     = false;
    bool                                       wasWritten = false;
    std::ofstream                              file (path);



    isOpen = file.is_open();
    CBR (isOpen);

    // Sort symbols by address for deterministic output
    sorted.assign (symbols.begin(), symbols.end());

    std::sort (sorted.begin(), sorted.end(),
        [] (const auto & a, const auto & b) { return a.second < b.second; });

    for (const auto & pair : sorted)
    {
        file << std::format ("${:04X}  {}\n", pair.second, pair.first);
    }

    wasWritten = file.good();
    CBR (wasWritten);

Error:
    return hr;
}
