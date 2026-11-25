#pragma once

#include "Pch.h"

#include "Seams/IUserClasses.h"
#include "Update/InstallTypeDetector.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileAssociations
//
//  Casso as the current user's program for the disk image types it reads:
//  each shows Casso's disk icon in File Explorer, opens in Casso with the
//  image in drive 1, and lists Casso Explorer under Open with. Each
//  extension is a separate type, with the text Casso Explorer's Type column
//  has for it, so File Explorer sorts and groups them apart.
//
//  REGISTERED AT STARTUP FOR A COPY WITHOUT AN INSTALLER. An installed
//  package declares the types in its manifest. A zip release or a developer
//  build registers itself when it starts, unless the types are already this
//  copy's, another copy of Casso on this PC has them, or the user turned
//  them off in Options. Everything is written below the current user's
//  key, so administrator rights aren't required.
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

    //  Each extension's type is this followed by the extension, as
    //  Casso.DiskImage.woz. An earlier build registered every extension as
    //  this one type, which Register replaces and Unregister also removes.
    static constexpr const wchar_t *  kCassoProgId      = L"Casso.DiskImage";

    static std::wstring  GetProgId (const std::wstring & extension) { return std::wstring (kCassoProgId) + extension; }

    //  Casso Explorer is offered under Open with by its program's own entry,
    //  not as a second type: for two types with no choice made yet, Windows
    //  displays its Open with prompt and a blank icon in place of Casso's.
    //  An earlier build registered it as a type, which Unregister also
    //  removes.
    static constexpr const wchar_t *  kExplorerApp      = L"CassoExplorer.exe";
    static constexpr const wchar_t *  kExplorerProgId   = L"CassoExplorer.DiskImage";
    static constexpr const wchar_t *  kPreviousName     = L"Casso.PreviousProgId";
    static constexpr int              kDiskIconId       = IDI_DISK_IMAGE;
    static constexpr const wchar_t *  kSettingsKey      = L"FileAssociations";
    static constexpr const wchar_t *  kDeclinedName     = L"Declined";

    static HRESULT  Register     (IUserClasses & classes, const std::wstring & cassoExe, const std::wstring & explorerExe);
    static HRESULT  Unregister   (IUserClasses & classes, IUserClasses & fileExts);
    static bool     IsRegistered (const IUserClasses & classes, const std::wstring & cassoExe, const std::wstring & explorerExe);

    //  Registers this copy at startup when it has no installer, by the rules
    //  in the class comment. `exists` returns whether a program file is on disk.
    static HRESULT  RegisterForInstall (IUserClasses & classes, const IUserClasses & settings, InstallType type, const std::wstring & cassoExe,
                                        const std::wstring & explorerExe, const std::function<bool (const std::wstring &)> & exists);

    //  RegisterForInstall for the programs beside this one, on a separate
    //  thread, since checking the program's signature takes a moment.
    static void     StartInstallRegistration ();

    //  Marks the types as turned off by the user, so startup doesn't register them.
    static HRESULT  SetDeclined (IUserClasses & settings, bool declined);

private:
    static HRESULT  WriteProgId  (IUserClasses & classes, const std::wstring & progId, const std::wstring & typeName,
                                  const std::wstring & appName, const std::wstring & iconExe, const std::wstring & command);
    static bool          IsCassoType        (const std::wstring & extension, const std::wstring & progId);
    static HRESULT       TakeType           (IUserClasses & classes, const std::wstring & extension);
    static HRESULT       ReturnType         (IUserClasses & classes, const std::wstring & extension);
    static HRESULT       ClearRecord        (IUserClasses & fileExts, const std::wstring & extension);
    static std::wstring  GetRegisteredCasso (const IUserClasses & classes);
};
