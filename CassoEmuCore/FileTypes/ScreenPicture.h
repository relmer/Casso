#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/PicturePreview.h"
#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenPicture
//
//  A saved hi-res, double hi-res or lo-res screen, written as a PNG picture.
//
//  A binary is a picture by PicturePreview's rule: its address and size. A
//  ProDOS picture file, $08, with an aux type below $4000 is an uncompressed
//  screen at $2000, hi-res by an 8 KB size and double hi-res by 16 KB; aux
//  type $4000 is a hi-res screen packed with PackBytes, and $4001 a double
//  hi-res screen packed the same way. $8066 is LZ4FH, which Lz4fh decodes.
//
//  DecodeOptions::hiResMono draws a hi-res picture in black and white, and
//  DecodeOptions::doubleHiRes sets how a double hi-res picture is drawn.
//
////////////////////////////////////////////////////////////////////////////////

class ScreenPicture
{
public:
    static bool     MatchesHiRes       (const FileFacts & facts, std::span<const Byte> bytes);
    static bool     MatchesDoubleHiRes (const FileFacts & facts, std::span<const Byte> bytes);
    static bool     MatchesLoRes       (const FileFacts & facts, std::span<const Byte> bytes);

    static HRESULT  DecodeHiRes        (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);
    static HRESULT  DecodeDoubleHiRes  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);
    static HRESULT  DecodeLoRes        (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

    //  The screen's bytes and the address they are drawn from: a packed
    //  picture file unpacked, and any other the file's bytes.
    static HRESULT  Unpack (FileFormat format, std::span<const Byte> bytes, const FileFacts & facts, std::vector<Byte> & outScreen, Word & outAddress, std::wstring & outError);

    //  The mode a screen is drawn in for the options.
    static PicturePreview::Mode  ChooseMode (FileFormat format, const FileFacts & facts, std::span<const Byte> screen, const DecodeOptions & options);

    //  The monitor a screen is drawn for: white for a hi-res picture in
    //  black and white, and color otherwise.
    static PicturePreview::Monitor  ChooseMonitor (FileFormat format, const DecodeOptions & options);

private:
    static constexpr Byte    kTypePicture       = 0x08;
    static constexpr Byte    kTypeDos33Binary   = 0x04;
    static constexpr Byte    kTypeProDosBinary  = 0x06;
    static constexpr Word    kAuxPacked         = 0x4000;
    static constexpr Word    kAuxPackedDouble   = 0x4001;
    static constexpr size_t  kHiResMin          = 8184;
    static constexpr size_t  kDoubleHiResMin    = 16368;

    static bool     IsBinary         (const FileFacts & facts);
    static bool     GetBinaryAddress (const FileFacts & facts, Word & outAddress);
    static bool     IsScreenFile     (const FileFacts & facts);
    static bool     IsPacked         (const FileFacts & facts, Word auxType);
    static HRESULT  Decode           (FileFormat format, std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);
};
