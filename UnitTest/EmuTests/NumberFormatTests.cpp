#include "Pch.h"
#include "Core/NumberFormat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormatTests
//
//  Numbers written with fixed regional settings: English (United States),
//  German (Germany), Hindi (India) and a negative order in parentheses. The
//  test module fixes English (United States) for every other test.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (NumberFormatTests)
{
public:

    static NumberSettings  MakeGerman()
    {
        NumberSettings  settings;



        settings.thousands = L".";
        settings.decimal   = L",";

        return settings;
    }


    TEST_METHOD (IntegersAreGroupedBySettings)
    {
        NumberSettings  english;
        NumberSettings  german = MakeGerman();
        NumberSettings  hindi;
        NumberSettings  once;



        hindi.grouping = 32;
        once.grouping  = 30;

        Assert::AreEqual (std::wstring (L"999"),         NumberFormat::FormatInteger (999, english));
        Assert::AreEqual (std::wstring (L"51,007"),      NumberFormat::FormatInteger (51007, english));
        Assert::AreEqual (std::wstring (L"1,000,000"),   NumberFormat::FormatInteger (1000000, english));
        Assert::AreEqual (std::wstring (L"47.790"),      NumberFormat::FormatInteger (47790, german));
        Assert::AreEqual (std::wstring (L"1,23,45,678"), NumberFormat::FormatInteger (12345678, hindi));
        Assert::AreEqual (std::wstring (L"12345,678"),   NumberFormat::FormatInteger (12345678, once));
        Assert::AreEqual (std::wstring (L"-1,234"),      NumberFormat::FormatInteger (-1234, english));
    }


    TEST_METHOD (FixedValuesUseTheDecimalSeparatorAndNegativeOrder)
    {
        NumberSettings  english;
        NumberSettings  german = MakeGerman();
        NumberSettings  bare;
        NumberSettings  parens;



        bare.leadingZero     = false;
        parens.negativeOrder = 0;

        Assert::AreEqual (std::wstring (L"1234.50"),  NumberFormat::FormatFixed (1234.5, 2, false, english));
        Assert::AreEqual (std::wstring (L"1,234.50"), NumberFormat::FormatFixed (1234.5, 2, true,  english));
        Assert::AreEqual (std::wstring (L"1.234,50"), NumberFormat::FormatFixed (1234.5, 2, true,  german));
        Assert::AreEqual (std::wstring (L".5"),       NumberFormat::FormatFixed (0.5, 1, false, bare));
        Assert::AreEqual (std::wstring (L"(2.0)"),    NumberFormat::FormatFixed (-2.0, 1, false, parens));
        Assert::AreEqual (std::wstring (L"0.0"),      NumberFormat::FormatFixed (-0.01, 1, false, english), L"zero isn't negative");
    }


    TEST_METHOD (TheModuleFixesEnglishAndSeparatorsAreRemovedForParsing)
    {
        Assert::AreEqual (std::wstring (L"1,234,567"), NumberFormat::FormatInteger (1234567));
        Assert::AreEqual (std::wstring (L"1234567"),   NumberFormat::RemoveThousandsSeparators (L"1,234,567"));
    }
};
