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
    constexpr int   kTextRows          = 24;
    constexpr int   kTextCols          = 40;
    constexpr Word  kTextBase          = 0x0400;
    constexpr Word  kRowGroupStride    = 0x28;
    constexpr Word  kRowSubgroupStride = 0x80;
    constexpr int   kRowsPerGroup      = 8;
    constexpr int   kTextCols80        = 80;



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
//  Copies the current emulator framebuffer to the clipboard as a CF_DIB.
//
//  CF_DIB is chosen over CF_BITMAP because it is device-independent: the
//  bytes are self-describing and every paste target understands them, with no
//  GDI object to create or leak.
//
//  Rows go out in REVERSE. A DIB with positive height is bottom-up by
//  definition, while the framebuffer is stored top-down, so copying it
//  straight through pastes the screen upside down.
//
//  The whole operation is done under the framebuffer lock, so the copy is one
//  coherent frame rather than a tear across two.
//
//  Once the clipboard is open it MUST be closed on every path, which is why
//  the two allocation failures fall through to the close rather than returning
//  -- leaving the clipboard open locks it for the entire desktop.
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
//  Queues clipboard text for delivery to the guest keyboard, translating it
//  to what an Apple II can actually receive.
//
//  Three translations happen, all of them necessary:
//
//    LF dropped     the Apple II line terminator is CR alone, so a Windows
//                   CRLF would deliver a spurious extra keystroke
//    CR mapped      to $0D, the code the keyboard latch expects
//    non-ASCII      dropped entirely -- there is no key for a character the
//                   machine has no encoding for, and passing one through
//                   would land as an arbitrary control code
//
//  Text is queued rather than typed. The guest reads the keyboard latch at its
//  own pace, so delivery is paced by DrainPasteBuffer against the strobe; this
//  function only fills the buffer.
//
//  The buffer is filled under the command mutex because it is drained on the
//  CPU thread.
//
////////////////////////////////////////////////////////////////////////////////

void ClipboardManager::PasteFromClipboard (HWND hwnd)
{
    constexpr Byte     kCarriageReturn = 0x0D;
    constexpr wchar_t  kNewline        = L'\n';
    constexpr wchar_t  kReturn         = L'\r';



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
//  Feeds ONE queued character to the guest keyboard, and only once the guest
//  has consumed the previous one.
//
//  The strobe is the handshake, and it is what makes paste work at all. The
//  Apple II keyboard latch holds a single character; writing a second before
//  the guest has read the first simply overwrites it, so a paste that ignored
//  the strobe would deliver a few random characters out of a whole paragraph.
//  Gating on IsStrobeClear paces the entire paste at exactly the speed the
//  running program reads.
//
//  Called from the per-instruction path, so it is deliberately cheap: a null
//  check, a strobe read, and a lock taken only when there is something to
//  send.
//
//  A zero character doubles as "nothing to send" -- the paste path never
//  queues a NUL, so it needs no separate empty flag.
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


