#include "Pch.h"

#include "Machines/Apple2/Common/CharacterRomData.h"

#include "../FixtureProvider.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


//  A 4K //e character generator holds 512 glyph cells of eight rows each.
static constexpr size_t  s_kGlyphBytes = 8;
static constexpr int     s_kGlyphRows  = 8;

//  The alternate set's $40-$5F. The unenhanced //e repeats the inverse
//  uppercase glyphs from $00-$1F here; the enhanced part replaces all 32
//  with MouseText.
static constexpr int  s_kAltRangeFirst = 0x40;
static constexpr int  s_kAltRangeCount = 0x20;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2eCharacterRomTests
//
//  Which character generator each //e is provisioned with.
//
//  The unenhanced //e uses 342-0133-A and the enhanced one 342-0265-A. The
//  two images are identical but for 32 glyphs, and those 32 are the whole
//  difference between a machine that has MouseText and one that does not.
//
//  Casso shipped the enhanced ROM for both, because AppleWin ships only that
//  one and Casso took its catalog from AppleWin's. Nothing caught it: the
//  renderer asks the ROM what to draw and drew what it was given, and no
//  test read that range, so a plain //e showed MouseText it never had.
//
//  These read the provisioned files rather than the rendered output on
//  purpose -- the defect was in which bytes arrived, and a renderer test
//  would have gone on passing against either one.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple2eCharacterRomTests)
{
public:

    TEST_METHOD (ThePlainIIeRepeatsItsInverseUppercaseInsteadOfMouseText)
    {
        std::vector<uint8_t>  rom = LoadBytes ("Apple2e_Video.rom");

        //  342-0133-A's tell: $40-$5F is a second copy of $00-$1F. The
        //  enhanced part has artwork there and matches nothing.
        for (int i = 0; i < s_kAltRangeCount; i++)
        {
            AssertGlyphsEqual (rom, s_kAltRangeFirst + i, i, true,
                std::format (L"the //e's alternate ${:02X} must repeat ${:02X} -- "
                             L"the machine is provisioned with 342-0133-A, not the "
                             L"enhanced 342-0265-A",
                             s_kAltRangeFirst + i, i));
        }
    }


    TEST_METHOD (TheEnhancedIIeReplacesThatRangeWithMouseText)
    {
        std::vector<uint8_t>  rom = LoadBytes ("Apple2eEnhanced_Video.rom");

        //  The other half of the pair. Without it, provisioning BOTH machines
        //  with the unenhanced part would satisfy the test above and quietly
        //  take MouseText away from the machine named for having it.
        for (int i = 0; i < s_kAltRangeCount; i++)
        {
            AssertGlyphsEqual (rom, s_kAltRangeFirst + i, i, false,
                std::format (L"the Enhanced //e's alternate ${:02X} must be MouseText, "
                             L"not a repeat of ${:02X}",
                             s_kAltRangeFirst + i, i));
        }
    }


    TEST_METHOD (TheTwoRomsAgreeOnEveryOtherGlyph)
    {
        CharacterRomData  plain    = Decode ("Apple2e_Video.rom");
        CharacterRomData  enhanced = Decode ("Apple2eEnhanced_Video.rom");

        //  Through the decoder this time, because this is the part that has
        //  to hold for the renderer: everything a program can print other
        //  than MouseText looks the same on both machines, so a difference
        //  here is a wrong file rather than a wrong model.
        for (int glyph = 0; glyph <= 0xFF; glyph++)
        {
            for (int row = 0; row < s_kGlyphRows; row++)
            {
                for (bool alt : { false, true })
                {
                    Byte  a = plain.GetGlyphRow    (static_cast<Byte> (glyph), row, alt);
                    Byte  b = enhanced.GetGlyphRow (static_cast<Byte> (glyph), row, alt);

                    if (a != b)
                    {
                        //  The MouseText range is the one place they may
                        //  differ, and the two tests above say how.
                        Assert::IsTrue (alt && glyph >= s_kAltRangeFirst &&
                                        glyph < s_kAltRangeFirst + s_kAltRangeCount,
                            std::format (L"glyph ${:02X} row {} (alt={}) differs between "
                                         L"the //e and Enhanced //e character ROMs, and "
                                         L"only MouseText may",
                                         glyph, row, alt).c_str());
                    }
                }
            }
        }
    }


private:

    static std::vector<uint8_t> LoadBytes (const char * fixtureName)
    {
        FixtureProvider       fixtures;
        std::vector<uint8_t>  bytes;
        HRESULT               hr = S_OK;

        hr = fixtures.OpenFixture (fixtureName, bytes);
        AssertSucceeded (hr, L"the character ROM fixture must be provisioned");

        Assert::AreEqual (size_t (4096), bytes.size(),
            L"a //e character generator is 4K");

        return (bytes);
    }


    static CharacterRomData Decode (const char * fixtureName)
    {
        std::vector<uint8_t>  bytes = LoadBytes (fixtureName);
        CharacterRomData      charRom;
        HRESULT               hr    = charRom.LoadFromMemory (bytes.data(), bytes.size());

        AssertSucceeded (hr, L"the character ROM must decode");
        Assert::IsTrue (charRom.HasAltCharSet(), L"a 4K //e ROM carries both sets");

        return (charRom);
    }


    static void AssertGlyphsEqual (const std::vector<uint8_t> & rom,
                                   int                          cellA,
                                   int                          cellB,
                                   bool                         expectEqual,
                                   const std::wstring         & message)
    {
        const uint8_t *  a     = rom.data() + cellA * s_kGlyphBytes;
        const uint8_t *  b     = rom.data() + cellB * s_kGlyphBytes;
        bool             equal = std::equal (a, a + s_kGlyphBytes, b);

        Assert::AreEqual (expectEqual, equal, message.c_str());
    }
};
