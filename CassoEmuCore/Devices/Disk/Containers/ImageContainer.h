#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ImageContainerKind
//
//  The image files the volume layer reads beside the emulator's 140K and
//  nibble formats: raw block images larger than a floppy, 2IMG and Disk Copy
//  4.2.
//
////////////////////////////////////////////////////////////////////////////////

enum class ImageContainerKind
{
    None,
    Blocks,
    TwoImg,
    DiskCopy42,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ImageContainer
//
//  Picks a file's container from its extension and size.
//
////////////////////////////////////////////////////////////////////////////////

class ImageContainer
{
public:
    static ImageContainerKind  GetKind           (const std::string & path, size_t fileBytes);
    static bool                IsListedExtension (const std::string & extension);
};
