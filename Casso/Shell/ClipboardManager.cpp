#include "Pch.h"

#include "ClipboardManager.h"

#include "Devices/AppleKeyboard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////





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
//  ClipboardManager::DecodeScreenByte
//
//  Map one raw text-screen byte to a printable wchar. Normal, inverse, and
//  flashing glyphs all live in the $80-$FF span, so strip the high bit and
//  blank anything outside printable ASCII.
//
////////////////////////////////////////////////////////////////////////////////

wchar_t ClipboardManager::DecodeScreenByte (Byte ch)
{
    constexpr Byte  kInverseHighStart = 0xA0;



    if (ch >= kInverseHighStart)
    {
        ch -= kHighBitMask;
    }
    else if (ch >= kHighBitMask)
    {
        ch -= kHighBitMask;
    }

    if (ch < kPrintableLow || ch > kPrintableHigh)
    {
        ch = ' ';
    }

    return static_cast<wchar_t> (ch);
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
    constexpr int   kTextRows         = 24;
    constexpr int   kTextCols         = 40;
    constexpr Word  kTextBase         = 0x0400;
    constexpr Word  kRowGroupStride   = 0x28;
    constexpr Word  kRowSubgroupStride = 0x80;
    constexpr int   kRowsPerGroup     = 8;
    constexpr int   kTextCols80       = 80;



    std::wstring  text;

    // 80-column text interleaves auxiliary memory (even display columns) with
    // main memory (odd columns). That layout is live only on a machine that
    // has an aux bank AND currently has the 80-column display switched on
    // (RD80VID, $C01F bit 7); otherwise the plain 40-column main page is read.
    bool  eighty = (auxRam != nullptr)
                && ((m_memoryBus.ReadByte (kRd80Vid) & kHighBitMask) != 0);
    int   cols   = eighty ? kTextCols80 : kTextCols;



    for (int row = 0; row < kTextRows; row++)
    {
        Word  base = static_cast<Word> (kTextBase
                                        + (row / kRowsPerGroup) * kRowGroupStride
                                        + (row % kRowsPerGroup) * kRowSubgroupStride);

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



    // Another process can hold the clipboard; a failed open is not an error
    // worth surfacing, the copy just does not happen.
    if (OpenClipboard (hwnd))
    {
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopyScreenshot
//
////////////////////////////////////////////////////////////////////////////////

void ClipboardManager::CopyScreenshot (HWND hwnd)
{
    constexpr int   kBytesPerPixel    = 4;
    constexpr WORD  kDibBitCount      = 32;



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

        dataSize  = static_cast<size_t> (w) * h * kBytesPerPixel;
        totalSize = sizeof (BITMAPINFOHEADER) + dataSize;

        // Once the clipboard is open it MUST be closed on every path, so the
        // two allocation failures below cannot simply return.
        if (OpenClipboard (hwnd))
        {
            EmptyClipboard();

            hMem = GlobalAlloc (GMEM_MOVEABLE, totalSize);

            if (hMem != nullptr)
            {
                pDest = static_cast<Byte *> (GlobalLock (hMem));
            }

            if (pDest != nullptr)
            {
                bih.biSize        = sizeof (bih);
                bih.biWidth       = w;
                bih.biHeight      = h;
                bih.biPlanes      = 1;
                bih.biBitCount    = kDibBitCount;
                bih.biCompression = BI_RGB;
                bih.biSizeImage   = static_cast<DWORD> (dataSize);

                memcpy (pDest, &bih, sizeof (bih));
                pDest += sizeof (bih);

                // A DIB is bottom-up, so the framebuffer's rows go out in reverse.
                for (y = h - 1; y >= 0; y--)
                {
                    memcpy (pDest,
                            &m_uiFramebuffer[static_cast<size_t> (y) * w],
                            static_cast<size_t> (w) * kBytesPerPixel);
                    pDest += static_cast<size_t> (w) * kBytesPerPixel;
                }

                GlobalUnlock (hMem);
                SetClipboardData (CF_DIB, hMem);
            }

            CloseClipboard();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PasteFromClipboard
//
////////////////////////////////////////////////////////////////////////////////

void ClipboardManager::PasteFromClipboard (HWND hwnd)
{
    constexpr Byte  kCarriageReturn   = 0x0D;
    constexpr wchar_t  kNewline       = L'\n';
    constexpr wchar_t  kReturn        = L'\r';



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

                if (ch == kNewline)
                {
                    continue;
                }

                if (ch == kReturn)
                {
                    m_pasteBuffer += static_cast<char> (kCarriageReturn);
                }
                else if (ch >= kPrintableLow && ch < (wchar_t) (kPrintableHigh + 1))
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

void ClipboardManager::DrainPasteBuffer()
{
    AppleKeyboard  * keyboard = nullptr;
    Byte             ch       = 0;



    keyboard = (m_pKeyboardSlot != nullptr) ? *m_pKeyboardSlot : nullptr;

    // One character per call, and only once the guest has consumed the last
    // one -- the strobe is the handshake that paces the whole paste.
    if (keyboard != nullptr && keyboard->IsStrobeClear())
    {
        {
            std::lock_guard<std::mutex>  lock (m_cmdMutex);

            if (!m_pasteBuffer.empty())
            {
                ch = static_cast<Byte> (m_pasteBuffer[0]);
                m_pasteBuffer.erase (m_pasteBuffer.begin());
            }
        }

        // ch stays 0 for an empty buffer; 0 is not a key the paste path ever
        // queues, so it doubles as "nothing to send".
        if (ch != 0)
        {
            keyboard->KeyPress (ch);
        }
    }
}


