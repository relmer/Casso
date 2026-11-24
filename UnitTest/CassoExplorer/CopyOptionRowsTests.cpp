#include "Pch.h"
#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoExplorer/Model/CopyOptionRows.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CopyOptionRowsTests
//
//  The rows Copy to folder with options has for a selection's files, their
//  choices read from and written to a DecodeOptions, and the choices saved
//  with Casso Explorer's settings.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CopyOptionRowsTests)
{
public:

    TEST_METHOD (Add_ARowEachKind_InOrder)
    {
        using Row      = CopyOptionRows::Row;
        using Encoding = DiskOperations::Encoding;

        std::vector<Row>  rows;



        CopyOptionRows::Add (FileFormat::None,              Encoding::Basic,    false, rows);
        CopyOptionRows::Add (FileFormat::HiResScreen,       Encoding::Verbatim, true,  rows);
        CopyOptionRows::Add (FileFormat::None,              Encoding::Text,     false, rows);
        CopyOptionRows::Add (FileFormat::None,              Encoding::Basic,    false, rows);
        CopyOptionRows::Add (FileFormat::None,              Encoding::Verbatim, true,  rows);
        CopyOptionRows::Add (FileFormat::AppleWorksWp,      Encoding::Verbatim, false, rows);

        Assert::IsTrue (rows == std::vector<Row> { Row::Charset, Row::HiRes, Row::Basic, Row::Code },
                        L"a picture binary is a picture, not code, and a format without options doesn't add a row");
    }


    TEST_METHOD (Choices_AreReadBackAsWritten)
    {
        using Row = CopyOptionRows::Row;

        DecodeOptions  options;



        CopyOptionRows::SetChoice (Row::Basic,       2, options);
        CopyOptionRows::SetChoice (Row::Code,        4, options);
        CopyOptionRows::SetChoice (Row::DoubleHiRes, 3, options);
        CopyOptionRows::SetChoice (Row::HiRes,       1, options);

        Assert::IsTrue   (options.basicColor && !options.basicRawControls);
        Assert::IsTrue   (options.disassembly == DisassemblyCpu::Wdc65816);
        Assert::IsTrue   (options.doubleHiRes == DoubleHiResForm::WindowedColor);
        Assert::IsTrue   (options.hiResMono);
        Assert::AreEqual (2, CopyOptionRows::GetChoice (Row::Basic, options));
        Assert::AreEqual (4, CopyOptionRows::GetChoice (Row::Code,  options));

        CopyOptionRows::SetChoice (Row::Basic, 1, options);

        Assert::IsTrue (!options.basicColor && options.basicRawControls);
    }


    TEST_METHOD (CopyOptions_AreSavedWithTheSettings)
    {
        CassoExplorerPrefs  saved;
        CassoExplorerPrefs  loaded;



        saved.copyOptions.charset       = Charset::MacOsRoman;
        saved.copyOptions.basicColor    = true;
        saved.copyOptions.disassembly   = DisassemblyCpu::Wdc65C02;
        saved.copyOptions.followFlags   = false;
        saved.copyOptions.doubleHiRes   = DoubleHiResForm::SimpleColor;

        AssertSucceeded (loaded.FromJson (saved.ToJson()));

        Assert::IsTrue (loaded.copyOptions.charset     == Charset::MacOsRoman);
        Assert::IsTrue (loaded.copyOptions.basicColor);
        Assert::IsTrue (loaded.copyOptions.disassembly == DisassemblyCpu::Wdc65C02);
        Assert::IsTrue (!loaded.copyOptions.followFlags);
        Assert::IsTrue (loaded.copyOptions.doubleHiRes == DoubleHiResForm::SimpleColor);
    }
};
