#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/Model/AppleTextFont.h"
#include "CassoExplorer/Model/AppleTextLayout.h"
#include "CassoExplorer/CassoExplorerWindow.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextTests
//
//  An Apple text preview's character set, from the video ROMs under an
//  in-memory asset directory, and its lines wrapped to the pane's columns.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleTextTests)
{
public:

    static constexpr const wchar_t *  kAssets = L"C:\\Assets";



    //  Copies a ROM fixture to where Casso installs it for a machine.
    static void Install (InMemoryFileSystem & fs, const char * fixture, const wchar_t * relative)
    {
        FixtureProvider    fixtures;
        std::vector<Byte>  bytes;

        AssertSucceeded (fixtures.OpenFixture (fixture, bytes));
        AssertSucceeded (fs.WriteAllText (std::wstring (kAssets) + L"\\" + relative, std::string (bytes.begin(), bytes.end())));
    }



    //  Whether the source pixels of a character cell match a glyph's dots,
    //  each dot two pixels across and two down.
    static bool IsGlyphDrawn (const std::vector<uint32_t> & bgra, int width, int column, const CharacterRomData & rom, Byte code)
    {
        bool  same = true;

        for (int y = 0; y < AppleTextFont::kCellHeight; y++)
        {
            for (int x = 0; x < AppleTextFont::kCellWidth; x++)
            {
                bool      lit   = (rom.GetGlyphRow (code, y / 2) & (1 << (x / 2))) != 0;
                uint32_t  pixel = bgra[(size_t) y * (size_t) width + (size_t) (column * AppleTextFont::kCellWidth + x)];

                same = same && pixel == (lit ? AppleTextFont::kLitArgb : AppleTextFont::kUnlitArgb);
            }
        }

        return same;
    }



    TEST_METHOD (Load_TakesTheEnhancedRomFirst)
    {
        InMemoryFileSystem  fs;
        AppleTextFont       font;

        Install (fs, "Apple2_Video.rom",          AppleTextFont::kRomPaths[3]);
        Install (fs, "Apple2eEnhanced_Video.rom", AppleTextFont::kRomPaths[0]);

        font.Load (fs, kAssets);

        Assert::AreEqual (std::wstring (kAssets) + L"\\" + AppleTextFont::kRomPaths[0], font.GetSource());
        Assert::IsTrue   (font.HasLowercase());
        Assert::AreEqual ((Byte) 0xE1, font.GetCode (L'a'), L"Lowercase is the normal set's own");
        Assert::AreEqual ((Byte) 0xC1, font.GetCode (L'A'));
        Assert::AreEqual ((Byte) 0xFB, font.GetCode (L'{'));
    }



    TEST_METHOD (Load_WithOnlyASmallRom_FoldsLowercase)
    {
        InMemoryFileSystem  fs;
        AppleTextFont       font;

        Install (fs, "Apple2_Video.rom", AppleTextFont::kRomPaths[3]);

        font.Load (fs, kAssets);

        Assert::AreEqual (std::wstring (kAssets) + L"\\" + AppleTextFont::kRomPaths[3], font.GetSource());
        Assert::IsFalse  (font.HasLowercase());
        Assert::AreEqual ((Byte) 0xC1, font.GetCode (L'a'), L"Lowercase is drawn as uppercase");
        Assert::AreEqual ((Byte) 0xDB, font.GetCode (L'{'), L"and the characters above it as those below");
        Assert::AreEqual ((Byte) 0xC0, font.GetCode (L'`'));
    }



    TEST_METHOD (Load_WithNoRom_UsesTheBuiltInTable)
    {
        InMemoryFileSystem     fs;
        AppleTextFont          font;
        CharacterRomData       builtIn;
        std::vector<uint32_t>  bgra;
        int                    width  = 0;
        int                    height = 0;

        AssertSucceeded (fs.WriteAllText (std::wstring (kAssets) + L"\\" + AppleTextFont::kRomPaths[0], "not a ROM"));

        font.Load (fs, kAssets);
        font.Render ({ L"A" }, 0, 1, 1, bgra, width, height);

        Assert::IsTrue  (font.GetSource().empty(), L"A file of the wrong size is passed over");
        Assert::IsFalse (font.HasLowercase());
        Assert::IsTrue  (IsGlyphDrawn (bgra, width, 0, builtIn, 0xC1));
    }



    TEST_METHOD (GetCode_DrawsAnythingUnprintableAsAQuestionMark)
    {
        AppleTextFont  font;

        Assert::AreEqual ((Byte) 0xBF, font.GetCode (L'\x7F'));
        Assert::AreEqual ((Byte) 0xBF, font.GetCode (L'\t'));
        Assert::AreEqual ((Byte) 0xBF, font.GetCode (L'\x00E9'));
        Assert::AreEqual ((Byte) 0xA0, font.GetCode (L' '));
    }



    TEST_METHOD (Render_DrawsEachCharacterFromTheRom)
    {
        InMemoryFileSystem     fs;
        AppleTextFont          font;
        FixtureProvider        fixtures;
        std::vector<Byte>      bytes;
        CharacterRomData       rom;
        std::vector<uint32_t>  bgra;
        int                    width  = 0;
        int                    height = 0;

        Install (fs, "Apple2eEnhanced_Video.rom", AppleTextFont::kRomPaths[0]);
        AssertSucceeded (fixtures.OpenFixture ("Apple2eEnhanced_Video.rom", bytes));
        AssertSucceeded (rom.LoadFromMemory (bytes.data(), bytes.size()));

        font.Load (fs, kAssets);
        font.Render ({ L"skip", L"Hi", L"after" }, 1, 1, 3, bgra, width, height);

        Assert::AreEqual (3 * AppleTextFont::kCellWidth, width, L"As wide as the columns");
        Assert::AreEqual (AppleTextFont::kCellHeight, height, L"and as tall as the row count");
        Assert::IsTrue   (IsGlyphDrawn (bgra, width, 0, rom, 0xC8), L"H");
        Assert::IsTrue   (IsGlyphDrawn (bgra, width, 1, rom, 0xE9), L"i, in lowercase");

        for (int y = 0; y < height; y++)
        {
            for (int x = 2 * AppleTextFont::kCellWidth; x < width; x++)
            {
                Assert::AreEqual (AppleTextFont::kUnlitArgb, bgra[(size_t) y * (size_t) width + (size_t) x], L"Past the row's end is black");
            }
        }
    }



    TEST_METHOD (Wrap_ContinuesALongLineOnTheNextRow)
    {
        std::vector<std::wstring>  rows = AppleTextLayout::Wrap ({ L"10 PRINT", L"", L"20 END" }, 4);

        Assert::AreEqual ((size_t) 5, rows.size());
        Assert::AreEqual (std::wstring (L"10 P"), rows[0]);
        Assert::AreEqual (std::wstring (L"RINT"), rows[1]);
        Assert::AreEqual (std::wstring(),       rows[2], L"An empty line is one empty row");
        Assert::AreEqual (std::wstring (L"20 E"), rows[3]);
        Assert::AreEqual (std::wstring (L"ND"),   rows[4]);
        Assert::AreEqual ((size_t) 6, AppleTextLayout::Wrap ({ L"ABCDEF" }, 0).size(), L"Fewer than one column is one");

        rows = AppleTextLayout::MakeScreenRows ({ L"10 PRINT" }, 6);
        Assert::IsTrue (rows == std::vector<std::wstring> ({ L"", L" 10 P", L" RINT", L"" }),
                        L"A blank row above and below, and a blank column each side");
    }



    //  A listing's lines with their addresses when set, a warning after a
    //  blank line, and the copy of them all with Windows line breaks.
    TEST_METHOD (Lines_HaveTheirAddressesAndWarningAndCopyWhole)
    {
        PreviewContent             preview;
        std::vector<std::wstring>  lines;

        preview.kind      = PreviewContent::Kind::Listing;
        preview.appleText = true;
        preview.lines     = { L"10  END" };
        preview.bytes     = { 0x07, 0x08, 0x0A, 0x00, 0x80, 0x00, 0x00, 0x00 };
        preview.warning   = L"Cut off.";

        lines = CassoExplorerWindow::GetAppleTextLines (preview, false);
        Assert::IsTrue (lines == std::vector<std::wstring> ({ L"10  END", L"", L"Cut off." }));

        lines = CassoExplorerWindow::GetAppleTextLines (preview, true);
        Assert::AreEqual (std::wstring (L"$0801 10  END"), lines[0], L"The line's address leads it");

        Assert::AreEqual (std::wstring (L"10  END\r\n\r\nCut off.\r\n"),
                          CassoExplorerWindow::JoinAppleText (CassoExplorerWindow::GetAppleTextLines (preview, false)));
    }
};
