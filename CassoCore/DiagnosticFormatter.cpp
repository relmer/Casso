#include "Pch.h"

#include "DiagnosticFormatter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiagnosticFormatter::Format
//
//  One diagnostic as `file:line: severity: message`, with `:column` inserted
//  when the position is known.
//
//  The file a diagnostic NAMES is its own when it has one, and the caller's
//  fallback otherwise. That single choice is what fixes include attribution:
//  the reporting side only ever holds the top-level input path, so a diagnostic
//  that did not carry its own file could never have named the file it came
//  from. Diagnostics that predate positions carry neither field and format
//  byte-for-byte as they always did.
//
//  Column 0 is omitted rather than printed. An editor jumping to column 0 lands
//  somewhere arbitrary, which is worse than being given only the line.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiagnosticFormatter::Format (const AssemblyError    & diagnostic,
                                         const std::string      & fallbackFile,
                                         DiagnosticSeverity       severity)
{
    std::string   result;
    std::string   file      = diagnostic.file.empty() ? fallbackFile : diagnostic.file;
    const char *  label     = (severity == DiagnosticSeverity::Error) ? "error" : "warning";
    bool          hasColumn = diagnostic.column > 0;



    if (hasColumn)
    {
        result = std::format ("{}:{}:{}: {}: {}", file, diagnostic.lineNumber, diagnostic.column, label, diagnostic.message);
    }
    else
    {
        result = std::format ("{}:{}: {}: {}", file, diagnostic.lineNumber, label, diagnostic.message);
    }

    return result;
}
