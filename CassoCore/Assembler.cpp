#include "Pch.h"

#include "Assembler.h"
#include "AssemblySession.h"
#include "Parser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DefaultFileReader::ReadFile
//
////////////////////////////////////////////////////////////////////////////////

FileReadResult DefaultFileReader::ReadFile (const std::string & filename, const std::string & baseDir)
{
    FileReadResult      result   = {};
    std::ostringstream  ss;
    std::string         fullPath = baseDir.empty() ? filename : baseDir + "/" + filename;
    std::ifstream       file (fullPath);



    // The path is reported in the error because a failed .INCLUDE is almost
    // always a wrong relative path, not a missing file.
    if (file.is_open())
    {
        ss << file.rdbuf();
        result.success  = true;
        result.contents = ss.str();
    }
    else
    {
        result.success = false;
        result.error   = "Cannot open file: " + fullPath;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Assembler
//
////////////////////////////////////////////////////////////////////////////////

Assembler::Assembler (const Microcode instructionSet[256], AssemblerOptions options) :
    m_instructionSets (instructionSet),
    m_options         (options)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Assembler::Assembler
//
//  Base and extended instruction sets, so a dialect's in-source CPU directive
//  has something to select.
//
////////////////////////////////////////////////////////////////////////////////

Assembler::Assembler (const Microcode baseSet[256], const Microcode extendedSet[256], AssemblerOptions options) :
    m_instructionSets (baseSet, extendedSet),
    m_options         (options)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Assembler::Assembler
//
//  From a provider the caller built.
//
////////////////////////////////////////////////////////////////////////////////

Assembler::Assembler (const InstructionSetProvider & instructionSets, AssemblerOptions options) :
    m_instructionSets (instructionSets),
    m_options         (options)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordWarning
//
//  Files a diagnostic according to the caller's warning mode, which decides
//  whether it is a warning, an error, or nothing at all.
//
//  Routing every warning through this one function is what makes
//  warnings-as-errors work: no site has to know the mode, and none can forget
//  to honor it. Under FatalWarnings the diagnostic is pushed onto the ERROR
//  list and the result is marked failed, so a build script that treats
//  warnings as errors gets a non-zero exit as well as the message.
//
//  NoWarn discards silently rather than filing and filtering later, so a
//  source with thousands of suppressed warnings costs nothing to assemble.
//
////////////////////////////////////////////////////////////////////////////////

void Assembler::RecordWarning (AssemblyResult & result, int lineNumber, const std::string & message)
{
    switch (m_options.warningMode)
    {
        case WarningMode::Warn:
        {
            AssemblyError warning = {};
            warning.lineNumber = lineNumber;
            warning.message    = message;
            result.warnings.push_back (warning);
            break;
        }

        case WarningMode::FatalWarnings:
        {
            AssemblyError error = {};
            error.lineNumber = lineNumber;
            error.message    = message;
            result.errors.push_back (error);
            result.success = false;
            break;
        }

        case WarningMode::NoWarn:
            // Discard silently
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Assemble
//
////////////////////////////////////////////////////////////////////////////////

AssemblyResult Assembler::Assemble (const std::string & sourceText)
{
    AssemblySession  session (m_instructionSets, m_options);



    return session.Run (sourceText);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatListingLine
//
//  Renders one assembled line in the AS65 listing layout: line number,
//  address, object bytes, then the original source text.
//
//  The COLUMN WIDTHS are the specification, not a preference. Existing tools
//  and eyeballs read these listings positionally, so the fields are padded to
//  fixed widths -- 5 for the line number, 4 for the address, 9 for the bytes
//  -- rather than separated by tabs.
//
//  At most three object bytes are shown, which is the longest 6502
//  instruction. A data directive that emits more is truncated in the listing
//  while assembling in full; the listing is a reading aid, not the output.
//
//  Three line states are distinguished in the address column and they are not
//  the same thing: a real address prints, a line inside a false conditional
//  prints a dash so the reader can see it was SKIPPED rather than merely
//  address-less, and anything else prints blank.
//
//  Cycle counts are inserted between the bytes and the source rather than
//  appended, so enabling them shifts the source text as a block instead of
//  producing ragged trailing annotations.
//
//  The macro-expansion marker occupies its own column, so expanded lines are
//  scannable down the page without reading their text.
//
////////////////////////////////////////////////////////////////////////////////

std::string Assembler::FormatListingLine (const AssemblyLine & line, bool showCycleCounts)
{
    std::string addrStr;
    std::string bytesStr;
    std::string cycleStr;



    // Line number column (cols 1-5, right-justified)
    std::string lineNumStr = std::format ("{:5d}", line.lineNumber);

    // Address column (cols 7-10, 4 hex digits, no $ prefix)

    if (line.isConditionalSkip)
    {
        addrStr = "   -";
    }
    else if (line.hasAddress)
    {
        addrStr = std::format ("{:04X}", line.address);
    }
    else
    {
        addrStr = "    ";
    }

    // Bytes column (cols 14-22, up to 3 hex bytes, padded to 9 chars)

    for (size_t i = 0; i < line.bytes.size() && i < 3; i++)
    {
        if (i > 0)
        {
            bytesStr += " ";
        }

        bytesStr += std::format ("{:02X}", line.bytes[i]);
    }

    while (bytesStr.size() < 9)
    {
        bytesStr += " ";
    }

    // Cycle counts column (optional, between bytes and prefix)

    if (showCycleCounts && line.cycleCounts > 0)
    {
        cycleStr = std::format ("[{}] ", line.cycleCounts);
    }

    // Macro expansion prefix (col 23)
    std::string prefix = line.isMacroExpansion ? ">" : " ";

    // AS65 layout: linenum(5) space(1) addr(4) spaces(3) bytes(9) prefix(1) source
    return lineNumStr + " " + addrStr + "   " +
           bytesStr + cycleStr + prefix + line.sourceText;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatListingRows
//
//  One listing row, wrapped to a column width.
//
//  The wrap point is the last space that fits, so a line breaks between words
//  where it can. A single word longer than the space available is cut at the
//  margin instead -- a symbol wider than the page has to go somewhere, and
//  refusing to break it would print a line wider than the width asked for.
//
//  Continuations carry the source column's indent and nothing else: no line
//  number, no address, no bytes. Repeating those would claim a second line
//  emitted the same bytes, and blanking them by hand at each call site is how
//  two callers end up disagreeing about the layout.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Assembler::FormatListingRows (const AssemblyLine & line,
                                                       bool showCycleCounts,
                                                       int  columnWidth)
{
    std::vector<std::string>  rows;
    std::string               full    = FormatListingLine (line, showCycleCounts);
    size_t                    width   = (columnWidth > 0) ? (size_t) columnWidth : 0;
    size_t                    indent  = 0;
    bool                      wraps   = (width > 0) && (full.size() > width);



    if (!wraps)
    {
        rows.push_back (full);
        return rows;
    }

    // Where the source text starts in the rendered row, found by subtracting it
    // rather than by recomputing the column arithmetic -- which would be a
    // second copy of the layout, free to drift from the first.
    indent = full.size() - line.sourceText.size();

    // A width that cannot fit the fixed columns plus one character of text
    // leaves nothing to wrap INTO, and looping would never advance.
    if (indent + 1 >= width)
    {
        rows.push_back (full);
        return rows;
    }

    rows.push_back (full.substr (0, width));

    for (size_t taken = width; taken < full.size(); )
    {
        size_t  room  = 0;
        size_t  chunk = 0;
        size_t  brk   = std::string::npos;

        // A continuation starts at text, not at whatever whitespace the cut
        // landed in the middle of. Done at the top so it covers the first
        // continuation as well, which begins at a hard cut rather than at a
        // break this loop chose.
        while (taken < full.size() && full[taken] == ' ')
        {
            taken++;
        }

        if (taken >= full.size())
        {
            break;
        }

        room  = width - indent;
        chunk = std::min (room, full.size() - taken);

        if (taken + chunk < full.size())
        {
            brk = full.rfind (' ', taken + chunk);
        }

        if (brk != std::string::npos && brk > taken)
        {
            chunk = brk - taken;
        }

        rows.push_back (std::string (indent, ' ') + full.substr (taken, chunk));

        taken += chunk;
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatListing
//
//  Every listing line, wrapped and paginated, under the source's own title.
//
//  THE PAGE COUNT IS IN ROWS. A line that wrapped to three rows fills three
//  lines of paper, and counting it as one would overrun the page by whatever
//  wrapping added -- which is the whole reason the two features have to be
//  composed here rather than applied by separate callers.
//
//  A SOURCE CAN ASK FOR A BREAK ITSELF, and that is recognized from the text of
//  the line rather than from a parsed directive, because the listing reproduces
//  the input line and this one is reproduced at the top of the new page.
//
////////////////////////////////////////////////////////////////////////////////

std::string Assembler::FormatListing (const AssemblyResult & result,
                                      int  pageHeight,
                                      bool showCycleCounts,
                                      int  columnWidth)
{
    std::string  output;
    std::string  header;
    int          onPage   = 0;
    bool         paginate = pageHeight > 0;



    if (!result.listingTitle.empty())
    {
        header = result.listingTitle + "\n\n";
    }

    output += header;

    for (const AssemblyLine & line : result.listing)
    {
        std::vector<std::string>  rows   = FormatListingRows (line, showCycleCounts, columnWidth);
        bool                      asked  = line.sourceText.find (".page") != std::string::npos ||
                                           line.sourceText.find (".PAGE") != std::string::npos ||
                                           line.sourceText.find ("page")  == 0;
        bool                      filled = paginate && onPage >= pageHeight;

        if (asked || filled)
        {
            output += "\f";
            output += header;
            onPage  = 0;
        }

        for (const std::string & row : rows)
        {
            output += row + "\n";
            onPage++;
        }
    }

    return output;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatSymbolTable
//
////////////////////////////////////////////////////////////////////////////////

std::string Assembler::FormatSymbolTable (const std::unordered_map<std::string, Word> & symbols,
                                           const std::unordered_map<std::string, SymbolKind> & symbolKinds)
{
    std::string output;



    // Sort symbols alphabetically
    std::vector<std::pair<std::string, Word>> sorted (symbols.begin(), symbols.end());

    std::sort (sorted.begin(), sorted.end(),
        [] (const auto & a, const auto & b) { return a.first < b.first; });


    for (const auto & pair : sorted)
    {
        auto  kindIt        = symbolKinds.find (pair.first);
        bool  isRedefinable = (kindIt != symbolKinds.end() && kindIt->second == SymbolKind::Set);

        std::string fullName = (isRedefinable ? "*" : "") + pair.first;

        //  Hex and decimal both, which is what AS65 lists: an address is read
        //  in hex and a constant standing for a count is read in decimal, and
        //  the table cannot tell which a symbol is.
        output += std::format ("{:<16s}${:04X}  {:5d}\n", fullName, pair.second, pair.second);
    }

    return output;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatDebugInfo
//
////////////////////////////////////////////////////////////////////////////////

std::string Assembler::FormatDebugInfo (const std::unordered_map<std::string, Word> & symbols)
{
    std::string                                output;
    std::vector<std::pair<std::string, Word>>  sorted (symbols.begin(), symbols.end());



    // Sort symbols by address for deterministic output
    std::sort (sorted.begin(), sorted.end(),
        [] (const auto & a, const auto & b) { return a.second < b.second; });

    output += "; by address\n";

    for (const auto & pair : sorted)
    {
        output += std::format ("{}=${:04X}\n", pair.first, pair.second);
    }

    // The same symbols again, by NAME. Reading a debug file is two different
    // questions -- "what is at $0310" and "where did FOO go" -- and a table
    // sorted for one answers the other by scanning. Both are cheap to write and
    // neither is cheap to reconstruct by hand.
    //
    // Case-insensitive, so `foo` and `FOO` sort together rather than landing in
    // separate runs of the alphabet. Ties fall back to the case-sensitive
    // comparison, because the sort has to be a total order: symbols differing
    // only in case are distinct symbols here, and leaving their order to the
    // hash container would make the file differ between runs.
    std::sort (sorted.begin(), sorted.end(),
        [] (const auto & a, const auto & b)
        {
            std::string  left  = Parser::ToUpper (a.first);
            std::string  right = Parser::ToUpper (b.first);

            return (left != right) ? (left < right) : (a.first < b.first);
        });

    output += "\n; by symbol\n";

    for (const auto & pair : sorted)
    {
        output += std::format ("{}=${:04X}\n", pair.first, pair.second);
    }

    return output;
}
