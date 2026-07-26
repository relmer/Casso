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
    FileReadResult result = {};

    std::string fullPath = baseDir.empty() ? filename : baseDir + "/" + filename;
    std::ifstream file (fullPath);

    if (!file.is_open())
    {
        result.success = false;
        result.error   = "Cannot open file: " + fullPath;
        return result;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    result.success  = true;
    result.contents = ss.str();
    return result;
}









////////////////////////////////////////////////////////////////////////////////
//
//  Assembler
//
////////////////////////////////////////////////////////////////////////////////

Assembler::Assembler (const Microcode instructionSet[256], AssemblerOptions options) :
    m_opcodeTable (instructionSet),
    m_options     (options)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordWarning
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
    AssemblySession session (m_opcodeTable, m_options);
    return session.Run (sourceText);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatListingLine
//
////////////////////////////////////////////////////////////////////////////////

std::string Assembler::FormatListingLine (const AssemblyLine & line, bool showCycleCounts)
{
    // Line number column (cols 1-5, right-justified)
    std::string lineNumStr = std::format ("{:5d}", line.lineNumber);

    // Address column (cols 7-10, 4 hex digits, no $ prefix)
    std::string addrStr;

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
    std::string bytesStr;

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
    std::string cycleStr;

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
//  FormatSymbolTable
//
////////////////////////////////////////////////////////////////////////////////

std::string Assembler::FormatSymbolTable (const std::unordered_map<std::string, Word> & symbols,
                                           const std::unordered_map<std::string, SymbolKind> & symbolKinds)
{
    // Sort symbols alphabetically
    std::vector<std::pair<std::string, Word>> sorted (symbols.begin(), symbols.end());

    std::sort (sorted.begin(), sorted.end(),
        [] (const auto & a, const auto & b) { return a.first < b.first; });

    std::string output;

    for (const auto & pair : sorted)
    {
        auto kindIt = symbolKinds.find (pair.first);
        bool isRedefinable = (kindIt != symbolKinds.end() && kindIt->second == SymbolKind::Set);

        std::string fullName = (isRedefinable ? "*" : "") + pair.first;
        output += std::format ("{:<16s}${:04X}\n", fullName, pair.second);
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
    // Sort symbols by address for deterministic output
    std::vector<std::pair<std::string, Word>> sorted (symbols.begin(), symbols.end());

    std::sort (sorted.begin(), sorted.end(),
        [] (const auto & a, const auto & b) { return a.second < b.second; });

    std::string output;

    for (const auto & pair : sorted)
    {
        output += std::format ("{}=${:04X}\n", pair.first, pair.second);
    }

    return output;
}
