#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiClipboard
//
//  Unicode text on the Windows clipboard, the only clipboard format Dxui uses.
//
//  FAILURES ARE SILENT BY DESIGN. Another application commonly has the
//  clipboard open, and an error dialog for a failed Ctrl+C would be worse
//  than the failure. An empty string is never set, so a copy with nothing
//  selected leaves the existing clipboard contents unchanged.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiClipboard
{
public:
    static void  SetText (HWND owner, const std::wstring & text);
    static bool  GetText (HWND owner, std::wstring & outText);
};
