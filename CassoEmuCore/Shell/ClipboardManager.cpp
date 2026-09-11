#include "Pch.h"

#include "ClipboardManager.h"

#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Shell/Input/CapsLockTracker.h"





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
    IHostClipboard           & clipboard,
    MemoryBus                & memoryBus,
    std::mutex               & cmdMutex,
    std::string              & pasteBuffer,
    std::mutex               & framebufferMutex,
    std::vector<uint32_t>    & uiFramebuffer,
    int                        framebufferWidth,
    int                        framebufferHeight,
    AppleKeyboard          * * pKeyboardSlot)
    : m_clipboard          (clipboard),
      m_memoryBus          (memoryBus),
      m_cmdMutex           (cmdMutex),
      m_pasteBuffer        (pasteBuffer),
      m_framebufferMutex   (framebufferMutex),
      m_uiFramebuffer      (uiFramebuffer),
      m_pKeyboardSlot      (pKeyboardSlot),
      m_framebufferWidth   (framebufferWidth),
      m_framebufferHeight  (framebufferHeight)
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
    int             cols               = 0;



    std::wstring  text;

    // 80-column text interleaves auxiliary memory (even display columns) with
    // main memory (odd columns). That layout is live only on a machine that
    // has an aux bank AND currently has the 80-column display switched on
    // (RD80VID, $C01F bit 7); otherwise the plain 40-column main page is read.
    bool  eighty = (auxRam != nullptr)
                && ((m_memoryBus.ReadByte (kRd80Vid) & kHighBitMask) != 0);
    cols = eighty ? kTextCols80 : kTextCols;



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
    bool  placed = m_clipboard.SetText (hwnd, BuildScreenText (auxRam));



    // Another process can hold the clipboard; a failed placement is not an
    // error worth surfacing, the copy just does not happen.
    IGNORE_RETURN_VALUE (placed, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopyScreenshot
//
//  Copies an already-captured image to the clipboard as a CF_DIB.
//
//  CF_DIB is chosen over CF_BITMAP because it is device-independent: the
//  bytes are self-describing and every paste target understands them, with no
//  GDI object to create or leak.
//
//  NO CONVERSION HAPPENS HERE. A 32bpp BI_RGB DIB is BGRA, which is what a
//  CapturedImage already holds -- that is the whole reason it holds BGRA
//  rather than RGBA, and the PNG encoder pays the one conversion instead.
//
//  Rows go out in REVERSE. A DIB with positive height is bottom-up by
//  definition, while a capture is stored top-down, so copying it straight
//  through pastes the screen upside down.
//
//  Once the clipboard is open it MUST be closed on every path, which is why
//  the two allocation failures fall through to the close rather than returning
//  -- leaving the clipboard open locks it for the entire desktop.
//
////////////////////////////////////////////////////////////////////////////////

bool ClipboardManager::CopyScreenshot (HWND hwnd, const CapturedImage & image)
{
    std::vector<Byte>  dib;



    if (!image.IsValid())
    {
        return false;
    }

    BuildDib (image, dib);

    return m_clipboard.SetDib (hwnd, dib);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildDib
//
//  CF_DIB is chosen over CF_BITMAP because it is device-independent: the
//  pixels go across as bytes, with no HBITMAP tied to a device context. A
//  DIB stores its rows bottom-up, so the capture's top row is written last.
//
////////////////////////////////////////////////////////////////////////////////

void ClipboardManager::BuildDib (const CapturedImage & image, std::vector<Byte> & outDib)
{
    constexpr WORD    kDibBitCount = 32;
    BITMAPINFOHEADER  bih          = {};
    size_t            rowBytes     = static_cast<size_t> (image.widthPx) * CapturedImage::kBytesPerPixel;
    size_t            dataSize     = rowBytes * image.heightPx;
    Byte *            pDest        = nullptr;
    int               y            = 0;



    bih.biSize        = sizeof (bih);
    bih.biWidth       = image.widthPx;
    bih.biHeight      = image.heightPx;
    bih.biPlanes      = 1;
    bih.biBitCount    = kDibBitCount;
    bih.biCompression = BI_RGB;
    bih.biSizeImage   = static_cast<DWORD> (dataSize);

    outDib.resize (sizeof (bih) + dataSize);
    pDest = outDib.data();

    memcpy (pDest, &bih, sizeof (bih));
    pDest += sizeof (bih);

    for (y = image.heightPx - 1; y >= 0; y--)
    {
        memcpy (pDest, &image.bgra[static_cast<size_t> (y) * rowBytes], rowBytes);
        pDest += rowBytes;
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
//  Caps Lock applies as it would to the same text typed: with it down, a
//  lower-case letter goes in upper case. Returns true when that changed at
//  least one letter, so the caller can say why the paste differs from the
//  clipboard.
//
//  The buffer is filled under the command mutex because it is drained on the
//  CPU thread.
//
////////////////////////////////////////////////////////////////////////////////

bool ClipboardManager::PasteFromClipboard (HWND hwnd, bool capsLockOn)
{
    std::wstring  text;
    bool          raisedLetters = false;



    if (!m_clipboard.GetText (hwnd, text))
    {
        return false;
    }

    {
        std::lock_guard<std::mutex>  lock (m_cmdMutex);

        raisedLetters = AppendPasteText (text, capsLockOn, m_pasteBuffer);
    }

    return raisedLetters;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendPasteText
//
////////////////////////////////////////////////////////////////////////////////

bool ClipboardManager::AppendPasteText (const std::wstring & text, bool capsLockOn, std::string & pasteBuffer)
{
    constexpr Byte     kCarriageReturn = 0x0D;
    constexpr wchar_t  kNewline        = L'\n';
    constexpr wchar_t  kReturn         = L'\r';
    bool               raisedLetters   = false;
    Byte               typed           = 0;
    Byte               delivered       = 0;



    for (wchar_t ch : text)
    {
        if (ch == kNewline)
        {
            continue;
        }

        if (ch == kReturn)
        {
            pasteBuffer += static_cast<char> (kCarriageReturn);
        }
        else if (ch >= kPrintableLow && ch < (wchar_t) (kPrintableHigh + 1))
        {
            typed         = static_cast<Byte> (ch);
            delivered     = CapsLockTracker::ApplyCapsLock (typed, capsLockOn);
            raisedLetters = raisedLetters || delivered != typed;
            pasteBuffer  += static_cast<char> (delivered);
        }
    }

    return raisedLetters;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrainPasteBuffer
//
//  Feeds ONE queued character to the guest keyboard, and only once the guest
//  has consumed the previous one AND settled back into its keyboard poll.
//
//  The strobe is the handshake, and it is what makes paste work at all. The
//  Apple II keyboard latch holds a single character; writing a second before
//  the guest has read the first simply overwrites it, so a paste that ignored
//  the strobe would deliver a few random characters out of a whole paragraph.
//
//  Strobe-clear ALONE is not enough, though: the guest clears the strobe the
//  moment it grabs a key, then may flush the keyboard again while processing
//  it -- the firmware's screen-wrap and scroll paths do, and DOS's boot
//  flushes wholesale -- so a character sent on the first clear reading races
//  into that window and is silently discarded (pastes used to lose a couple
//  of characters at every 40-column wrap). The settle threshold requires the
//  strobe to stay clear for a stretch of GUEST TIME comfortably longer than
//  a full-screen scroll, so the send lands only once the guest is genuinely
//  polling.
//
//  Called once per execution slice with that slice's cycle budget, so it is
//  deliberately cheap: a null check, a strobe read, an accumulate, and a
//  lock taken only when a send is due.
//
//  A zero character doubles as "nothing to send" -- the paste path never
//  queues a NUL, so it needs no separate empty flag.
//
////////////////////////////////////////////////////////////////////////////////

void ClipboardManager::DrainPasteBuffer (uint32_t cyclesElapsed)
{
    // 20k cycles = 20 ms of emulated 1 MHz time: longer than a worst-case
    // 24-row text scroll (~15k cycles), short enough that a whole-line paste
    // lands in a couple of seconds.
    constexpr uint32_t   kStrobeSettleCycles = 20000;



    AppleKeyboard      * keyboard = nullptr;
    Byte                 ch       = 0;



    keyboard = (m_pKeyboardSlot != nullptr) ? *m_pKeyboardSlot : nullptr;

    if (keyboard == nullptr)
    {
        return;
    }

    if (!keyboard->IsStrobeClear())
    {
        m_strobeClearCycles = 0;
        return;
    }

    if (m_strobeClearCycles < kStrobeSettleCycles)
    {
        m_strobeClearCycles += cyclesElapsed;
        return;
    }

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
        keyboard->PressKey (ch);
        m_strobeClearCycles = 0;
    }
}


