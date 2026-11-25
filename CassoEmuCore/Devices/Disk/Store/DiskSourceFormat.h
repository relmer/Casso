#pragma once

#include "Pch.h"

#include "Devices/Disk/IDiskImage.h"
#include "Devices/Disk/MountDiagnosis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSourceFormat
//
//  Which container an existing image file is, from its extension, and why a
//  load of its bytes failed. Pure: no filesystem access.
//
////////////////////////////////////////////////////////////////////////////////

class DiskSourceFormat
{
public:
    static HRESULT         GetSourceFormatByExtension (const string & path, DiskFormat & outFmt);

    //  Calls GetSourceFormatByExtension rather than checking a second list, so
    //  a file filter and the loader cannot disagree.
    static bool            IsMountableImageExtension  (const string  & path);
    static bool            IsMountableImageExtension  (const wstring & path);

    static MountDiagnosis  ClassifyLoadFailure        (DiskFormat fmt, const vector<Byte> & bytes);
};
