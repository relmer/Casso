#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/VolumeTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IImagePreviews
//
//  The disk image previews as the other previews call them: an image's
//  catalog, and one file in an image.
//
////////////////////////////////////////////////////////////////////////////////

class IImagePreviews
{
public:
    virtual ~IImagePreviews() = default;

    virtual void  PreviewImageCatalog (const std::wstring & imagePath) = 0;
    virtual void  PreviewEntry        (const std::wstring                      & imagePath,
                                       const std::string                       & entryPath,
                                       const FileEntry                         & entry,
                                       VolumeKind                                kind,
                                       std::span<const Byte>                     diskSectors = {},
                                       std::span<const std::vector<Byte>>        otherDisks  = {}) = 0;
};
