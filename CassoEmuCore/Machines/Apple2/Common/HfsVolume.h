#pragma once

#include "Pch.h"

#include "Devices/Disk/BlockImage.h"
#include "Machines/Apple2/Common/IVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HfsVolume
//
//  The Macintosh Hierarchical File System, as GS/OS reads and writes it on
//  800K disks and hard disks, over a BlockImage, so a volume of a gigabyte or
//  more reads only the blocks a listing or a file touches. A change is made
//  by HfsWriter on a copy of the volume's bytes.
//
//  The layout is from Apple's Inside Macintosh: Files, "Data Organization on
//  Volumes". Block 2 is the master directory block. The volume is a run of
//  allocation blocks from drAlBlSt, each drAlBlkSiz bytes. The catalog and
//  extents overflow files are B-trees of 512-byte nodes. A catalog key is a
//  parent directory ID and a name of up to 31 Mac OS Roman characters; its
//  record is a directory, a file, or a thread from a directory ID to its
//  parent and name. A fork's first three extents are in its file record, and
//  the rest in the extents overflow file. The root directory's ID is 2.
//
//  All numbers are big-endian. Dates are seconds since 1904.
//
//  A file's ProDOS type and aux type are taken from its Finder type and
//  creator by Apple's File Type Note for GS/OS: creator 'pdos' with type
//  'p', then the ProDOS type and the aux type; 'PSYS' and 'PS16' are system
//  files, 'TEXT' is text, and any other type is $00.
//
////////////////////////////////////////////////////////////////////////////////

class HfsVolume : public IVolume
{
public:
    static constexpr Word      kSignature        = 0x4244;   // 'BD'
    static constexpr uint32_t  kRootId           = 2;
    static constexpr uint32_t  kExtentsFileId    = 3;
    static constexpr uint32_t  kCatalogFileId    = 4;
    static constexpr size_t    kMdbOffset        = 1024;
    static constexpr size_t    kSectorBytes      = 512;
    static constexpr size_t    kNodeBytes        = 512;
    static constexpr size_t    kMaxNameLength    = 31;
    static constexpr int64_t   kMacToUnixSeconds = 2082844800;

    explicit HfsVolume (const BlockImage & image);

    //  Whether block 2 has the HFS signature and a plausible allocation.
    static bool  IsFound (const BlockImage & image);

    HRESULT  Enumerate          (VolumeListing & outListing) const override;
    HRESULT  EnumerateDirectory (const FilePath & directory, VolumeListing & outListing) const override;
    HRESULT  Read               (const FilePath & path, FilePayload & outPayload) const override;

    HRESULT  CreateDirectory  (const FilePath & path, vector<Byte> & outBuffer) const override;
    HRESULT  BuildRemovalPlan (const FilePath & path, DirectoryRemovalPlan & outPlan) const override;
    HRESULT  RemoveDirectory  (const FilePath  & path,
                               bool              force,
                               vector<Byte>    & outBuffer,
                               DeleteOutcome   & outOutcome) const override;

    HRESULT  Write     (const FilePath     & path,
                        const FilePayload  & payload,
                        vector<Byte>       & outBuffer) const override;
    HRESULT  Delete    (const FilePath & path, vector<Byte> & outBuffer) const override;
    HRESULT  Delete    (const FilePath  & path,
                        vector<Byte>    & outBuffer,
                        DeleteOutcome   & outOutcome) const override;
    HRESULT  BuildIntegrityReport (VolumeIntegrityReport & outReport) const override;
    HRESULT  SetStartupProgram    (const FilePath & path, vector<Byte> & outBuffer) const override;
    HRESULT  Rename    (const FilePath     & from,
                        const std::string  & to,
                        vector<Byte>       & outBuffer) const override;
    HRESULT  SetAttributes (const FilePath        & path,
                            const FileAttributes  & attributes,
                            vector<Byte>          & outBuffer) const override;

    //  The same, into a buffer of the blocks the change touches, which is all
    //  a change to a large volume over a store reads.
    HRESULT  CreateDirectory   (const FilePath & path, BlockBuffer & outBuffer) const override;
    HRESULT  RemoveDirectory   (const FilePath & path, bool force, BlockBuffer & outBuffer, DeleteOutcome & outOutcome) const override;
    HRESULT  Write             (const FilePath & path, const FilePayload & payload, BlockBuffer & outBuffer) const override;
    HRESULT  Delete            (const FilePath & path, BlockBuffer & outBuffer, DeleteOutcome & outOutcome) const override;
    HRESULT  Rename            (const FilePath & from, const std::string & to, BlockBuffer & outBuffer) const override;
    HRESULT  SetAttributes     (const FilePath & path, const FileAttributes & attributes, BlockBuffer & outBuffer) const override;

