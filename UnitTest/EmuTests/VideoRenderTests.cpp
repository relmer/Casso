#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>
#include <filesystem>

#include "Core/MemoryBus.h"
#include "Core/EmuCpu.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Video/AppleTextMode.h"
#include "Video/Apple80ColTextMode.h"
#include "Video/AppleLoResMode.h"
#include "Video/AppleHiResMode.h"
#include "Video/AppleDoubleHiResMode.h"
#include "Video/CharacterRom.h"
#include "Video/CharacterRomData.h"
#include "Video/NtscColorTable.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace fs = std::filesystem;


static constexpr int kFbW = 560;
static constexpr int kFbH = 384;

static constexpr uint32_t kGreen = 0xFF00FF00u;
static constexpr uint32_t kBlack = 0xFF000000u;
static constexpr uint32_t kWhite = 0xFFFFFFFFu;

// Lo-res color table (must match AppleLoResMode.cpp exactly — both use
// B8G8R8A8 byte layout matching the swap chain format)
static const uint32_t kExpectedLoRes[16] =
{
    0xFF000000,   //  0: Black
    0xFFDD2266,   //  1: Magenta
    0xFF000099,   //  2: Dark Blue
    0xFFDD0044,   //  3: Purple
    0xFF002200,   //  4: Dark Green
    0xFF555555,   //  5: Grey 1
    0xFF0022CC,   //  6: Medium Blue
    0xFF66AAFF,   //  7: Light Blue
    0xFF885500,   //  8: Brown
    0xFFFF4400,   //  9: Orange
    0xFFAAAAAA,   // 10: Grey 2
    0xFFFF8888,   // 11: Pink
    0xFF00DD00,   // 12: Light Green
    0xFFFFFF00,   // 13: Yellow
    0xFF44FFDD,   // 14: Aquamarine
    0xFFFFFFFF,   // 15: White
};





