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
//  UNDONE COMPLETELY. Register stores each type's previous program beside it,
//  and Unregister restores that program, then removes every value and key
//  Register made. File Explorer adds Casso's type to its own record of each
//  extension's programs, and Unregister removes that entry too.
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

    //  Casso Explorer is offered under Open with by its program's own entry,
    //  not as a second type: for two types with no choice made yet, Windows
    //  displays its Open with prompt and a blank icon in place of Casso's.
    //  An earlier build registered it as a type, which Unregister also
    //  removes.
    static constexpr const wchar_t *  kExplorerApp      = L"CassoExplorer.exe";
    static constexpr const wchar_t *  kExplorerProgId   = L"CassoExplorer.DiskImage";
    static constexpr const wchar_t *  kPreviousName     = L"Casso.PreviousProgId";
    static constexpr int              kDiskIconId       = IDI_DISK_IMAGE;

    static HRESULT  Register     (IUserClasses & classes, const std::wstring & cassoExe, const std::wstring & explorerExe);
    static HRESULT  Unregister   (IUserClasses & classes, IUserClasses & fileExts);
    static bool     IsRegistered (const IUserClasses & classes);

private:
    static HRESULT  WriteProgId  (IUserClasses & classes, const std::wstring & progId, const std::wstring & appName,
                                  const std::wstring & iconExe, const std::wstring & command);
    static HRESULT  TakeType     (IUserClasses & classes, const std::wstring & extension);
    static HRESULT  ReturnType   (IUserClasses & classes, const std::wstring & extension);
    static HRESULT  ClearRecord  (IUserClasses & fileExts, const std::wstring & extension);
};
