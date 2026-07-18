#pragma once

#include "Pch.h"

#include "Core/MemoryBus.h"


class AppleKeyboard;





////////////////////////////////////////////////////////////////////////////////
//
//  ClipboardManager
//
//  Owner of the host-OS clipboard interactions: copying the Apple II
//  text screen as Unicode text, copying the rendered framebuffer as a
//  DIB, and feeding host-paste data into the CPU-side paste buffer for
//  later draining onto the emulated keyboard.
//
//  The class holds references to the shared state it operates on
//  (memory bus, paste-buffer + its mutex, framebuffer + its mutex) and
//  takes the active keyboard via a pointer-to-pointer so machine
//  switches in EmulatorShell do not require re-wiring the manager.
//
////////////////////////////////////////////////////////////////////////////////

class ClipboardManager
{
public:
    ClipboardManager  (MemoryBus                & memoryBus,
                       std::mutex               & cmdMutex,
                       std::string              & pasteBuffer,
                       std::mutex               & fbMutex,
                       std::vector<uint32_t>    & uiFramebuffer,
                       int                        framebufferWidth,
                       int                        framebufferHeight,
                       AppleKeyboard          * * pKeyboardSlot);
    ~ClipboardManager () = default;

    // auxRam is the //e/c auxiliary 64 KiB bank (nullptr on ][/][+). When
    // present AND the 80-column display is on (RD80VID), the text screen is
    // read as 80 interleaved columns (aux = even, main = odd); otherwise the
    // plain 40-column main page is read.
    void  CopyScreenText     (HWND hwnd, const Byte * auxRam) const;
    void  CopyScreenshot     (HWND hwnd);
    void  PasteFromClipboard (HWND hwnd);
    void  DrainPasteBuffer   ();

    // Screen-text scrape, factored out of CopyScreenText so it can be unit
    // tested without the Win32 clipboard. Returns CRLF-terminated rows with
    // trailing spaces trimmed.
    std::wstring  BuildScreenText (const Byte * auxRam) const;

private:
    // Map one raw text-screen byte to a printable wchar (high bit stripped,
    // non-printables blanked).
    static wchar_t  DecodeScreenByte (Byte ch);

    MemoryBus              & m_memoryBus;
    std::mutex             & m_cmdMutex;
    std::string            & m_pasteBuffer;
    std::mutex             & m_fbMutex;
    std::vector<uint32_t>  & m_uiFramebuffer;
    AppleKeyboard *        * m_pKeyboardSlot     = nullptr;
    int                      m_framebufferWidth  = 0;
    int                      m_framebufferHeight = 0;
};
