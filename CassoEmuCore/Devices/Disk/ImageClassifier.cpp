#include "Pch.h"

#include "Devices/BlockDevice/BlockDeviceMedia.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/ImageClassifier.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"
#include "Devices/Floppy35/Gcr35Track.h"
#include "Machines/Apple2/Common/WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Extensions by the drive that mounts them: a Disk II's sector images, the
//  block images the 3.5-inch drives and the hard disks open, and the tape
//  recordings. A test compares these with DiskImageStore's, BlockImageFile's
//  and TapeImageLoader's.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr const wchar_t * s_kSectorExtensions[] = { L".dsk", L".do", L".po" };

static constexpr const wchar_t * s_kNibbleExtensions[] = { L".nib", L".nb2" };

static constexpr const wchar_t * s_kBlockExtensions[] =
{
    L".po", L".hdv", L".2mg", L".2img", L".img", L".dc", L".dc42", L".image", L".iso", L".hd",
};

static constexpr const wchar_t * s_kTapeExtensions[] =
{
    L".wav", L".aif", L".aiff", L".aifc", L".mp3", L".flac", L".ogg",
};





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier::ClassifyHeader
//
//  The container is found from the first bytes before the extension is
//  trusted, as the drives find it: a WOZ signature, a 2IMG header or a Disk
//  Copy 4.2 header wins over the name. A file with none of them is a sector,
//  nibble or raw block image by its extension and length.
//
////////////////////////////////////////////////////////////////////////////////

