#pragma once

#include "Pch.h"

#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PhysicalCardFileIo
//
//  An IDiskFileIo over another in which a partition of an open card, such as
//  `\\.\PhysicalDrive2|#3.po`, is a file. Every path that is not on a
//  physical drive is passed through.
//
//  A commit's temporary file beside a partition is stored in memory, and
//  replacing the partition with it writes the partition through
//  PhysicalCards, which is where the user confirms the write. Nothing else
//  on a drive can be written, removed or created.
//
////////////////////////////////////////////////////////////////////////////////

class PhysicalCardFileIo : public IDiskFileIo
{
public:
    PhysicalCardFileIo (IDiskFileIo & inner, PhysicalCards & cards) : m_inner (inner), m_cards (cards) {}

    HRESULT  ReadAllBytes           (const std::string & path, std::vector<Byte> & outBytes) override;
    HRESULT  WriteAllBytes          (const std::string & path, const std::vector<Byte> & bytes) override;
    HRESULT  WriteAt                (const std::string & path, uint64_t offset, std::span<const Byte> bytes) override;
    HRESULT  ReadAt                 (const std::string & path, uint64_t offset, std::span<Byte> outBytes) override;
    HRESULT  Stat                   (const std::string & path, FileStamp & outStamp) override;
    bool     Exists                 (const std::string & path) override;
    HRESULT  Remove                 (const std::string & path) override;
    HRESULT  ListFiles              (const std::string & folder, std::vector<std::string> & outNames) override;
    HRESULT  ReplaceAtomically      (const std::string & tempPath, const std::string & targetPath) override;
    HRESULT  FlushToStorage         (const std::string & path) override;
    HRESULT  CopyFileMetadata       (const std::string & fromPath, const std::string & toPath) override;
    HRESULT  RenameWithoutReplacing (const std::string & tempPath, const std::string & targetPath) override;
    bool     IsHeldByAnotherProcess (const std::string & path) override;

    HRESULT  WritePayloadToStandardOutput (const std::vector<Byte> & bytes) override;

private:
    struct Target
    {
        PhysicalCards::PathKind  kind      = PhysicalCards::PathKind::None;
        int                      drive     = -1;
        int                      partition = 0;
    };

    static Target  Resolve (const std::string & path);

    bool  TryGetPending (const std::string & path, std::vector<Byte> & outBytes, bool remove);

    IDiskFileIo                               & m_inner;
    PhysicalCards                             & m_cards;
    std::mutex                                  m_mutex;
    std::map<std::string, std::vector<Byte>>    m_pending;
};