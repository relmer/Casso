#include "Pch.h"
#include "Core/NumberFormat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NumberFormatTests
//
//  Numbers written with the user's regional settings. Each comparison is
//  built from the settings read here, for any region.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (NumberFormatTests)
{
public:

    static std::wstring  ReadSetting (LCTYPE type)
    {
        wchar_t  value[16] = {};



        (void) GetLocaleInfoEx (LOCALE_NAME_USER_DEFAULT, type, value, (int) std::size (value));

        return value;
    }


    TEST_METHOD (IntegersAreGroupedWithTheRegionalSeparator)
    {
        std::wstring  separator = ReadSetting (LOCALE_STHOUSAND);



        Assert::AreEqual (std::wstring (L"999"), NumberFormat::FormatInteger (999));
        Assert::AreEqual (separator, NumberFormat::GetThousandsSeparator());

        if (ReadSetting (LOCALE_SGROUPING) == L"3;0")
        {
            Assert::AreEqual (L"51" + separator + L"007",                     NumberFormat::FormatInteger (51007));
            Assert::AreEqual (L"1" + separator + L"000" + separator + L"000", NumberFormat::FormatInteger (1000000));
        }
    }


    TEST_METHOD (FixedValuesUseTheRegionalDecimalSeparator)
    {
        std::wstring  decimal = ReadSetting (LOCALE_SDECIMAL);
        std::wstring  plain   = NumberFormat::FormatFixed (1234.5, 2, false);



        Assert::AreEqual (L"1234" + decimal + L"50", plain);
        Assert::IsTrue   (NumberFormat::FormatFixed (-2.0, 1, true).find (L"2" + decimal + L"0") != std::wstring::npos);
    }


    TEST_METHOD (SeparatorsAreRemovedForParsing)
    {
        Assert::AreEqual (std::wstring (L"1234567"), NumberFormat::RemoveThousandsSeparators (NumberFormat::FormatInteger (1234567)));
        Assert::AreEqual (std::wstring (L"1234567"), NumberFormat::RemoveThousandsSeparators (L"1,234,567"));
    }
};
