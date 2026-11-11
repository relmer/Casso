#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/HfsBTree.h"
#include "Machines/Apple2/Common/IVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter
//
//  Changes to an HFS volume, each made on a copy of its bytes: the catalog
//  and extents overflow trees are read as records, the change is made to
//  them and to the volume bitmap, both trees are built again in their files,
//  and the master directory block and its copy in the volume's second-last
//  block are brought up to date. A record's fields this class doesn't
//  interpret are written back as they were read.
//
//  A new fork takes at most three runs of allocation blocks, the most a
//  catalog record has room for; a volume too fragmented for that fails with
//  ERROR_DISK_FULL. A catalog or extents file whose tree outgrows it gains
//  a run of blocks when its record in the master directory block has room
//  for one.
//
//  A ProDOS file copied in gets GS/OS's Finder type and creator from Apple's
//  File Type Note: 'TEXT', 'PSYS' and 'PS16' for text, system and GS/OS
//  system files, and for any other, creator 'pdos' with type 'p' followed
//  by the ProDOS type and aux type. Finder information in the payload is
//  written instead.
//
////////////////////////////////////////////////////////////////////////////////

class HfsWriter
{
public:
    static HRESULT  Write           (std::vector<Byte> & ioImage, const FilePath & path, const FilePayload & payload);
    static HRESULT  Delete          (std::vector<Byte> & ioImage, const FilePath & path, DeleteOutcome & outOutcome);
    static HRESULT  CreateDirectory (std::vector<Byte> & ioImage, const FilePath & path);
    static HRESULT  RemoveDirectory (std::vector<Byte> & ioImage, const FilePath & path, bool force, DeleteOutcome & outOutcome);
    static HRESULT  Rename          (std::vector<Byte> & ioImage, const FilePath & from, const std::string & to);
    static HRESULT  SetAttributes   (std::vector<Byte> & ioImage, const FilePath & path, const FileAttributes & attributes);

    //  The Finder type and creator GS/OS maps a ProDOS type and aux type to.
    static void  MapFromProDos (Byte type, Word auxType, uint32_t & outMacType, uint32_t & outCreator);

    //  An empty HFS volume of `sectors`, 800K or a hard disk's size, with the
    //  name given.
    static HRESULT  Format (std::vector<Byte> & outImage, size_t sectors, const std::string & name);

    //  A name HFS can store: 1 to 31 Mac OS Roman characters, no colon.
    static bool  IsValidName (const std::string & name);

    //  The catalog's leaf records in the order the volume's tree links them.
    static HRESULT  ReadCatalogRecords (std::vector<Byte> & image, std::vector<HfsBTree::Record> & outRecords);

private:
    struct Extent
    {
        Word  start = 0;
        Word  count = 0;
    };

    struct State
    {
        std::vector<Byte>              * image            = nullptr;
        Word                             bitmapStart      = 0;
        Word                             allocationBlocks = 0;
        uint32_t                         allocationBytes  = 0;
        Word                             firstBlock       = 0;
        uint32_t                         nextId           = 0;
        uint32_t                         extentsBytes     = 0;
        Extent                           extentsFile[3];
        uint32_t                         catalogBytes     = 0;
        Extent                           catalogFile[3];
        std::vector<HfsBTree::Record>    catalog;
        std::vector<HfsBTree::Record>    extents;
    };

    static constexpr size_t    kSectorBytes      = 512;
    static constexpr size_t    kMdbOffset        = 1024;
    static constexpr Word      kSignature        = 0x4244;
    static constexpr uint32_t  kRootId           = 2;
    static constexpr uint32_t  kRootParentId     = 1;
    static constexpr uint32_t  kExtentsFileId    = 3;
    static constexpr uint32_t  kCatalogFileId    = 4;
    static constexpr uint32_t  kFirstUserId      = 16;
    static constexpr Byte      kRecordDirectory  = 1;
    static constexpr Byte      kRecordFile       = 2;
    static constexpr Byte      kRecordDirThread  = 3;
    static constexpr Byte      kRecordFileThread = 4;
    static constexpr size_t    kDirectoryBytes   = 70;
    static constexpr size_t    kFileBytes        = 102;
    static constexpr size_t    kThreadBytes      = 46;
    static constexpr Byte      kForkData         = 0x00;
    static constexpr Byte      kForkResource     = 0xFF;
    static constexpr Byte      kFileLocked       = 0x01;
    static constexpr size_t    kMaxNameLength    = 31;
    static constexpr int64_t   kMacToUnixSeconds = 2082844800;

    static Word      GetWord (const std::vector<Byte> & bytes, size_t at) { return (Word) ((bytes[at] << 8) | bytes[at + 1]); }
    static uint32_t  GetLong (const std::vector<Byte> & bytes, size_t at) { return ((uint32_t) GetWord (bytes, at) << 16) | GetWord (bytes, at + 2); }
    static void      PutWord (std::vector<Byte> & bytes, size_t at, uint32_t value);
    static void      PutLong (std::vector<Byte> & bytes, size_t at, uint32_t value);

    static uint32_t  GetMacNow       ();
    static uint32_t  ToMacDate       (Word date, Word time);
    static bool      EncodeName      (const std::string & name, std::vector<Byte> & outName);
    static std::vector<Byte>  MakeCatalogKey (uint32_t parent, const std::vector<Byte> & name);

    static HRESULT   Load            (std::vector<Byte> & image, State & outState);
    static HRESULT   Save            (State & state);
    static HRESULT   ReadTree        (State & state, const Extent (& file)[3], uint32_t bytes, std::vector<HfsBTree::Record> & outRecords);
    static HRESULT   WriteTree       (State & state, HfsBTree::Kind kind, Extent (& file)[3], uint32_t & ioBytes, const std::vector<HfsBTree::Record> & records);
    static HRESULT   GrowTree        (State & state, Extent (& file)[3], uint32_t & ioBytes, uint32_t needed);
    static size_t    GetBlockOffset  (const State & state, Word block) { return (size_t) state.firstBlock * kSectorBytes + (size_t) block * state.allocationBytes; }

    static bool      IsAllocated     (const State & state, Word block);
    static void      SetAllocated    (State & state, Word block, bool allocated);
    static HRESULT   Allocate        (State & state, uint32_t blocks, std::vector<Extent> & outRuns);
    static void      Free            (State & state, const Extent & run);
    static HRESULT   WriteFork       (State & state, const std::vector<Byte> & bytes, std::vector<Extent> & outRuns);

    static HRESULT   FindDirectory   (const State & state, const FilePath & path, size_t components, uint32_t & outId);
    static int       FindRecord      (const State & state, uint32_t parent, const std::vector<Byte> & name, Byte type);
    static int       FindThread      (const State & state, uint32_t id);
    static HRESULT   ResolveLeaf     (const State & state, const FilePath & path, uint32_t & outParent, std::vector<Byte> & outName);

    static void      AdjustValence   (State & state, uint32_t directory, int delta);
    static void      AdjustCount     (State & state, size_t at, int delta);
    static HRESULT   RemoveFile      (State & state, size_t record, DeleteOutcome & outOutcome);
    static void      FreeForkExtents (State & state, const std::vector<Byte> & data, size_t at, uint32_t fileId, Byte fork, DeleteOutcome & outOutcome);
};
