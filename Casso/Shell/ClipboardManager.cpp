#include "Pch.h"

#include "ClipboardManager.h"

#include "Devices/AppleKeyboard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr int   s_kTextRows         = 24;
    constexpr int   s_kTextCols         = 40;
    constexpr Word  s_kTextBase         = 0x0400;
    constexpr Word  s_kRowGroupStride   = 0x28;
    constexpr Word  s_kRowSubgroupStride = 0x80;
    constexpr int   s_kRowsPerGroup     = 8;
    constexpr Byte  s_kHighBitMask      = 0x80;
    constexpr Byte  s_kInverseHighStart = 0xA0;
    constexpr Byte  s_kPrintableLow     = 0x20;
    constexpr Byte  s_kPrintableHigh    = 0x7E;
    constexpr Byte  s_kCarriageReturn   = 0x0D;
    constexpr wchar_t  s_kNewline       = L'\n';
    constexpr wchar_t  s_kReturn        = L'\r';
    constexpr int   s_kBitsPerByte      = 8;
    constexpr int   s_kBytesPerPixel    = 4;
    constexpr WORD  s_kDibBitCount      = 32;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClipboardManager
//
////////////////////////////////////////////////////////////////////////////////

ClipboardManager::ClipboardManager (
    MemoryBus                & memoryBus,
    std::mutex               & cmdMutex,
    std::string              & pasteBuffer,
    std::mutex               & fbMutex,
    std::vector<uint32_t>    & uiFramebuffer,
    int                        framebufferWidth,
    int                        framebufferHeight,
    AppleKeyboard          * * pKeyboardSlot)
    : m_memoryBus         (memoryBus),
      m_cmdMutex          (cmdMutex),
      m_pasteBuffer       (pasteBuffer),
      m_fbMutex           (fbMutex),
      m_uiFramebuffer     (uiFramebuffer),
      m_pKeyboardSlot     (pKeyboardSlot),
      m_framebufferWidth  (framebufferWidth),
      m_framebufferHeight (framebufferHeight)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopyScreenText
//
//  Read the 40x24 text screen via the memory bus rather than the
//  CPU's internal memory[] buffer. On the //e the MMU owns its own
//  RAM device(s), so writes from firmware land in the bus-side buffer
//  while the CPU's mirror stays uninitialized. Trim trailing spaces
//  per row and CRLF-terminate to match Windows clipboard conventions.
//
////////////////////////////////////////////////////////////////////////////////

