#pragma once

#include "Pch.h"

#include "Config/IFileSystem.h"

#include <algorithm>
#include <map>
#include <mutex>





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
//  or the new content — never a partial write).
//
//  Header-only on purpose; lives only in the test binary.
//
////////////////////////////////////////////////////////////////////////////////

class InMemoryFileSystem : public IFileSystem
{
public:
    HRESULT ReadAllText (
        const std::wstring  & path,
        std::string         & outContent) override
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        std::wstring                 key   = Normalize (path);
        auto                         it    = m_files.find (key);

        outContent.clear ();

        if (it == m_files.end ())
        {
            return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
        }

        outContent = it->second;
        return S_OK;
    }


    HRESULT WriteAllText (
        const std::wstring  & path,
        const std::string   & content) override
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        m_files[Normalize (path)] = content;
        return S_OK;
    }


    bool Exists (const std::wstring & path) override
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        return m_files.find (Normalize (path)) != m_files.end ();
    }


    HRESULT Delete (const std::wstring & path) override
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        m_files.erase (Normalize (path));
        return S_OK;
    }


    HRESULT EnumerateFiles (
        const std::wstring         & directory,
        std::vector<std::wstring>  & outFilenames) override
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        std::wstring                 prefix = Normalize (directory);

        outFilenames.clear ();

        if (!prefix.empty () && prefix.back () != L'/')
        {
            prefix += L'/';
        }

        for (const auto & kv : m_files)
        {
            if (kv.first.compare (0, prefix.size (), prefix) == 0)
            {
                std::wstring  remainder = kv.first.substr (prefix.size ());
                if (remainder.find (L'/') == std::wstring::npos &&
                    !remainder.empty ())
                {
                    outFilenames.push_back (remainder);
                }
            }
        }

        return S_OK;
    }


    // ---- Test-only helpers --------------------------------------------

    size_t FileCount ()
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        return m_files.size ();
    }


    std::string PeekContent (const std::wstring & path)
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        auto                         it = m_files.find (Normalize (path));
        if (it == m_files.end ())
        {
            return std::string ();
        }
        return it->second;
    }


    void Clear ()
    {
        std::lock_guard<std::mutex>  lock (m_mutex);
        m_files.clear ();
    }


private:
    static std::wstring Normalize (const std::wstring & path)
    {
        std::wstring  result = path;
        size_t        i      = 0;

        for (i = 0; i < result.size (); ++i)
        {
            if (result[i] == L'\\')
            {
                result[i] = L'/';
            }
            else if (result[i] >= L'A' && result[i] <= L'Z')
            {
                result[i] = (wchar_t) (result[i] - L'A' + L'a');
            }
        }
        return result;
    }


    std::mutex                              m_mutex;
    std::map<std::wstring, std::string>     m_files;
};
