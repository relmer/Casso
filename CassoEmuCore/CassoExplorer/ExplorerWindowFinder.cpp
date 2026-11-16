#include "Pch.h"

#include "CassoExplorer/ExplorerWindowFinder.h"
#include "Core/PathResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerWindowFinder::GetWindowClass
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ExplorerWindowFinder::GetWindowClass()
{
    static const std::wstring  s_kClass = MakeWindowClass();



    return s_kClass.c_str();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerWindowFinder::MakeWindowClass
//
//  A test copy's class ends in a hash of its profile folder, so two test
//  copies with different profile folders open separate apps.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerWindowFinder::MakeWindowClass()
{
    wchar_t       folder[MAX_PATH] = {};
    DWORD         length           = 0;
    std::wstring  path;



    if (!PathResolver::IsProfileFolderSet())
    {
        return kWindowClass;
    }

    length = GetEnvironmentVariableW (PathResolver::kpszProfileVariable, folder, ARRAYSIZE (folder));
    path.assign (folder, (length < ARRAYSIZE (folder)) ? length : 0);

    std::transform (path.begin(), path.end(), path.begin(), [] (wchar_t c) { return (wchar_t) towlower (c); });

    return std::format (L"{}.{:08X}", kTestCopyClass, (uint32_t) std::hash<std::wstring>() (path));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerWindowFinder::FindWindowForOwner
//
////////////////////////////////////////////////////////////////////////////////

HWND ExplorerWindowFinder::FindWindowForOwner (HWND owner)
{
    HWND  found = nullptr;
    HWND  next  = nullptr;



    if (owner == nullptr)
    {
        return nullptr;
    }

    do
    {
        next = FindWindowExW (nullptr, next, GetWindowClass(), nullptr);

        if (next != nullptr && (HWND) GetPropW (next, kOwnerProperty) == owner)
        {
            found = next;
        }
    }
    while (next != nullptr && found == nullptr);

    return found;
}
