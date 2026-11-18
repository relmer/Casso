#pragma once

#include "Pch.h"

#include "FileTypes/DecodedFile.h"

enum class VolumeKind;





////////////////////////////////////////////////////////////////////////////////
//
//  BasicListingOutput
//
//  An Applesoft or Integer BASIC program's listing for the host, by the
//  options of a copy: plain text, as Get writes it; with an Applesoft
//  listing's control characters as the bytes themselves rather than a caret
//  and a letter (DecodeOptions::basicRawControls); or as RTF, its line
//  numbers, keywords, strings and remarks each in a color
//  (DecodeOptions::basicColor).
//
//  The colors are read from the listing's text. Applesoft turns any keyword
//  typed outside a string, a remark or DATA into its token, so a keyword in
//  a listing's text there is one of the program's tokens.
//
////////////////////////////////////////////////////////////////////////////////

class BasicListingOutput
{
public:
    //  Whether the options change a listing from the text Get writes.
    static bool  Applies (const DecodeOptions & options) { return options.basicColor || options.basicRawControls; }

    //  A listing's host name ending .rtf in place of .txt when it is colored.
    static void  RenameForOptions (std::wstring & inOutName, const DecodeOptions & options);

    static bool  IsInteger (Byte type, VolumeKind kind);

    static HRESULT  Write (std::span<const Byte> program, bool integer, const DecodeOptions & options, std::vector<Byte> & outBytes, std::wstring & outError);

    //  Each ^ and a letter replaced by its control character, and ^^ by a
    //  caret.
    static void  RestoreControls (std::string & inOutListing);

    //  One listing line as runs.
    static void  ColorLine (const std::string & line, bool integer, std::vector<Run> & outRuns);

private:
    static constexpr Byte  kProDosInteger = 0xFA;
    static constexpr char  kCaret         = '^';
    static constexpr char  kControlBase   = 0x40;

    static const std::vector<std::string> &  GetKeywords (bool integer);
    static size_t                            MatchKeyword (const std::string & line, size_t at, bool integer);
    static void                              AddRun       (std::vector<Run> & ioRuns, const std::string & text, RunColor color);
};
