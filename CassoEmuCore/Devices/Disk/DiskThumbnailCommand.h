#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskCommandResult.h"

struct CommandLineOptions;
class ThumbnailStore;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskThumbnailCommand
//
//  The disk tool's `thumbnail`: a picture file as an image's thumbnail,
//  marked as set by the user; the removal of the thumbnail the user set; or
//  the image's thumbnail written to a PNG file.
//
////////////////////////////////////////////////////////////////////////////////

class DiskThumbnailCommand
{
public:
    static void     Run          (const CommandLineOptions & options, ThumbnailStore & store, DiskCommandResult & result);

    //  A picture file's bytes in any format WIC decodes, as a PNG.
    static HRESULT  ConvertToPng (const std::vector<Byte> & picture, std::vector<Byte> & outPng);

private:
    static void     Set          (const CommandLineOptions & options, ThumbnailStore & store, DiskCommandResult & result);
    static void     Remove       (const CommandLineOptions & options, ThumbnailStore & store, DiskCommandResult & result);
    static void     WriteOut     (const CommandLineOptions & options, ThumbnailStore & store, DiskCommandResult & result);

    static constexpr int  kPngDpi = 96;
};