////////////////////////////////////////////////////////////////////////////////
//
//  VideoRenderWithMemoryTests
//
//  Adversarial tests that PROVE video rendering actually works end-to-end.
//  These tests verify specific pixel patterns, exact colors, and address
//  calculations -- not just "non-black" or "didn't crash".
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (VideoRenderWithMemoryTests)
{
public:

    TEST_METHOD (TextMode_RenderWithMemory_ProducesNonBlackOutput)
    {
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        std::vector<Byte> mem (0x10000, 0x00);
        mem[0x0400] = 0xC1;

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        std::vector<uint32_t> fb (kFbW * kFbH, kBlack);
        textMode.Render (mem.data (), fb.data (), kFbW, kFbH);

        bool hasNonBlack = false;

        for (int i = 0; i < kFbW * kFbH && !hasNonBlack; i++)
        {
            if (fb[i] != kBlack)
            {
                hasNonBlack = true;
            }
        }

        Assert::IsTrue (hasNonBlack,
            L"Rendering with memory pointer should produce non-black output");
    }

    TEST_METHOD (TextMode_RenderWithNullMemory_FallsBackToBus)
    {
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        bus.WriteByte (0x0400, 0xC1);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        std::vector<uint32_t> fb (kFbW * kFbH, 0);
        textMode.Render (nullptr, fb.data (), kFbW, kFbH);

        bool hasGreen = false;

        for (int y = 0; y < 16 && !hasGreen; y++)
        {
            for (int x = 0; x < 14 && !hasGreen; x++)
            {
                if (fb[y * kFbW + x] == kGreen)
                {
                    hasGreen = true;
                }
            }
        }

        Assert::IsTrue (hasGreen,
            L"Null videoRam should fall back to bus and still render 'A'");
    }

    TEST_METHOD (TextMode_NormalA_ExactGlyphPixelPattern)
    {
        // Verify every pixel of the 'A' glyph matches CharacterRom exactly.
        // $C1 normal 'A': glyphIndex = $C1 - $80 = $41, romOffset = ($41-$20)*8 = 264
        std::vector<Byte> mem (0x10000, 0xA0);
        mem[0x0400] = 0xC1;

        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        std::vector<uint32_t> fb (kFbW * kFbH, 0);
        textMode.Render (mem.data (), fb.data (), kFbW, kFbH);

        int romOffset = (0x41 - 0x20) * 8;

        for (int py = 0; py < 8; py++)
        {
            Byte glyphRow = kApple2CharRom[romOffset + py];

            for (int px = 0; px < 7; px++)
            {
                bool expectOn = (glyphRow & (1 << px)) != 0;
                uint32_t expectedColor = expectOn ? kGreen : kBlack;

                int fbX = px * 2;
                int fbY = py * 2;
                uint32_t actual = fb[fbY * kFbW + fbX];

                Assert::AreEqual (expectedColor, actual,
                    std::format (L"'A' pixel ({},{}) byte=${:02X} expect={}",
                        px, py, glyphRow, expectOn ? L"green" : L"black").c_str ());
            }
        }
    }

    TEST_METHOD (TextMode_InverseSpace_EntireCellGreen)
    {
        // $20 in inverse range. Space glyph = all zeros. Inverse flips all ON.
        std::vector<Byte> mem (0x10000, 0xA0);
        mem[0x0400] = 0x20;

        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        std::vector<uint32_t> fb (kFbW * kFbH, 0);
        textMode.Render (mem.data (), fb.data (), kFbW, kFbH);

        for (int y = 0; y < 16; y++)
        {
            for (int x = 0; x < 14; x++)
            {
                Assert::AreEqual (kGreen, fb[y * kFbW + x],
                    std::format (L"Inverse space ({},{}) must be green", x, y).c_str ());
            }
        }
    }

    TEST_METHOD (TextMode_NormalSpace_EntireCellBlack)
    {
        // $A0 normal space = all zeros, no inversion = all black.
        std::vector<Byte> mem (0x10000, 0xA0);

        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        std::vector<uint32_t> fb (kFbW * kFbH, kGreen);  // Pre-fill green
        textMode.Render (mem.data (), fb.data (), kFbW, kFbH);

        for (int y = 0; y < 16; y++)
        {
            for (int x = 0; x < 14; x++)
            {
                Assert::AreEqual (kBlack, fb[y * kFbW + x],
                    std::format (L"Normal space ({},{}) must be black", x, y).c_str ());
            }
        }
    }

    TEST_METHOD (TextMode_All24RowAddresses_Correct)
    {
        // Verify interleaved row addresses for ALL 24 rows
        Word expected[24];

        for (int r = 0; r < 24; r++)
        {
            expected[r] = static_cast<Word> (0x0400 + 128 * (r % 8) + 40 * (r / 8));
        }

        std::vector<Byte> mem (0x10000, 0xA0);

        for (int r = 0; r < 24; r++)
        {
            mem[expected[r]] = 0xC1;
        }

        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        std::vector<uint32_t> fb (kFbW * kFbH, 0);
        textMode.Render (mem.data (), fb.data (), kFbW, kFbH);

        for (int r = 0; r < 24; r++)
        {
            int fbY = r * 16;
            bool hasGreen = false;

            for (int y = fbY; y < fbY + 16 && !hasGreen; y++)
            {
                for (int x = 0; x < 14 && !hasGreen; x++)
                {
                    if (fb[y * kFbW + x] == kGreen)
                    {
                        hasGreen = true;
                    }
                }
            }

            Assert::IsTrue (hasGreen,
                std::format (L"Row {} (addr=${:04X}) must render 'A'",
                    r, expected[r]).c_str ());
        }
    }

    TEST_METHOD (TextMode_Page2_ReadsFrom0800)
    {
        std::vector<Byte> mem (0x10000, 0xA0);
        mem[0x0800] = 0xC1;

        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (true);

        std::vector<uint32_t> fb (kFbW * kFbH, 0);
        textMode.Render (mem.data (), fb.data (), kFbW, kFbH);

        bool hasGreen = false;

        for (int y = 0; y < 16 && !hasGreen; y++)
        {
            for (int x = 0; x < 14 && !hasGreen; x++)
            {
                if (fb[y * kFbW + x] == kGreen)
                {
                    hasGreen = true;
                }
            }
        }

        Assert::IsTrue (hasGreen, L"Page 2 should read from $0800");
    }

    TEST_METHOD (TextMode_Page2_DoesNotShowPage1Data)
    {
        std::vector<Byte> mem (0x10000, 0xA0);
        mem[0x0400] = 0xC1;  // 'A' on page 1 only

        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (true);

        std::vector<uint32_t> fb (kFbW * kFbH, kGreen);
        textMode.Render (mem.data (), fb.data (), kFbW, kFbH);

        for (int y = 0; y < 16; y++)
        {
            for (int x = 0; x < 14; x++)
            {
                Assert::AreEqual (kBlack, fb[y * kFbW + x],
                    L"Page 2 must NOT show page 1 data");
            }
        }
    }

    TEST_METHOD (LoRes_TopBottomNybbles_DifferentColors)
    {
        // $72: low nybble = 2 (dark blue top), high nybble = 7 (light blue bottom)
        std::vector<Byte> mem (0x10000, 0x00);
        mem[0x0400] = 0x72;

        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        AppleLoResMode lores (bus);
        lores.SetPage2 (false);

        std::vector<uint32_t> fb (kFbW * kFbH, 0);
        lores.Render (mem.data (), fb.data (), kFbW, kFbH);

        int blockH = kFbH / 48;

        Assert::AreEqual (kExpectedLoRes[2], fb[0],
            L"Low nybble 2 should render dark blue in top block");

        Assert::AreEqual (kExpectedLoRes[7], fb[blockH * kFbW],
            L"High nybble 7 should render light blue in bottom block");
    }

    TEST_METHOD (LoRes_All16Colors_MatchExpectedRGBA)
    {
        std::vector<Byte> mem (0x10000, 0x00);

        for (int c = 0; c < 16; c++)
        {
            mem[0x0400 + c] = static_cast<Byte> (c);
        }

        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        AppleLoResMode lores (bus);
        lores.SetPage2 (false);

        std::vector<uint32_t> fb (kFbW * kFbH, 0);
        lores.Render (mem.data (), fb.data (), kFbW, kFbH);

        int blockW = kFbW / 40;

        for (int c = 0; c < 16; c++)
        {
            int fbX = c * blockW;
            Assert::AreEqual (kExpectedLoRes[c], fb[fbX],
                std::format (L"Lo-res color {} should be ${:08X}", c, kExpectedLoRes[c]).c_str ());
        }
    }

    TEST_METHOD (LoRes_Page2_ReadsFrom0800)
    {
        std::vector<Byte> mem (0x10000, 0x00);
        mem[0x0800] = 0xFF;

        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        AppleLoResMode lores (bus);
        lores.SetPage2 (true);

        std::vector<uint32_t> fb (kFbW * kFbH, 0);
        lores.Render (mem.data (), fb.data (), kFbW, kFbH);

        Assert::AreEqual (kWhite, fb[0],
            L"Lo-res page 2 should read from $0800");
    }

    TEST_METHOD (HiRes_SingleByte_7PixelsDecoded)
    {
        // $55 = 01010101: bit7=0 (palette 0), bits 0,2,4,6 ON
        std::vector<Byte> mem (0x10000, 0x00);
        mem[0x2000] = 0x55;

        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        AppleHiResMode hires (bus);
        hires.SetPage2 (false);

        std::vector<uint32_t> fb (kFbW * kFbH, 0);
        hires.Render (mem.data (), fb.data (), kFbW, kFbH);

        // Bits 0,2,4,6 are ON (isolated pixels — no adjacent pair)
        for (int bit = 0; bit < 7; bit++)
        {
            bool expectOn = (0x55 & (1 << bit)) != 0;
            uint32_t pixel = fb[bit * 2];

            if (expectOn)
            {
                Assert::AreNotEqual (kNtscBlack, pixel,
                    std::format (L"Bit {} of $55 should be non-black", bit).c_str ());
            }
            else
            {
                Assert::AreEqual (kNtscBlack, pixel,
                    std::format (L"Bit {} of $55 should be black", bit).c_str ());
            }
        }
    }

    TEST_METHOD (HiRes_PaletteBit_ChangesColorGroup)
    {
        // Palette 0: even col = violet. Palette 1: even col = blue.
        std::vector<Byte> mem (0x10000, 0x00);
        mem[0x2000] = 0x01;  // Palette 0, bit 0 ON (even col 0)
        mem[0x2001] = 0x81;  // Palette 1, bit 0 ON (even col 7)

        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        AppleHiResMode hires (bus);
        hires.SetPage2 (false);

        std::vector<uint32_t> fb (kFbW * kFbH, 0);
        hires.Render (mem.data (), fb.data (), kFbW, kFbH);

        Assert::AreEqual (kNtscViolet, fb[0],
            L"Palette 0 even col should be violet");

        Assert::AreEqual (kNtscOrange, fb[7 * 2],
            L"Palette 1 odd col should be orange");
    }

    TEST_METHOD (HiRes_AdjacentPixels_RenderWhite)
    {
        std::vector<Byte> mem (0x10000, 0x00);
        mem[0x2000] = 0x03;

        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        AppleHiResMode hires (bus);
        hires.SetPage2 (false);

        std::vector<uint32_t> fb (kFbW * kFbH, 0);
        hires.Render (mem.data (), fb.data (), kFbW, kFbH);

        Assert::AreEqual (kNtscWhite, fb[0],
            L"Adjacent ON pixels should render white");
        Assert::AreEqual (kNtscWhite, fb[1 * 2],
            L"Both adjacent ON pixels should be white");
    }

    TEST_METHOD (HiRes_Page2_ReadsFrom4000)
    {
        std::vector<Byte> mem (0x10000, 0x00);
        mem[0x4000] = 0x03;

        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        AppleHiResMode hires (bus);
        hires.SetPage2 (true);

        std::vector<uint32_t> fb (kFbW * kFbH, 0);
        hires.Render (mem.data (), fb.data (), kFbW, kFbH);

        Assert::AreEqual (kNtscWhite, fb[0],
            L"Hi-res page 2 should read from $4000");
    }

    TEST_METHOD (TextMode_MixedMode_BottomRowAddressesValid)
    {
        // In mixed mode, bottom 4 text rows (20-23) show text.
        // Verify those row addresses are correct.
        Word expected20 = static_cast<Word> (0x0400 + 128 * (20 % 8) + 40 * (20 / 8));
        Word expected21 = static_cast<Word> (0x0400 + 128 * (21 % 8) + 40 * (21 / 8));
        Word expected22 = static_cast<Word> (0x0400 + 128 * (22 % 8) + 40 * (22 / 8));
        Word expected23 = static_cast<Word> (0x0400 + 128 * (23 % 8) + 40 * (23 / 8));

        Assert::AreEqual (static_cast<Word> (0x0650), expected20, L"Row 20 = $0650");
        Assert::AreEqual (static_cast<Word> (0x06D0), expected21, L"Row 21 = $06D0");
        Assert::AreEqual (static_cast<Word> (0x0750), expected22, L"Row 22 = $0750");
        Assert::AreEqual (static_cast<Word> (0x07D0), expected23, L"Row 23 = $07D0");
    }
};




