#pragma once

#include "Pch.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkVirtualDrives
//
//  The two drives the CassoLink ProDOS disk's block driver installs (FR-200):
//  each an image on the PC, read and written a block at a time at the
//  block's place in the file. A 140K .dsk or .do is in DOS 3.3 sector order,
//  so a block is its two sectors where DOS 3.3 puts them.
//
//  A write is made whole or not at all (FR-201): the block goes to a journal
//  beside the image, the image path plus kJournalSuffix, then into the
//  image, and then the journal is removed, each step flushed before the
//  next. A journal found when a drive is next used is applied first, and one
//  that is damaged, from a write cut short before it was flushed, is
//  removed, as the image wasn't written yet.
//
//  The images are set on the UI thread and served on the link's thread, so
//  both go through a lock.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkVirtualDrives
{
public:
    explicit CassoLinkVirtualDrives (IDiskFileIo & fileIo);

    static constexpr size_t  kDrives      = 2;
    static constexpr size_t  kBlockSize   = 512;
    static constexpr size_t  kSectorSize  = 256;
    static constexpr size_t  kJournalSize = 4 + 4 + kBlockSize + 2;

    //  ProDOS's errors, as the driver returns them.
    static constexpr Byte    kNoError       = 0x00;
    static constexpr Byte    kIoError       = 0x27;
    static constexpr Byte    kNoDevice      = 0x28;
    static constexpr Byte    kWriteProtect  = 0x2B;

    static constexpr const char *  kJournalSuffix = ".journal";

    //  Drive 1 or 2's image, empty for none.
    void         SetImage   (int drive, const std::string & path);
    std::string  GetImage   (int drive) const;

    //  Each returns ProDOS's error for the driver: kNoDevice for a drive
    //  with no image or a block past its end, kWriteProtect for an image
    //  that can't be written and kIoError for a failed read or write.
    Byte         GetBlocks  (int drive, uint32_t & outBlocks);
    Byte         ReadBlock  (int drive, uint32_t block, std::span<Byte> outData);
    Byte         WriteBlock (int drive, uint32_t block, std::span<const Byte> data);

    static std::string  GetJournalPath (const std::string & imagePath);

private:
    static constexpr Byte    s_kJournalMagic[4]      = { 'C', 'L', 'J', '1' };
    static constexpr size_t  s_kSectorsPerTrack      = 16;
    static constexpr size_t  s_kDosOrderSize         = 143360;
    static constexpr size_t  s_kBlockSectorToDos[16] = { 0, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 15 };

    struct Drive
    {
        std::string  path;
        uint64_t     size      = 0;
        bool         recovered = false;
    };

    HRESULT  OpenDrive    (Drive & drive);
    HRESULT  Recover      (Drive & drive);
    HRESULT  WriteInPlace (const Drive & drive, uint32_t block, std::span<const Byte> data);
    HRESULT  ReadInPlace  (const Drive & drive, uint32_t block, std::span<Byte> outData);
    bool     IsDosOrder   (const Drive & drive) const;
    uint64_t GetOffset    (const Drive & drive, uint32_t block, size_t half) const;
    Drive *  FindDrive    (int drive);

    static Byte  ToProDosError (HRESULT hr);

    IDiskFileIo                    & m_fileIo;
    mutable std::mutex               m_lock;
    std::array<Drive, kDrives>       m_drives;
};
