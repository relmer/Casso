#include "Pch.h"
#include <filesystem>

#include "HeadlessHost.h"
#include "KeystrokeInjector.h"
#include "Devices/Apple2eMmu.h"
#include "Video/CharacterRomData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace fs = std::filesystem;

namespace
{
    static constexpr uint64_t  kColdBootCycles = 5'000'000ULL;
    static constexpr uint64_t  kAfterCommand   = 2'000'000ULL;
    static constexpr int       kMaxAncestorWalk = 10;


    // Find the directory containing `Machines/` by walking up from CWD.
    // Mirrors the resolver pattern in BackwardsCompatTests so these
    // tests stay filesystem-independent across CI vs local builds.
    fs::path FindRepoRoot ()
    {
        std::error_code ec;
        fs::path        cursor = fs::current_path (ec);
        if (ec) return fs::path ();

        for (int i = 0; i < kMaxAncestorWalk; i++)
        {
            if (fs::exists (cursor / "Machines", ec) &&
                fs::is_directory (cursor / "Machines", ec))
            {
                return cursor;
            }
            if (!cursor.has_parent_path () || cursor == cursor.parent_path ())
            {
                break;
            }
            cursor = cursor.parent_path ();
        }
        return fs::path ();
    }


    fs::path FindRomPath (const std::string & relPath)
    {
        fs::path root = FindRepoRoot ();
        if (root.empty ()) return fs::path ();
        return root / relPath;
    }
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
//  Also documents the //e PR#3 cursor visibility model (originally
//  filed as a "cursor blink loop never runs" bug, but on a stock //e
//  without a mouse card the PR#3 cursor is *static*, not blinking --
//  the firmware in $C800-$CFFF never polls $C019 / RDVBLBAR and never
//  XORs the inverse bit into the cursor cell, both of which we
//  verified by scanning the ROM end-to-end). The cursor cell is
//  written *once* on entry to BASICIN with $20 (inverse-space, which
//  with ALTCHARSET=1 renders as a solid block) and stays solid until
//  the user types a key. This is the same behavior a real Apple //e
//  exhibits in stock-no-mouse-card mode. The originally observed
//  "invisible cursor" was a side-effect of the Decode4K alt-set bug
//  rendering $20 as a garbage glyph instead of a solid block.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Pr3AuxClearTest)
{
public:

    TEST_METHOD (RealCharRom_Decodes_SpaceAsBlank_AltSet)
    {
        fs::path romPath = FindRomPath ("ROMs/Apple2e_Video.rom");
        if (romPath.empty () || !fs::exists (romPath))
        {
            Logger::WriteMessage ("SKIPPED: ROMs/Apple2e_Video.rom "
                                  "not present (CI runners do not provision "
                                  "Apple-owned ROMs).\n");
            return;
        }

        CharacterRomData rom;
        HRESULT hr = rom.LoadFromFile (romPath.string ());
        Assert::IsTrue (SUCCEEDED (hr), L"Must load Apple2e_Video.rom");

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


    // Verifies PR#3 leaves aux text page 1 cleared. Every cell must be
    // $A0 (inverse-space) EXCEPT aux $0480 -- after PR#3 the //e
    // firmware echoes a CR/LF and then BASIC reprints its prompt
    // character ']' = $5D | $80 = $DD at column 0 of row 1, which in
    // 80-col interleaved layout is aux $0480. So aux $0480 = $DD is
    // the *prompt*, not garbage. The static cursor that follows the
    // prompt lives at column 1 of row 1 = main $0480 (see the
    // companion Pr3_StaticCursor_Lands_At_Main0480 test).
    TEST_METHOD (Pr3_Clears_AuxTextPage1_AllRows)
    {
        HeadlessHost   host;
        EmulatorCore   core;

        HRESULT  hr = host.BuildApple2e (core);
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
                if (a == 0x0480) continue;     // BASIC prompt ']' lands here
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


    // Verifies the BASIC prompt and static cursor land where they
    // should after PR#3 + Return. With 80-col mode and 80STORE active,
    // the row-1 cells interleave aux/main: col 0 -> aux $0480,
    // col 1 -> main $0480, col 2 -> aux $0481, etc.
    //
    //   - aux $0480 must be $DD (the ']' prompt = $5D | $80)
    //   - main $0480 must be $20 (the inverse-space cursor character;
    //     with ALTCHARSET=1 this glyph is rendered as a solid block
    //     and is the visible cursor)
    //   - $057B (OURCH, 80-col cursor column) must be $01 (cursor sits
    //     immediately after the prompt)
    //   - $25 (CV, cursor row) must be $01 (row 1)
    //
    // Pre-fix to commit 60b13a6 (Decode4K alt-set bug), $20 in the alt
    // set was loaded from the wrong half of the ROM and rendered as a
    // garbage glyph, which made the cursor "invisible" -- not because
    // the firmware failed to draw it, but because the glyph it drew
    // was incorrectly decoded. After 60b13a6 the cursor is a clean
    // solid block. This test pins the byte-level state so the cursor
    // can't silently regress.
    TEST_METHOD (Pr3_StaticCursor_Lands_At_Main0480)
    {
        HeadlessHost   host;
        EmulatorCore   core;

        HRESULT  hr = host.BuildApple2e (core);
        Assert::IsTrue (SUCCEEDED (hr));

        core.PowerCycle();
        core.RunCycles (kColdBootCycles);

        size_t  consumed = KeystrokeInjector::InjectLine (core, "PR#3", kAfterCommand);
        Assert::AreEqual (size_t (5), consumed);

        Byte * auxBuf = core.mmu->GetAuxBuffer();
        Assert::IsNotNull (auxBuf);

        Byte   prompt   = auxBuf[0x0480];
        Byte   cursor   = core.bus->ReadByte (0x0480);
        Byte   ourch    = core.bus->ReadByte (0x057B);
        Byte   cv       = core.bus->ReadByte (0x0025);

        Assert::AreEqual (Byte (0xDD), prompt, L"aux $0480 must be the ']' prompt ($DD)");
        Assert::AreEqual (Byte (0x20), cursor, L"main $0480 must be inverse-space ($20) cursor");
        Assert::AreEqual (Byte (0x01), ourch,  L"$057B (OURCH) must be col 1 (after prompt)");
        Assert::AreEqual (Byte (0x01), cv,     L"$25 (CV) must be row 1");
    }
};
