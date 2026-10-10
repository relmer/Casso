#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskCopy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorImageLoader
//
//  An image file read into a copy the inspector can analyze, for a disk no
//  drive holds: an image file compared with the window's disk, or a drive's
//  file as saved now (FR-117). Every format a drive mounts loads the same
//  way it mounts, and a file that cannot be opened gives the reason a mount
//  would. Each load gets a media id of its own, apart from the drives' ids.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorImageLoader
{
public:
    static constexpr uint64_t  kFirstMediaId = 1ull << 62;

    static HRESULT  LoadFile  (const std::string & utf8Path, std::shared_ptr<const DiskCopy> & outCopy, std::wstring & outReason);
    static HRESULT  LoadBytes (const vector<Byte> & bytes, const std::string & utf8Path, bool isReadOnly,
                               std::shared_ptr<const DiskCopy> & outCopy, std::wstring & outReason);

    static uint64_t  MakeMediaId ();
};