    //  The ProDOS type and aux type GS/OS maps a Finder type and creator to.
    static void  MapToProDos (uint32_t macType, uint32_t macCreator, Byte & outType, Word & outAux);

private:
    struct Extent
    {
        Word  start = 0;
        Word  count = 0;
    };

    struct Mdb
    {
        Word         volumeBitmapStart = 0;
        Word         allocationBlocks  = 0;
        uint32_t     allocationBytes   = 0;
        Word         firstBlock        = 0;
        Word         freeBlocks        = 0;
        std::string  name;
        uint32_t     extentsFileBytes  = 0;
        Extent       extentsFile[3];
        uint32_t     catalogFileBytes  = 0;
        Extent       catalogFile[3];
    };

    struct CatalogRecord
    {
        uint32_t           parentId     = 0;
        std::string        name;
        Byte               type         = 0;
        uint32_t           id           = 0;
        bool               isLocked     = false;
        uint32_t           macType      = 0;
        uint32_t           macCreator   = 0;
        uint32_t           dataBytes    = 0;
        uint32_t           dataPhysical = 0;
        uint32_t           forkBytes    = 0;
        uint32_t           forkPhysical = 0;
        uint32_t           created      = 0;
        uint32_t           modified     = 0;
        Extent             dataExtents[3];
        Extent             forkExtents[3];
        std::vector<Byte>  finderInfo;
    };

    static constexpr Byte  kRecordDirectory  = 1;
    static constexpr Byte  kRecordFile       = 2;
    static constexpr Byte  kNodeLeaf         = 0xFF;
    static constexpr Byte  kForkData         = 0x00;
    static constexpr Byte  kForkResource     = 0xFF;
    static constexpr int   kMaxNodes         = 65536;

    Byte      ReadByte  (size_t at) const { return at < m_image.size() ? m_image[at] : (Byte) 0; }
    Word      ReadWord  (size_t at) const { return (Word) ((ReadByte (at) << 8) | ReadByte (at + 1)); }
    uint32_t  ReadLong  (size_t at) const { return ((uint32_t) ReadWord (at) << 16) | ReadWord (at + 2); }

    static Word      GetWord (const vector<Byte> & bytes, size_t at) { return (Word) ((bytes[at] << 8) | bytes[at + 1]); }
    static uint32_t  GetLong (const vector<Byte> & bytes, size_t at) { return ((uint32_t) GetWord (bytes, at) << 16) | GetWord (bytes, at + 2); }

    static std::string  DecodeName  (const Byte * name, size_t length);
    static void         ReadExtents (const vector<Byte> & bytes, size_t at, Extent (& outExtents)[3]);

    HRESULT  ReadMdb          (Mdb & outMdb) const;
    HRESULT  GetForkExtents   (const Mdb & mdb, uint32_t fileId, Byte fork, const Extent (& first)[3], uint32_t physicalBytes, vector<Extent> & outExtents) const;
    HRESULT  ReadForkBytes    (const Mdb & mdb, const vector<Extent> & extents, uint64_t offset, size_t length, vector<Byte> & outBytes) const;
    HRESULT  ReadNode         (const Mdb & mdb, const vector<Extent> & extents, uint32_t node, vector<Byte> & outNode) const;
    HRESULT  ReadLeafRecords  (const Mdb & mdb, const vector<Extent> & extents, const std::function<void (const vector<Byte> & node, size_t key, size_t keyEnd)> & visit) const;
    HRESULT  LoadCatalog      (const Mdb & mdb, vector<CatalogRecord> & outRecords) const;
    HRESULT  FindDirectory    (const vector<CatalogRecord> & records, const FilePath & path, uint32_t & outId) const;
    HRESULT  ListDirectory    (const Mdb & mdb, const vector<CatalogRecord> & records, uint32_t id, VolumeListing & outListing) const;

    HRESULT      ApplyChange       (BlockBuffer & outBuffer, const std::function<HRESULT (BlockBuffer &)> & change) const;
    HRESULT      ApplyChange       (vector<Byte> & outBuffer, const std::function<HRESULT (BlockBuffer &)> & change) const;
    static void  AddRemovalEntries (const vector<CatalogRecord> & records, uint32_t id, const std::string & prefix, DirectoryRemovalPlan & ioPlan);

    BlockImage  m_image;
};
