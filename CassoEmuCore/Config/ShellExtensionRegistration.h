#pragma once

#include "Pch.h"

#include "Seams/IUserClasses.h"
#include "Update/IInstallEnvironment.h"
#include "Update/ISignatureVerifier.h"
#include "CassoExplorer/RawCopyDropHandler.h"





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
//  A developer build registers on request, from Options' DEBUG button: a
//  copy of its DLL, under kDevClsid and kDevHandlerName, so the release's
//  registration is left alone and a rebuild is never blocked by File
//  Explorer having the DLL loaded.
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
    static constexpr const wchar_t *  kHandlerName    = L"CassoExplorer";
    static constexpr const wchar_t *  kDevHandlerName = L"CassoExplorerDev";
    static constexpr const wchar_t *  kDevCopyFolder  = L"Casso\\DevShellExtension";   // below %LOCALAPPDATA%

    //  The folder types the handler is registered for: file folders and
    //  drives, as File Explorer's right-drop targets.
    static constexpr const wchar_t *  kTargetTypes[] = { L"Directory", L"Drive" };

    static HRESULT  Register     (IUserClasses & classes, const std::wstring & dllPath);

    //  dllPath as the developer handler, in place of any earlier one.
    static HRESULT  RegisterDeveloper (IUserClasses & classes, const std::wstring & dllPath);

    //  Copies the DLL beside this program to a new file in kDevCopyFolder,
    //  registers the copy as the developer handler, and deletes the older
    //  copies File Explorer doesn't have loaded. outCopy is the copy's path.
    static HRESULT  RegisterDeveloperCopy (std::wstring & outCopy);

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
    static HRESULT  RegisterClass (IUserClasses & classes, const std::wstring & dllPath, REFCLSID clsid, const wchar_t * handlerName, const wchar_t * description);

    //  The class's key below Classes, as CLSID\{...}.
    static std::wstring  GetClassKey   (REFCLSID clsid = RawCopyDropHandler::kClsid);

    //  The class's id in braces, as File Explorer reads it from a handler entry.
    static std::wstring  GetClsidText  (REFCLSID clsid = RawCopyDropHandler::kClsid);

    //  A folder type's entry for the handler, below its shellex key.
    static std::wstring  GetHandlerKey (const wchar_t * targetType, const wchar_t * handlerName = kHandlerName);
};
