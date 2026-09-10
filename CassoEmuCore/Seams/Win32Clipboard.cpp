#include "Pch.h"

#include "Seams/Win32Clipboard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Place
//
//  The clipboard takes ownership of the global block once SetClipboardData
//  succeeds; until then it is ours to free. Another process can hold the
//  clipboard, and a failed open is reported as false rather than surfaced.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32Clipboard::Place (HWND owner, UINT format, const void * bytes, size_t count)
{
    HGLOBAL  hMem   = nullptr;
    void *   pDest  = nullptr;
    bool     placed = false;



    if (!OpenClipboard (owner))
    {
        return false;
    }

    EmptyClipboard();

    hMem = GlobalAlloc (GMEM_MOVEABLE, count);

    if (hMem != nullptr)
    {
        pDest = GlobalLock (hMem);
    }

    if (pDest != nullptr)
    {
        memcpy (pDest, bytes, count);
        GlobalUnlock (hMem);

        placed = (SetClipboardData (format, hMem) != nullptr);
    }

    if (!placed && hMem != nullptr)
    {
        GlobalFree (hMem);
    }

    CloseClipboard();

    return placed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetText / SetDib
//
////////////////////////////////////////////////////////////////////////////////

bool Win32Clipboard::SetText (HWND owner, const std::wstring & text)
{
    return Place (owner, CF_UNICODETEXT, text.c_str(), (text.size() + 1) * sizeof (wchar_t));
}


bool Win32Clipboard::SetDib (HWND owner, const std::vector<Byte> & dib)
{
    return Place (owner, CF_DIB, dib.data(), dib.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetText
//
//  Unicode text only; anything else on the clipboard reads as nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32Clipboard::GetText (HWND owner, std::wstring & outText)
{
    HANDLE           hData = nullptr;
    const wchar_t *  pText = nullptr;
    bool             got   = false;



    outText.clear();

    if (!OpenClipboard (owner))
    {
        return false;
    }

    hData = GetClipboardData (CF_UNICODETEXT);

    if (hData != nullptr)
    {
        pText = static_cast<const wchar_t *> (GlobalLock (hData));

        if (pText != nullptr)
        {
            outText = pText;
            got     = true;
            GlobalUnlock (hData);
        }
    }

    CloseClipboard();

    return got;
}
