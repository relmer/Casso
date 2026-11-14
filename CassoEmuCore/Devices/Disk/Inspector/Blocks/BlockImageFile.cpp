#include "Pch.h"

#include "Devices/Disk/Containers/DiskCopy42Container.h"
#include "Devices/Disk/FileBlockStore.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  The 2IMG header's fields, from the 2IMG format's published
//  specification, and Disk Copy 4.2's, from Apple's technical note: offset,
//  length and label. A length of 2 or 4 is a number, any other a run.
//
////////////////////////////////////////////////////////////////////////////////

struct ContainerHeaderField
{
    uint32_t  offset;
    uint32_t  length;
    LPCWSTR   label;
};


static constexpr ContainerHeaderField s_kTwoImgFields[] =
{
    {  0,  4, L"2IMG: magic"               },
    {  4,  4, L"2IMG: creator"             },
    {  8,  2, L"2IMG: header size"         },
    { 10,  2, L"2IMG: version"             },
    { 12,  4, L"2IMG: image format"        },
    { 16,  4, L"2IMG: flags"               },
    { 20,  4, L"2IMG: blocks"              },
    { 24,  4, L"2IMG: data offset"         },
    { 28,  4, L"2IMG: data length"         },
    { 32,  4, L"2IMG: comment offset"      },
    { 36,  4, L"2IMG: comment length"      },
    { 40,  4, L"2IMG: creator data offset" },
    { 44,  4, L"2IMG: creator data length" },
    { 48, 16, L"2IMG: reserved"            },
};


