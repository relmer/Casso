#pragma once

#include "Pch.h"

#include "Devices/Disk/IDiskImage.h"

class BlockImageFile;
class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDeviceMedia
//
//  The image on a hard disk, ProFile or 3.5-inch block device: a ProDOS-order
//  file (.po, .hdv), a 2IMG file in ProDOS order, or a DiskCopy 4.2 image,
//  read a block at a time. Writes change blocks in memory, whole blocks at a
//  time, and Flush writes them to the file, so a reset during a guest's write
//  leaves either the old block or the new one.
//
//  One path is attached to one drive at a time, across every drive.
//
////////////////////////////////////////////////////////////////////////////////

class BlockDeviceMedia
{
public:
    static constexpr size_t  kBlockBytes = 512;

    BlockDeviceMedia();
    ~BlockDeviceMedia();

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

    bool                       IsAttached        () const { return m_file != nullptr; }
    uint32_t                   GetBlockCount     () const { return m_blockCount; }
    bool                       IsWriteProtected  () const { return m_protect.Any(); }
    const WriteProtectInfo   & GetWriteProtect   () const { return m_protect; }
    bool                       HasPendingWrites  () const { return m_hasPendingWrites; }
    const std::string        & GetPath           () const { return m_path; }
    bool                       HasChangedOnHost  ();

    void     SetUserWriteProtected (bool isProtected) { m_protect.userSetting = isProtected; }

    static bool  IsPathAttached (const std::string & path);

private:
    static std::string  NormalizePath  (const std::string & path);
    static bool         ChangeClaim    (const std::string & key, bool isClaim);
    static bool         IsLockedTwoImg (const BlockImageFile & file);

    std::unique_ptr<BlockImageFile>   m_file;
    std::string                       m_path;
    std::string                       m_pathKey;
    uint32_t                          m_blockCount       = 0;
    WriteProtectInfo                  m_protect;
    bool                              m_hasPendingWrites = false;
};
