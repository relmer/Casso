#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "HeadlessHost.h"
#include "KeystrokeInjector.h"
#include "Devices/AppleIIeMmu.h"
#include "Video/CharacterRomData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace
{
    static constexpr uint64_t  kColdBootCycles = 5'000'000ULL;
    static constexpr uint64_t  kAfterCommand   = 2'000'000ULL;
}


////////////////////////////////////////////////////////////////////////////////
//
//  Pr3AuxClearTest
//
//  Gates the //e enhanced-video-ROM Decode4K alt-set fix. Before the
//  fix, the alt set was loaded from the wrong half of the 4 KB ROM
//  file, so PR#3 (which engages ALTCHARSET=1) rendered every character
//  cell as garbage glyphs. Now PR#3 produces a clean blank screen.
//
//  The fix matches the //e enhanced video ROM layout documented in
//  UTAIIe ch. 8 (Sather): the alt set's 256 chars all live in the
//  FIRST 2 KB of the file (chars $00-$7F as inverse, $80-$FF reusing
//  the primary set's normal text). The second 2 KB of the ROM file
//  isn't used by the standard //e enhanced ROM at all.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Pr3AuxClearTest)
{
public:

    TEST_METHOD (RealCharRom_Decodes_SpaceAsBlank_AltSet)
    {
        CharacterRomData rom;
        HRESULT hr = rom.LoadFromFile (
            "C:\\Users\\relmer\\source\\repos\\relmer\\Casso\\ROMs\\apple2e-enhanced-video.rom");
        Assert::IsTrue (SUCCEEDED (hr), L"Must locate apple2e-enhanced-video.rom");

        for (int y = 0; y < 8; y++)
        {
            Byte primary = rom.GetGlyphRow (0xA0, y, false);
            Byte alt     = rom.GetGlyphRow (0xA0, y, true);

            wchar_t msg[80];
            swprintf_s (msg, L"$A0 row %d: primary=$%02X alt=$%02X (must both be 0)",
                        y, primary, alt);
            Assert::AreEqual (Byte (0x00), primary, msg);
            Assert::AreEqual (Byte (0x00), alt,     msg);
        }
    }


    // Verifies PR#3 leaves both main and aux text page 1 cleared,
    // so 80-col rendering doesn't show garbage in the aux columns.
    //
    // FIXME: aux $0480 (cursor save cell -- the byte the //e cursor-
    // blink routine reads / restores in the spot the cursor lands at
    // after CR/LF following PR#3) is left at its PRNG-randomized value
    // because Casso's //e cursor-blink loop never runs (likely a VBL
    // interrupt or similar timer not yet wired). The cursor is
    // therefore invisible in 80-col mode -- separate bug, separate
    // fix. Whitelist that one cell so this test gates the Decode4K
    // alt-set correctness in isolation.
    TEST_METHOD (Pr3_Clears_AuxTextPage1_AllRows)
    {
        HeadlessHost   host;
        EmulatorCore   core;

        HRESULT  hr = host.BuildAppleIIe (core);
        Assert::IsTrue (SUCCEEDED (hr));

        core.PowerCycle ();
        core.RunCycles (kColdBootCycles);

        size_t  consumed = KeystrokeInjector::InjectLine (core, "PR#3", kAfterCommand);
        Assert::AreEqual (size_t (5), consumed);

        Byte * auxBuf = core.mmu->GetAuxBuffer ();
        Assert::IsNotNull (auxBuf);

        wchar_t  msg[1024] = {};
        int totalBad = 0;
        for (int row = 0; row < 24; row++)
        {
            Word rowBase = static_cast<Word> (0x0400 + 128 * (row % 8) + 40 * (row / 8));
            int  rowBad  = 0;
            for (int memCol = 0; memCol < 40; memCol++)
            {
                Word a = static_cast<Word> (rowBase + memCol);
                if (a == 0x0480) continue;
                if (auxBuf[a] != 0xA0) rowBad++;
            }
            if (rowBad > 0)
            {
                wchar_t  line[64] = {};
                swprintf_s (line, L"row %d ($%04X) bad=%d  ", row, rowBase, rowBad);
                wcscat_s (msg, line);
                totalBad += rowBad;
            }
        }

        Assert::AreEqual (0, totalBad, msg);
    }
};