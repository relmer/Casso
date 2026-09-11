#include "Pch.h"

#include "Core/MemoryBus.h"
#include "Shell/ClipboardManager.h"

#include "FakeHostClipboard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ClipboardSeamTests
//
//  What reaches the host clipboard, and what comes back from it, with the
//  clipboard itself replaced by one made of vectors.
//
//  Both directions are decided in core. Going out, the picture is packed as a
//  DIB and the text screen scraped to Unicode; coming in, pasted text is cut
//  down to what an Apple II keyboard can type. Only the handoff was the
//  operating system's, and with that behind a seam every one of those
//  decisions is a thing a test reads back.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ClipboardSeamTests)
{
public:

    TEST_METHOD (ACapturedImageGoesOutAsADibWithItsRowsBottomUp)
    {
        FakeHostClipboard    clipboard;
        ClipboardManager     manager = MakeManager (clipboard);
        CapturedImage        image;
        BITMAPINFOHEADER     header  = {};
        const Byte *         rows    = nullptr;

        //  Two rows of two pixels, each pixel its own value.
        image.widthPx  = 2;
        image.heightPx = 2;
        image.bgra     = { 0x01, 0x02, 0x03, 0x04,   0x11, 0x12, 0x13, 0x14,     // top row
                           0x21, 0x22, 0x23, 0x24,   0x31, 0x32, 0x33, 0x34 };   // bottom row

        Assert::IsTrue (manager.CopyScreenshot (nullptr, image), L"placed");
        Assert::AreEqual (sizeof (BITMAPINFOHEADER) + 16, clipboard.placedDib.size(), L"header plus pixels");

        memcpy (&header, clipboard.placedDib.data(), sizeof (header));
        Assert::AreEqual ((DWORD) sizeof (BITMAPINFOHEADER), header.biSize);
        Assert::AreEqual (2L, header.biWidth);
        Assert::AreEqual (2L, header.biHeight,     L"positive height: bottom-up rows");
        Assert::AreEqual ((WORD) 32, header.biBitCount);
        Assert::AreEqual ((DWORD) BI_RGB, header.biCompression);
        Assert::AreEqual ((DWORD) 16, header.biSizeImage);

        //  A DIB stores its rows bottom-up, so the capture's bottom row comes
        //  first and the top row last.
        rows = clipboard.placedDib.data() + sizeof (header);
        Assert::AreEqual ((Byte) 0x21, rows[0],  L"first DIB row is the capture's bottom row");
        Assert::AreEqual ((Byte) 0x34, rows[7]);
        Assert::AreEqual ((Byte) 0x01, rows[8],  L"second DIB row is the capture's top row");
        Assert::AreEqual ((Byte) 0x14, rows[15]);
    }


    TEST_METHOD (AnInvalidImageIsNotPlacedAndSaysSo)
    {
        FakeHostClipboard  clipboard;
        ClipboardManager   manager = MakeManager (clipboard);
        CapturedImage      empty;

        Assert::IsFalse (manager.CopyScreenshot (nullptr, empty));
        Assert::AreEqual (0, clipboard.setCalls, L"nothing was offered to the clipboard");
    }


    TEST_METHOD (AClipboardAnotherProcessHoldsCostsTheCopyAndNothingElse)
    {
        FakeHostClipboard  clipboard;
        ClipboardManager   manager = MakeManager (clipboard);
        CapturedImage      image;

        image.widthPx  = 1;
        image.heightPx = 1;
        image.bgra     = { 1, 2, 3, 4 };
        clipboard.refusing = true;

        Assert::IsFalse (manager.CopyScreenshot (nullptr, image),
                         L"the caller records a clipboard failure so it does not cost the file");
    }


    TEST_METHOD (TheTextScreenGoesOutAsTheScrapeProduces)
    {
        FakeHostClipboard  clipboard;
        MemoryBus          bus;
        ClipboardManager   manager = MakeManager (clipboard, bus);

        manager.CopyScreenText (nullptr, nullptr);

        Assert::AreEqual (manager.BuildScreenText (nullptr), clipboard.placedText,
                          L"what was placed is exactly the scrape, unchanged");
    }


    TEST_METHOD (PastedTextIsCutDownToWhatTheKeyboardCanType)
    {
        FakeHostClipboard  clipboard;
        ClipboardManager   manager = MakeManager (clipboard);

        //  CRLF is one return, a lone LF is nothing, a tab and an accented
        //  letter are outside the keyboard's reach, and 0x7F is past the last
        //  printable.
        clipboard.hasText     = true;
        clipboard.textToPaste = L"10 PRINT \"HI\"\r\n20 GOTO 10\n\tR\x00C9SUM\x7F";

        manager.PasteFromClipboard (nullptr);

        Assert::AreEqual (std::string ("10 PRINT \"HI\"\r20 GOTO 10RSUM"), m_pasteBuffer);
    }


    TEST_METHOD (NothingToPasteLeavesTheBufferAlone)
    {
        FakeHostClipboard  clipboard;
        ClipboardManager   manager = MakeManager (clipboard);

        m_pasteBuffer = "KEEP";
        clipboard.hasText = false;

        manager.PasteFromClipboard (nullptr);

        Assert::AreEqual (std::string ("KEEP"), m_pasteBuffer);
    }


private:

    std::mutex             m_cmdMutex;
    std::mutex             m_framebufferMutex;
    std::string            m_pasteBuffer;
    std::vector<uint32_t>  m_uiFramebuffer;
    AppleKeyboard *        m_keyboardSlot = nullptr;
    MemoryBus              m_bus;


    ClipboardManager  MakeManager (IHostClipboard & clipboard)
    {
        return MakeManager (clipboard, m_bus);
    }


    ClipboardManager  MakeManager (IHostClipboard & clipboard, MemoryBus & bus)
    {
        return ClipboardManager (clipboard, bus, m_cmdMutex, m_pasteBuffer, m_framebufferMutex,
                                 m_uiFramebuffer, 0, 0, &m_keyboardSlot);
    }
};
