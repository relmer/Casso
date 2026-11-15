#pragma once

#include "Pch.h"

#include "Devices/Disk/BlockBuffer.h"
#include "Machines/Apple2/Common/HfsBTree.h"
#include "Machines/Apple2/Common/HfsBTreeEditor.h"
#include "Machines/Apple2/Common/IVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HfsWriter
//
//  Changes to an HFS volume, each made on a BlockBuffer over its bytes, so
//  a change reads the blocks it touches and the trees' files. The catalog
//  and extents overflow trees are changed in place by HfsBTreeEditor, and
//  only the nodes it changes are written back, with the volume bitmap, the
//  master directory block and its copy in the volume's second-last block.
//  A record's fields this class doesn't interpret are written back as they
//  were read.
//
//  A new fork takes at most three runs of allocation blocks, the most a
//  catalog record has room for; a volume too fragmented for that fails with
//  ERROR_DISK_FULL. A tree file with too few free nodes for an insert grows
//  first: its last extent is lengthened when the blocks after it are free,
//  and otherwise the file is moved to new runs.
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
    static HRESULT  Write           (BlockBuffer & ioImage, const FilePath & path, const FilePayload & payload);
    static HRESULT  Delete          (BlockBuffer & ioImage, const FilePath & path, DeleteOutcome & outOutcome);
    static HRESULT  CreateDirectory (BlockBuffer & ioImage, const FilePath & path);
    static HRESULT  RemoveDirectory (BlockBuffer & ioImage, const FilePath & path, bool force, DeleteOutcome & outOutcome);
    static HRESULT  Rename          (BlockBuffer & ioImage, const FilePath & from, const std::string & to);
    static HRESULT  SetAttributes   (BlockBuffer & ioImage, const FilePath & path, const FileAttributes & attributes);

    //  The same changes on a whole image.
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

    //  A volume name: 1 to 27 Mac OS Roman characters, no colon.
    static bool  IsValidVolumeName (const std::string & name);

    //  The catalog's leaf records in the order the volume's tree links them.
    static HRESULT  ReadCatalogRecords (std::vector<Byte> & image, std::vector<HfsBTree::Record> & outRecords);

    //  Each broken invariant of the catalog and extents trees, as
    //  HfsBTreeEditor::Check reports them.
    static HRESULT  CheckTrees (std::vector<Byte> & image, std::string & outProblems);

