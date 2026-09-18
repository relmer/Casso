#pragma once

#include "Debugger/DebugFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileReader
//
//  Reads cc65's debug-info format, version 2, from any assembler or linker
//  that writes it.
//
//  One record a line: a keyword, a tab, then key=value pairs separated by
//  commas. A value is a decimal or 0x hex number, a quoted string, or ids
//  joined by '+'. An unknown keyword or key is skipped, as cc65's own reader
//  skips one. A file whose version is not 2, or whose records name ids that
//  are not there, is refused whole, and nothing of it is kept.
//
////////////////////////////////////////////////////////////////////////////////

class DebugFileReader
{
public:
    static HRESULT  Read (std::string_view text, DebugFile & out, std::string & error);

    //  Whether text is this format: its first record is `version` with a major
    //  number. Detection is by contents, since cc65 and the earlier Casso
    //  symbol file both use the .dbg extension.
    static bool  IsDebugFile (std::string_view text);

private:
    static constexpr int  kSupportedMajor = 2;

    using Fields = std::vector<std::pair<std::string_view, std::string>>;

    static std::string_view  GetNextLine    (std::string_view text, size_t & at);
    static void              SplitFields    (std::string_view pairs, Fields & out);
    static const std::string * FindField    (const Fields & fields, std::string_view key);
    static int               GetInt         (const Fields & fields, std::string_view key, int fallback);
    static uint64_t          GetNumber      (const Fields & fields, std::string_view key, uint64_t fallback);
    static std::string       GetString      (const Fields & fields, std::string_view key);
    static std::vector<int>  GetIdList      (const Fields & fields, std::string_view key);
    static bool              TryParseNumber (std::string_view text, uint64_t & value);

    static void     ReadRecord (std::string_view keyword, const Fields & fields, DebugFile & file);
    static HRESULT  Validate   (const DebugFile & file, std::string & error);
};
