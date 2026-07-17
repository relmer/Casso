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
    constexpr Word  s_kRd80Vid          = 0xC01F;   // 80-column display status
    constexpr int   s_kTextCols80       = 80;


    // Map one raw text-screen byte to a printable wchar. Normal, inverse, and
    // flashing glyphs all live in the $80-$FF span, so strip the high bit and
    // blank anything outside printable ASCII.
    wchar_t DecodeScreenByte (Byte ch)
    {
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

        return static_cast<wchar_t> (ch);
    }
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
//  BuildScreenText
//
//  Read the 24-row text screen as Unicode, 40 or 80 columns wide. Trailing
//  spaces are trimmed per row and rows are CRLF-terminated to match Windows
//  clipboard conventions. Pure (no clipboard/HWND) so it is unit testable.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ClipboardManager::BuildScreenText (const Byte * auxRam) const
{
    std::wstring  text;

    // 80-column text interleaves auxiliary memory (even display columns) with
    // main memory (odd columns). That layout is live only on a machine that
    // has an aux bank AND currently has the 80-column display switched on
    // (RD80VID, $C01F bit 7); otherwise the plain 40-column main page is read.
    bool  eighty = (auxRam != nullptr)
                && ((m_memoryBus.ReadByte (s_kRd80Vid) & s_kHighBitMask) != 0);
    int   cols   = eighty ? s_kTextCols80 : s_kTextCols;


    for (int row = 0; row < s_kTextRows; row++)
    {
        Word  base = static_cast<Word> (s_kTextBase
                                        + (row / s_kRowsPerGroup) * s_kRowGroupStride
                                        + (row % s_kRowsPerGroup) * s_kRowSubgroupStride);

        for (int col = 0; col < cols; col++)
        {
            Byte  ch = 0;

            if (eighty)
            {
                Word  addr = static_cast<Word> (base + col / 2);

                // Even columns come from aux memory, odd from main. The bus
                // returns main at $0400-$07FF the same way the 40-column path
                // relies on, so the aux read is the only new access.
                ch = ((col & 1) == 0) ? auxRam[addr]
                                      : m_memoryBus.ReadByte (addr);
            }
            else
            {
                ch = m_memoryBus.ReadByte (static_cast<Word> (base + col));
            }

            text += DecodeScreenByte (ch);
        }

        while (!text.empty() && text.back() == L' ')
        {
            text.pop_back();
        }

        text += L"\r\n";
    }

    return text;
}




////////////////////////////////////////////////////////////////////////////////
//
//  CopyScreenText
//
//  Scrape the emulated text screen (BuildScreenText) and hand it to the host
//  clipboard as Unicode. Reads via the memory bus rather than the CPU's
//  internal memory[] buffer: on the //e the MMU owns its own RAM device(s), so
//  firmware writes land in the bus-side buffer while the CPU mirror stays
//  uninitialized.
//
////////////////////////////////////////////////////////////////////////////////

void ClipboardManager::CopyScreenText (HWND hwnd, const Byte * auxRam) const
{
    HGLOBAL       hMem  = nullptr;
    wchar_t     * pDest = nullptr;
    std::wstring  text  = BuildScreenText (auxRam);


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
