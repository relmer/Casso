#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerWindowFinder
//
//  The Casso Explorer window class, and the lookup of a window by the
//  emulator window it serves. The owner is stored as a window property.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerWindowFinder
{
public:
    static constexpr const wchar_t *  kWindowClass   = L"CassoExplorerWindow";
    static constexpr const wchar_t *  kTestCopyClass = L"CassoExplorerTestCopyWindow";
    static constexpr const wchar_t *  kOwnerProperty = L"CassoExplorerOwner";

    //  The window class: kWindowClass, or for a copy run with a profile
    //  folder set (PathResolver::IsProfileFolderSet), kTestCopyClass and a
    //  hash of the folder. A launch is then forwarded only to windows of the
    //  same copy and profile folder.
    static const wchar_t *  GetWindowClass     ();

    //  The window already serving `owner`, or null.
    static HWND             FindWindowForOwner (HWND owner);

private:
    static std::wstring     MakeWindowClass    ();
};
