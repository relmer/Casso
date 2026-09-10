#pragma once

#include "Pch.h"

#include "Seams/IHostClipboard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32Clipboard
//
//  IHostClipboard over OpenClipboard, GlobalAlloc and SetClipboardData.
//
////////////////////////////////////////////////////////////////////////////////

class Win32Clipboard : public IHostClipboard
{
public:

    bool  SetText (HWND owner, const std::wstring & text) override;
    bool  SetDib  (HWND owner, const std::vector<Byte> & dib) override;
    bool  GetText (HWND owner, std::wstring & outText) override;

private:

    //  Places one block of bytes under one format, with the clipboard held
    //  for the duration and emptied first.
    static bool  Place (HWND owner, UINT format, const void * bytes, size_t count);
};
