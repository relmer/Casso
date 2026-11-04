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


    TEST_METHOD (MakeMeta_EveryStandardKeyInOrder_ThenOthersWithAValue)
    {
        std::vector<WozLoader::MetaField>  meta = WozProperties::MakeMeta ({ { "publisher", L"Broderbund" }, { "x_dumper", L"4am" },
                                                                             { "title",     L"Karateka" },   { "x_empty",  L"" } });
        size_t                             keys = WozProperties::GetMetaKeys().size();

        Assert::AreEqual (keys + 1, meta.size(), L"the standard keys and the one other key with a value");
        Assert::AreEqual (std::string ("title"),     meta[0].key);
        Assert::AreEqual (std::string ("Karateka"),  meta[0].value);
        Assert::AreEqual (std::string ("subtitle"),  meta[1].key);
        Assert::AreEqual (std::string(),             meta[1].value, L"a standard key with no value is written empty");
        Assert::AreEqual (std::string ("publisher"), meta[2].key);
        Assert::AreEqual (std::string ("x_dumper"),  meta[keys].key);
    }


    TEST_METHOD (MakeMeta_ListKeys_JoinTheirLinesWithBars)
    {
        std::vector<WozLoader::MetaField>  meta = WozProperties::MakeMeta ({ { "developer", L"David Balsam\n\nMartin Kahn\n" }, { "requires_machine", L"2\n2+" },
                                                                             { "genre",     L"action\nstrategy" } });

        auto  find = [&meta] (const char * key) { return std::find_if (meta.begin(), meta.end(), [key] (const WozLoader::MetaField & f) { return f.key == key; })->value; };

        Assert::AreEqual (std::string ("David Balsam|Martin Kahn"), find ("developer"), L"blank lines left out");
        Assert::AreEqual (std::string ("2|2+"),                     find ("requires_machine"));
        Assert::AreEqual (std::string ("action|strategy"),          find ("genre"), L"another key's lines too");
    }


    TEST_METHOD (ToMetaValue_DropsBars)
    {
        Assert::AreEqual (std::string ("AB"), WozProperties::ToMetaValue (L"A|B"));
    }


    TEST_METHOD (SplitList_IsAnItemAValue)
    {
        std::vector<std::wstring>  items = WozProperties::SplitList (L"Dane Bigham|Gene Portwood|Lauren Elliott");

        Assert::AreEqual (size_t (3), items.size());
        Assert::AreEqual (std::wstring (L"Lauren Elliott"), items[2]);
        Assert::IsTrue   (WozProperties::SplitList (L"").empty());
    }


    TEST_METHOD (GetKeyLabel_StandardKnownAndOther)
    {
        Assert::AreEqual (std::wstring (L"Side name"),    WozProperties::GetKeyLabel ("side_name"));
        Assert::AreEqual (std::wstring (L"Requires ROM"), WozProperties::GetKeyLabel ("requires_rom"));
        Assert::AreEqual (std::wstring (L"Box art"),      WozProperties::GetKeyLabel ("box_art"));
    }


    TEST_METHOD (GetLanguages_EnglishFirst_OtherLast_TheRestSorted)
    {
        const std::vector<std::wstring>  & languages = WozProperties::GetLanguages();

        Assert::AreEqual (std::wstring (L"English"), languages.front());
        Assert::AreEqual (std::wstring (L"Other"),   languages.back());
        Assert::IsTrue   (std::is_sorted (languages.begin() + 1, languages.end() - 1));
    }


    TEST_METHOD (ImageDate_TypedInLocalTime_RoundTrips)
    {
        std::string  stored = "2019-01-17T18:41:42.228Z";
        std::string  typed;

        Assert::IsTrue   (WozProperties::TryParseTypedImageDate (WozProperties::FormatImageDate (stored), typed));
        Assert::AreEqual (std::string ("2019-01-17T18:41:42.000Z"), typed, L"the same moment, without the fraction");
    }


    TEST_METHOD (ImageDate_NotADate_IsRefused_AndEmptyIsNone)
    {
        std::string  typed = "x";

        Assert::IsFalse  (WozProperties::TryParseTypedImageDate (L"next Tuesday-ish", typed));
        Assert::IsTrue   (WozProperties::TryParseTypedImageDate (L"  ", typed));
        Assert::AreEqual (std::string(), typed);
        Assert::AreEqual (std::wstring (L"sometime"), WozProperties::FormatImageDate ("sometime"), L"a stored value that isn't RFC 3339 is returned as stored");
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
