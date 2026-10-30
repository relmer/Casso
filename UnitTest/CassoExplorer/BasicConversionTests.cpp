#include "Pch.h"
#include "CassoExplorer/Model/BasicConversion.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "ApplesoftTokenizer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BasicConversionTests
//
//  Convert to's rules, and Lock's access bytes, with no window.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BasicConversionTests)
{
public:

    TEST_METHOD (Kind_IsAListingForAProgram_AndAProgramForText)
    {
        using Kind = BasicConversion::Kind;

        Assert::IsTrue (BasicConversion::GetKind (VolumeKind::Dos33,  0x02) == Kind::ToListing);
        Assert::IsTrue (BasicConversion::GetKind (VolumeKind::Dos33,  0x01) == Kind::ToListing, L"Integer BASIC lists too");
        Assert::IsTrue (BasicConversion::GetKind (VolumeKind::Dos33,  0x00) == Kind::ToApplesoft);
        Assert::IsTrue (BasicConversion::GetKind (VolumeKind::Dos33,  0x04) == Kind::None, L"a binary converts to nothing");
        Assert::IsTrue (BasicConversion::GetKind (VolumeKind::ProDos, 0xFC) == Kind::ToListing);
        Assert::IsTrue (BasicConversion::GetKind (VolumeKind::ProDos, 0xFA) == Kind::ToListing);
        Assert::IsTrue (BasicConversion::GetKind (VolumeKind::ProDos, 0x04) == Kind::ToApplesoft);
        Assert::IsTrue (BasicConversion::GetKind (VolumeKind::ProDos, 0x06) == Kind::None);
    }



    //  A program converted to its listing and back is the same program.
    TEST_METHOD (Convert_ListsAProgram_AndTokenizesTheListingBack)
    {
        FilePayload            program;
        FilePayload            listing;
        FilePayload            again;
        std::wstring           error;
        ApplesoftListingError  tokenizeError;

        AssertSucceeded (ApplesoftTokenizer::Tokenize ("10 PRINT \"HELLO\"\n20 GOTO 10\n", program.bytes, tokenizeError));
        program.type = BasicConversion::kDos33Applesoft;

        AssertSucceeded (BasicConversion::Convert (program, VolumeKind::Dos33, BasicConversion::Kind::ToListing, listing, error));
        Assert::AreEqual (BasicConversion::kDos33Text, listing.type);
        Assert::IsTrue   ((listing.bytes[0] & 0x80) != 0, L"text on the disk is high-bit ASCII");

        AssertSucceeded (BasicConversion::Convert (listing, VolumeKind::Dos33, BasicConversion::Kind::ToApplesoft, again, error));
        Assert::AreEqual (BasicConversion::kDos33Applesoft, again.type);
        Assert::IsTrue   (again.bytes == program.bytes, L"the same program after the round trip");
    }



    TEST_METHOD (Convert_ReportsTextThatIsNotAListing)
    {
        FilePayload   text;
        FilePayload   program;
        std::wstring  error;

        text.bytes = { 0xC8, 0xC5, 0xCC, 0xCC, 0xCF, 0x8D };   // HELLO, with no line number
        text.type  = BasicConversion::kProDosText;

        HRESULT  hr = BasicConversion::Convert (text, VolumeKind::ProDos, BasicConversion::Kind::ToApplesoft, program, error);

        Assert::IsTrue  (FAILED (hr));
        Assert::IsFalse (error.empty(), L"the reason, for the message");
    }



    TEST_METHOD (Name_AddsTheSuffix_FitsTheFileSystem_AndAvoidsTakenNames)
    {
        using Kind = BasicConversion::Kind;

        Assert::AreEqual (std::string ("HELLO.LIST"),      BasicConversion::MakeName ("HELLO", VolumeKind::Dos33, Kind::ToListing, {}));
        Assert::AreEqual (std::string ("STARTUP.BAS"),     BasicConversion::MakeName ("STARTUP", VolumeKind::ProDos, Kind::ToApplesoft, {}));
        Assert::AreEqual (std::string ("ABCDEFGHIJ.LIST"), BasicConversion::MakeName ("ABCDEFGHIJKLMNO", VolumeKind::ProDos, Kind::ToListing, {}), L"15 characters on ProDOS");
        Assert::AreNotEqual (std::string ("HELLO.LIST"),   BasicConversion::MakeName ("HELLO", VolumeKind::Dos33, Kind::ToListing, { "HELLO.LIST" }));
    }



    //  Lock clears ProDOS's destroy, rename and write bits, not read and
    //  backup; Unlock sets them. A DOS 3.3 entry has no access byte.
    TEST_METHOD (LockAttributes_ChangeOnlyTheLockBits)
    {
        FileEntry  proDos;
        FileEntry  dos33;

        proDos.access    = 0xE3;
        proDos.hasAccess = true;

        Assert::AreEqual ((Byte) 0x21, CassoExplorerActions::MakeLockAttributes (proDos, true).access, L"backup and read stay");
        Assert::IsTrue   (CassoExplorerActions::MakeLockAttributes (proDos, true).hasAccess);

        proDos.access = 0x21;
        Assert::AreEqual ((Byte) 0xE3, CassoExplorerActions::MakeLockAttributes (proDos, false).access);

        Assert::AreEqual ((Byte) 0x01, CassoExplorerActions::MakeLockAttributes (dos33, true).access);
        Assert::AreEqual ((Byte) 0xC3, CassoExplorerActions::MakeLockAttributes (dos33, false).access);
    }
};
