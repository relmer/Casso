#include "Pch.h"

#include "Ui/Debugger/DebuggerFilePicker.h"
#include "Ui/Debugger/IDebuggerWindowHost.h"
#include "Debugger/Source/SourcePathList.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerFilePicker::PickToOpen
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebuggerFilePicker::PickToOpen (IDebuggerWindowHost * host, HWND owner, const std::string & kind, FileDialogSpec spec, std::filesystem::path & chosen, bool & picked)
{
    HRESULT      hr  = S_OK;
    std::string  key = "folder." + kind;



    CBRAEx (host != nullptr, E_UNEXPECTED);

    spec.initialFolder = SourcePathList::Utf8ToWide (host->GetDebuggerWindowState (key));

    hr = host->GetHostDialogs().PickFileToOpen (owner, spec, chosen, picked);
    CHR (hr);

    if (picked)
    {
        host->SetDebuggerWindowState (key, SourcePathList::WideToUtf8 (chosen.parent_path().wstring()));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerFilePicker::PickToSave
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebuggerFilePicker::PickToSave (IDebuggerWindowHost * host, HWND owner, const std::string & kind, FileDialogSpec spec, std::filesystem::path & chosen, bool & picked)
{
    HRESULT      hr  = S_OK;
    std::string  key = "folder." + kind;



    CBRAEx (host != nullptr, E_UNEXPECTED);

    spec.initialFolder = SourcePathList::Utf8ToWide (host->GetDebuggerWindowState (key));

    hr = host->GetHostDialogs().PickFileToSave (owner, spec, chosen, picked);
    CHR (hr);

    if (picked)
    {
        host->SetDebuggerWindowState (key, SourcePathList::WideToUtf8 (chosen.parent_path().wstring()));
    }

Error:
    return hr;
}
