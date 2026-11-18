#pragma once

#include "Pch.h"

#include "Devices/Disk/Archives/ContainerPath.h"
#include "Devices/Disk/IDiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WrappedImageFileIo
//
//  An image inside a gzip file or a ZIP, at any depth, as one file in
//  memory: `disk.po.gz` or `games.zip|disks/a.hdv`. Load reads the host file
//  and opens each container; reads and writes then go to the image's bytes,
//  whatever their path. FlushToStorage writes changed bytes back: each
//  container is rebuilt around them, compressed again, and the host file is
//  committed through a temporary file. The image's time is the host file's
//  at Load and stays so after each flush, so a check for a change made since
//  a read detects only a change by something else.
//
////////////////////////////////////////////////////////////////////////////////

class WrappedImageFileIo : public IDiskFileIo
{
public:
    WrappedImageFileIo (IDiskFileIo & host, const std::string & path);

    //  Whether a path is an image in a container: a gzip file, or a ContainerPath
    //  with a ZIP member or a gzip file on the way.
    static bool  IsWrapped (const std::string & path);

    HRESULT              Load         (std::wstring & outError);
    const std::string &  GetInnerName () const { return m_innerName; }

    HRESULT  ReadAllBytes           (const std::string & path, std::vector<Byte> & outBytes) override;
    HRESULT  WriteAllBytes          (const std::string & path, const std::vector<Byte> & bytes) override;
    HRESULT  WriteAt                (const std::string & path, uint64_t offset, std::span<const Byte> bytes) override;
    HRESULT  ReadAt                 (const std::string & path, uint64_t offset, std::span<Byte> outBytes) override;
    HRESULT  Stat                   (const std::string & path, FileStamp & outStamp) override;
    bool     Exists                 (const std::string & path) override { (void) path; return true; }
    HRESULT  Remove                 (const std::string & path) override { (void) path; return E_NOTIMPL; }
    HRESULT  ListFiles              (const std::string & folder, std::vector<std::string> & outNames) override { (void) folder; outNames.clear(); return E_NOTIMPL; }
    HRESULT  ReplaceAtomically      (const std::string & tempPath, const std::string & targetPath) override { (void) tempPath; (void) targetPath; return E_NOTIMPL; }
    HRESULT  FlushToStorage         (const std::string & path) override;
    HRESULT  CopyFileMetadata       (const std::string & fromPath, const std::string & toPath) override { (void) fromPath; (void) toPath; return S_OK; }
    HRESULT  RenameWithoutReplacing (const std::string & tempPath, const std::string & targetPath) override { (void) tempPath; (void) targetPath; return E_NOTIMPL; }
    bool     IsHeldByAnotherProcess (const std::string & path) override { (void) path; return m_host.IsHeldByAnotherProcess (m_path.GetHost()); }

    HRESULT  WritePayloadToStandardOutput (const std::vector<Byte> & bytes) override { return m_host.WritePayloadToStandardOutput (bytes); }

    //  The host file's bytes with `image` in place of the image, each
    //  container rebuilt and compressed, for a copy of the host file such as
    //  a repaired one. The host file isn't written.
    HRESULT              RebuildHost (std::span<const Byte> image, std::vector<Byte> & outHostBytes, std::wstring & outError);
    const std::string &  GetHost     () const { return m_path.GetHost(); }

private:
    static ContainerPath  ParseWrapped (const std::string & path);

    IDiskFileIo        & m_host;
    ContainerPath        m_path;
    std::vector<Byte>    m_bytes;
    std::string          m_innerName;
    FileStamp            m_hostStamp;
    int64_t              m_loadedUnix = 0;
    bool                 m_isChanged  = false;
};
