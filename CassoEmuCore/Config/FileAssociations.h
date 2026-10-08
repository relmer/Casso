#pragma once

#include "Pch.h"

#include "Seams/IUserClasses.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileAssociations
//
//  Casso as the current user's program for the disk image types it reads:
//  each shows Casso's disk icon in File Explorer, opens in Casso with the
//  image in drive 1, and lists Casso Explorer under Open with.
//
//  ONLY BY THE USER'S CHOICE. Nothing calls Register but a command the user
//  picks; installing or starting Casso writes nothing. No administrator
//  rights are needed, since everything is the user's own.
//
//  UNDONE COMPLETELY. Each type remembers the program it had, and Unregister
//  gives it back, then removes every value and key Register made, leaving the
//  user's types as they were.
//
//  A choice the user made in Windows' own Open with dialog outranks these, as
//  Windows intends; Casso does not write it.
//
////////////////////////////////////////////////////////////////////////////////

class FileAssociations
{
public:
    static constexpr const wchar_t *  kExtensions[]     = { L".dsk", L".do", L".po", L".woz", L".nib", L".nb2" };
    static constexpr const wchar_t *  kCassoProgId      = L"Casso.DiskImage";
    static constexpr const wchar_t *  kExplorerProgId   = L"CassoExplorer.DiskImage";
    static constexpr const wchar_t *  kPreviousName     = L"Casso.PreviousProgId";
    static constexpr int              kDiskIconId       = IDI_DISK_IMAGE;

    static HRESULT  Register     (IUserClasses & classes, const std::wstring & cassoExe, const std::wstring & explorerExe);
    static HRESULT  Unregister   (IUserClasses & classes);
    static bool     IsRegistered (const IUserClasses & classes);

private:
    static HRESULT  WriteProgId  (IUserClasses & classes, const std::wstring & progId, const std::wstring & appName,
                                  const std::wstring & iconExe, const std::wstring & command);
    static HRESULT  TakeType     (IUserClasses & classes, const std::wstring & extension);
    static HRESULT  ReturnType   (IUserClasses & classes, const std::wstring & extension);
};
