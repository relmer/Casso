#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/VolumeTypes.h"

enum class VolumeKind;





////////////////////////////////////////////////////////////////////////////////
//
//  CrossVolumeCopy
//
//  A file read from one file system, made ready to write on another.
//
//  Text is converted: Apple Pascal's header and indent codes, the high bits and
//  $8D line ends of DOS 3.3 and ProDOS, and CP/M's CR LF and $1A end. Anything
//  else keeps its bytes, with the type that means the same thing on the target.
//  Apple Pascal and CP/M binaries have no load address, so on DOS 3.3 they are
//  type S, which isn't runnable, rather than a type B that loads at 0.
//
////////////////////////////////////////////////////////////////////////////////

class CrossVolumeCopy
{
public:
    //  Whether a payload is text on its file system. CP/M has no types, so a
    //  .TXT name or bytes that are all text decide it there.
    static bool  IsText (VolumeKind kind, const std::string & name, const FilePayload & payload);

    //  Converts the payload for `target`. `outNote` is what the target can't
    //  store, for a warning, and empty when it stores everything.
    static void  Convert (VolumeKind           source,
                          VolumeKind           target,
                          const std::string  & name,
                          FilePayload        & inOutPayload,
                          std::wstring       & outNote);

    //  The name a text file takes on `target`: .TEXT on Apple Pascal, whose
    //  Editor expects it, and .TXT on CP/M, when the name fits.
    static std::string  FitTextName (const std::string & name, VolumeKind target);

private:
    static void  DecodeToHostText   (VolumeKind kind, const FilePayload & payload, std::string & outText);
    static bool  TryEncodeText      (const std::string & text, VolumeKind target, FilePayload & inOutPayload);
    static void  ConvertBinaryType  (VolumeKind source, VolumeKind target, FilePayload & inOutPayload);
    static bool  EndsWith           (const std::string & name, const char * suffix);
};
