#pragma once

#include "Pch.h"

#include "Devices/Disk/IDiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo
//
//  An IDiskFileIo over another that reads and writes paths inside
//  containers, a ContainerPath's text such as `C:\Disks\games.zip|a.po`,
//  and passes every other path through. A write to a file that isn't in its
//  container yet is stored in memory, as a commit's temporary file, until a
//  rename or replace puts it at its target. Writing a nested file rebuilds
//  each container around it and commits the host file through a temporary
//  file beside it. The host file's size and time are the nested file's, so
//  a commit's check for a change made since the read applies to the host.
//
////////////////////////////////////////////////////////////////////////////////

class ContainerDiskFileIo : public IDiskFileIo
{
public:
    explicit ContainerDiskFileIo (IDiskFileIo & inner) : m_inner (inner) {}

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
    HRESULT  ReadNested   (const std::string & path, std::vector<Byte> & outBytes);
    HRESULT  CommitNested (const std::string & path, std::span<const Byte> bytes);
    bool     TakePending  (const std::string & path, std::vector<Byte> & outBytes);

    IDiskFileIo                               & m_inner;
    std::mutex                                  m_mutex;
    std::map<std::string, std::vector<Byte>>    m_pending;
};
