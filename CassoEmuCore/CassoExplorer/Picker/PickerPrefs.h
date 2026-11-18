#pragma once

#include "Pch.h"

struct CassoExplorerPrefs;
struct PickerRequest;





////////////////////////////////////////////////////////////////////////////////
//
//  PickerPrefs
//
//  A picker's saved view: its columns, their order and widths, the sort and
//  the window's placement, in a file for the request's view key. A request
//  without a receiving drive has its last folder there too. The file is
//  apart from Casso Explorer's preferences and session, which a picker
//  neither reads nor writes, and it has no CassoLink connections.
//
////////////////////////////////////////////////////////////////////////////////

class PickerPrefs
{
public:
    static constexpr const wchar_t *  kFilePrefix = L"CassoPickerPrefs";
    static constexpr const wchar_t *  kDefaultKey = L"Default";

    //  "CassoPickerPrefs-<key>.json" in baseDir. Characters of the key that
    //  are not letters, digits, '-' or '_' are left out.
    static std::wstring  GetFilePath (const std::wstring & baseDir, const std::wstring & viewKey);

    //  An absent or unreadable file is the defaults. `theme` is the theme
    //  of the window that opened the picker.
    static void     Load (const std::wstring & baseDir, IFileSystem & fs, const PickerRequest & request, const std::string & theme, CassoExplorerPrefs & outPrefs);
    static HRESULT  Save (const std::wstring & baseDir, IFileSystem & fs, const PickerRequest & request, const CassoExplorerPrefs & prefs);

private:
    static void  Scrub (const PickerRequest & request, CassoExplorerPrefs & prefs);
};
