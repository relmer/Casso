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
    bool            isOpen  = false;



    // No listing file named means the listing goes to stdout, which cannot
    // fail to open.
    if (!options.listingFile.empty())
    {
        listFile.open (options.listingFile);
        isOpen = listFile.is_open();

        if (!isOpen)
        {
            std::cerr << "Error: Cannot write listing file: " << options.listingFile << "\n";
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
