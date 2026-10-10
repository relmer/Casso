#pragma once

#include "Pch.h"

#include "Update/IUpdateFileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MockUpdateFileSystem
//
//  An in-memory IUpdateFileSystem that can fail on demand. Every mutating
//  call is numbered from 1; `failAt` fails exactly that call, and
//  `failFrom` fails that call and every one after it (so a restore after
//  the failure fails too). A failed call changes nothing. Paths compare
//  without regard to case, as NTFS does.
//
//  Folders are not modeled beyond what the installer needs: a file's
//  folder is assumed to exist, and CreateDirectoryTree only counts.
//
//  Header-only on purpose; lives only in the test binary.
//
////////////////////////////////////////////////////////////////////////////////

class MockUpdateFileSystem : public IUpdateFileSystem
{
public:
    static constexpr int  kNever = 0;

    std::map<std::wstring, std::vector<Byte>>  files;
    ReleaseVersion                             fileVersion { 1, 31, 0 };
    int                                        failAt      = kNever;
    int                                        failFrom    = kNever;
    int                                        operations  = 0;
    int                                        renames     = 0;

    void Put (const std::wstring & path, const std::string & content)
    {
        files[Normalize (path)] = std::vector<Byte> (content.begin(), content.end());
    }

    std::string Get (const std::wstring & path) const
    {
        auto  it = files.find (Normalize (path));

        return (it == files.end()) ? std::string ("<missing>") : std::string (it->second.begin(), it->second.end());
    }

    bool Exists (const std::wstring & path) override
    {
        return files.contains (Normalize (path));
    }

    HRESULT CreateDirectoryTree (const std::wstring &) override
    {
        return Count();
    }

    HRESULT WriteAllBytes (const std::wstring & path, std::span<const Byte> bytes) override
    {
        HRESULT  hr = Count();

        if (SUCCEEDED (hr))
        {
            files[Normalize (path)] = std::vector<Byte> (bytes.begin(), bytes.end());
        }

        return hr;
    }

    HRESULT RemoveFile (const std::wstring & path) override
    {
        HRESULT  hr = Count();

        if (SUCCEEDED (hr) && files.erase (Normalize (path)) == 0)
        {
            hr = HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        }

        return hr;
    }

    HRESULT RenameFile (const std::wstring & from, const std::wstring & to) override
    {
        HRESULT       hr      = Count();
        std::wstring  fromKey = Normalize (from);
        std::wstring  toKey   = Normalize (to);

        if (SUCCEEDED (hr) && !files.contains (fromKey))
        {
            hr = HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        }

        if (SUCCEEDED (hr) && files.contains (toKey))
        {
            hr = HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS);
        }

        if (SUCCEEDED (hr))
        {
            files[toKey] = std::move (files[fromKey]);
            files.erase (fromKey);
            renames++;
        }

        return hr;
    }

    HRESULT RemoveDirectoryTree (const std::wstring & path) override
    {
        HRESULT       hr     = Count();
        std::wstring  prefix = Normalize (path) + L"\\";

        if (SUCCEEDED (hr))
        {
            std::erase_if (files, [&prefix] (const auto & kv) { return kv.first.starts_with (prefix); });
        }

        return hr;
    }

    HRESULT GetFileVersion (const std::wstring & path, ReleaseVersion & outVersion) override
    {
        outVersion = fileVersion;
        return Exists (path) ? S_OK : HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
    }

    // The files outside the installer's own work folders, which are what a
    // failed update must leave exactly as it found them.
    std::map<std::wstring, std::vector<Byte>> GetInstalledFiles (const std::wstring & installDir) const
    {
        std::map<std::wstring, std::vector<Byte>>  result;
        std::wstring                               work = Normalize (installDir) + L"\\.update-";

        for (const auto & kv : files)
        {
            if (!kv.first.starts_with (work))
            {
                result.insert (kv);
            }
        }

        return result;
    }

    bool HasFilesUnder (const std::wstring & folder) const
    {
        std::wstring  prefix = Normalize (folder) + L"\\";

        return std::any_of (files.begin(), files.end(), [&prefix] (const auto & kv) { return kv.first.starts_with (prefix); });
    }

private:
    HRESULT Count()
    {
        operations++;

        bool  isFailed = operations == failAt || (failFrom != kNever && operations >= failFrom);

        return isFailed ? E_ACCESSDENIED : S_OK;
    }

    static std::wstring Normalize (const std::wstring & path)
    {
        std::wstring  key = path;

        std::transform (key.begin(), key.end(), key.begin(), [] (wchar_t ch) { return (wchar_t) towlower (ch); });

        return key;
    }
};