static constexpr ContainerHeaderField s_kDiskCopyFields[] =
{
    { 64,  4, L"Disk Copy: data size"   },
    { 68,  4, L"Disk Copy: tag size"    },
    { 80,  1, L"Disk Copy: disk format" },
    { 81,  1, L"Disk Copy: format byte" },
    { 82,  2, L"Disk Copy: private"     },
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::~BlockImageFile
//
////////////////////////////////////////////////////////////////////////////////

BlockImageFile::~BlockImageFile() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::ReadLittleLong
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockImageFile::ReadLittleLong (std::span<const Byte> bytes, size_t at)
{
    return (uint32_t) bytes[at] | ((uint32_t) bytes[at + 1] << 8) | ((uint32_t) bytes[at + 2] << 16) | ((uint32_t) bytes[at + 3] << 24);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::ReadBigLong
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockImageFile::ReadBigLong (std::span<const Byte> bytes, size_t at)
{
    return ((uint32_t) bytes[at] << 24) | ((uint32_t) bytes[at + 1] << 16) | ((uint32_t) bytes[at + 2] << 8) | (uint32_t) bytes[at + 3];
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::IsTwoImgHeader
//
////////////////////////////////////////////////////////////////////////////////

bool BlockImageFile::IsTwoImgHeader (std::span<const Byte> header)
{
    constexpr std::array<Byte, 4>  kMagic = { '2', 'I', 'M', 'G' };



    return header.size() >= kTwoImgHeader && std::equal (kMagic.begin(), kMagic.end(), header.begin());
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::IsDiskCopyHeader
//
//  The marker word at $52, a name length that fits its field, and data and
//  tag sizes that add up to the file's.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockImageFile::IsDiskCopyHeader (std::span<const Byte> header, uint64_t fileBytes)
{
    if (header.size() < kDcHeader)
    {
        return false;
    }

    return header[kDcPrivate] == (Byte) (kDcMarker >> 8)
        && header[kDcPrivate + 1] == (Byte) (kDcMarker & 0xFF)
        && header[0] < kDcNameBytes
        && kDcHeader + (uint64_t) ReadBigLong (header, kDcDataSize) + ReadBigLong (header, kDcTagSize) == fileBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::IsBlockExtension
//
////////////////////////////////////////////////////////////////////////////////

bool BlockImageFile::IsBlockExtension (const std::string & path)
{
    static constexpr LPCSTR  kExtensions[] = { ".po", ".hdv", ".2mg", ".2img", ".img", ".dc", ".dc42", ".image", ".iso", ".hd" };



    std::string  lower = path;



    std::transform (lower.begin(), lower.end(), lower.begin(), [] (char c) { return (char) std::tolower ((unsigned char) c); });

    for (LPCSTR extension : kExtensions)
    {
        if (lower.ends_with (extension))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::IsBlockImage
//
////////////////////////////////////////////////////////////////////////////////

bool BlockImageFile::IsBlockImage (IDiskFileIo & fileIo, const std::string & path)
{
    HRESULT       hr      = S_OK;
    FileStamp     stamp;
    vector<Byte>  header (kDcHeader);
    bool  isBlock  = false;
    bool  isListed = IsBlockExtension (path);



    BAIL_OUT_IF (!isListed, S_OK);

    hr = fileIo.Stat (path, stamp);
    CHR (hr);

    BAIL_OUT_IF (stamp.sizeBytes < kDcHeader, S_OK);

    hr = fileIo.ReadAt (path, 0, header);
    CHR (hr);

    if (IsDiskCopyHeader (header, stamp.sizeBytes))
    {
        isBlock = true;
    }
    else if (IsTwoImgHeader (header))
    {
        isBlock = ReadLittleLong (header, kTwoImgFormat) == kTwoImgProDos
               && ReadLittleLong (header, kTwoImgDataLength) > kFloppyBytes;
    }
    else
    {
        isBlock = stamp.sizeBytes > kFloppyBytes && stamp.sizeBytes % kBlockBytes == 0;
    }

Error:
    return isBlock;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::Open
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockImageFile::Open (
    IDiskFileIo                       & fileIo,
    const std::string                 & path,
    std::unique_ptr<BlockImageFile>   & outFile,
    std::wstring                      & outError)
{
    HRESULT                          hr       = S_OK;
    std::unique_ptr<BlockImageFile>  file (new BlockImageFile (fileIo, path));
    FileStamp                        stamp;
    vector<Byte>                     header (kDcHeader);
    uint64_t                         length   = 0;
    bool                             isFit    = false;
    bool                             isProDos = false;



    outFile.reset();

    hr = fileIo.Stat (path, stamp);
    CHRF (hr, outError = L"The file can't be read.");

    length         = stamp.sizeBytes;
    file->m_stamp  = stamp;

    if (length >= kDcHeader)
    {
        hr = fileIo.ReadAt (path, 0, header);
        CHRF (hr, outError = L"The file can't be read.");
    }

    if (IsDiskCopyHeader (header, length))
    {
        file->m_kind          = BlockContainerKind::DiskCopy42;
        file->m_header        = header;
        file->m_payloadOffset = kDcHeader;
        length                = ReadBigLong (header, kDcDataSize);
        file->m_tagOffset     = kDcHeader + length;
        file->m_tagBytes      = ReadBigLong (header, kDcTagSize);
    }
    else if (IsTwoImgHeader (header))
    {
        isProDos = ReadLittleLong (header, kTwoImgFormat) == kTwoImgProDos;
        CBREx (isProDos, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));

        file->m_kind          = BlockContainerKind::TwoImg;
        file->m_header.assign (header.begin(), header.begin() + kTwoImgHeader);
        file->m_payloadOffset = ReadLittleLong (header, kTwoImgDataOffset);
        length                = ReadLittleLong (header, kTwoImgDataLength);

        isFit = file->m_payloadOffset + length <= stamp.sizeBytes;
        CBRF (isFit, outError = L"The 2IMG header's data runs past the end of the file.");
    }

    file->m_store = std::make_unique<FileBlockStore> (fileIo, path, file->m_payloadOffset, (uint32_t) std::min<uint64_t> (length / kBlockBytes, UINT32_MAX));

    outFile = std::move (file);

Error:
    if (hr == HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT) && outError.empty())
    {
        outError = L"Only a 2IMG file in ProDOS order opens in block mode.";
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::TryReadTags
//
////////////////////////////////////////////////////////////////////////////////

bool BlockImageFile::TryReadTags (uint32_t block, std::array<Byte, kTagBytesPerBlock> & outTags)
{
    uint64_t  at     = m_tagOffset + (uint64_t) block * kTagBytesPerBlock;
    HRESULT   hr     = S_OK;
    bool      isRead = m_kind == BlockContainerKind::DiskCopy42 && (uint64_t) (block + 1) * kTagBytesPerBlock <= m_tagBytes;



    outTags.fill (0);

    if (isRead)
    {
        hr     = m_fileIo.ReadAt (m_path, at, outTags);
        isRead = SUCCEEDED (hr);
    }

    return isRead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::ComputeDataChecksum
//
//  Disk Copy 4.2's checksum: each big-endian word added, and the sum
//  rotated right a bit after each.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockImageFile::ComputeDataChecksum (uint32_t & outSum)
{
    HRESULT       hr    = S_OK;
    vector<Byte>  data ((size_t) ReadBigLong (m_header, kDcDataSize));



    hr = m_fileIo.ReadAt (m_path, m_payloadOffset, data);
    CHR (hr);

    outSum = DiskCopy42Container::ComputeChecksum (data);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::IsDataChecksumValid
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockImageFile::IsDataChecksumValid (bool & outValid)
{
    HRESULT   hr       = S_OK;
    uint32_t  computed = 0;



    outValid = true;
    BAIL_OUT_IF (m_kind != BlockContainerKind::DiskCopy42, S_OK);

    hr = ComputeDataChecksum (computed);
    CHR (hr);

    outValid = computed == ReadBigLong (m_header, kDcDataSum);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::ComputeTagChecksum
//
//  The tag checksum starts after the first block's 12 tag bytes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockImageFile::ComputeTagChecksum (uint32_t & outSum)
{
    HRESULT       hr = S_OK;
    vector<Byte>  tags ((size_t) m_tagBytes);



    outSum = 0;

    BAIL_OUT_IF (m_tagBytes <= kDcTagSumSkip, S_OK);

    hr = m_fileIo.ReadAt (m_path, m_tagOffset, tags);
    CHR (hr);

    outSum = DiskCopy42Container::ComputeChecksum (std::span<const Byte> (tags).subspan (kDcTagSumSkip));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::AnnotateHeader
//
////////////////////////////////////////////////////////////////////////////////

void BlockImageFile::AnnotateHeader (vector<BlockField> & outFields)
{
    BlockFieldList  list (m_header, outFields);



    outFields.clear();

    if (m_kind == BlockContainerKind::TwoImg)
    {
        AnnotateTwoImg (list);
    }
    else if (m_kind == BlockContainerKind::DiskCopy42)
    {
        AnnotateDiskCopy (list);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::AnnotateTwoImg
//
////////////////////////////////////////////////////////////////////////////////

void BlockImageFile::AnnotateTwoImg (BlockFieldList & list)
{
    constexpr uint32_t  kWordBytes = 2;
    constexpr uint32_t  kLongBytes = 4;
    constexpr uint32_t  kMagicAt   = 0;
    constexpr uint32_t  kCreatorAt = 4;



    for (const ContainerHeaderField & field : s_kTwoImgFields)
    {
        if (field.offset == kMagicAt || field.offset == kCreatorAt)
        {
            list.AddName (field.offset, field.length, field.label, false);
        }
        else if (field.length == kWordBytes)
        {
            list.AddWord (field.offset, field.label);
        }
        else if (field.length == kLongBytes)
        {
            list.AddLong (field.offset, field.length, field.label);
        }
        else
        {
            list.AddRun (field.offset, field.length, field.label);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::AnnotateDiskCopy
//
//  Each checksum's value has "matches" or the sum of the data as it is.
//
////////////////////////////////////////////////////////////////////////////////

void BlockImageFile::AnnotateDiskCopy (BlockFieldList & list)
{
    constexpr int       kSumDigits = 8;
    constexpr uint32_t  kWordBytes = 2;
    constexpr uint32_t  kLongBytes = 4;



    uint32_t  computed = 0;
    uint32_t  stored   = 0;
    HRESULT   hrSum    = S_OK;



    list.AddName (1, std::min<uint32_t> (m_header[0], kDcNameBytes - 1), L"Disk Copy: disk name", false);

    for (const ContainerHeaderField & field : s_kDiskCopyFields)
    {
        if (field.length == kLongBytes)
        {
            list.AddBigLong (field.offset, field.label);
        }
        else if (field.length == kWordBytes)
        {
            list.AddBigWord (field.offset, field.label);
        }
        else
        {
            list.AddByte (field.offset, field.label);
        }
    }

    for (const auto & [at, label, isData] : { std::tuple<size_t, LPCWSTR, bool> (kDcDataSum, L"Disk Copy: data checksum", true),
                                              std::tuple<size_t, LPCWSTR, bool> (kDcTagSum,  L"Disk Copy: tag checksum",  false) })
    {
        stored = ReadBigLong (m_header, at);
        hrSum  = isData ? ComputeDataChecksum (computed) : ComputeTagChecksum (computed);

        list.AddRun ((uint32_t) at, kLongBytes, label,
                     BlockFieldList::FormatHex (stored, kSumDigits)
                     + (FAILED (hrSum)       ? std::wstring (L", the data can't be read")
                        : computed == stored ? std::wstring (L", matches")
                                             : L", the data sums to " + BlockFieldList::FormatHex (computed, kSumDigits)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::Commit
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockImageFile::Commit()
{
    HRESULT  hr = S_OK;



    hr = m_store->Commit();
    CHR (hr);

    if (m_kind == BlockContainerKind::DiskCopy42)
    {
        hr = WriteDataChecksum();
        CHR (hr);
    }

    hr = m_fileIo.Stat (m_path, m_stamp);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::WriteDataChecksum
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockImageFile::WriteDataChecksum()
{
    HRESULT              hr  = S_OK;
    uint32_t             sum = 0;
    std::array<Byte, 4>  big = {};



    hr = ComputeDataChecksum (sum);
    CHR (hr);

    big = { (Byte) (sum >> 24), (Byte) (sum >> 16), (Byte) (sum >> 8), (Byte) sum };

    hr = m_fileIo.WriteAt (m_path, kDcDataSum, big);
    CHR (hr);

    std::copy (big.begin(), big.end(), m_header.begin() + kDcDataSum);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile::IsFileUnchanged
//
////////////////////////////////////////////////////////////////////////////////

bool BlockImageFile::IsFileUnchanged()
{
    FileStamp  stamp;
    HRESULT    hr    = m_fileIo.Stat (m_path, stamp);



    return SUCCEEDED (hr) && stamp.sizeBytes == m_stamp.sizeBytes && stamp.modifiedUnix == m_stamp.modifiedUnix;
}