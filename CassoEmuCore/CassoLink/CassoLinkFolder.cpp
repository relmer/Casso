#include "Pch.h"

#include "CassoLink/CassoLinkFolder.h"
#include "Config/IFileSystem.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DurableCommit.h"
#include "Devices/Disk/IDiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFolder::CassoLinkFolder
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkFolder::CassoLinkFolder (IFileSystem & fileSystem, IDiskFileIo & fileIo) :
    m_fileSystem (fileSystem),
    m_fileIo     (fileIo)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFolder::GetFolderFor
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkFolder::GetFolderFor (const Location & location)
{
    std::wstring  folder;



    switch (location.kind)
    {
        case Location::Kind::HostFolder:
            folder = location.path;
            break;

        case Location::Kind::DiskImage:
        case Location::Kind::DiskDirectory:
            folder = std::filesystem::path (location.path).parent_path().wstring();
            break;

        default:
            break;
    }

    return folder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFolder::SetLocation
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkFolder::SetLocation (const Location & location)
{
    std::wstring                  folder = GetFolderFor (location);
    std::lock_guard<std::mutex>   guard (m_lock);



    m_folder = folder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFolder::SetFolder
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkFolder::SetFolder (const std::wstring & folder)
{
    std::lock_guard<std::mutex>  guard (m_lock);



    m_folder = folder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFolder::IsOpen
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkFolder::IsOpen() const
{
    return !GetFolder().empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFolder::GetPath
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkFolder::GetPath() const
{
    std::string  path = TextEncoding::WideToNarrow (GetFolder());



    if (!path.empty() && path.back() != '\\')
    {
        path += '\\';
    }

    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFolder::List
//
//  Sub-folders and files both, so ADTPro's CD has its targets on screen.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkFolder::List (std::vector<std::string> & outNames)
{
    HRESULT                    hr     = S_OK;
    std::wstring               folder = GetFolder();
    std::vector<std::wstring>  folders;
    std::vector<std::wstring>  files;
    bool                       open   = !folder.empty();



    outNames.clear();
    CBREx (open, HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND));

    hr = m_fileSystem.EnumerateDirectories (folder, folders);
    CHR (hr);

    hr = m_fileSystem.EnumerateFiles (folder, files);
    CHR (hr);

    for (const std::wstring & name : folders)
    {
        outNames.push_back (TextEncoding::WideToNarrow (std::filesystem::path (name).filename().wstring()));
    }

    for (const std::wstring & name : files)
    {
        outNames.push_back (TextEncoding::WideToNarrow (std::filesystem::path (name).filename().wstring()));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFolder::Exists
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkFolder::Exists (const std::string & name)
{
    std::string  path = GetFilePath (name);



    return !path.empty() && m_fileIo.Exists (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFolder::Read
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkFolder::Read (const std::string & name, std::vector<Byte> & outBytes)
{
    HRESULT      hr   = S_OK;
    std::string  path = GetFilePath (name);
    bool         open = !path.empty();



    CBREx (open, HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND));

    hr = m_fileIo.ReadAllBytes (path, outBytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFolder::Write
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkFolder::Write (const std::string & name, const std::vector<Byte> & bytes)
{
    HRESULT                 hr       = S_OK;
    std::string             path     = GetFilePath (name);
    bool                    open     = !path.empty();
    CommitPlan::Progress    progress = {};



    CBREx (open, HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND));

    hr = DurableCommit::Commit (m_fileIo, path, bytes, m_writeTag++, CommitMode::Replace, progress);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFolder::ChangeDirectory
//
//  ".." is the parent; any other name is a folder inside the current one,
//  and must not contain a separator. The folder must exist.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkFolder::ChangeDirectory (const std::string & name)
{
    HRESULT                     hr      = S_OK;
    std::filesystem::path       current = GetFolder();
    std::filesystem::path       target;
    bool                        open    = !current.empty();
    bool                        exists  = false;
    bool                        plain   = name.find_first_of ("\\/:") == std::string::npos;



    CBREx (open, HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND));
    CBREx (plain, HRESULT_FROM_WIN32 (ERROR_BAD_PATHNAME));

    if (name == "..")
    {
        target = current.parent_path();
    }
    else
    {
        target = current / TextEncoding::NarrowToWide (name);
    }

    exists = !target.empty() && m_fileSystem.DirectoryExists (target.wstring());
    CBREx (exists, HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND));

    {
        std::lock_guard<std::mutex>  guard (m_lock);

        m_folder = target.wstring();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFolder::GetFolder
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkFolder::GetFolder() const
{
    std::lock_guard<std::mutex>  guard (m_lock);



    return m_folder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFolder::GetFilePath
//
//  Empty with no folder open, or for a name that is not one file in the
//  folder: one with a separator in it, or "..".
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkFolder::GetFilePath (const std::string & name) const
{
    std::wstring  folder = GetFolder();
    bool          plain  = !name.empty() && name.find_first_of ("\\/:") == std::string::npos && name != "..";
    std::string   path;



    if (!folder.empty() && plain)
    {
        path = TextEncoding::WideToNarrow ((std::filesystem::path (folder) / TextEncoding::NarrowToWide (name)).wstring());
    }

    return path;
}
