#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFile
//
//  A program's debug information in the shape of cc65's debug-info format,
//  version 2: its source files, where their segments were placed, the byte
//  spans each source line produced, and its symbols, modules and scopes.
//
//  IDS ARE THE FILE'S OWN. Records refer to each other by id, and a reader that
//  loaded a file has checked that every id a record names is there. A span is
//  relative to its segment, so the address of its first byte is the segment's
//  start plus the span's.
//
//  A Merlin listing loads into the same shape, with the listing itself as the
//  only source file.
//
////////////////////////////////////////////////////////////////////////////////

struct DebugSourceFile
{
    int          id     = 0;
    std::string  name;              // relative to the debug file, '/' separated
    uint64_t     size   = 0;
    uint64_t     mtime  = 0;        // for cc65; never used to decide a match
    std::string  sha1;              // 40 hex digits, or empty when not recorded
    int          module = -1;
};

struct DebugSegment
{
    int          id    = 0;
    std::string  name;
    uint32_t     start = 0;
    uint32_t     size  = 0;
};

struct DebugSpan
{
    int       id      = 0;
    int       segment = 0;
    uint32_t  start   = 0;
    uint32_t  size    = 0;
};

enum class DebugLineType
{
    Asm            = 0,
    External       = 1,
    Macro          = 2,
    MacroParameter = 3,
};

struct DebugLine
{
    int               id    = 0;
    int               file  = 0;
    int               line  = 0;
    DebugLineType     type  = DebugLineType::Asm;
    int               depth = 0;         // macro nesting, from the record's count
    std::vector<int>  spans;
};

struct DebugSymbol
{
    int          id      = 0;
    std::string  name;
    uint32_t     value   = 0;
    int          segment = -1;
    int          scope   = -1;
    std::string  type;                  // "lab" for an address, "equ" for a value
};

struct DebugScope
{
    int          id     = 0;
    std::string  name;
    int          module = -1;
    int          parent = -1;
};

struct DebugModule
{
    int          id   = 0;
    std::string  name;
    int          file = -1;
};

struct DebugFile
{
    int                           major = 0;
    int                           minor = 0;
    std::vector<DebugSourceFile>  files;
    std::vector<DebugSegment>     segments;
    std::vector<DebugSpan>        spans;
    std::vector<DebugLine>        lines;
    std::vector<DebugSymbol>      symbols;
    std::vector<DebugScope>       scopes;
    std::vector<DebugModule>      modules;
};
