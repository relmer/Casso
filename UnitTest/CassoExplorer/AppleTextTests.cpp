#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/Model/AppleFontFile.h"
#include "CassoExplorer/Model/AppleTextFont.h"
#include "CassoExplorer/CassoExplorerWindow.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextTests
//
//  An Apple text preview's character set, from the video ROMs under an
//  in-memory asset directory, the TrueType font made from it as DirectWrite
//  reads it, and the lines of the preview.
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



    //  The font file as DirectWrite loads it, its one family's first face.
    static void LoadFace (const std::vector<Byte> & file, ComPtr<IDWriteFontFace> & outFace, std::wstring & outFamily)
    {
        ComPtr<IDWriteFactory5>                factory;
        ComPtr<IDWriteInMemoryFontFileLoader>  loader;
        ComPtr<IDWriteFontFile>                fontFile;
        ComPtr<IDWriteFontSetBuilder1>         builder;
        ComPtr<IDWriteFontSet>                 set;
        ComPtr<IDWriteFontCollection1>         collection;
        ComPtr<IDWriteFontFamily>              family;
        ComPtr<IDWriteFont>                    font;
        ComPtr<IDWriteLocalizedStrings>        names;
        wchar_t                                name[64] = {};

        AssertSucceeded (DWriteCreateFactory (DWRITE_FACTORY_TYPE_ISOLATED, __uuidof (IDWriteFactory5), (IUnknown **) factory.GetAddressOf()));
        AssertSucceeded (factory->CreateInMemoryFontFileLoader (&loader));
        AssertSucceeded (factory->RegisterFontFileLoader (loader.Get()));
        AssertSucceeded (loader->CreateInMemoryFontFileReference (factory.Get(), file.data(), (UINT32) file.size(), nullptr, &fontFile));
        AssertSucceeded (factory->CreateFontSetBuilder (&builder));
        AssertSucceeded (builder->AddFontFile (fontFile.Get()));
        AssertSucceeded (builder->CreateFontSet (&set));
        AssertSucceeded (factory->CreateFontCollectionFromFontSet (set.Get(), &collection));
        Assert::AreEqual (1u, collection->GetFontFamilyCount(), L"DirectWrite reads the file as one family");
        AssertSucceeded (collection->GetFontFamily (0, &family));
        AssertSucceeded (family->GetFamilyNames (&names));
        AssertSucceeded (names->GetString (0, name, (UINT32) std::size (name)));
        AssertSucceeded (family->GetFont (0, &font));
        AssertSucceeded (font->CreateFontFace (&outFace));

        outFamily = name;
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
        InMemoryFileSystem  fs;
        AppleTextFont       font;
        CharacterRomData    builtIn;

        AssertSucceeded (fs.WriteAllText (std::wstring (kAssets) + L"\\" + AppleTextFont::kRomPaths[0], "not a ROM"));

        font.Load (fs, kAssets);

        Assert::IsTrue  (font.GetSource().empty(), L"A file of the wrong size is passed over");
        Assert::IsFalse (font.HasLowercase());

        for (int row = 0; row < AppleTextFont::kDotsHigh; row++)
        {
            Assert::AreEqual (builtIn.GetGlyphRow (0xC1, row), font.GetGlyphRow (L'A', row));
        }
    }



    TEST_METHOD (GetCode_DrawsAnythingUnprintableAsAQuestionMark)
    {
        AppleTextFont  font;

        Assert::AreEqual ((Byte) 0xBF, font.GetCode (L'\x7F'));
        Assert::AreEqual ((Byte) 0xBF, font.GetCode (L'\t'));
        Assert::AreEqual ((Byte) 0xBF, font.GetCode (L'\x00E9'));
        Assert::AreEqual ((Byte) 0xA0, font.GetCode (L' '));
    }



    //  DirectWrite reads the file as one family whose glyph for each
    //  character spans the ROM's lit dots, a dot to a square of the grid.
    TEST_METHOD (FontFile_IsReadByDirectWriteWithTheRomsShapes)
    {
        InMemoryFileSystem       fs;
        AppleTextFont            font;
        std::vector<Byte>        file;
        ComPtr<IDWriteFontFace>  face;
        std::wstring             family;
        DWRITE_FONT_METRICS      metrics         = {};
        UINT32                   codes[3]        = { L'A', L'a', L' ' };
        UINT16                   glyphs[3]       = {};
        DWRITE_GLYPH_METRICS     glyphMetrics[3] = {};
        int                      left            = INT_MAX;
        int                      right           = INT_MIN;

        Install (fs, "Apple2eEnhanced_Video.rom", AppleTextFont::kRomPaths[0]);
        font.Load (fs, kAssets);

        AppleFontFile::Build (font, file);
        LoadFace (file, face, family);

        face->GetMetrics (&metrics);
        AssertSucceeded (face->GetGlyphIndices (codes, 3, glyphs));
        AssertSucceeded (face->GetDesignGlyphMetrics (glyphs, 3, glyphMetrics, FALSE));

        Assert::AreEqual (std::wstring (AppleFontFile::kFamily), family);
        Assert::AreEqual ((UINT16) AppleFontFile::kUnitsPerEm, metrics.designUnitsPerEm);
        Assert::AreEqual ((UINT16) AppleFontFile::kAscent, metrics.ascent);
        Assert::AreEqual ((UINT16) AppleFontFile::kDescent, metrics.descent);
        Assert::AreEqual ((UINT16) 96, face->GetGlyphCount(), L".notdef and the printable characters");
        Assert::AreEqual ((UINT16) (L'A' - L' ' + 1), glyphs[0]);
        Assert::AreNotEqual (glyphs[0], glyphs[1], L"Lowercase has glyphs of its own");

        for (int i = 0; i < 3; i++)
        {
            Assert::AreEqual ((UINT32) AppleFontFile::kAdvance, glyphMetrics[i].advanceWidth, L"Every character is one dot grid wide");
        }

        //  Every character's outline starts at its first lit dot and ends at
        //  its last, which a glyph offset by one would miss.
        for (wchar_t ch = L'!'; ch <= L'~'; ch++)
        {
            UINT32                code   = ch;
            UINT16                glyph  = 0;
            DWRITE_GLYPH_METRICS  metric = {};

            left  = INT_MAX;
            right = INT_MIN;

            for (int row = 0; row < AppleTextFont::kDotsHigh; row++)
            {
                for (int dot = 0; dot < AppleTextFont::kDotsWide; dot++)
                {
                    if ((font.GetGlyphRow (ch, row) & (1 << dot)) != 0)
                    {
                        left  = (std::min) (left, dot);
                        right = (std::max) (right, dot + 1);
                    }
                }
            }

            AssertSucceeded (face->GetGlyphIndices (&code, 1, &glyph));
            AssertSucceeded (face->GetDesignGlyphMetrics (&glyph, 1, &metric, FALSE));

            Assert::AreEqual (left * AppleFontFile::kUnitsPerDot, metric.leftSideBearing, std::format (L"{} starts at its first lit dot", ch).c_str());
            Assert::AreEqual ((AppleTextFont::kDotsWide - right) * AppleFontFile::kUnitsPerDot, metric.rightSideBearing, std::format (L"{} ends at its last", ch).c_str());
        }
    }



    //  A listing's lines with their addresses when set.
    TEST_METHOD (Lines_HaveTheirAddresses)
    {
        PreviewContent             preview;
        std::vector<std::wstring>  lines;

        preview.kind      = PreviewContent::Kind::Listing;
        preview.appleText = true;
        preview.lines     = { L"10  END" };
        preview.bytes     = { 0x07, 0x08, 0x0A, 0x00, 0x80, 0x00, 0x00, 0x00 };

        lines = CassoExplorerWindow::GetAppleTextLines (preview, false);
        Assert::IsTrue (lines == std::vector<std::wstring> ({ L"10  END" }));

        lines = CassoExplorerWindow::GetAppleTextLines (preview, true);
        Assert::AreEqual (std::wstring (L"$0801 10  END"), lines[0], L"The line's address leads it");

        //  In the text view, each line is one cell, the number in the line as
        //  on the Apple's screen, and the warning after a blank row.
        preview.warning = L"Cut off.";

        std::vector<DxuiTextView::Row>  rows = CassoExplorerWindow::BuildTextRows (preview, true);

        Assert::AreEqual ((size_t) 3, rows.size());
        Assert::IsTrue   (rows[0].cells == std::vector<std::wstring> ({ L"$0801 10  END" }));
        Assert::IsTrue   (rows[2].warning);
    }
};
