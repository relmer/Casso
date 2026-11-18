#pragma once

#include "Pch.h"

#include "Debugger/Channel/IHostPathResolver.h"
#include "UiTests/InMemoryFileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostPathResolver
//
//  IHostPathResolver over a list of folders, the files of an in-memory file
//  system, and links: a path under a link's folder resolves to the same
//  path under its target, as GetFinalPathNameByHandle resolves a junction.
//  Paths compare without regard to case or separator. Header-only; the test
//  binary only.
//
////////////////////////////////////////////////////////////////////////////////

class FakeHostPathResolver : public IHostPathResolver
{
public:
    explicit FakeHostPathResolver (InMemoryFileSystem * files = nullptr) : m_files (files)
    {
        knownFolders[HostKnownFolder::Documents]     = L"C:\\Users\\Tester\\Documents";
        knownFolders[HostKnownFolder::Startup]       = L"C:\\Users\\Tester\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup";
        knownFolders[HostKnownFolder::CommonStartup] = L"C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\StartUp";
    }

    void AddFolder (const std::wstring & folder)
    {
        std::wstring  path = Key (folder);



        //  A folder's parents exist too, up to the drive.
        while (path.size() > 3)
        {
            m_folders.insert (path);
            path = path.substr (0, path.find_last_of (L'\\'));
        }
    }

    void AddLink (const std::wstring & link, const std::wstring & target)
    {
        m_links.push_back ({ Key (link), target });
        AddFolder (link.substr (0, link.find_last_of (L'\\')));
    }

    bool TryGetFinalPath (const std::wstring & path, std::wstring & finalPath) override
    {
        std::wstring  key = Key (path);



        finalPath = Follow (path);

        if (key.size() <= 3 || m_folders.contains (Key (finalPath)) || (m_files != nullptr && m_files->Exists (finalPath)))
        {
            return true;
        }

        return false;
    }

    bool Exists (const std::wstring & path) override
    {
        std::wstring  finalPath;



        return TryGetFinalPath (path, finalPath);
    }

    bool TryGetKnownFolder (HostKnownFolder folder, std::wstring & path) override
    {
        if (!knownFolders.contains (folder))
        {
            return false;
        }

        path = knownFolders[folder];
        return true;
    }

    std::map<HostKnownFolder, std::wstring>  knownFolders;

private:
    std::wstring Follow (const std::wstring & path) const
    {
        std::wstring  key = Key (path);



        for (const auto & [link, target] : m_links)
        {
            if (key == link || (key.starts_with (link) && key[link.size()] == L'\\'))
            {
                return target + path.substr (link.size());
            }
        }

        return path;
    }

    static std::wstring Key (const std::wstring & path)
    {
        std::wstring  key = path;



        for (wchar_t & ch : key)
        {
            ch = (ch == L'/') ? L'\\' : (wchar_t) towlower (ch);
        }

        while (key.size() > 3 && key.back() == L'\\')
        {
            key.pop_back();
        }

        return key;
    }

    InMemoryFileSystem                                * m_files = nullptr;
    std::set<std::wstring>                              m_folders;
    std::vector<std::pair<std::wstring, std::wstring>>  m_links;
};
