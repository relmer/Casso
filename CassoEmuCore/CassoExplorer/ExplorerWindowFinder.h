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

    //  The window class: kWindowClass, or for a copy run with a profile folder
    //  set (PathResolver::IsProfileFolderSet) kTestCopyClass and a number made
    //  from the folder, so a launch is forwarded only to a copy with the same
    //  folder, and two test copies with different folders run side by side.
    static const wchar_t  * GetWindowClass     ();
    static std::wstring     MakeWindowClass    (const std::wstring & profileFolder);

    //  The window already serving `owner`, or null.
    static HWND             FindWindowForOwner (HWND owner);
};