ImageClass ImageClassifier::ClassifyHeader (std::wstring_view extension, uint64_t fileBytes, std::span<const Byte> head)
{
    bool  isWozName = _wcsicmp (std::wstring (extension).c_str(), L".woz") == 0;



    if (IsTapeExtension (extension))
    {
        return MakeClassified (ImageFormFactor::Tape, ImageLayout::Tape, 0, 0);
    }

    if (!IsImageExtension (extension))
    {
        return ImageClass();
    }

    if (HasSignature (head, "WOZ1") || HasSignature (head, "WOZ2"))
    {
        return ClassifyWoz (head);
    }

    if (isWozName)
    {
        return MakeUnclassified();
    }

    if (BlockImageFile::IsTwoImgHeader (head))
    {
        return ClassifyTwoImg (fileBytes, head);
    }

    if (BlockImageFile::IsDiskCopyHeader (head, fileBytes))
    {
        return ClassifyDiskCopy (head);
    }

    if (IsExtensionIn (extension, s_kNibbleExtensions))
    {
        return ClassifyNibble (fileBytes);
    }

    return ClassifyRaw (extension, fileBytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier::Classify
//
//  A file that can't be read is a result, not a failure, so one locked or
//  missing file does not stop a listing.
//
////////////////////////////////////////////////////////////////////////////////

ImageClass ImageClassifier::Classify (IDiskFileIo & fileIo, const std::string & path)
{
    HRESULT       hr        = S_OK;
    FileStamp     stamp;
    std::wstring  extension = std::filesystem::path (path).extension().wstring();
    vector<Byte>  head;
    ImageClass    result;
    bool          isImage   = IsImageExtension (extension);
    bool          hasHead   = false;



    BAIL_OUT_IF (!isImage, S_OK);

    result = MakeUnclassified();

    hr = fileIo.Stat (path, stamp);
    CHR (hr);

    head.resize ((size_t) std::min<uint64_t> (stamp.sizeBytes, kHeadBytes));
    hasHead = !head.empty();

    if (hasHead)
    {
        hr = fileIo.ReadAt (path, 0, head);
        CHR (hr);
    }

    result = ClassifyHeader (extension, stamp.sizeBytes, head);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier::IsImageExtension
//
////////////////////////////////////////////////////////////////////////////////

bool ImageClassifier::IsImageExtension (std::wstring_view extension)
{
    return IsExtensionIn (extension, s_kSectorExtensions)
        || IsExtensionIn (extension, s_kNibbleExtensions)
        || IsExtensionIn (extension, s_kBlockExtensions)
        || IsTapeExtension (extension)
        || _wcsicmp (std::wstring (extension).c_str(), L".woz") == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier::IsTapeExtension
//
////////////////////////////////////////////////////////////////////////////////

bool ImageClassifier::IsTapeExtension (std::wstring_view extension)
{
    return IsExtensionIn (extension, s_kTapeExtensions);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier::FormatSize
//
//  Under a megabyte in kilobytes, as Apple labels its floppies; under two
//  megabytes with one decimal, as a 1.44 MB disk is labeled; whole
//  megabytes, rounded, above that, as Apple labels its hard disks: a
//  9,728-block ProFile is 4.75 MB and labeled 5 MB.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ImageClassifier::FormatSize (const ImageClass & imageClass)
{
    constexpr uint64_t  kKilobyte        = 1024;
    constexpr uint64_t  kMegabyte        = kKilobyte * kKilobyte;
    constexpr uint64_t  kOneDecimalBelow = 2 * kMegabyte;



    uint64_t  bytes = imageClass.formattedBytes;



    if (!imageClass.IsClassified() || imageClass.formFactor == ImageFormFactor::Tape)
    {
        return std::wstring();
    }

    if (bytes < kMegabyte)
    {
        return std::format (L"{}K", (bytes + kKilobyte / 2) / kKilobyte);
    }

    if (bytes < kOneDecimalBelow)
    {
        return std::format (L"{:.1f} MB", (double) bytes / (double) kMegabyte);
    }

    return std::format (L"{} MB", (bytes + kMegabyte / 2) / kMegabyte);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier::FormatFormFactor
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ImageClassifier::FormatFormFactor (const ImageClass & imageClass)
{
    if (!imageClass.IsClassified())
    {
        return std::wstring();
    }

    switch (imageClass.formFactor)
    {
        case ImageFormFactor::Floppy525:  return L"5.25-inch";
        case ImageFormFactor::Floppy35:   return L"3.5-inch";
        case ImageFormFactor::HardDisk:   return L"Hard disk";
        case ImageFormFactor::Tape:       return L"Tape";
        case ImageFormFactor::None:       return std::wstring();
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier::GetBlockFormFactor
//
//  The floppy sizes are BlockDeviceMedia's, which the hard disks refuse.
//
////////////////////////////////////////////////////////////////////////////////

ImageFormFactor ImageClassifier::GetBlockFormFactor (uint32_t blockCount)
{
    constexpr uint32_t  k525Blocks = (uint32_t) (k525Bytes / BlockImageFile::kBlockBytes);



    if (!BlockDeviceMedia::IsFloppyBlockCount (blockCount))
    {
        return ImageFormFactor::HardDisk;
    }

    return (blockCount == k525Blocks) ? ImageFormFactor::Floppy525 : ImageFormFactor::Floppy35;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier::IsExtensionIn
//
////////////////////////////////////////////////////////////////////////////////

bool ImageClassifier::IsExtensionIn (std::wstring_view extension, std::span<const wchar_t * const> list)
{
    std::wstring  text (extension);



    for (const wchar_t * entry : list)
    {
        if (_wcsicmp (text.c_str(), entry) == 0)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier::HasSignature
//
////////////////////////////////////////////////////////////////////////////////

bool ImageClassifier::HasSignature (std::span<const Byte> head, std::string_view signature)
{
    if (head.size() < signature.size())
    {
        return false;
    }

    return std::equal (signature.begin(), signature.end(), head.begin(), [] (char a, Byte b) { return (Byte) a == b; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier::MakeClassified
//
////////////////////////////////////////////////////////////////////////////////

ImageClass ImageClassifier::MakeClassified (ImageFormFactor formFactor, ImageLayout layout, uint64_t formattedBytes, uint32_t blockCount)
{
    ImageClass  result;



    result.status         = ImageStatus::Classified;
    result.formFactor     = formFactor;
    result.layout         = layout;
    result.formattedBytes = formattedBytes;
    result.blockCount     = blockCount;

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier::MakeUnclassified
//
////////////////////////////////////////////////////////////////////////////////

ImageClass ImageClassifier::MakeUnclassified()
{
    ImageClass  result;



    result.status = ImageStatus::Unclassified;

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier::ClassifyWoz
//
//  INFO is the first chunk, so the head has it. A 3.5-inch disk's side
//  count is INFO's from version 2 on; a version 1 image is taken as two
//  sides. A 3.5-inch WOZ is readable as blocks; a 5.25-inch one is not.
//
////////////////////////////////////////////////////////////////////////////////

ImageClass ImageClassifier::ClassifyWoz (std::span<const Byte> head)
{
    WozLoader::Description  about;
    vector<Byte>            bytes (head.begin(), head.end());
    bool                    isOneSide = false;
    uint32_t                blocks    = 0;



    WozLoader::Describe (bytes, about);

    if (!about.isWoz)
    {
        return MakeUnclassified();
    }

    if (about.diskType == WozLoader::kDiskType525)
    {
        return MakeClassified (ImageFormFactor::Floppy525, ImageLayout::Woz, k525Bytes, 0);
    }

    if (about.diskType != WozLoader::kDiskType35)
    {
        return MakeUnclassified();
    }

    isOneSide = about.info.sides == 1;
    blocks    = isOneSide ? Gcr35Track::kBlocksOneSide : Gcr35Track::kBlocksTwoSides;

    return MakeClassified (ImageFormFactor::Floppy35, ImageLayout::Woz, (uint64_t) blocks * BlockImageFile::kBlockBytes, blocks);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier::ClassifyTwoImg
//
//  The payload's order and length from the header. A DOS-order or nibble
//  payload is a 5.25-inch disk; a ProDOS-order payload is blocks.
//
////////////////////////////////////////////////////////////////////////////////

ImageClass ImageClassifier::ClassifyTwoImg (uint64_t fileBytes, std::span<const Byte> head)
{
    uint32_t  format     = BlockImageFile::ReadLittleLong (head, BlockImageFile::kTwoImgFormat);
    uint64_t  dataOffset = BlockImageFile::ReadLittleLong (head, BlockImageFile::kTwoImgDataOffset);
    uint64_t  dataBytes  = BlockImageFile::ReadLittleLong (head, BlockImageFile::kTwoImgDataLength);



    if (dataBytes == 0 || dataOffset + dataBytes > fileBytes)
    {
        return MakeUnclassified();
    }

    if (format == BlockImageFile::kTwoImgProDos)
    {
        return ClassifyBlocks (ImageLayout::TwoImg, dataBytes);
    }

    if ((format == BlockImageFile::kTwoImgDos && dataBytes == k525Bytes) || (format == BlockImageFile::kTwoImgNibble && dataBytes == kNibBytes))
    {
        return MakeClassified (ImageFormFactor::Floppy525, ImageLayout::TwoImg, k525Bytes, 0);
    }

    return MakeUnclassified();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier::ClassifyDiskCopy
//
//  The data size is big-endian, after the 64-byte name.
//
////////////////////////////////////////////////////////////////////////////////

ImageClass ImageClassifier::ClassifyDiskCopy (std::span<const Byte> head)
{
    uint64_t  dataBytes = BlockImageFile::ReadBigLong (head, BlockImageFile::kDcDataSize);



    return ClassifyBlocks (ImageLayout::DiskCopy42, dataBytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier::ClassifyNibble
//
//  Either track size circulates under either extension, so the length
//  decides.
//
////////////////////////////////////////////////////////////////////////////////

ImageClass ImageClassifier::ClassifyNibble (uint64_t fileBytes)
{
    if (fileBytes != kNibBytes && fileBytes != kNb2Bytes)
    {
        return MakeUnclassified();
    }

    return MakeClassified (ImageFormFactor::Floppy525, ImageLayout::Nibble, k525Bytes, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier::ClassifyRaw
//
//  A 5.25-inch disk's length under a Disk II's extension is a sector image.
//  Any other length is blocks, which the 3.5-inch drives and the hard disks
//  open under any extension.
//
////////////////////////////////////////////////////////////////////////////////

ImageClass ImageClassifier::ClassifyRaw (std::wstring_view extension, uint64_t fileBytes)
{
    constexpr uint32_t  k525Blocks = (uint32_t) (k525Bytes / BlockImageFile::kBlockBytes);



    if (fileBytes == k525Bytes && IsExtensionIn (extension, s_kSectorExtensions))
    {
        return MakeClassified (ImageFormFactor::Floppy525, ImageLayout::Sector, k525Bytes, k525Blocks);
    }

    return ClassifyBlocks (ImageLayout::Raw, fileBytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageClassifier::ClassifyBlocks
//
////////////////////////////////////////////////////////////////////////////////

ImageClass ImageClassifier::ClassifyBlocks (ImageLayout layout, uint64_t dataBytes)
{
    constexpr uint64_t  kMaxBlocks = UINT32_MAX;



    uint64_t  blocks = dataBytes / BlockImageFile::kBlockBytes;



    if (dataBytes == 0 || dataBytes % BlockImageFile::kBlockBytes != 0 || blocks > kMaxBlocks)
    {
        return MakeUnclassified();
    }

    return MakeClassified (GetBlockFormFactor ((uint32_t) blocks), layout, dataBytes, (uint32_t) blocks);
}
