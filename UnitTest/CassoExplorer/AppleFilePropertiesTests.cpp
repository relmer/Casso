#include "Pch.h"
#include "CassoExplorer/Model/AppleFileProperties.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFilePropertiesTests
//
//  The Apple file properties dialog's rules, with no window.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleFilePropertiesTests)
{
public:

    TEST_METHOD (Dos33Types_ListTheSpareTypesOnlyForAFileWithOne)
    {
        auto  plain = AppleFileProperties::GetTypeChoices (VolumeKind::Dos33, 0x04);
        auto  spare = AppleFileProperties::GetTypeChoices (VolumeKind::Dos33, 0x40);

        Assert::AreEqual ((size_t) 6, plain.size());
        Assert::AreEqual (std::wstring (L"B    04  Binary"), plain[3].label);
        Assert::AreEqual (-1, AppleFileProperties::FindChoice (plain, 0x40));
        Assert::AreEqual (6,  AppleFileProperties::FindChoice (spare, 0x40));
        Assert::AreEqual (std::wstring (L"B    40  Binary (second B)"), spare[6].label);
    }



    TEST_METHOD (ProDosTypes_NameTheCommonOnesAndLeaveTheRestToHex)
    {
        auto  choices = AppleFileProperties::GetTypeChoices (VolumeKind::ProDos, 0x06);

        Assert::AreEqual (std::wstring (L"BIN  06  Binary"), choices[(size_t) AppleFileProperties::FindChoice (choices, 0x06)].label);
        Assert::AreEqual (-1, AppleFileProperties::FindChoice (choices, 0xC1), L"a type with no name is entered as hex");
    }



    TEST_METHOD (SecondField_FollowsTheType)
    {
        using Aux = AppleFileProperties::AuxKind;

        Assert::IsTrue (AppleFileProperties::GetAuxKind (VolumeKind::Dos33,  0x04) == Aux::LoadAddress);
        Assert::IsTrue (AppleFileProperties::GetAuxKind (VolumeKind::Dos33,  0x02) == Aux::None, L"DOS 3.3 stores an address in a binary only");
        Assert::IsTrue (AppleFileProperties::GetAuxKind (VolumeKind::ProDos, 0x06) == Aux::LoadAddress);
        Assert::IsTrue (AppleFileProperties::GetAuxKind (VolumeKind::ProDos, 0xFF) == Aux::LoadAddress);
        Assert::IsTrue (AppleFileProperties::GetAuxKind (VolumeKind::ProDos, 0x04) == Aux::RecordLength);
        Assert::IsTrue (AppleFileProperties::GetAuxKind (VolumeKind::ProDos, 0xFC) == Aux::AuxType);
    }



    TEST_METHOD (Hex_TakesUpToItsDigitsAndNoDollarSign)
    {
        unsigned  value = 0;

        Assert::IsTrue  (AppleFileProperties::TryParseHex (L"0803", 4, value));
        Assert::AreEqual (0x0803u, value);
        Assert::IsTrue  (AppleFileProperties::TryParseHex (L"c1", 2, value));
        Assert::AreEqual (0xC1u, value);
        Assert::IsFalse (AppleFileProperties::TryParseHex (L"$0803", 4, value));
        Assert::IsFalse (AppleFileProperties::TryParseHex (L"10000", 4, value));
        Assert::IsFalse (AppleFileProperties::TryParseHex (L"", 4, value));
        Assert::AreEqual (std::wstring (L"0803"), AppleFileProperties::FormatHex (0x803, 4));
    }



    TEST_METHOD (Dates_RoundTripInTheWindowsFormatAndKeepToProDossYears)
    {
        Word  date    = (Word) ((88 << 9) | (3 << 5) | 5);
        Word  time    = (Word) ((14 << 8) | 54);
        Word  outDate = 0;
        Word  outTime = 0;

        Assert::IsTrue  (AppleFileProperties::TryParseDate (AppleFileProperties::FormatDate (date, time), outDate, outTime));
        Assert::AreEqual (date, outDate);
        Assert::AreEqual (time, outTime);

        //  Years 00 to 39 are 2000 to 2039.
        date = (Word) ((12 << 9) | (6 << 5) | 1);
        Assert::IsTrue  (AppleFileProperties::TryParseDate (AppleFileProperties::FormatDate (date, 0), outDate, outTime));
        Assert::AreEqual (date, outDate);

        Assert::IsTrue  (AppleFileProperties::FormatDate (0, 0).empty(), L"a zero date formats as empty text");
        Assert::IsTrue  (AppleFileProperties::TryParseDate (L"", outDate, outTime) && outDate == 0, L"and an empty field is a zero date");
        Assert::IsFalse (AppleFileProperties::TryParseDate (L"1/1/2050", outDate, outTime), L"two-digit years end at 2039");
        Assert::IsFalse (AppleFileProperties::TryParseDate (L"not a date", outDate, outTime));
    }



    TEST_METHOD (Names_FollowEachFileSystemsRule)
    {
        Assert::IsTrue  (AppleFileProperties::IsValidName (VolumeKind::Dos33,  L"MY GAME/TWO"));
        Assert::IsFalse (AppleFileProperties::IsValidName (VolumeKind::Dos33,  L"A,B"));
        Assert::IsTrue  (AppleFileProperties::IsValidName (VolumeKind::ProDos, L"GAME.TWO"));
        Assert::IsFalse (AppleFileProperties::IsValidName (VolumeKind::ProDos, L"MY GAME"));
        Assert::IsFalse (AppleFileProperties::IsValidName (VolumeKind::ProDos, L"2GAME"));
    }
};
