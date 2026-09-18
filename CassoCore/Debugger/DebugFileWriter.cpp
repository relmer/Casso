#include "Pch.h"

#include "Debugger/DebugFileWriter.h"
#include "Sha1.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileWriter::Build
//
////////////////////////////////////////////////////////////////////////////////

DebugFile DebugFileWriter::Build (const AssemblyResult & result, const std::map<std::string, DebugSourceName> & names)
{
    DebugFile                   file;
    std::map<std::string, int>  fileIds;
    std::map<size_t, int>       segmentIds;



    file.major = 2;
    file.minor = 0;

    AddFiles    (result, names, file, fileIds);
    AddSegments (result, file, segmentIds);
    AddLines    (result, fileIds, segmentIds, file);
    AddSymbols  (result, file);

    return file;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileWriter::AddFiles
//
//  Every file the assembly read, and any a line names that it did not, which
//  is a file recorded without a size or a hash. The top-level input is first,
//  because the module record points at file 0.
//
////////////////////////////////////////////////////////////////////////////////

void DebugFileWriter::AddFiles (const AssemblyResult & result, const std::map<std::string, DebugSourceName> & names,
                                DebugFile & file, std::map<std::string, int> & fileIds)
{
    std::vector<std::string>  keys;



    for (const auto & text : result.sourceTexts)
    {
        keys.push_back (text.first);
    }

    for (const DebugLineRecord & record : result.debugLines)
    {
        for (const SourceFrame & frame : record.positions)
        {
            if (std::find (keys.begin(), keys.end(), frame.file) == keys.end())
            {
                keys.push_back (frame.file);
            }
        }
    }

    for (const std::string & key : keys)
    {
        auto             named = names.find (key);
        auto             text  = result.sourceTexts.find (key);
        DebugSourceFile  each;

        each.id     = (int) file.files.size();
        each.name   = (named != names.end()) ? named->second.name  : key;
        each.mtime  = (named != names.end()) ? named->second.mtime : 0;
        each.module = 0;

        if (text != result.sourceTexts.end())
        {
            each.size = text->second.size();
            each.sha1 = Sha1::ComputeTextHex (text->second);
        }

        fileIds[key] = each.id;
        file.files.push_back (each);
    }

    file.modules.push_back ({ 0, GetModuleName (file.files.empty() ? std::string() : file.files[0].name), 0 });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileWriter::AddSegments
//
//  One segment per output, from its lowest address to its highest.
//
////////////////////////////////////////////////////////////////////////////////

void DebugFileWriter::AddSegments (const AssemblyResult & result, DebugFile & file, std::map<size_t, int> & segmentIds)
{
    std::map<size_t, std::pair<uint32_t, uint32_t>>  extent;



    for (const DebugLineRecord & record : result.debugLines)
    {
        uint32_t  first = record.address;
        uint32_t  last  = record.address + (uint32_t) record.size;
        auto      found = extent.find (record.outputIndex);

        if (found == extent.end())
        {
            extent[record.outputIndex] = { first, last };
            continue;
        }

        found->second.first  = std::min (found->second.first, first);
        found->second.second = std::max (found->second.second, last);
    }

    for (const auto & each : extent)
    {
        int          id   = (int) file.segments.size();
        std::string  name = (id == 0) ? std::string ("CODE") : std::format ("CODE{}", id);

        segmentIds[each.first] = id;
        file.segments.push_back ({ id, name, each.second.first, each.second.second - each.second.first });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileWriter::AddLines
//
//  A span per run of bytes, and a line record per source line gathering every
//  span it produced. A position's depth in the stack is the record's `count`.
//
////////////////////////////////////////////////////////////////////////////////

void DebugFileWriter::AddLines (const AssemblyResult & result, const std::map<std::string, int> & fileIds,
                                const std::map<size_t, int> & segmentIds, DebugFile & file)
{
    std::map<std::tuple<int, int, int>, size_t>  lineIndex;



    for (const DebugLineRecord & record : result.debugLines)
    {
        int        segment = segmentIds.at (record.outputIndex);
        DebugSpan  span    = { (int) file.spans.size(), segment,
                               record.address - file.segments[segment].start, (uint32_t) record.size };

        file.spans.push_back (span);

        for (size_t depth = 0; depth < record.positions.size(); depth++)
        {
            const SourceFrame &           frame = record.positions[depth];
            std::tuple<int, int, int>     key   = { fileIds.at (frame.file), frame.line, (int) depth };
            auto                          found = lineIndex.find (key);

            if (found == lineIndex.end())
            {
                DebugLine  line;

                line.id    = (int) file.lines.size();
                line.file  = std::get<0> (key);
                line.line  = frame.line;
                line.type  = (depth == 0) ? DebugLineType::Asm : DebugLineType::Macro;
                line.depth = (int) depth;

                found = lineIndex.emplace (key, file.lines.size()).first;
                file.lines.push_back (line);
            }

            file.lines[found->second].spans.push_back (span.id);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileWriter::AddSymbols
//
//  In name order, so two assemblies of one source write the same file. The
//  names the assembler supplied itself are left out.
//
////////////////////////////////////////////////////////////////////////////////

void DebugFileWriter::AddSymbols (const AssemblyResult & result, DebugFile & file)
{
    std::map<std::string, Word>  sorted (result.symbols.begin(), result.symbols.end());
    bool                         anyLocal = false;



    for (const auto & symbol : sorted)
    {
        auto         kind    = result.symbolKinds.find (symbol.first);
        bool         isLabel = (kind != result.symbolKinds.end()) && (kind->second == SymbolKind::Label);
        bool         isLocal = result.localSymbols.contains (symbol.first) || result.macroSymbols.contains (symbol.first);
        DebugSymbol  each;

        if (result.builtinSymbols.contains (symbol.first))
        {
            continue;
        }

        each.id      = (int) file.symbols.size();
        each.name    = symbol.first;
        each.value   = symbol.second;
        each.scope   = isLocal ? kLocalScope : kModuleScope;
        each.type    = isLabel ? "lab" : "equ";
        each.segment = isLabel ? FindSegment (file, symbol.second) : -1;
        anyLocal    |= isLocal;

        file.symbols.push_back (each);
    }

    file.scopes.push_back ({ kModuleScope, "", 0, -1 });

    if (anyLocal)
    {
        file.scopes.push_back ({ kLocalScope, "local", 0, kModuleScope });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileWriter::FindSegment
//
////////////////////////////////////////////////////////////////////////////////

int DebugFileWriter::FindSegment (const DebugFile & file, uint32_t address)
{
    for (const DebugSegment & segment : file.segments)
    {
        if (address >= segment.start && address < segment.start + segment.size)
        {
            return segment.id;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileWriter::Quote
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugFileWriter::Quote (const std::string & text)
{
    std::string  quoted = "\"";



    for (char c : text)
    {
        if (c == '"' || c == '\\')
        {
            quoted += '\\';
        }

        quoted += c;
    }

    return quoted + "\"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileWriter::GetModuleName
//
//  The file's name without its folders or its extension.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugFileWriter::GetModuleName (const std::string & fileName)
{
    size_t  slash = fileName.find_last_of ("/\\");
    size_t  start = (slash == std::string::npos) ? 0 : slash + 1;
    size_t  dot   = fileName.find_last_of ('.');



    if (dot == std::string::npos || dot < start)
    {
        dot = fileName.size();
    }

    return fileName.substr (start, dot - start);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileWriter::Format
//
//  The records in cc65's order: version, info, file, mod, seg, span, line,
//  sym, scope. A line's type and count are left out for an assembler line.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugFileWriter::Format (const DebugFile & file)
{
    std::string  out;



    out += std::format ("version\tmajor={},minor={}\n", file.major, file.minor);
    out += std::format ("info\tcsym=0,file={},lib=0,line={},mod={},scope={},seg={},span={},sym={},type=0\n",
                        file.files.size(), file.lines.size(), file.modules.size(), file.scopes.size(),
                        file.segments.size(), file.spans.size(), file.symbols.size());

    for (const DebugSourceFile & each : file.files)
    {
        out += std::format ("file\tid={},name={},size={},mtime=0x{:08X},mod={}", each.id, Quote (each.name),
                            each.size, each.mtime, each.module);
        out += each.sha1.empty() ? std::string ("\n") : std::format (",sha1={}\n", Quote (each.sha1));
    }

    for (const DebugModule & each : file.modules)
    {
        out += std::format ("mod\tid={},name={},file={}\n", each.id, Quote (each.name), each.file);
    }

    for (const DebugSegment & each : file.segments)
    {
        out += std::format ("seg\tid={},name={},start=0x{:04X},size=0x{:04X},addrsize=absolute,type=rw\n",
                            each.id, Quote (each.name), each.start, each.size);
    }

    for (const DebugSpan & each : file.spans)
    {
        out += std::format ("span\tid={},seg={},start={},size={}\n", each.id, each.segment, each.start, each.size);
    }

    for (const DebugLine & each : file.lines)
    {
        std::string  spans;

        for (int span : each.spans)
        {
            spans += (spans.empty() ? "" : "+") + std::to_string (span);
        }

        out += std::format ("line\tid={},file={},line={}", each.id, each.file, each.line);
        out += (each.type == DebugLineType::Asm) ? std::string()
                                                 : std::format (",type={},count={}", (int) each.type, each.depth);
        out += std::format (",span={}\n", spans);
    }

    for (const DebugSymbol & each : file.symbols)
    {
        out += std::format ("sym\tid={},name={},addrsize=absolute,scope={},val=0x{:04X}", each.id, Quote (each.name),
                            each.scope, each.value);
        out += (each.segment < 0) ? std::string() : std::format (",seg={}", each.segment);
        out += std::format (",type={}\n", each.type);
    }

    for (const DebugScope & each : file.scopes)
    {
        out += std::format ("scope\tid={},name={},mod={}", each.id, Quote (each.name), each.module);
        out += (each.parent < 0) ? std::string ("\n") : std::format (",parent={}\n", each.parent);
    }

    return out;
}
