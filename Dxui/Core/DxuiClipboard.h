#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiClipboard
//
//  The Windows clipboard, in the one form Dxui puts on it and takes off it.
//
//  FAILURES ARE SILENT BY DESIGN. Another application holding the clipboard
//  open is routine, and an error dialog for a failed Ctrl+C would be worse
//  than the failure. An empty string is not put on the clipboard at all, so
//  a copy with nothing selected leaves what the user copied earlier alone.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiClipboard
{
public:
    static void  SetText (HWND owner, const std::wstring & text);
    static bool  GetText (HWND owner, std::wstring & outText);
};
