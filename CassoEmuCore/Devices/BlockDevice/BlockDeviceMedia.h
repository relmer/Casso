#pragma once

#include "Pch.h"

#include "Devices/BlockDevice/IBlockSource.h"
#include "Devices/BlockDevice/IReplayMedia.h"
#include "Devices/Disk/IDiskImage.h"

class BlockImageFile;
class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDeviceMedia
//
//  The image on a hard disk, ProFile or 3.5-inch block device: a ProDOS-order
//  file (.po, .hdv), a 2IMG file in ProDOS order, a DiskCopy 4.2 image, or
//  a WOZ 2 3.5-inch image, read a block at a time. A WOZ image is decoded to
//  blocks when it is attached, and is write protected: its blocks aren't
//  written back as GCR. Writes change blocks in memory, whole blocks at a
//  time, and Flush writes them to the file, so a reset during a guest's write
//  results in either the old block or the new one.
//
//  One path is attached to one drive at a time, across every drive.
//
////////////////////////////////////////////////////////////////////////////////

class StateReader;
class StateWriter;





class BlockDeviceMedia : public IReplayMedia
{
public:
    static constexpr size_t  kBlockBytes = 512;

    BlockDeviceMedia();
    ~BlockDeviceMedia() override;

    BlockDeviceMedia (const BlockDeviceMedia &)             = delete;
    BlockDeviceMedia & operator= (const BlockDeviceMedia &) = delete;

    //  `hostProtect` has the host file's read-only and permission state,
    //  which the caller probes. A drive that already has media detaches it
    //  first.
    HRESULT  Attach  (IDiskFileIo             & fileIo,
                      const std::string       & path,
                      uint32_t                  maxBlocks,
                      const WriteProtectInfo  & hostProtect,
                      std::wstring            & outError);

    //  Writes pending blocks, then releases the file.
    HRESULT  Detach  ();

    HRESULT  ReadBlock  (uint32_t block, std::span<Byte> outBytes);
    HRESULT  WriteBlock (uint32_t block, std::span<const Byte> bytes);

    //  Writes the changed blocks to the file. Fails, writing nothing, when
    //  the file changed on the host since it was attached or last flushed.
    HRESULT  Flush   ();

    bool                       IsAttached        () const { return m_source != nullptr; }
    uint32_t                   GetBlockCount     () const { return m_blockCount; }
    bool                       IsWriteProtected  () const { return m_protect.Any(); }
    const WriteProtectInfo   & GetWriteProtect   () const { return m_protect; }
    bool                       HasPendingWrites  () const;
    const std::string        & GetPath           () const { return m_path; }
    bool                       HasChangedOnHost  ();

    void     SetUserWriteProtected (bool isProtected) { m_protect.userSetting = isProtected; }

    //  The blocks written since the media was attached, as each was written,
    //  for a machine state; a keyframe shares the block buffers. A load puts
    //  them back, and a block written since the save gets its content from
    //  before the first write. A state saved with other media doesn't load.
    void     SaveState (StateWriter & writer) const;
    HRESULT  LoadState (StateReader & reader);

    //  The state an empty media saves, written without making one, since a
    //  keyframe saves it for every drive with no disk and may not allocate.
    static void  SaveEmptyState (StateWriter & writer);

    //  IReplayMedia: the blocks as they were when the media was attached,
    //  made once and shared by every copy after.
    HRESULT  CopyForReplay   (ReplayBlockMedia & outCopy) override;
    HRESULT  MountReplayCopy (const ReplayBlockMedia & copy) override;

    static bool  IsPathAttached (const IDiskFileIo & fileIo, const std::string & path);

    //  Whether a block count is the size of a floppy disk a Casso drive
    //  mounts: 280 blocks (140K), 800 (400K) or 1,600 (800K). A hard disk
    //  doesn't mount a floppy image; any other size it fits, it mounts. 1.44 MB
    //  (2,880 blocks) isn't one: no drive Casso models mounts a 1.44 MB disk,
    //  so such an image mounts on a hard disk until one does.
    static bool  IsFloppyBlockCount (uint32_t blockCount);

private:
    static std::string  NormalizePath  (const std::string & path);
    static std::string  GetClaimKey    (const IDiskFileIo & fileIo, const std::string & path);
    static bool         ChangeClaim    (const std::string & key, bool isClaim);
    static bool         IsLockedTwoImg (const BlockImageFile & file);
    static bool         IsNibbleImage  (const std::string & path);

    //  The blocks of a WOZ 2 3.5-inch image in `bytes`.
    HRESULT  DecodeWoz (const std::vector<Byte> & bytes, std::vector<Byte> & outBlocks, std::wstring & outError);

    //  The file or the blocks in memory; null with no media.
    std::unique_ptr<IBlockSource>     m_source;
    std::string                       m_path;
    std::string                       m_pathKey;
    uint32_t                          m_blockCount = 0;
    WriteProtectInfo                  m_protect;

    //  Each block written since the media was attached, as it is now.
    IBlockSource::WrittenBlocks       m_written;
};
