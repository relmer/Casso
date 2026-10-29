#include "Pch.h"

#include "LongPath.h"





static constexpr const wchar_t *  s_kpszLocalPrefix  = L"\\\\?\\";
static constexpr const wchar_t *  s_kpszUncPrefix    = L"\\\\?\\UNC\\";
static constexpr const wchar_t *  s_kpszDevicePrefix = L"\\\\.\\";





////////////////////////////////////////////////////////////////////////////////
//
//  LongPath::ToWin32
//
//  GetFullPathNameW makes the path absolute against the current directory,
//  turns forward slashes into backslashes and resolves . and .. segments,
//  which Windows does not do for a \\?\ path.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring LongPath::ToWin32 (const std::wstring & path)
{
    std::wstring  full;
    std::wstring  result;
    DWORD         length      = 0;
    bool          isWin32Form = path.starts_with (s_kpszLocalPrefix) || path.starts_with (s_kpszDevicePrefix);



    if (path.empty() || isWin32Form)
    {
        result = path;
    }
    else
    {
        length = GetFullPathNameW (path.c_str(), 0, nullptr, nullptr);

        if (length != 0)
        {
            full.resize (length);
            length = GetFullPathNameW (path.c_str(), length, full.data(), nullptr);
            full.resize (length);
        }

        if (full.empty())
        {
            full = path;
        }

        if (full.starts_with (L"\\\\"))
        {
            result = s_kpszUncPrefix + full.substr (2);
        }
        else
        {
            result = s_kpszLocalPrefix + full;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LongPath::ToWin32, for a narrow path
//
//  Converted as std::filesystem converts a narrow path, through the process's
//  code page, which Casso's manifest sets to UTF-8.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring LongPath::ToWin32 (const std::string & path)
{
    return ToWin32 (std::filesystem::path (path).wstring());
}





////////////////////////////////////////////////////////////////////////////////
//
//  LongPath::FromWin32
//
////////////////////////////////////////////////////////////////////////////////

std::wstring LongPath::FromWin32 (const std::wstring & path)
{
    std::wstring  result = path;



    if (path.starts_with (s_kpszUncPrefix))
    {
        result = L"\\\\" + path.substr (wcslen (s_kpszUncPrefix));
    }
    else if (path.starts_with (s_kpszLocalPrefix))
    {
        result = path.substr (wcslen (s_kpszLocalPrefix));
    }

    return result;
}
