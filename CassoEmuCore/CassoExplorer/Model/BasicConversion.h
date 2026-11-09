#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/VolumeTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BasicConversion
//
//  Convert to's two conversions, apart from the window so they are testable:
//  an Applesoft or Integer BASIC program to its text listing, and a text file
//  to an Applesoft BASIC program. Each produces a new file's payload and name
//  for beside the original; the original is not changed.
//
////////////////////////////////////////////////////////////////////////////////

class BasicConversion
{
public:
    enum class Kind { None, ToListing, ToApplesoft };

    //  What a file of this type converts to: a listing for a BASIC program,
    //  an Applesoft program for a text file, and nothing for any other.
    static Kind     GetKind      (VolumeKind volume, Byte type);

    //  The converted file's payload, typed for the volume. On a failure,
    //  outError has the reason in a sentence.
    static HRESULT  Convert      (const FilePayload & source, VolumeKind volume, Kind kind, FilePayload & outPayload, std::wstring & outError);

    //  The new file's name: the original's with .LIST or .BAS added, shortened
    //  to fit the file system, and numbered when another entry has it.
    static std::string  MakeName (const std::string & original, VolumeKind volume, Kind kind, const std::vector<std::string> & taken);

    static constexpr Byte  kDos33Text       = 0x00;
    static constexpr Byte  kDos33Integer    = 0x01;
    static constexpr Byte  kDos33Applesoft  = 0x02;
    static constexpr Byte  kProDosText      = 0x04;
    static constexpr Byte  kProDosInteger   = 0xFA;
    static constexpr Byte  kProDosApplesoft = 0xFC;
};
