#include "Pch.h"

#include "Shell/ClipboardManager.h"
#include "Core/MemoryBus.h"
#include "Core/MemoryDevice.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  ClipboardTextTests
//
//  Covers ClipboardManager::BuildScreenText -- the pure text-scrape behind
//  Edit > Copy Text. The regression under test: 80-column screens interleave
//  auxiliary memory (even display columns) with main memory (odd columns), so a
//  scrape that reads only main RAM captures every other character. The fix reads
//  both banks when RD80VID ($C01F bit 7) says the 80-column display is on.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ClipboardTextTests)
{
    // Minimal $C01F responder. The I/O range is device-routed on the bus (a RAM
    // page can't back it, and open bus reads as 0xFF), so the 80-column status
    // bit has to come from a real device.
    class Rd80VidStub : public MemoryDevice
    {
    public:
        explicit Rd80VidStub (bool on) : m_val (on ? 0x80 : 0x00) {}

        Byte Read (Word) override { return m_val; }
        void Write (Word, Byte) override {}
        Word GetStart() const override { return 0xC01F; }
        Word GetEnd() const override { return 0xC01F; }
        void Reset() override {}

    private:
        Byte  m_val;
    };

    // Row 0 of the text screen lives at $0400; consecutive character cells map
    // to $0400, $0401, ... in each bank. Seed a distinctive interleave:
    //   aux[$0400]='A' main[$0400]='B' aux[$0401]='C' main[$0401]='D'
    // so an 80-column read yields "ABCD" and a main-only read yields "BD".
    static constexpr Word  kRow0 = 0x0400;

    static Byte  Screen (char c) { return static_cast<Byte> (c) | 0x80; }   // normal glyph

    static void  MapMainTextPages (MemoryBus & bus, std::vector<Byte> & main)
    {
        main.assign (0x10000, 0);
        for (int page = 0x04; page <= 0x07; ++page)
        {
            bus.SetReadPage (page, main.data() + page * 0x100);
        }
        main[kRow0 + 0] = Screen ('B');   // odd display column 1
        main[kRow0 + 1] = Screen ('D');   // odd display column 3
    }

    // The ClipboardManager keeps references to these; BuildScreenText never
    // reads them, but they must outlive the manager, so they live on the
    // (per-test) fixture rather than as shared statics.
    std::mutex             m_cmdMutex;
    std::mutex             m_fbMutex;
    std::string            m_pasteBuffer;
    std::vector<uint32_t>  m_uiFramebuffer;
    AppleKeyboard *        m_keyboardSlot = nullptr;

    ClipboardManager  MakeClipboard (MemoryBus & bus)
    {
        return ClipboardManager (bus, m_cmdMutex, m_pasteBuffer, m_fbMutex,
                                 m_uiFramebuffer, 0, 0, &m_keyboardSlot);
    }

    static std::wstring  FirstRow (const std::wstring & text)
    {
        size_t  nl = text.find (L"\r\n");
        return (nl == std::wstring::npos) ? text : text.substr (0, nl);
    }

public:

    TEST_METHOD (BuildScreenText_EightyColumn_InterleavesAuxAndMain)
    {
        MemoryBus          bus;
        std::vector<Byte>  main;
        std::vector<Byte>  aux (0x10000, 0);
        Rd80VidStub        rd80 (/*on*/ true);

        MapMainTextPages (bus, main);
        bus.AddDevice (&rd80);
        aux[kRow0 + 0] = Screen ('A');   // even display column 0
        aux[kRow0 + 1] = Screen ('C');   // even display column 2

        ClipboardManager  clip = MakeClipboard (bus);

        std::wstring  row = FirstRow (clip.BuildScreenText (aux.data()));

        Assert::AreEqual (std::wstring (L"ABCD"), row,
                          L"80-column scrape must interleave aux(even)+main(odd)");
    }

    TEST_METHOD (BuildScreenText_FortyColumn_ReadsMainOnly)
    {
        MemoryBus          bus;
        std::vector<Byte>  main;
        Rd80VidStub        rd80 (/*on*/ false);

        MapMainTextPages (bus, main);
        bus.AddDevice (&rd80);

        ClipboardManager  clip = MakeClipboard (bus);

        // No aux bank (nullptr) => plain 40-column main-page read.
        std::wstring  row = FirstRow (clip.BuildScreenText (nullptr));

        Assert::AreEqual (std::wstring (L"BD"), row,
                          L"40-column scrape reads the main text page unchanged");
    }

    TEST_METHOD (BuildScreenText_EightyOffButAuxPresent_StaysFortyColumn)
    {
        MemoryBus          bus;
        std::vector<Byte>  main;
        std::vector<Byte>  aux (0x10000, 0);
        Rd80VidStub        rd80 (/*on*/ false);

        // Aux bank is wired, but the 80-column display is OFF (RD80VID clear),
        // so the scrape must not fold aux memory in.
        MapMainTextPages (bus, main);
        bus.AddDevice (&rd80);
        aux[kRow0 + 0] = Screen ('A');
        aux[kRow0 + 1] = Screen ('C');

        ClipboardManager  clip = MakeClipboard (bus);

        std::wstring  row = FirstRow (clip.BuildScreenText (aux.data()));

        Assert::AreEqual (std::wstring (L"BD"), row,
                          L"RD80VID clear must gate off the aux interleave");
    }
};
