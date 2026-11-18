#include "Pch.h"

#include "Debugger/DebugAddress.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugAddressTests
//
//  The bank-qualified address: conversion from a Word, equality, the stop
//  matching rule, the text form with each separator, and the errors.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DebugAddressTests)
{
public:

    static constexpr HRESULT  kInvalidData = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);


    static DebugAddress ParseOk (std::string_view text, char separator)
    {
        DebugAddress  result;
        HRESULT       hr = S_OK;



        hr = DebugAddress::Parse (text, separator, result);
        Assert::AreEqual (S_OK, hr);

        return result;
    }


    static HRESULT ParseResult (std::string_view text, char separator)
    {
        DebugAddress  result;



        return DebugAddress::Parse (text, separator, result);
    }


    TEST_METHOD (Word_ConvertsToTheCurrentView)
    {
        DebugAddress  address = (Word) 0x3000;



        Assert::IsTrue (address.IsCurrentView());
        Assert::AreEqual ((Word) 0x3000, address.GetAddress());
    }


    TEST_METHOD (Equality_ComparesAddressSpaceAndBank)
    {
        Assert::IsTrue  (DebugAddress::MakeBank (2, 0x3000) == DebugAddress::MakeBank (2, 0x3000));
        Assert::IsFalse (DebugAddress::MakeBank (2, 0x3000) == DebugAddress::MakeBank (1, 0x3000));
        Assert::IsFalse (DebugAddress::MakeBank (2, 0x3000) == DebugAddress ((Word) 0x3000));
        Assert::IsFalse (DebugAddress::MakeSystem (0xA000) == DebugAddress::MakeRom (0xA000));
    }


    TEST_METHOD (Matches_CurrentViewIsEveryBank_ABankIsOnlyThatBank)
    {
        DebugAddress  anyBank = (Word) 0x3000;
        DebugAddress  bank2   = DebugAddress::MakeBank (2, 0x3000);



        Assert::IsTrue  (anyBank.Matches (DebugAddress::MakeBank (1, 0x3000)));
        Assert::IsTrue  (anyBank.Matches (DebugAddress::MakeBank (2, 0x3000)));
        Assert::IsTrue  (bank2.Matches   (DebugAddress::MakeBank (2, 0x3000)));
        Assert::IsFalse (bank2.Matches   (DebugAddress::MakeBank (1, 0x3000)));
        Assert::IsFalse (bank2.Matches   (DebugAddress::MakeBank (2, 0x3001)));
    }


    TEST_METHOD (Parse_EachSpaceWithEachSeparator)
    {
        for (char separator : { DebugAddress::kColon, DebugAddress::kSlash })
        {
            std::string  sep (1, separator);



            Assert::IsTrue (ParseOk ("3000",               separator) == DebugAddress ((Word) 0x3000));
            Assert::IsTrue (ParseOk ("$3000",              separator) == DebugAddress ((Word) 0x3000));
            Assert::IsTrue (ParseOk ("2" + sep + "3000",   separator) == DebugAddress::MakeBank (2, 0x3000));
            Assert::IsTrue (ParseOk ("e" + sep + "$9fff",  separator) == DebugAddress::MakeBank (0xE, 0x9FFF));
            Assert::IsTrue (ParseOk ("S" + sep + "A000",   separator) == DebugAddress::MakeSystem (0xA000));
            Assert::IsTrue (ParseOk (" r" + sep + "F00 ",  separator) == DebugAddress::MakeRom (0x0F00));
        }
    }


    TEST_METHOD (Parse_TheOtherSeparatorIsAnError)
    {
        Assert::AreEqual (kInvalidData, ParseResult ("2/3000", DebugAddress::kColon));
        Assert::AreEqual (kInvalidData, ParseResult ("2:3000", DebugAddress::kSlash));
    }


    TEST_METHOD (Parse_RejectsBadText)
    {
        Assert::AreEqual (kInvalidData, ParseResult ("",        DebugAddress::kColon));
        Assert::AreEqual (kInvalidData, ParseResult ("   ",     DebugAddress::kColon));
        Assert::AreEqual (kInvalidData, ParseResult ("12345",   DebugAddress::kColon));
        Assert::AreEqual (kInvalidData, ParseResult ("F:3000",  DebugAddress::kColon));
        Assert::AreEqual (kInvalidData, ParseResult ("X:3000",  DebugAddress::kColon));
        Assert::AreEqual (kInvalidData, ParseResult ("2:",      DebugAddress::kColon));
        Assert::AreEqual (kInvalidData, ParseResult ("2:30G0",  DebugAddress::kColon));
    }


    TEST_METHOD (Format_EachSpaceWithEachSeparator)
    {
        Assert::AreEqual (std::string ("3000"),   DebugAddress ((Word) 0x3000).Format (DebugAddress::kColon));
        Assert::AreEqual (std::string ("2:3000"), DebugAddress::MakeBank (2, 0x3000).Format (DebugAddress::kColon));
        Assert::AreEqual (std::string ("2/3000"), DebugAddress::MakeBank (2, 0x3000).Format (DebugAddress::kSlash));
        Assert::AreEqual (std::string ("E:00FF"), DebugAddress::MakeBank (0xE, 0x00FF).Format (DebugAddress::kColon));
        Assert::AreEqual (std::string ("S:A000"), DebugAddress::MakeSystem (0xA000).Format (DebugAddress::kColon));
        Assert::AreEqual (std::string ("R/F000"), DebugAddress::MakeRom (0xF000).Format (DebugAddress::kSlash));
    }


    TEST_METHOD (Validate_BankOutsideTheBankedRegionOrPastTheLastIsAnError)
    {
        Assert::AreEqual (S_OK,         DebugAddress::MakeBank (2, 0x2000).Validate (3));
        Assert::AreEqual (S_OK,         DebugAddress::MakeBank (2, 0x9FFF).Validate (3));
        Assert::AreEqual (kInvalidData, DebugAddress::MakeBank (2, 0x1FFF).Validate (3));
        Assert::AreEqual (kInvalidData, DebugAddress::MakeBank (2, 0xA000).Validate (3));
        Assert::AreEqual (kInvalidData, DebugAddress::MakeBank (3, 0x3000).Validate (3));
        Assert::AreEqual (kInvalidData, DebugAddress::MakeBank (0, 0x3000).Validate (0));
        Assert::AreEqual (S_OK,         DebugAddress::MakeSystem (0xC000).Validate (0));
        Assert::AreEqual (S_OK,         DebugAddress ((Word) 0x3000).Validate (0));
    }


    TEST_METHOD (FormatBankRule_ListsTheLastBank)
    {
        Assert::AreEqual (std::string ("A bank applies only to addresses $2000-$9FFF, and this machine has banks 0 through 6."),
                          DebugAddress::FormatBankRule (7));
    }
};
