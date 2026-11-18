#include "Pch.h"

#pragma comment(lib, "propsys.lib")

#include "Seams/Win32ShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::~Win32ShellItemVerbs
//
////////////////////////////////////////////////////////////////////////////////

Win32ShellItemVerbs::~Win32ShellItemVerbs()
{
    if (m_menuHost != nullptr)
    {
        DestroyWindow (m_menuHost);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShellItemVerbs::ShowInFolder
//
//  A new File Explorer window at the file's folder with the file selected.
//  Explorer has no supported way to bring forward a tab already open on the
//  folder, and reusing a window without selecting the file in it is worse
//  than a new one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShellItemVerbs::ShowInFolder (const std::wstring & path)
{
    HRESULT           hr     = S_OK;
    SHELLEXECUTEINFOW info   = { sizeof (info) };
    std::wstring      params = L"/n,/select,\"" + path + L"\"";
    BOOL              ran    = FALSE;



    info.fMask        = SEE_MASK_FLAG_NO_UI;
    info.lpFile       = L"explorer.exe";
    info.lpParameters = params.c_str();
    info.nShow        = SW_SHOWNORMAL;

    ran = ShellExecuteExW (&info);
    CWR (ran);

Error:
    return hr;
}
