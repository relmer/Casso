#pragma once

#include "AssemblerTypes.h"
#include "Debugger/DebugFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSourceName
//
//  How one assembled file is recorded in a debug file: its path relative to
//  the debug file, '/' separated, and its last-write time as Unix seconds.
//
////////////////////////////////////////////////////////////////////////////////

struct DebugSourceName
{
    std::string  name;
    uint64_t     mtime = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileWriter
//
//  An assembly's debug information as cc65's debug-info format, version 2,
//  with each file's SHA-1 added as a `sha1` key.
//
//  EVERY LINE THAT PRODUCED A BYTE IS A LINE RECORD OVER THAT BYTE'S SPAN. A
//  macro expansion's bytes belong to the invocation and to each body line down
//  to the innermost, each its own record with `type=2` and its depth as
//  `count`, so an address answers with all of them.
//
//  One segment per output, placed at that output's lowest address. Spans are
//  relative to their segment, so a later linker can move a segment without
//  touching its spans.
//
//  Local labels and labels a macro expansion made go into a scope of their
//  own, so a reader can list the source's top-level names without them.
//
////////////////////////////////////////////////////////////////////////////////

class DebugFileWriter
{
public:
    //  `names` is keyed as AssemblyResult::sourceTexts is: empty for the
    //  top-level input, the include's name as written otherwise. A file with
    //  no entry is recorded under its key.
    static DebugFile    Build  (const AssemblyResult & result, const std::map<std::string, DebugSourceName> & names);
    static std::string  Format (const DebugFile & file);

    static constexpr int  kModuleScope = 0;
    static constexpr int  kLocalScope  = 1;

private:
    static void         AddFiles      (const AssemblyResult & result, const std::map<std::string, DebugSourceName> & names,
                                       DebugFile & file, std::map<std::string, int> & fileIds);
    static void         AddSegments   (const AssemblyResult & result, DebugFile & file, std::map<size_t, int> & segmentIds);
    static void         AddLines      (const AssemblyResult & result, const std::map<std::string, int> & fileIds,
                                       const std::map<size_t, int> & segmentIds, DebugFile & file);
    static void         AddSymbols    (const AssemblyResult & result, DebugFile & file);
    static int          FindSegment   (const DebugFile & file, uint32_t address);
    static std::string  Quote         (const std::string & text);
    static std::string  GetModuleName (const std::string & fileName);
};
