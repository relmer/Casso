#include "Pch.h"

#include "Devices/Disk/Containers/ImageContainer.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ImageContainer::GetKind
//
//  `.hdv`, `.img` and `.iso` are raw blocks at any size; a `.po`, `.do` or
//  `.dsk` is raw blocks only when it is larger than a floppy, and a 140K one
//  is left to the emulator's formats, as it is today.
//
////////////////////////////////////////////////////////////////////////////////

ImageContainerKind ImageContainer::GetKind (const std::string & path, size_t fileBytes)
{
    std::string  extension = std::filesystem::path (path).extension().string();
    bool         isFloppy  = fileBytes <= (size_t) NibblizationLayer::kImageByteSize;



    std::transform (extension.begin(), extension.end(), extension.begin(),
                    [] (char c) { return (char) std::tolower ((unsigned char) c); });

    if (extension == ".2mg" || extension == ".2img")
    {
        return ImageContainerKind::TwoImg;
    }

    if (extension == ".dc" || extension == ".dc42" || extension == ".image")
    {
        return ImageContainerKind::DiskCopy42;
    }

    if (extension == ".hdv" || extension == ".img" || extension == ".iso")
    {
        return ImageContainerKind::Blocks;
    }

    if (!isFloppy && (extension == ".po" || extension == ".do" || extension == ".dsk"))
    {
        return ImageContainerKind::Blocks;
    }

    return ImageContainerKind::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageContainer::IsListedExtension
//
//  The extensions, lower case and without the dot, that a folder lists as
//  disk images. `.img` and `.iso` are opened by the command-line tool but
//  aren't listed, since most files with them on a PC are PC disk images.
//
////////////////////////////////////////////////////////////////////////////////

bool ImageContainer::IsListedExtension (const std::string & extension)
{
    static constexpr const char *  kpszListed[] = { "hdv", "2mg", "2img", "dc", "dc42", "image" };



    return std::any_of (std::begin (kpszListed), std::end (kpszListed),
                        [&] (const char * listed) { return extension == listed; });
}