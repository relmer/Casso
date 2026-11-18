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
        Assert::AreEqual (std::wstring (L"B\t04\tBinary"), plain[3].label);
        Assert::AreEqual (-1, AppleFileProperties::FindChoice (plain, 0x40));
        Assert::AreEqual (6,  AppleFileProperties::FindChoice (spare, 0x40));
        Assert::AreEqual (std::wstring (L"B\t40\tBinary (second B)"), spare[6].label);
    }



    TEST_METHOD (ProDosTypes_NameTheCommonOnesAndLeaveTheRestToHex)
    {
        auto  choices = AppleFileProperties::GetTypeChoices (VolumeKind::ProDos, 0x06);

        Assert::AreEqual (std::wstring (L"BIN\t06\tBinary"), choices[(size_t) AppleFileProperties::FindChoice (choices, 0x06)].label);
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



    //  Only the fields edited change: an access bit edited replaces that bit
    //  alone, and the second field goes only to a type that has one.
    TEST_METHOD (Attributes_CarryOnlyTheEdits)
    {
        FileEntry                   entry;
        AppleFileProperties::Edits  edits;
        FileAttributes              attributes;

        entry.type      = 0x06;
        entry.access    = 0xE3;
        entry.hasAccess = true;

        attributes = AppleFileProperties::MakeAttributes (entry, VolumeKind::ProDos, edits);
        Assert::IsFalse (attributes.hasType || attributes.hasAuxType || attributes.hasAccess || attributes.hasCreated || attributes.hasModified);

        edits.accessMask = AppleFileProperties::kAccessWrite;
        edits.access     = 0;
        attributes = AppleFileProperties::MakeAttributes (entry, VolumeKind::ProDos, edits);
        Assert::IsTrue   (attributes.hasAccess);
        Assert::AreEqual ((Byte) 0xE1, attributes.access, L"write cleared, the other bits as they were");

        edits            = AppleFileProperties::Edits();
        edits.typeEdited = true;
        edits.type       = 0xFF;
        edits.auxEdited  = true;
        edits.aux        = 0x2000;
        attributes = AppleFileProperties::MakeAttributes (entry, VolumeKind::ProDos, edits);
        Assert::IsTrue   (attributes.hasType && attributes.hasAuxType);
        Assert::AreEqual ((Word) 0x2000, attributes.auxType);

        //  A DOS 3.3 text file has no load address to set.
        edits.type = 0x00;
        attributes = AppleFileProperties::MakeAttributes (entry, VolumeKind::Dos33, edits);
        Assert::IsFalse (attributes.hasAuxType);

        //  On DOS 3.3 the Locked box is the lock.
        edits              = AppleFileProperties::Edits();
        edits.lockedEdited = true;
        edits.locked       = true;
        attributes = AppleFileProperties::MakeAttributes (FileEntry(), VolumeKind::Dos33, edits);
        Assert::AreEqual ((Byte) 0x01, attributes.access);
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
