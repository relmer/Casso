#pragma once

#include "Pch.h"

#include "Config/IFileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InMemoryFileSystem
//
//  Test-only `IFileSystem` implementation backed by a `std::map`. Path
//  comparison is case-insensitive (matching Win32 NTFS default) and
//  done after normalizing all backslashes to forward slashes so
//  callers don't have to think about separator style.
//
//  Thread-safety: a single std::mutex serializes every operation.
//  WriteAllText is atomic (the entry either contains the prior content
//  or the new content — never a partial write) and, like the real
//  filesystem, fails with E_ACCESSDENIED on a read-only entry.
//
//  Entries carry the spec-017 metadata (read-only flag, modified stamp)
//  so FileBrowseModel and the write-protect toggle are testable; the
//  Set* helpers below let tests stage that state directly.
//
//  Header-only on purpose; lives only in the test binary.
//
////////////////////////////////////////////////////////////////////////////////

class InMemoryFileSystem : public IFileSystem
{
public:
    HRESULT ReadAllText          (const std::wstring & path,
                                  std::string        & outContent) override
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        std::wstring                 key  = Normalize (path);
        auto                         it   = m_files.find (key);

        outContent.clear();

        if (it == m_files.end())
        {
            return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        }

        outContent = it->second.content;
        return S_OK;
    }


    HRESULT WriteAllText         (const std::wstring & path,
                                  const std::string  & content) override
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        std::wstring                 key  = Normalize (path);
        auto                         it   = m_files.find (key);

        if (it != m_files.end() && it->second.readOnly)
        {
            return E_ACCESSDENIED;
        }

        m_files[key].content  = content;
        m_files[key].original = NormalizeSeparators (path);
        return S_OK;
    }


    bool    Exists               (const std::wstring & path) override
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        return m_files.find (Normalize (path)) != m_files.end();
    }


    HRESULT Delete               (const std::wstring & path) override
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        m_files.erase (Normalize (path));
        return S_OK;
    }


    HRESULT EnumerateFiles       (const std::wstring        & directory,
                                  std::vector<std::wstring> & outFilenames) override
    {
        std::lock_guard<std::mutex>  lock   (m_mutex);
        std::wstring                 prefix = Normalize (directory);

        outFilenames.clear();

        if (!prefix.empty() && prefix.back() != L'/')
        {
            prefix += L'/';
        }

        for (const auto & kv : m_files)
        {
            if (kv.first.compare (0, prefix.size(), prefix) == 0)
            {
                std::wstring  remainder = kv.first.substr (prefix.size());
                if (remainder.find (L'/') == std::wstring::npos &&
                    !remainder.empty())
                {
                    outFilenames.push_back (remainder);
                }
            }
        }

        return S_OK;
    }


    HRESULT EnumerateDirectories (const std::wstring        & directory,
                                  std::vector<std::wstring> & outDirNames) override
    {
        std::lock_guard<std::mutex>  lock   (m_mutex);
        std::wstring                 prefix = Normalize (directory);
        std::set<std::wstring>       unique;

        outDirNames.clear();

        if (!prefix.empty() && prefix.back() != L'/')
        {
            prefix += L'/';
        }

        for (const auto & kv : m_files)
        {
            if (kv.first.compare (0, prefix.size(), prefix) == 0)
            {
                std::wstring  remainder = kv.first.substr (prefix.size());
                size_t        slashPos  = remainder.find (L'/');

                if (slashPos != std::wstring::npos && slashPos > 0)
                {
                    unique.insert (remainder.substr (0, slashPos));
                }
            }
        }

        outDirNames.assign (unique.begin(), unique.end());
        return S_OK;
    }


    HRESULT EnumerateEntries     (const std::wstring           & directory,
                                  std::vector<FileSystemEntry> & outEntries) override
    {
        std::lock_guard<std::mutex>  lock   (m_mutex);
        std::wstring                 prefix = Normalize (directory);
        std::set<std::wstring>       dirs;

        outEntries.clear();

        if (!prefix.empty() && prefix.back() != L'/')
        {
            prefix += L'/';
        }

        for (const auto & kv : m_files)
        {
            if (kv.first.compare (0, prefix.size(), prefix) != 0)
            {
                continue;
            }

            // Display names come from the case-preserved original path; the
            // normalized key and the original are position-aligned because
            // normalization never changes length.
            std::wstring  remainder = kv.second.original.substr (prefix.size());
            size_t        slashPos  = remainder.find (L'/');

            if (remainder.empty())
            {
                continue;
            }

            if (slashPos == std::wstring::npos)
            {
                FileSystemEntry  entry;

                entry.name         = remainder;
                entry.isFolder     = false;
                entry.sizeBytes    = kv.second.content.size();
                entry.modifiedUnix = kv.second.modifiedUnix;

                outEntries.push_back (std::move (entry));
            }
            else if (slashPos > 0)
            {
                dirs.insert (remainder.substr (0, slashPos));
            }
        }

        for (const std::wstring & d : dirs)
        {
            FileSystemEntry  entry;

            entry.name     = d;
            entry.isFolder = true;

            outEntries.push_back (std::move (entry));
        }

        return S_OK;
    }


    HRESULT GetReadOnlyAttribute (const std::wstring & path,
                                  bool               & outReadOnly) override
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        auto                         it   = m_files.find (Normalize (path));

        outReadOnly = false;

        if (it == m_files.end())
        {
            return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        }

        outReadOnly = it->second.readOnly;
        return S_OK;
    }


    HRESULT SetReadOnlyAttribute (const std::wstring & path,
                                  bool                 readOnly) override
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        auto                         it   = m_files.find (Normalize (path));

        if (it == m_files.end())
        {
            return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        }

        it->second.readOnly = readOnly;
        return S_OK;
    }


    // Test-only helpers

    size_t      FileCount   ()
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        return m_files.size();
    }


    std::string PeekContent (const std::wstring & path)
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        auto                         it   = m_files.find (Normalize (path));
        if (it == m_files.end())
        {
            return std::string();
        }

        return it->second.content;
    }


    void        SetModifiedUnix (const std::wstring & path, int64_t modifiedUnix)
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        auto                         it   = m_files.find (Normalize (path));
        if (it != m_files.end())
        {
            it->second.modifiedUnix = modifiedUnix;
        }
    }


    void        Clear       ()
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        m_files.clear();
    }


private:
    struct Entry
    {
        std::string   content;
        std::wstring  original;       // case-preserved path, separators unified
        bool          readOnly     = false;
        int64_t       modifiedUnix = 0;
    };


    static std::wstring Normalize (const std::wstring & path)
    {
        std::wstring  result = path;

        for (wchar_t & ch : result)
        {
            if (ch == L'\\')
            {
                ch = L'/';
            }
            else if (ch >= L'A' && ch <= L'Z')
            {
                ch = (wchar_t) (ch - L'A' + L'a');
            }
        }

        return result;
    }


    static std::wstring NormalizeSeparators (const std::wstring & path)
    {
        std::wstring  result = path;

        for (wchar_t & ch : result)
        {
            if (ch == L'\\')
            {
                ch = L'/';
            }
        }

        return result;
    }


    std::mutex                     m_mutex;
    std::map<std::wstring, Entry>  m_files;
};
