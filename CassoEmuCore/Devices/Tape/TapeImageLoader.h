#pragma once

#include "Devices/Tape/ITapeAudioDecoder.h"
#include "Devices/Tape/TapeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeImageLoader
//
//  Builds a TapeImage from a file's bytes. The format comes from the content,
//  never the extension. The caller reads the file and says whether it is
//  read-only.
//
////////////////////////////////////////////////////////////////////////////////

class TapeImageLoader
{
public:
    static HRESULT  Load (std::span<const Byte>   bytes,
                          const std::string     & path,
                          bool                    isReadOnly,
                          ITapeAudioDecoder     & compressedDecoder,
                          TapeImage             & image,
                          std::string           & error);

    static bool     IsMp3  (std::span<const Byte> bytes);
    static bool     IsFlac (std::span<const Byte> bytes);

    //  Whether a path's extension is one a tape recording uses, for the
    //  picker's folder scan and the file filters. Insertion itself goes by
    //  content.
    static bool     IsTapeFileExtension (const std::wstring & path);
};
