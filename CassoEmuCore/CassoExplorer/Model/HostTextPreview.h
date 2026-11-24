#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/PreviewDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostTextPreview
//
//  Which host files are text, in what character set, and their bytes as the
//  lines of a text preview.
//
//  Pure: no I/O, no Win32.
//
////////////////////////////////////////////////////////////////////////////////

class HostTextPreview
{
public:
    //  How many of a file's first bytes are looked at to find its character
    //  set.
    static constexpr size_t  kTextSampleBytes = 64 * 1024;

    static HostCharset  GetHostCharset (std::span<const Byte> sample);

    //  Whether a file is an assembler's source by its extension, which
    //  previews as text with its line numbers.
    static bool         IsSourceName      (const std::wstring & name);

    //  Whether a file is a BASIC listing or text Casso Explorer exported, by
    //  its suffix, which previews as lines of text even in plain ASCII.
    static bool         IsAppleExportName (const std::wstring & name);

    //  The bytes as lines of text in the preview, and its kind, source flag
    //  and bytes set to match. False, with the preview as it was, for a file
    //  that is not text.
    static bool         TryPreviewText    (const std::wstring & name, std::span<const Byte> bytes, PreviewContent & outPreview);

private:
    static constexpr Byte              kFormFeed              = 0x0C;
    static constexpr Byte              kDosEndOfFile          = 0x1A;
    static constexpr Byte              kDosDrawingFirst       = 0xB0;    // code page 437's shades and box drawing
    static constexpr Byte              kDosDrawingLast        = 0xDF;
    static constexpr size_t            kTextControlRatio      = 100;     // at most one byte in this many a DOS symbol
    static constexpr const wchar_t *   kpszSourceExtensions[] = { L".a65", L".asm", L".s", L".inc", L".a", L".src", L".mac", L".ca65" };
    static constexpr unsigned          kDosCodePage           = 437;
    static constexpr unsigned          kWindowsCodePage       = 1252;
    static constexpr wchar_t           kByteOrderMark         = 0xFEFF;
    static constexpr size_t            kTabStop               = 8;

    static bool  IsUtf8 (std::span<const Byte> sample);
};
