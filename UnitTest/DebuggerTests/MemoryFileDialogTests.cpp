#include "Pch.h"

#include "Ui/Debugger/MemoryFileDialog.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MemoryFileDialogTests
    //
    //  The fields of File > Load file into memory... and Save memory to
    //  file...: the BLOAD or BSAVE they make, and the reason for fields that
    //  make none.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MemoryFileDialogTests)
    {
    public:
        using Format = MemoryFileDialog::Format;
        using Fields = MemoryFileDialog::Fields;

        static bool MakeLoad (const Fields & fields, DebuggerAction & action, std::wstring & error)
        {
            return MemoryFileDialog::TryMakeLoad ("game.bin", fields, CommandMode::AppleWin, action, error);
        }



        TEST_METHOD (HexIsOneToFourDigitsWithOrWithoutADollarSign)
        {
            uint32_t  value = 0;



            Assert::IsTrue   (MemoryFileDialog::TryParseHex (L" $6000 ", 4, value));
            Assert::AreEqual (0x6000u, value);
            Assert::IsTrue   (MemoryFileDialog::TryParseHex (L"c0", 4, value));
            Assert::AreEqual (0xC0u, value);
            Assert::IsTrue   (MemoryFileDialog::TryParseHex (L"10000", 5, value), L"a length of all memory");
            Assert::IsFalse  (MemoryFileDialog::TryParseHex (L"10000", 4, value), L"no five-digit address");
            Assert::IsFalse  (MemoryFileDialog::TryParseHex (L"$", 4, value));
            Assert::IsFalse  (MemoryFileDialog::TryParseHex (L"60g0", 4, value));
        }



        TEST_METHOD (ALoadIsBloadInItsFormatAtItsAddress)
        {
            DebuggerAction  action;
            std::wstring    error;



            Assert::IsTrue   (MakeLoad ({ Format::Detect, L"", L"" }, action, error), L"the format and address from the file");
            Assert::AreEqual (std::string ("BLOAD \"game.bin\""), action.echo);
            Assert::IsFalse  (action.command.hasA1);

            Assert::IsTrue   (MakeLoad ({ Format::Dos33, L"$4000", L"" }, action, error), L"in place of the file's address");
            Assert::AreEqual (std::string ("game.bin,DOS"), action.command.text);
            Assert::AreEqual ((Word) 0x4000, action.command.a1);

            Assert::IsTrue   (MakeLoad ({ Format::Raw, L"300", L"$100" }, action, error));
            Assert::AreEqual (std::string ("BLOAD \"game.bin\",RAW 0300,100"), action.echo);
            Assert::AreEqual ((Word) 0x03FF, action.command.a2, L"the last address");
        }



        TEST_METHOD (FieldsThatMakeNoLoadSayWhy)
        {
            DebuggerAction  action;
            std::wstring    error;



            Assert::IsFalse (MakeLoad ({ Format::Raw, L"", L"" }, action, error), L"no address for raw bytes");
            Assert::IsTrue  (error.find (L"address") != std::wstring::npos, error.c_str());

            Assert::IsFalse (MakeLoad ({ Format::IntelHex, L"$300", L"" }, action, error), L"an address for a HEX file");
            Assert::IsFalse (MakeLoad ({ Format::Detect, L"", L"$10" }, action, error), L"a length with no address");
            Assert::IsFalse (MakeLoad ({ Format::Raw, L"$FF00", L"$200" }, action, error), L"past the end of memory");
            Assert::IsFalse (MakeLoad ({ Format::Raw, L"$300", L"0" }, action, error), L"nothing to load");
            Assert::IsFalse (MakeLoad ({ Format::Raw, L"lots", L"" }, action, error));
        }



        TEST_METHOD (ASaveIsBsaveOfARangeThatNeedsBothFields)
        {
            DebuggerAction  action;
            std::wstring    error;



            Assert::IsTrue   (MemoryFileDialog::TryMakeSave ("page.bin", { Format::Raw, L"$400", L"$400" }, CommandMode::AppleWin, action, error));
            Assert::AreEqual (std::string ("BSAVE \"page.bin\" 0400,400"), action.echo);
            Assert::AreEqual ((Word) 0x07FF, action.command.a2);

            Assert::IsFalse  (MemoryFileDialog::TryMakeSave ("page.bin", { Format::Raw, L"$400", L"" }, CommandMode::AppleWin, action, error), L"no length");
            Assert::IsFalse  (MemoryFileDialog::TryMakeSave ("page.bin", { Format::Raw, L"", L"$10" }, CommandMode::AppleWin, action, error), L"no address");
        }
    };
}