void ClipboardManager::CopyScreenText (HWND hwnd) const
{
    HGLOBAL    hMem  = nullptr;
    wchar_t  * pDest = nullptr;
    std::wstring text;
    int        row   = 0;
    int        col   = 0;



    for (row = 0; row < s_kTextRows; row++)
    {
        Word  base = static_cast<Word> (s_kTextBase
                                        + (row / s_kRowsPerGroup) * s_kRowGroupStride
                                        + (row % s_kRowsPerGroup) * s_kRowSubgroupStride);

        for (col = 0; col < s_kTextCols; col++)
        {
            Byte  ch = m_memoryBus.ReadByte (static_cast<Word> (base + col));

            if (ch >= s_kInverseHighStart)
            {
                ch -= s_kHighBitMask;
            }
            else if (ch >= s_kHighBitMask)
            {
                ch -= s_kHighBitMask;
            }

            if (ch < s_kPrintableLow || ch > s_kPrintableHigh)
            {
                ch = ' ';
            }

            text += static_cast<wchar_t> (ch);
        }

        while (!text.empty() && text.back() == L' ')
        {
            text.pop_back();
        }

        text += L"\r\n";
    }

    if (!OpenClipboard (hwnd))
    {
        return;
    }

    EmptyClipboard();

    hMem = GlobalAlloc (GMEM_MOVEABLE, (text.size() + 1) * sizeof (wchar_t));

    if (hMem != nullptr)
    {
        pDest = static_cast<wchar_t *> (GlobalLock (hMem));

        if (pDest != nullptr)
        {
            memcpy (pDest, text.c_str(), (text.size() + 1) * sizeof (wchar_t));
            GlobalUnlock (hMem);
            SetClipboardData (CF_UNICODETEXT, hMem);
        }
    }

    CloseClipboard();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopyScreenshot
//
////////////////////////////////////////////////////////////////////////////////

void ClipboardManager::CopyScreenshot (HWND hwnd)
{
    HGLOBAL          hMem      = nullptr;
    BITMAPINFOHEADER bih       = {};
    size_t           dataSize  = 0;
    size_t           totalSize = 0;
    Byte           * pDest     = nullptr;
    int              w         = m_framebufferWidth;
    int              h         = m_framebufferHeight;
    int              y         = 0;



    {
        std::lock_guard<std::mutex>  lock (m_fbMutex);

        dataSize  = static_cast<size_t> (w) * h * s_kBytesPerPixel;
        totalSize = sizeof (BITMAPINFOHEADER) + dataSize;

        if (!OpenClipboard (hwnd))
        {
            return;
        }

        EmptyClipboard();

        hMem = GlobalAlloc (GMEM_MOVEABLE, totalSize);

        if (hMem == nullptr)
        {
            CloseClipboard();
            return;
        }

        pDest = static_cast<Byte *> (GlobalLock (hMem));

        if (pDest == nullptr)
        {
            CloseClipboard();
            return;
        }

        bih.biSize        = sizeof (bih);
        bih.biWidth       = w;
        bih.biHeight      = h;
        bih.biPlanes      = 1;
        bih.biBitCount    = s_kDibBitCount;
        bih.biCompression = BI_RGB;
        bih.biSizeImage   = static_cast<DWORD> (dataSize);

        memcpy (pDest, &bih, sizeof (bih));
        pDest += sizeof (bih);

        for (y = h - 1; y >= 0; y--)
        {
            memcpy (pDest,
                    &m_uiFramebuffer[static_cast<size_t> (y) * w],
                    static_cast<size_t> (w) * s_kBytesPerPixel);
            pDest += static_cast<size_t> (w) * s_kBytesPerPixel;
        }

        GlobalUnlock (hMem);
        SetClipboardData (CF_DIB, hMem);
    }

    CloseClipboard();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PasteFromClipboard
//
////////////////////////////////////////////////////////////////////////////////

void ClipboardManager::PasteFromClipboard (HWND hwnd)
{
    HANDLE     hData = nullptr;
    wchar_t  * pText = nullptr;
    size_t     i     = 0;



    if (!OpenClipboard (hwnd))
    {
        return;
    }

    hData = GetClipboardData (CF_UNICODETEXT);

    if (hData != nullptr)
    {
        pText = static_cast<wchar_t *> (GlobalLock (hData));

        if (pText != nullptr)
        {
            std::lock_guard<std::mutex>  lock (m_cmdMutex);

            for (i = 0; pText[i] != L'\0'; i++)
            {
                wchar_t  ch = pText[i];

                if (ch == s_kNewline)
                {
                    continue;
                }

                if (ch == s_kReturn)
                {
                    m_pasteBuffer += static_cast<char> (s_kCarriageReturn);
                }
                else if (ch >= s_kPrintableLow && ch < (wchar_t) (s_kPrintableHigh + 1))
                {
                    m_pasteBuffer += static_cast<char> (ch);
                }
            }

            GlobalUnlock (hData);
        }
    }

    CloseClipboard();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrainPasteBuffer
//
////////////////////////////////////////////////////////////////////////////////

void ClipboardManager::DrainPasteBuffer ()
{
    AppleKeyboard  * keyboard = nullptr;
    Byte             ch       = 0;



    if (m_pKeyboardSlot == nullptr)
    {
        return;
    }

    keyboard = *m_pKeyboardSlot;
    if (keyboard == nullptr)
    {
        return;
    }

    if (!keyboard->IsStrobeClear())
    {
        return;
    }

    {
        std::lock_guard<std::mutex>  lock (m_cmdMutex);

        if (m_pasteBuffer.empty())
        {
            return;
        }

        ch = static_cast<Byte> (m_pasteBuffer[0]);
        m_pasteBuffer.erase (m_pasteBuffer.begin());
    }

    keyboard->KeyPress (ch);
}
