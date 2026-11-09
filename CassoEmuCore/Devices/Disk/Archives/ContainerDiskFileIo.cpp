#include "Pch.h"

#include "Devices/Disk/Archives/ContainerDiskFileIo.h"
#include "Devices/Disk/Archives/ContainerPath.h"
#include "Devices/Disk/DurableCommit.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo::ReadAllBytes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerDiskFileIo::ReadAllBytes (const std::string & path, std::vector<Byte> & outBytes)
{
    return ContainerPath::IsNested (path) ? ReadNested (path, outBytes) : m_inner.ReadAllBytes (path, outBytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo::WriteAllBytes
//
//  A nested file that is in its container is replaced there; any other is
//  stored in memory until a rename or replace.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerDiskFileIo::WriteAllBytes (const std::string & path, const std::vector<Byte> & bytes)
{
    HRESULT  hr = S_OK;



    if (!ContainerPath::IsNested (path))
    {
        return m_inner.WriteAllBytes (path, bytes);
    }

    if (Exists (path))
    {
        hr = CommitNested (path, bytes);
    }
    else
    {
        std::lock_guard<std::mutex>  lock (m_mutex);

        m_pending[path] = bytes;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo::WriteAt
//
//  A nested file is written whole, never in place.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerDiskFileIo::WriteAt (const std::string & path, uint64_t offset, std::span<const Byte> bytes)
{
    return ContainerPath::IsNested (path) ? HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED) : m_inner.WriteAt (path, offset, bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo::ReadAt
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerDiskFileIo::ReadAt (const std::string & path, uint64_t offset, std::span<Byte> outBytes)
{
    HRESULT            hr     = S_OK;
    bool               inside = false;
    std::vector<Byte>  bytes;



    if (!ContainerPath::IsNested (path))
    {
        return m_inner.ReadAt (path, offset, outBytes);
    }

    hr = ReadNested (path, bytes);
    CHR (hr);

    inside = offset <= bytes.size() && outBytes.size() <= bytes.size() - offset;
    CBREx (inside, HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

    std::copy_n (bytes.begin() + (ptrdiff_t) offset, outBytes.size(), outBytes.begin());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo::Stat
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerDiskFileIo::Stat (const std::string & path, FileStamp & outStamp)
{
    return m_inner.Stat (ContainerPath::IsNested (path) ? ContainerPath::Parse (path).GetHost() : path, outStamp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo::Exists
//
//  A nested file exists when it is stored in memory or opens.
//
////////////////////////////////////////////////////////////////////////////////

bool ContainerDiskFileIo::Exists (const std::string & path)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  bytes;



    if (!ContainerPath::IsNested (path))
    {
        return m_inner.Exists (path);
    }

    hr = ReadNested (path, bytes);

    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo::Remove
//
//  Only a nested file stored in memory is removed; a file in a container is
//  deleted through the container's volume.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerDiskFileIo::Remove (const std::string & path)
{
    std::vector<Byte>  bytes;
    bool               taken = false;



    if (!ContainerPath::IsNested (path))
    {
        return m_inner.Remove (path);
    }

    taken = TakePending (path, bytes);

    return taken ? S_OK : HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo::ListFiles
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerDiskFileIo::ListFiles (const std::string & folder, std::vector<std::string> & outNames)
{
    return m_inner.ListFiles (folder, outNames);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo::ReplaceAtomically
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerDiskFileIo::ReplaceAtomically (const std::string & tempPath, const std::string & targetPath)
{
    HRESULT            hr    = S_OK;
    bool               taken = false;
    std::vector<Byte>  bytes;



    if (!ContainerPath::IsNested (targetPath))
    {
        return m_inner.ReplaceAtomically (tempPath, targetPath);
    }

    taken = TakePending (tempPath, bytes);
    CBREx (taken, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = CommitNested (targetPath, bytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo::FlushToStorage
//
//  A nested file is flushed when its host file is committed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerDiskFileIo::FlushToStorage (const std::string & path)
{
    return ContainerPath::IsNested (path) ? S_OK : m_inner.FlushToStorage (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo::CopyFileMetadata
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerDiskFileIo::CopyFileMetadata (const std::string & fromPath, const std::string & toPath)
{
    bool  nested = ContainerPath::IsNested (fromPath) || ContainerPath::IsNested (toPath);



    return nested ? S_OK : m_inner.CopyFileMetadata (fromPath, toPath);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo::RenameWithoutReplacing
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerDiskFileIo::RenameWithoutReplacing (const std::string & tempPath, const std::string & targetPath)
{
    HRESULT            hr     = S_OK;
    bool               taken  = false;
    bool               exists = false;
    std::vector<Byte>  bytes;



    if (!ContainerPath::IsNested (targetPath))
    {
        return m_inner.RenameWithoutReplacing (tempPath, targetPath);
    }

    exists = Exists (targetPath);
    CBREx (!exists, HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS));

    taken = TakePending (tempPath, bytes);
    CBREx (taken, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = CommitNested (targetPath, bytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo::IsHeldByAnotherProcess
//
////////////////////////////////////////////////////////////////////////////////

bool ContainerDiskFileIo::IsHeldByAnotherProcess (const std::string & path)
{
    return m_inner.IsHeldByAnotherProcess (ContainerPath::IsNested (path) ? ContainerPath::Parse (path).GetHost() : path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo::WritePayloadToStandardOutput
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerDiskFileIo::WritePayloadToStandardOutput (const std::vector<Byte> & bytes)
{
    return m_inner.WritePayloadToStandardOutput (bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo::ReadNested
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerDiskFileIo::ReadNested (const std::string & path, std::vector<Byte> & outBytes)
{
    HRESULT            hr     = S_OK;
    ContainerPath      parsed = ContainerPath::Parse (path);
    std::vector<Byte>  host;
    OpenedContainer    opened;
    std::string        error;



    outBytes.clear();

    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        auto                         it   = m_pending.find (path);

        if (it != m_pending.end())
        {
            outBytes = it->second;
            return S_OK;
        }
    }

    hr = m_inner.ReadAllBytes (parsed.GetHost(), host);
    CHR (hr);

    hr = ContainerPath::OpenBytes (parsed, host, opened, error);
    CHR (hr);

    outBytes = std::move (opened.bytes);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo::CommitNested
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ContainerDiskFileIo::CommitNested (const std::string & path, std::span<const Byte> bytes)
{
    HRESULT               hr     = S_OK;
    ContainerPath         parsed = ContainerPath::Parse (path);
    std::vector<Byte>     host;
    std::vector<Byte>     rebuilt;
    std::string           error;
    CommitPlan::Progress  progress;



    hr = m_inner.ReadAllBytes (parsed.GetHost(), host);
    CHR (hr);

    hr = ContainerPath::ReplaceBytes (parsed, host, bytes, rebuilt, error);
    CHR (hr);

    hr = DurableCommit::Commit (m_inner, parsed.GetHost(), rebuilt, CommitPlan::NextInvocationTag(), CommitMode::Replace, progress);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerDiskFileIo::TakePending
//
////////////////////////////////////////////////////////////////////////////////

bool ContainerDiskFileIo::TakePending (const std::string & path, std::vector<Byte> & outBytes)
{
    std::lock_guard<std::mutex>  lock  (m_mutex);
    auto                         it    = m_pending.find (path);
    bool                         found = it != m_pending.end();



    if (found)
    {
        outBytes = std::move (it->second);
        m_pending.erase (it);
    }

    return found;
}
