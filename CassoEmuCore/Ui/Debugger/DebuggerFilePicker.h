#pragma once

#include "Seams/IHostDialogs.h"

class IDebuggerWindowHost;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerFilePicker
//
//  A file the user picks for the debugger to open or save, starting in the
//  folder the last file of the same kind was picked from. The host saves
//  that folder per kind, as "folder." and the kind.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerFilePicker
{
public:
    static HRESULT  PickToOpen (IDebuggerWindowHost * host, HWND owner, const std::string & kind, FileDialogSpec spec, std::filesystem::path & chosen, bool & picked);
    static HRESULT  PickToSave (IDebuggerWindowHost * host, HWND owner, const std::string & kind, FileDialogSpec spec, std::filesystem::path & chosen, bool & picked);
};
