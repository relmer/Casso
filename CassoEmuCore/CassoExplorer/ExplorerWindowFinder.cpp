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
    static constexpr DWORD  kFolderChars = 32768;



    static const std::wstring  windowClass = [] ()
    {
        std::wstring  folder (kFolderChars, L'\0');
        DWORD         length = GetEnvironmentVariableW (PathResolver::kpszProfileVariable, folder.data(), kFolderChars);

        folder.resize (length < kFolderChars ? length : 0);
        return MakeWindowClass (folder);
    } ();



    return windowClass.c_str();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerWindowFinder::MakeWindowClass
//
//  kTestCopyClass and the folder's 32-bit FNV-1a hash in hex, taken over its
//  path in lowercase so that C:\Temp and c:\temp give the same class;
//  kWindowClass for no folder.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerWindowFinder::MakeWindowClass (const std::wstring & profileFolder)
{
    static constexpr uint32_t  kOffsetBasis = 2166136261u;
    static constexpr uint32_t  kPrime       = 16777619u;



    uint32_t      hash        = kOffsetBasis;
    std::wstring  windowClass = kWindowClass;



    for (wchar_t ch : profileFolder)
    {
        hash = (hash ^ static_cast<uint32_t> (towlower (ch))) * kPrime;
    }

    if (!profileFolder.empty())
    {
        windowClass = std::format (L"{}_{:08X}", kTestCopyClass, hash);
    }

    return windowClass;
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
