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

    void  CopyScreenText     (HWND hwnd) const;
    void  CopyScreenshot     (HWND hwnd);
    void  PasteFromClipboard (HWND hwnd);
    void  DrainPasteBuffer   ();

private:
    MemoryBus              & m_memoryBus;
    std::mutex             & m_cmdMutex;
    std::string            & m_pasteBuffer;
    std::mutex             & m_fbMutex;
    std::vector<uint32_t>  & m_uiFramebuffer;
    AppleKeyboard *        * m_pKeyboardSlot     = nullptr;
    int                      m_framebufferWidth  = 0;
    int                      m_framebufferHeight = 0;
};