private:
    struct Extent
    {
        Word  start = 0;
        Word  count = 0;
    };

    struct Tree
    {
        Extent                           file[3];
        uint32_t                         bytes   = 0;
        std::vector<Byte>                nodes;
        std::unique_ptr<HfsBTreeEditor>  editor;
        bool                             isMoved = false;
    };

    struct State
    {
        BlockBuffer        * image            = nullptr;
        Word                 bitmapStart      = 0;
        Word                 allocationBlocks = 0;
        uint32_t             allocationBytes  = 0;
        Word                 firstBlock       = 0;
        uint32_t             nextId           = 0;
        Tree                 extents;
        Tree                 catalog;
    };

    static constexpr size_t    kSectorBytes         = 512;
    static constexpr size_t    kMdbOffset           = 1024;
    static constexpr Word      kSignature           = 0x4244;
    static constexpr uint32_t  kRootId              = 2;
    static constexpr uint32_t  kRootParentId        = 1;
    static constexpr uint32_t  kExtentsFileId       = 3;
    static constexpr uint32_t  kCatalogFileId       = 4;
    static constexpr uint32_t  kFirstUserId         = 16;
    static constexpr Byte      kRecordDirectory     = 1;
    static constexpr Byte      kRecordFile          = 2;
    static constexpr Byte      kRecordDirThread     = 3;
    static constexpr Byte      kRecordFileThread    = 4;
    static constexpr size_t    kDirectoryBytes      = 70;
    static constexpr size_t    kFileBytes           = 102;
    static constexpr size_t    kThreadBytes         = 46;
    static constexpr Byte      kForkData            = 0x00;
    static constexpr Byte      kForkResource        = 0xFF;
    static constexpr Byte      kFileLocked          = 0x01;
    static constexpr size_t    kMaxNameLength       = 31;
    static constexpr size_t    kMaxVolumeNameLength = 27;
    static constexpr int64_t   kMacToUnixSeconds    = 2082844800;

    static Word      GetWord (const std::vector<Byte> & bytes, size_t at) { return (Word) ((bytes[at] << 8) | bytes[at + 1]); }
    static uint32_t  GetLong (const std::vector<Byte> & bytes, size_t at) { return ((uint32_t) GetWord (bytes, at) << 16) | GetWord (bytes, at + 2); }
    static void      PutWord (std::vector<Byte> & bytes, size_t at, uint32_t value);
    static void      PutLong (std::vector<Byte> & bytes, size_t at, uint32_t value);

    //  The image is read through a const reference, so a block read isn't
    //  counted as touched.
    static Word      GetWord  (const BlockBuffer & image, size_t at) { return (Word) ((image[at] << 8) | image[at + 1]); }
    static uint32_t  GetLong  (const BlockBuffer & image, size_t at) { return ((uint32_t) GetWord (image, at) << 16) | GetWord (image, at + 2); }
    static void      PutWord  (BlockBuffer & image, size_t at, uint32_t value);
    static void      PutLong  (BlockBuffer & image, size_t at, uint32_t value);
    static void      PutBytes (BlockBuffer & image, size_t at, const Byte * bytes, size_t length);
    static void      Zero     (BlockBuffer & image, size_t at, size_t length);
    static HRESULT   Edit     (std::vector<Byte> & ioImage, const std::function<HRESULT (BlockBuffer &)> & change);

    static uint32_t           GetMacNow      ();
    static uint32_t           ToMacDate      (Word date, Word time);
    static bool               EncodeName     (const std::string & name, std::vector<Byte> & outName);
    static std::vector<Byte>  MakeCatalogKey (uint32_t parent, const std::vector<Byte> & name);

    static HRESULT   Load            (BlockBuffer & image, State & outState);
    static HRESULT   Save            (State & state);
    static HRESULT   LoadTree        (State & state, HfsBTree::Kind kind, Tree & ioTree);
    static void      WriteTree       (State & state, const Tree & tree);
    static HRESULT   GrowTree        (State & state, Tree & ioTree, uint32_t needed);
    static HRESULT   EnsureNodes     (State & state, Tree & ioTree, uint32_t count);
    static HRESULT   InsertRecord    (State & state, Tree & ioTree, const HfsBTree::Record & record);
    static HRESULT   ReplaceRecord   (State & state, Tree & ioTree, const HfsBTree::Record & record);
    static size_t    GetBlockOffset  (const State & state, Word block) { return (size_t) state.firstBlock * kSectorBytes + (size_t) block * state.allocationBytes; }

    static bool      IsAllocated     (const State & state, Word block);
    static void      SetAllocated    (State & state, Word block, bool allocated);
    static HRESULT   Allocate        (State & state, uint32_t blocks, std::vector<Extent> & outRuns);
    static void      Free            (State & state, const Extent & run);
    static HRESULT   WriteFork       (State & state, const std::vector<Byte> & bytes, std::vector<Extent> & outRuns);

    static HRESULT   FindDirectory   (const State & state, const FilePath & path, size_t components, uint32_t & outId);
    static bool      FindRecord      (const State & state, uint32_t parent, const std::vector<Byte> & name, Byte type, HfsBTree::Record & outRecord);
    static bool      FindThread      (const State & state, uint32_t id, HfsBTree::Record & outRecord);
    static HRESULT   ResolveLeaf     (const State & state, const FilePath & path, uint32_t & outParent, std::vector<Byte> & outName);

    static HRESULT   AdjustValence   (State & state, uint32_t directory, int delta);
    static void      AdjustCount     (State & state, size_t at, int delta);
    static HRESULT   RemoveFile      (State & state, const HfsBTree::Record & record, DeleteOutcome & outOutcome);
    static HRESULT   FreeForkExtents (State & state, const std::vector<Byte> & data, size_t at, uint32_t fileId, Byte fork, DeleteOutcome & outOutcome);
};
