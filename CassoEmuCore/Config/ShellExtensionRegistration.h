#pragma once

#include "Pch.h"

#include "Seams/IUserClasses.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellExtensionRegistration
//
//  CassoShellExtension.dll as the current user's drag-and-drop handler for
//  folders and drives, so File Explorer's right-drag menu offers Copy here
//  as raw bytes for a drag out of Casso Explorer (RawCopyDropHandler).
//
//  ONLY BY THE USER'S CHOICE, as FileAssociations: nothing calls Register but
//  the Options dialog's box, and Unregister removes every key Register made.
//  An installed package declares the handler in its manifest instead.
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
    static HRESULT  Unregister   (IUserClasses & classes);
    static bool     IsRegistered (const IUserClasses & classes, const std::wstring & dllPath);

private:
    //  The class's key below Classes, as CLSID\{...}.
    static std::wstring  GetClassKey   ();

    //  The class's id in braces, as File Explorer reads it from a handler entry.
    static std::wstring  GetClsidText  ();

    //  A folder type's entry for the handler, below its shellex key.
    static std::wstring  GetHandlerKey (const wchar_t * targetType);
};
