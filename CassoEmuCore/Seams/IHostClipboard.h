#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IHostClipboard
//
//  The host clipboard, as three things the emulator does with it.
//
//  What goes on the clipboard is decided in core: the text screen scraped to
//  Unicode, the captured picture packed as a device-independent bitmap, the
//  pasted text filtered down to what an Apple II keyboard can type. Only the
//  handoff is the operating system's, and this is the whole of that handoff,
//  so a test gives the manager a clipboard made of vectors and reads back
//  exactly what would have been placed.
//
//  Every call answers false when the clipboard could not be had -- another
//  process can hold it -- and the caller treats that as the copy not
//  happening, never as an error worth a dialog.
//
////////////////////////////////////////////////////////////////////////////////

class IHostClipboard
{
public:

    virtual ~IHostClipboard () = default;

    virtual bool  SetText (HWND owner, const std::wstring & text)     = 0;

    //  A complete CF_DIB payload: BITMAPINFOHEADER followed by bottom-up rows.
    virtual bool  SetDib  (HWND owner, const std::vector<Byte> & dib)  = 0;

    virtual bool  GetText (HWND owner, std::wstring & outText)         = 0;
};
