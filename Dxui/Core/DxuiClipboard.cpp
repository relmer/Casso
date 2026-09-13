#include "Pch.h"

#include "DxuiClipboard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiClipboard::SetText
//
//  ownsGlobal indicates whether this function must free the memory. The
//  clipboard owns it only after SetClipboardData succeeds: freeing it after a
//  successful set corrupts the clipboard, and not freeing it after a failed
//  one leaks. The flag is set at allocation and cleared only on success, so
//  the single cleanup block is correct on every exit path.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiClipboard::SetText (HWND owner, const std::wstring & text)
{
    HRESULT   hr         = S_OK;
    HGLOBAL   hGlobal    = nullptr;
    void    * pBuf       = nullptr;
    size_t    bytes      = (text.size() + 1) * sizeof (wchar_t);
    bool      isEmpty    = text.empty();
    bool      isOpen     = false;
    bool      wasEmptied = false;
    bool      ownsGlobal = false;



    BAIL_OUT_IF (isEmpty, S_OK);

    isOpen = OpenClipboard (owner) != FALSE;

    BAIL_OUT_IF (!isOpen, S_OK);

    wasEmptied = EmptyClipboard() != FALSE;

    BAIL_OUT_IF (!wasEmptied, S_OK);

    hGlobal    = GlobalAlloc (GMEM_MOVEABLE, bytes);
    ownsGlobal = (hGlobal != nullptr);

    BAIL_OUT_IF (!ownsGlobal, S_OK);

    pBuf = GlobalLock (hGlobal);

    BAIL_OUT_IF (pBuf == nullptr, S_OK);

    memcpy (pBuf, text.c_str(), bytes);
    GlobalUnlock (hGlobal);

    ownsGlobal = (SetClipboardData (CF_UNICODETEXT, hGlobal) == nullptr);

Error:
    if (ownsGlobal)
    {
        GlobalFree (hGlobal);
    }

    if (isOpen)
    {
        CloseClipboard();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiClipboard::GetText
//
//  Only CF_UNICODETEXT is requested. Windows synthesizes it from ANSI text,
//  so requesting the wide format loses no compatibility and avoids a codepage
//  conversion here.
//
//  The text is copied and the clipboard CLOSED before returning, so nothing
//  the caller does with the text, including a callback that copies again, can
//  deadlock on the clipboard lock.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiClipboard::GetText (HWND owner, std::wstring & outText)
{
    HRESULT     hr      = S_OK;
    HANDLE      hData   = nullptr;
    wchar_t   * pBuf    = nullptr;
    bool        isOpen  = false;
    bool        hasText = false;



    isOpen = OpenClipboard (owner) != FALSE;

    BAIL_OUT_IF (!isOpen, S_OK);

    hData = GetClipboardData (CF_UNICODETEXT);

    BAIL_OUT_IF (hData == nullptr, S_OK);

    pBuf = (wchar_t *) GlobalLock (hData);

    BAIL_OUT_IF (pBuf == nullptr, S_OK);

    outText.assign (pBuf);
    hasText = true;
    GlobalUnlock (hData);

Error:
    if (isOpen)
    {
        CloseClipboard();
    }

    return hasText;
}
