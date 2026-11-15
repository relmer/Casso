#pragma once

#include "Pch.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  ImageStatus, ImageFormFactor, ImageLayout
//
////////////////////////////////////////////////////////////////////////////////

enum class ImageStatus
{
    NotAnImage,     // no image extension
    Unclassified,   // an image extension whose length or header is not consistent with its container
    Classified,
};


enum class ImageFormFactor
{
    None,
    Floppy525,
    Floppy35,
    HardDisk,
    Tape,
};


enum class ImageLayout
{
    None,
    Sector,       // a 5.25-inch disk's sectors in DOS or ProDOS order: .dsk, .do, .po
    Nibble,       // .nib, .nb2
    Woz,
    Raw,          // blocks in ProDOS order with no header
    TwoImg,
    DiskCopy42,
    Tape,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClass
//
//  What one file is: its form factor and the formatted size of the medium.
//  A tape has no formatted size. blockCount is the 512-byte block count of an
//  image read as blocks, else 0.
//
////////////////////////////////////////////////////////////////////////////////

struct ImageClass
{
    ImageStatus      status         = ImageStatus::NotAnImage;
    ImageFormFactor  formFactor     = ImageFormFactor::None;
    ImageLayout      layout         = ImageLayout::None;
    uint64_t         formattedBytes = 0;
    uint32_t         blockCount     = 0;

    bool  IsClassified () const { return status == ImageStatus::Classified; }

    bool  operator== (const ImageClass &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier
//
//  A file's form factor and formatted size from its extension, its length
//  and its first kHeadBytes bytes, so a folder of hundreds of images is
//  classified without reading any image whole.
//
////////////////////////////////////////////////////////////////////////////////

class ImageClassifier
{
public:
    static constexpr size_t    kHeadBytes   = 512;
    static constexpr uint64_t  k525Bytes    = 143360;
    static constexpr uint64_t  kNibBytes    = 232960;
    static constexpr uint64_t  kNb2Bytes    = 223440;

    //  `extension` includes the dot and is matched without regard to case.
    //  `head` is the file's first bytes, up to kHeadBytes.
    static ImageClass    ClassifyHeader (std::wstring_view extension, uint64_t fileBytes, std::span<const Byte> head);

    //  One Stat and one ReadAt. A file that can't be read is Unclassified
    //  when its extension is an image's, NotAnImage otherwise.
    static ImageClass    Classify       (IDiskFileIo & fileIo, const std::string & path);

    static bool          IsImageExtension  (std::wstring_view extension);
    static bool          IsTapeExtension   (std::wstring_view extension);

    //  A file dialog pattern of every disk image extension, and of every
    //  tape extension.
    static std::wstring  GetDiskMask       ();
    static std::wstring  GetTapeMask       ();

    //  "140K", "800K", "1.4 MB", "32 MB"; empty for a tape or a file with
    //  no class.
    static std::wstring  FormatSize        (const ImageClass & imageClass);

    //  "5.25-inch", "3.5-inch", "Hard disk", "Tape"; empty for a file with
    //  no class.
    static std::wstring  FormatFormFactor  (const ImageClass & imageClass);

    //  The form factor of a block count: 280 blocks is 5.25-inch, 800 and
    //  1,600 are 3.5-inch, and any other count is a hard disk.
    static ImageFormFactor  GetBlockFormFactor (uint32_t blockCount);

private:
    static bool        IsExtensionIn    (std::wstring_view extension, std::span<const wchar_t * const> list);
    static void        AppendMasks      (std::wstring & mask, std::span<const wchar_t * const> list);
    static bool        HasSignature     (std::span<const Byte> head, std::string_view signature);
    static ImageClass  MakeClassified   (ImageFormFactor formFactor, ImageLayout layout, uint64_t formattedBytes, uint32_t blockCount);
    static ImageClass  MakeUnclassified ();
    static ImageClass  ClassifyWoz      (std::span<const Byte> head);
    static ImageClass  ClassifyTwoImg   (uint64_t fileBytes, std::span<const Byte> head);
    static ImageClass  ClassifyDiskCopy (std::span<const Byte> head);
    static ImageClass  ClassifyNibble   (uint64_t fileBytes);
    static ImageClass  ClassifyRaw      (std::wstring_view extension, uint64_t fileBytes);
    static ImageClass  ClassifyBlocks   (ImageLayout layout, uint64_t dataBytes);
};
