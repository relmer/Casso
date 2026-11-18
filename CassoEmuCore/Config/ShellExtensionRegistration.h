#pragma once

#include "Pch.h"

#include "Seams/IUserClasses.h"
#include "Update/IInstallEnvironment.h"
#include "Update/ISignatureVerifier.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellExtensionRegistration
//
//  CassoShellExtension.dll as the current user's drag-and-drop handler for
//  folders and drives, so File Explorer's right-drag menu offers Copy here
//  as raw bytes for a drag out of Casso Explorer (RawCopyDropHandler).
//
//  REGISTERED WITH THE INSTALL. An installed package declares the handler in
//  its manifest. A copy unzipped from a release has no installer, so Casso
//  Explorer registers it at startup, for the DLL beside the program, until
//  the registration matches that DLL. A developer build is never
//  registered: a DLL File Explorer has loaded from the build output can't be relinked.
//
//  Options turns the menu item off through CassoExplorerPrefs::rawCopyMenu,
//  without changing the registration: with it off, a drag isn't marked and
//  the handler adds nothing.
//
////////////////////////////////////////////////////////////////////////////////

class ShellExtensionRegistration
{
public:
    static constexpr const wchar_t *  kDllName     = L"CassoShellExtension.dll";
    static constexpr const wchar_t *  kHandlerName = L"CassoExplorer";

    //  The folder types the handler is registered for: file folders and
    //  drives, as File Explorer's right-drop targets.
    static constexpr const wchar_t *  kTargetTypes[] = { L"Directory", L"Drive" };

    static HRESULT  Register     (IUserClasses & classes, const std::wstring & dllPath);

    //  Registers dllPath when this copy is an official zip release and it
    //  isn't registered yet. A package or a developer build is left alone.
    static HRESULT  RegisterForInstall (IUserClasses & classes, IInstallEnvironment & environment, ISignatureVerifier & verifier,
                                        const std::wstring & dllPath);

    //  RegisterForInstall for the DLL beside this program, on a separate
    //  thread, since checking the program's signature takes a moment.
    static void     StartInstallRegistration ();

    static HRESULT  Unregister               (IUserClasses & classes);
    static bool     IsRegistered             (const IUserClasses & classes, const std::wstring & dllPath);

private:
    //  The class's key below Classes, as CLSID\{...}.
    static std::wstring  GetClassKey   ();

    //  The class's id in braces, as File Explorer reads it from a handler entry.
    static std::wstring  GetClsidText  ();

    //  A folder type's entry for the handler, below its shellex key.
    static std::wstring  GetHandlerKey (const wchar_t * targetType);
};
