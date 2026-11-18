#pragma once

#include "Pch.h"

#include "Devices/Disk/Archives/WrappedImageFileIo.h"
#include "Devices/Disk/IBlockStore.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Inspector/Blocks/BlockFieldList.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockContainerKind
//
////////////////////////////////////////////////////////////////////////////////

enum class BlockContainerKind
{
    Raw,
    TwoImg,
    DiskCopy42,
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockImageFile
//
//  An image file opened in block mode: a raw image of blocks in ProDOS
//  order, a 2IMG file whose payload is in ProDOS order, or a Disk Copy 4.2
//  image. Its blocks are read from the file as they are touched and written
//  in place, so a 32 MB volume or a card is never read whole. The container's
//  header is read whole and annotated from the formats' documentation: the
//  2IMG format's header from its published specification, and Disk Copy 4.2's
//  from Apple's Disk Copy 4.2 technical note, with both checksums verified.
//
//  A Disk Copy 4.2 image's data checksum is written again after a commit.
//
//  An image in a gzip file or a ZIP is read whole into memory instead, and a
//  commit writes its containers again around it, compressed.
//
////////////////////////////////////////////////////////////////////////////////

class BlockImageFile
{
public:
    static constexpr size_t    kBlockBytes       = IBlockStore::kBlockBytes;
    static constexpr size_t    kFloppyBytes      = 143360;
    static constexpr size_t    kTagBytesPerBlock = 12;

    ~BlockImageFile();

    //  Whether a file opens in block mode rather than the track inspector:
    //  a block image larger than a 5.25" disk, a 2IMG file of more than 280
    //  blocks in ProDOS order, or a Disk Copy 4.2 image. Only the header is
    //  read.
    static bool     IsBlockImage (IDiskFileIo & fileIo, const std::string & path);

    static HRESULT  Open (IDiskFileIo                       & fileIo,
                          const std::string                 & path,
                          std::unique_ptr<BlockImageFile>   & outFile,
                          std::wstring                      & outError);

    IBlockStore         & GetStore         ()       { return *m_store; }
    BlockContainerKind    GetKind          () const { return m_kind; }
    uint32_t              GetBlockCount    () const { return m_store->GetBlockCount(); }
    const vector<Byte>  & GetHeader        () const { return m_header; }
    uint64_t              GetPayloadOffset () const { return m_payloadOffset; }
    const std::string   & GetPath          () const { return m_path; }

    //  The image's containers, or null for an image that is a file itself.
    WrappedImageFileIo  * GetWrapped       () const { return m_wrapped.get(); }

    //  A Disk Copy 4.2 block's 12 tag bytes, or false when the image has
    //  none.
    bool     TryReadTags (uint32_t block, std::array<Byte, kTagBytesPerBlock> & outTags);

    //  The header's fields, with each Disk Copy 4.2 checksum checked against
    //  the data.
    void     AnnotateHeader (vector<BlockField> & outFields);

    //  The store's writes made to the file, then the Disk Copy 4.2 data
    //  checksum computed again and written. The file is flushed after, even
    //  when a write fails, which ends a card's confirmation for the thread.
    HRESULT  Commit ();

    //  Whether the file's size and time are as they were when it was opened
    //  or last committed.
    bool     IsFileUnchanged ();

    //  Whether a Disk Copy 4.2 image's stored data checksum matches its data;
    //  true for any other container.
    HRESULT  IsDataChecksumValid (bool & outValid);

    static uint32_t  ReadLittleLong (std::span<const Byte> bytes, size_t at);
    static uint32_t  ReadBigLong    (std::span<const Byte> bytes, size_t at);

    //  The 2IMG and Disk Copy 4.2 headers' fields, for a reader of the
    //  container's first bytes, such as the image classifier.
    static constexpr size_t    kTwoImgHeader     = 64;
    static constexpr size_t    kTwoImgFormat     = 0x0C;
    static constexpr size_t    kTwoImgBlocks     = 0x14;
    static constexpr size_t    kTwoImgDataOffset = 0x18;
    static constexpr size_t    kTwoImgDataLength = 0x1C;
    static constexpr uint32_t  kTwoImgDos        = 0;
    static constexpr uint32_t  kTwoImgProDos     = 1;
    static constexpr uint32_t  kTwoImgNibble     = 2;
    static constexpr size_t    kDcHeader         = 84;
    static constexpr size_t    kDcDataSize       = 64;
    static constexpr size_t    kDcTagSize        = 68;

    static bool  IsTwoImgHeader    (std::span<const Byte> header);
    static bool  IsDiskCopyHeader  (std::span<const Byte> header, uint64_t fileBytes);
    static bool  IsBlockExtension  (const std::string & path);

private:
    static constexpr size_t    kDcDataSum        = 72;
    static constexpr size_t    kDcTagSum         = 76;
    static constexpr size_t    kDcPrivate        = 82;
    static constexpr Word      kDcMarker         = 0x0100;
    static constexpr size_t    kDcNameBytes      = 64;
    static constexpr size_t    kDcTagSumSkip     = 12;

    BlockImageFile (IDiskFileIo & fileIo, std::string path) : m_fileIo (fileIo), m_path (std::move (path)) {}

    static bool     IsWrappedBlockImage (IDiskFileIo & fileIo, const std::string & path);
    static HRESULT  OpenWrapped         (IDiskFileIo & fileIo, const std::string & path, std::unique_ptr<BlockImageFile> & outFile, std::wstring & outError);

    HRESULT  ComputeDataChecksum (uint32_t & outSum);
    HRESULT  ComputeTagChecksum  (uint32_t & outSum);
    HRESULT  WriteDataChecksum   ();
    void     AnnotateTwoImg      (BlockFieldList & list);
    void     AnnotateDiskCopy    (BlockFieldList & list);

    std::unique_ptr<WrappedImageFileIo>  m_wrapped;
    IDiskFileIo                        & m_fileIo;
    std::string                          m_path;
    std::unique_ptr<IBlockStore>         m_store;
    BlockContainerKind                   m_kind          = BlockContainerKind::Raw;
    vector<Byte>                         m_header;
    uint64_t                             m_payloadOffset = 0;
    uint64_t                             m_tagOffset     = 0;
    uint64_t                             m_tagBytes      = 0;
    FileStamp                            m_stamp;
};
