#include "Pch.h"

#include "Debugger/Channel/GuardedFileSystem.h"

#include "Debugger/Channel/HostAccessGuard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GuardedFileSystem::GuardedFileSystem
//
////////////////////////////////////////////////////////////////////////////////

GuardedFileSystem::GuardedFileSystem (IFileSystem & inner, HostAccessGuard & guard) :
    m_inner (inner),
    m_guard (guard)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuardedFileSystem::ReadAllText
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GuardedFileSystem::ReadAllText (
    const std::wstring  & path,
    std::string         & outContent)
{
    HRESULT  hr = S_OK;



    outContent.clear();

    hr = CheckAccess (path, HostAccess::Read);
    CHR (hr);

    hr = m_inner.ReadAllText (path, outContent);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuardedFileSystem::WriteAllText
//
//  A file this write creates may be written again in the same line, and
//  deleted, without the overwrite option.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GuardedFileSystem::WriteAllText (
    const std::wstring  & path,
    const std::string   & content)
{
    HRESULT  hr        = S_OK;
    bool     isPresent = m_inner.Exists (path);



    hr = CheckAccess (path, HostAccess::Write);
    CHR (hr);

    hr = m_inner.WriteAllText (path, content);
    CHR (hr);

    if (!isPresent)
    {
        m_guard.NoteCreated (path);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuardedFileSystem::AppendAllText
//
//  An append changes an existing file, so it follows the overwrite rule as a
//  write does.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GuardedFileSystem::AppendAllText (
    const std::wstring  & path,
    const std::string   & content)
{
    HRESULT  hr        = S_OK;
    bool     isPresent = m_inner.Exists (path);



    hr = CheckAccess (path, HostAccess::Write);
    CHR (hr);

    hr = m_inner.AppendAllText (path, content);
    CHR (hr);

    if (!isPresent)
    {
        m_guard.NoteCreated (path);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuardedFileSystem::Exists
//
////////////////////////////////////////////////////////////////////////////////

bool GuardedFileSystem::Exists (const std::wstring & path)
{
    return m_guard.IsReadable (path) && m_inner.Exists (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuardedFileSystem::DirectoryExists
//
////////////////////////////////////////////////////////////////////////////////

bool GuardedFileSystem::DirectoryExists (const std::wstring & path)
{
    return m_guard.IsReadable (path) && m_inner.DirectoryExists (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuardedFileSystem::MakeDirectory
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GuardedFileSystem::MakeDirectory (const std::wstring & path)
{
    HRESULT  hr = S_OK;



    hr = CheckAccess (path, HostAccess::Write);
    CHR (hr);

    hr = m_inner.MakeDirectory (path);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuardedFileSystem::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GuardedFileSystem::Delete (const std::wstring & path)
{
    HRESULT  hr = S_OK;



    hr = CheckAccess (path, HostAccess::Delete);
    CHR (hr);

    hr = m_inner.Delete (path);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuardedFileSystem::EnumerateFiles
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GuardedFileSystem::EnumerateFiles (
    const std::wstring         & directory,
    std::vector<std::wstring>  & outFilenames)
{
    HRESULT  hr = S_OK;



    hr = CheckAccess (directory, HostAccess::Read);
    CHR (hr);

    hr = m_inner.EnumerateFiles (directory, outFilenames);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuardedFileSystem::EnumerateDirectories
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GuardedFileSystem::EnumerateDirectories (
    const std::wstring         & directory,
    std::vector<std::wstring>  & outDirNames)
{
    HRESULT  hr = S_OK;



    hr = CheckAccess (directory, HostAccess::Read);
    CHR (hr);

    hr = m_inner.EnumerateDirectories (directory, outDirNames);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuardedFileSystem::EnumerateEntries
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GuardedFileSystem::EnumerateEntries (
    const std::wstring            & directory,
    std::vector<FileSystemEntry>  & outEntries)
{
    HRESULT  hr = S_OK;



    hr = CheckAccess (directory, HostAccess::Read);
    CHR (hr);

    hr = m_inner.EnumerateEntries (directory, outEntries);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuardedFileSystem::GetReadOnlyAttribute
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GuardedFileSystem::GetReadOnlyAttribute (
    const std::wstring  & path,
    bool                & outReadOnly)
{
    HRESULT  hr = S_OK;



    hr = CheckAccess (path, HostAccess::Read);
    CHR (hr);

    hr = m_inner.GetReadOnlyAttribute (path, outReadOnly);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuardedFileSystem::SetReadOnlyAttribute
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GuardedFileSystem::SetReadOnlyAttribute (
    const std::wstring  & path,
    bool                  readOnly)
{
    HRESULT  hr = S_OK;



    hr = CheckAccess (path, HostAccess::Write);
    CHR (hr);

    hr = m_inner.SetReadOnlyAttribute (path, readOnly);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GuardedFileSystem::CheckAccess
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GuardedFileSystem::CheckAccess (const std::wstring & path, HostAccess access)
{
    HRESULT  hr        = S_OK;
    bool     isAllowed = m_guard.TryAccess (path, access);



    CBREx (isAllowed, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

Error:
    return hr;
}
