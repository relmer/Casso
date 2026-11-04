#include "Pch.h"
#include "../EhmTestHelper.h"

#include "CassoExplorer/Model/WozProperties.h"
#include "Machines/Apple2/Common/WozCompatibility.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WozPropertiesTests)
{
public:

    TEST_METHOD (MachineCodes_RoundTrip_InTheReferencesOrder)
    {
        uint16_t  machines = WozCompatibility::kApple2c | WozCompatibility::kApple2 | WozCompatibility::kApple2eEnhanced;

        Assert::AreEqual (std::string ("2|2c|2e+"), WozProperties::FormatMachineCodes (machines));
        Assert::AreEqual (machines, WozProperties::ParseMachineCodes ("2|2c|2e+"));
    }


    TEST_METHOD (ParseMachineCodes_UnknownCode_SetsNoBit)
    {
        Assert::AreEqual ((uint16_t) WozCompatibility::kApple2gs, WozProperties::ParseMachineCodes ("2gs|9x"));
        Assert::AreEqual ((uint16_t) 0, WozProperties::ParseMachineCodes (""));
    }


    TEST_METHOD (ToMetaValue_TabsAndLineBreaks_BecomeSpaces)
    {
        Assert::AreEqual (std::string ("Side A of the disk"), WozProperties::ToMetaValue (L"  Side A\tof\r\nthe disk "));
        Assert::AreEqual (std::string(),                      WozProperties::ToMetaValue (L" \t "));
    }


    TEST_METHOD (ToMetaValue_IsUtf8)
    {
        Assert::AreEqual (std::string ("Br\xC3\xB8" "derbund"), WozProperties::ToMetaValue (L"Br\x00F8" L"derbund"));
    }


    TEST_METHOD (MakeMeta_StandardKeysInOrder_ThenOthers_WithoutEmptyValues)
    {
        std::vector<WozLoader::MetaField>  meta = WozProperties::MakeMeta ({ { "publisher", L"Broderbund" }, { "x_dumper", L"4am" },
                                                                             { "title",     L"Karateka" },   { "notes",    L"" } });

        Assert::AreEqual (size_t (3), meta.size());
        Assert::AreEqual (std::string ("title"),     meta[0].key);
        Assert::AreEqual (std::string ("publisher"), meta[1].key);
        Assert::AreEqual (std::string ("x_dumper"),  meta[2].key);
        Assert::AreEqual (std::string ("4am"),       meta[2].value);
    }


    TEST_METHOD (FormatEditError_EachRefusal_HasItsOwnReason)
    {
        std::wstring  crc     = WozProperties::FormatEditError (HRESULT_FROM_WIN32 (ERROR_CRC));
        std::wstring  order   = WozProperties::FormatEditError (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));
        std::wstring  denied  = WozProperties::FormatEditError (E_ACCESSDENIED);

        Assert::IsTrue (crc.find (L"CRC") != std::wstring::npos);
        Assert::IsTrue (order.find (L"in place") != std::wstring::npos);
        Assert::IsTrue (denied.starts_with (L"The changes weren't written: "));
        Assert::IsTrue (denied.size() > wcslen (L"The changes weren't written: "), L"with the system's reason");
    }
};
