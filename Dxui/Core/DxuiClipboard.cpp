#include "Pch.h"

#include "DxuiClipboard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiClipboard::SetText
//
//  ownsGlobal tracks who is responsible for the memory. The clipboard takes
//  ownership only when SetClipboardData succeeds: freeing after a successful
//  set corrupts the clipboard, and not freeing after a failed one leaks. The
//  flag is raised at allocation and lowered exactly on success, so the single
//  cleanup block does the right thing from every exit.
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
//  so asking for the wide format costs no compatibility and avoids a codepage
//  conversion here.
//
//  The text is copied out and the clipboard CLOSED before the caller sees it,
//  so whatever the caller does with it -- including a callback that copies
//  again -- cannot deadlock against our own lock.
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