////////////////////////////////////////////////////////////////////////////////
//
//  Phase 12 Golden Hash Tests (T111)
//
//  Deterministic test patterns hashed with FNV-1a-64. The golden constants
//  are baked into the test; if rendering changes, these tests fail and the
//  expected hashes must be updated intentionally.
//
////////////////////////////////////////////////////////////////////////////////

namespace Phase12GoldenHelpers
{
    static uint64_t Fnv1a64 (const uint32_t* data, size_t count)
    {
        uint64_t h = 0xcbf29ce484222325ULL;

        for (size_t i = 0; i < count; i++)
        {
            uint32_t v = data[i];

            for (int b = 0; b < 4; b++)
            {
                uint8_t byte = static_cast<uint8_t> ((v >> (b * 8)) & 0xFF);
                h ^= byte;
                h *= 0x100000001b3ULL;
            }
        }
        return h;
    }

    static uint32_t Prng (uint32_t& state)
    {
        state = state * 1664525u + 1013904223u;
        return state;
    }
}


TEST_CLASS (Phase12GoldenHashTests)
{
public:

    TEST_METHOD (DhrTestPattern_HashMatches_Golden)
    {
        // Deterministic DHR pattern from PRNG seed 0xCA550001.
        uint32_t seed = 0xCA550001u;
        std::vector<Byte> aux (0x10000, 0x00);
        std::vector<Byte> main (0x10000, 0x00);

        for (int row = 0; row < 192; row++)
        {
            Word base = static_cast<Word> (
                0x2000 +
                ((row & 7) << 10) +
                (((row >> 3) & 7) << 7) +
                ((row >> 6) * 40));

            for (int col = 0; col < 40; col++)
            {
                aux[base + col]  = static_cast<Byte> (Phase12GoldenHelpers::Prng (seed) & 0x7F);
                main[base + col] = static_cast<Byte> (Phase12GoldenHelpers::Prng (seed) & 0x7F);
            }
        }

        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        AppleDoubleHiResMode dhr (bus);
        dhr.SetAuxMemory (aux.data ());
        dhr.SetPage2 (false);

        std::vector<uint32_t> fb (kFbW * kFbH, 0);
        dhr.Render (main.data (), fb.data (), kFbW, kFbH);

        uint64_t hash = Phase12GoldenHelpers::Fnv1a64 (fb.data (), fb.size ());

        // Golden hash captured from initial deterministic render.
        // Updated 2026-05-13 alongside the DHR palette byte-layout fix
        // (4 entries — Magenta, Dark Blue, Light Blue, Brown — were
        // stored with R/B bytes swapped relative to the R8G8B8A8 swap-
        // chain format and rendered as red shades on screen).
        //
        // Updated 2026-05-14 alongside the swap-chain format change
        // from R8G8B8A8_UNORM to B8G8R8A8_UNORM (see
        // Video/PixelFormat.h). The on-screen colors are identical;
        // the framebuffer byte sequence differs for any pixel with
        // R != B, which moves the hash.
        constexpr uint64_t kExpected = 0x56CE5AB7DF017725ULL;

        Assert::AreEqual (kExpected, hash,
            std::format (L"DHR golden hash mismatch: got 0x{:016X}", hash).c_str ());
    }

    TEST_METHOD (Mixed80Col_TestProgram_HashMatches_Golden)
    {
        // Deterministic 80-col text pattern: aux + main interleaved bytes
        // generated from a fixed PRNG seed. Hash the rendered framebuffer.
        uint32_t seed = 0xCA550001u;
        std::vector<Byte> auxBuf (0x10000, 0xA0);

        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        for (int row = 0; row < 24; row++)
        {
            Word rowBase = static_cast<Word> (0x0400 + 128 * (row % 8) + 40 * (row / 8));

            for (int col = 0; col < 40; col++)
            {
                Byte a = static_cast<Byte> (0x80 | (Phase12GoldenHelpers::Prng (seed) & 0x7F));
                Byte m = static_cast<Byte> (0x80 | (Phase12GoldenHelpers::Prng (seed) & 0x7F));
                auxBuf[rowBase + col] = a;
                bus.WriteByte (rowBase + col, m);
            }
        }

        Apple80ColTextMode text80 (bus);
        text80.SetAuxMemory (auxBuf.data ());
        text80.SetAltCharSet (false);
        text80.SetFlashState (true);

        std::vector<uint32_t> fb (kFbW * kFbH, 0);
        text80.Render (nullptr, fb.data (), kFbW, kFbH);

        uint64_t hash = Phase12GoldenHelpers::Fnv1a64 (fb.data (), fb.size ());

        // Golden hash captured from initial deterministic render.
        constexpr uint64_t kExpected = 0x67F023CDC3099DA5ULL;

        Assert::AreEqual (kExpected, hash,
            std::format (L"80-col golden hash mismatch: got 0x{:016X}", hash).c_str ());
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  IIeRom_*_InverseSpace_RendersAsSolidBlock
    //
    //  Regression for the missing 80-col cursor: the //e firmware
    //  draws the BASIC cursor as `$20` (inverse-space, expected to
    //  render as a solid block). The //e enhanced video ROM stores
    //  the inverse-range slots ($00-$3F, in BOTH primary and alt
    //  sets) already in their visual / pre-inverted bitmap form
    //  (UTAIIe Tables 8.2/8.3) -- so the renderer's XOR-inversion
    //  path must NOT fire for the //e ROM; doing so re-inverts the
    //  pre-inverted bitmap and produces an empty cell.
    //
    //  These tests load the real Apple2e_Video.rom via
    //  CharacterRomData::LoadFromFile (skipping cleanly on CI
    //  runners that don't provision Apple-owned ROMs) and assert
    //  that an inverse-space cell renders as solid green pixels in
    //  both 40-col and 80-col text modes.
    //
    ////////////////////////////////////////////////////////////////////////////

    static fs::path FindIIeCharRom()
    {
        fs::path  cursor = fs::current_path();

        for (int depth = 0; depth < 8; depth++)
        {
            fs::path  candidate = cursor / "ROMs" / "Apple2e_Video.rom";

            if (fs::exists (candidate))
            {
                return candidate;
            }

            if (cursor.parent_path() == cursor)
            {
                break;
            }

            cursor = cursor.parent_path();
        }

        return {};
    }

    TEST_METHOD (IIeRom_AppleTextMode_InverseSpace_RendersSolidBlock)
    {
        fs::path romPath = FindIIeCharRom();

        if (romPath.empty())
        {
            Logger::WriteMessage ("SKIPPED: ROMs/Apple2e_Video.rom "
                                  "not present (CI runners do not provision "
                                  "Apple-owned ROMs).\n");
            return;
        }

        CharacterRomData rom;
        HRESULT          hrLoad = rom.LoadFromFile (romPath.string());
        Assert::IsTrue (SUCCEEDED (hrLoad), L"Must load Apple2e_Video.rom");
        Assert::IsTrue (rom.HasAltCharSet(),
            L"4K //e ROM must register as having alt-set");

        // Place inverse-space at column 0 of row 0.
        std::vector<Byte> mem (0x10000, 0xA0);
        mem[0x0400] = 0x20;

        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        AppleTextMode textMode (bus, rom);
        textMode.SetPage2     (false);
        textMode.SetAltCharSet (false);

        std::vector<uint32_t> fb (kFbW * kFbH, 0);
        textMode.Render (mem.data(), fb.data(), kFbW, kFbH);

        // 40-col cell origin: 14px wide, 16px tall (each char doubled).
        for (int y = 0; y < 16; y++)
        {
            for (int x = 0; x < 14; x++)
            {
                Assert::AreEqual (kGreen, fb[y * kFbW + x],
                    std::format (L"//e inverse-space at ({},{}) must be green "
                                 L"(would be black if XOR-inversion ran "
                                 L"against pre-inverted ROM byte)", x, y).c_str());
            }
        }
    }

    TEST_METHOD (IIeRom_Apple80ColTextMode_InverseSpace_RendersSolidBlock)
    {
        fs::path romPath = FindIIeCharRom();

        if (romPath.empty())
        {
            Logger::WriteMessage ("SKIPPED: ROMs/Apple2e_Video.rom "
                                  "not present (CI runners do not provision "
                                  "Apple-owned ROMs).\n");
            return;
        }

        CharacterRomData rom;
        HRESULT          hrLoad = rom.LoadFromFile (romPath.string());
        Assert::IsTrue (SUCCEEDED (hrLoad), L"Must load Apple2e_Video.rom");

        // Mirror the //e PR#3 cursor placement: prompt ']' ($DD) at
        // aux $0480 (col 0 of row 1), inverse-space cursor ($20) at
        // main $0480 (col 1 of row 1).
        std::vector<Byte> mainMem (0x10000, 0xA0);
        std::vector<Byte> auxMem  (0x10000, 0xA0);
        mainMem[0x0480] = 0x20;
        auxMem [0x0480] = 0xDD;

        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        Apple80ColTextMode text80 (bus, rom);
        text80.SetAuxMemory  (auxMem.data());
        text80.SetAltCharSet (true);

        std::vector<uint32_t> fb (kFbW * kFbH, 0);
        text80.Render (mainMem.data(), fb.data(), kFbW, kFbH);

        // 80-col cell origin: 7px wide. Row 1 in framebuffer:
        // y = 1 * 8 * 2 .. 1 * 8 * 2 + 15 = 16..31.
        // Col 1 in framebuffer: x = 1 * 7 .. 1 * 7 + 6 = 7..13.
        constexpr int kCellW = 7;
        constexpr int kCellH = 16;
        constexpr int kRow   = 16;
        constexpr int kCol   = 7;

        for (int y = 0; y < kCellH; y++)
        {
            for (int x = 0; x < kCellW; x++)
            {
                Assert::AreEqual (kGreen, fb[(kRow + y) * kFbW + (kCol + x)],
                    std::format (L"80-col cursor cell ({},{}) must be green",
                                 kCol + x, kRow + y).c_str());
            }
        }
    }
};
